#include <stdafx.hpp>

namespace
{
	constexpr auto autostop_btn_forward = static_cast< std::uintptr_t >( cstypes::in_forward );
	constexpr auto autostop_btn_back = static_cast< std::uintptr_t >( cstypes::in_back );
	constexpr auto autostop_btn_left = static_cast< std::uintptr_t >( cstypes::in_moveleft );
	constexpr auto autostop_btn_right = static_cast< std::uintptr_t >( cstypes::in_moveright );
	constexpr auto autostop_btn_mask = autostop_btn_forward | autostop_btn_back | autostop_btn_left | autostop_btn_right;

	[[nodiscard]] std::uintptr_t autostop_buttons_from_keys( const std::vector<std::uint16_t>& keys )
	{
		auto mask = static_cast< std::uintptr_t >( 0 );

		for ( const auto key : keys )
		{
			switch ( key )
			{
			case 'W': mask |= autostop_btn_forward; break;
			case 'S': mask |= autostop_btn_back; break;
			case 'A': mask |= autostop_btn_left; break;
			case 'D': mask |= autostop_btn_right; break;
			default: break;
			}
		}

		return mask;
	}

	[[nodiscard]] bool set_autostop_movement_buttons( std::uintptr_t pawn, std::uintptr_t pressed_mask )
	{
		if ( !pawn )
		{
			return false;
		}

		const auto movement_services = g::memory.read<std::uintptr_t>( pawn + SCHEMA( "C_BasePlayerPawn", "m_pMovementServices"_hash ) );
		if ( !movement_services )
		{
			return false;
		}

		auto buttons = g::memory.read<std::uintptr_t>( movement_services + SCHEMA( "CPlayer_MovementServices", "m_nButtons"_hash ) );
		buttons &= ~autostop_btn_mask;
		buttons |= ( pressed_mask & autostop_btn_mask );
		g::memory.write<std::uintptr_t>( movement_services + SCHEMA( "CPlayer_MovementServices", "m_nButtons"_hash ), buttons );
		return true;
	}

	[[nodiscard]] float resolve_lethal_min_damage( float configured_min_damage, int target_health )
	{
		const auto safe_health = std::max( static_cast< float >( target_health ), 1.0f );
		if ( configured_min_damage > safe_health )
		{
			// Prefer a lethal threshold (health + 5) but never increase above the user slider.
			return std::min( configured_min_damage, safe_health + 5.0f );
		}

		return configured_min_damage;
	}

	[[nodiscard]] float autostop_distance_limit( const systems::collector::player& target )
	{
		// High HP + armored target: only engage autostop once we are close enough.
		// Unarmored or <=85 HP keeps infinite distance behavior.
		constexpr auto close_range_limit{ 650.0f };
		if ( target.health > 85 && target.armor > 0 )
		{
			return close_range_limit;
		}

		return std::numeric_limits<float>::infinity( );
	}

	[[nodiscard]] bool autostop_within_distance( const math::vector3& from_pos, const systems::collector::player& target )
	{
		const auto limit = autostop_distance_limit( target );
		if ( !std::isfinite( limit ) )
		{
			return true;
		}

		const auto dist_2d = ( target.origin - from_pos ).length_2d( );
		return dist_2d <= limit;
	}

	[[nodiscard]] bool legit_mode_active( ) noexcept
	{
		return settings::g_misc.m_safe_mode;
	}

	[[nodiscard]] bool local_player_in_air( )
	{
		const auto pawn = systems::g_local.view_pawn( );
		if ( !pawn )
		{
			return false;
		}

		const auto flags = g::memory.read<std::uint32_t>( pawn + SCHEMA( "C_BaseEntity", "m_fFlags"_hash ) );
		return ( flags & 1u ) == 0u;
	}

	[[nodiscard]] bool local_player_noclip( std::uintptr_t pawn )
	{
		if ( !pawn )
		{
			return false;
		}

		// Source move type: noclip is 8.
		constexpr auto movetype_noclip{ static_cast< std::uint8_t >( 8 ) };
		static const auto actual_move_type_off = systems::g_schemas.lookup( "C_BaseEntity", "m_nActualMoveType"_hash );
		static const auto move_type_off = systems::g_schemas.lookup( "C_BaseEntity", "m_MoveType"_hash );

		if ( actual_move_type_off > 0 )
		{
			const auto move_type = g::memory.read<std::uint8_t>( pawn + static_cast< std::uintptr_t >( actual_move_type_off ) );
			if ( move_type == movetype_noclip )
			{
				return true;
			}
		}

		if ( move_type_off > 0 )
		{
			const auto move_type = g::memory.read<std::uint8_t>( pawn + static_cast< std::uintptr_t >( move_type_off ) );
			if ( move_type == movetype_noclip )
			{
				return true;
			}
		}

		return false;
	}

	[[nodiscard]] bool semirage_auto_mode_enabled( const settings::combat::group_config& cfg ) noexcept
	{
		return !legit_mode_active( ) && cfg.triggerbot.auto_mode;
	}

	[[nodiscard]] float semirage_auto_hitchance( const math::vector3& from_pos, const systems::collector::player& target )
	{
		if ( local_player_in_air( ) )
		{
			// Jump profile:
			// - scoped: keep stricter in-air hitchance
			// - unscoped: stay permissive for jump shots
			const auto& live_ctx = features::combat::g_shared.ctx( );
			const auto scoped = live_ctx.valid && live_ctx.is_scoped;
			return scoped ? 50.0f : 30.0f;
		}

		const auto distance = ( target.origin - from_pos ).length_2d( );
		const auto dist_factor = std::clamp( distance / 2200.0f, 0.0f, 1.0f );

		auto hitchance = 78.0f - dist_factor * 28.0f; // close: 78, far: 50

		if ( target.speed_2d < 20.0f )
		{
			hitchance += 8.0f;
		}
		else if ( target.speed_2d > 180.0f )
		{
			hitchance -= 8.0f;
		}

		if ( target.health <= 40 )
		{
			hitchance += 5.0f;
		}

		return std::clamp( hitchance, 30.0f, 92.0f );
	}

	[[nodiscard]] float semirage_auto_min_damage( const math::vector3& from_pos, const systems::collector::player& target )
	{
		const auto clamped_health = std::clamp( target.health, 1, 100 );
		const auto lethal = clamped_health >= 100
			? 100.0f
			: std::clamp( static_cast< float >( clamped_health ) + 5.0f, 35.0f, 100.0f );

		if ( local_player_in_air( ) )
		{
			// Jump profile: always force lethal threshold.
			return lethal;
		}

		const auto distance = ( target.origin - from_pos ).length_2d( );
		const auto dist_factor = std::clamp( distance / 2200.0f, 0.0f, 1.0f );

		auto min_damage = 35.0f + dist_factor * 12.0f; // base: 35, far: 47

		if ( target.speed_2d < 20.0f )
		{
			// Target mostly still -> allow stronger body preference.
			min_damage -= 6.0f;
		}
		else if ( target.speed_2d > 180.0f )
		{
			min_damage += 8.0f;
		}

		// Auto mode should prefer lethal threshold.
		min_damage = std::max( min_damage, lethal );

		return std::clamp( min_damage, 35.0f, 100.0f );
	}

	[[nodiscard]] bool weapon_supports_scope( const features::combat::shared::context& ctx )
	{
		if ( !ctx.weapon_vdata )
		{
			return false;
		}

		const auto zoom_levels = g::memory.read<std::int32_t>(
			ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_nZoomLevels"_hash ) );
		return zoom_levels > 0;
	}

	[[nodiscard]] int weapon_zoom_levels( const features::combat::shared::context& ctx )
	{
		if ( !ctx.weapon_vdata )
		{
			return 0;
		}

		const auto levels = g::memory.read<std::int32_t>(
			ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_nZoomLevels"_hash ) );
		return std::clamp( levels, 0, 8 );
	}

	[[nodiscard]] int weapon_zoom_level( const features::combat::shared::context& ctx )
	{
		if ( !ctx.weapon )
		{
			return ctx.is_scoped ? 1 : 0;
		}

		const auto level = g::memory.read<std::int32_t>(
			ctx.weapon + SCHEMA( "C_CSWeaponBaseGun", "m_zoomLevel"_hash ) );
		if ( level < 0 || level > 8 )
		{
			return ctx.is_scoped ? 1 : 0;
		}

		return level;
	}

	[[nodiscard]] int desired_auto_scope_zoom_level( const features::combat::shared::context& ctx )
	{
		const auto levels = weapon_zoom_levels( ctx );
		if ( levels <= 0 )
		{
			return 0;
		}

		if ( ctx.weapon_type == cstypes::sniper )
		{
			// For snipers, user wants max precision: go to second zoom when available.
			return std::min( 2, levels );
		}

		return 1;
	}

	[[nodiscard]] bool auto_scope_needs_toggle( const features::combat::shared::context& ctx )
	{
		const auto desired = desired_auto_scope_zoom_level( ctx );
		if ( desired <= 0 )
		{
			return false;
		}

		const auto current = weapon_zoom_level( ctx );
		return current < desired;
	}

	[[nodiscard]] bool weapon_is_currently_scoped( const features::combat::shared::context& ctx )
	{
		if ( ctx.is_scoped )
		{
			return true;
		}

		if ( !ctx.weapon )
		{
			return false;
		}

		const auto zoom_level = g::memory.read<std::int32_t>(
			ctx.weapon + SCHEMA( "C_CSWeaponBaseGun", "m_zoomLevel"_hash ) );
		return zoom_level > 0;
	}

	const settings::combat::group_config& resolve_combat_cfg( std::uint16_t item_def_idx, std::uint32_t weapon_type )
	{
		if ( legit_mode_active( ) )
		{
			return settings::g_legit_bot.resolve_runtime_cfg( item_def_idx, weapon_type );
		}

		return settings::g_combat.resolve_runtime_cfg( item_def_idx, weapon_type );
	}

	struct recoil_snapshot
	{
		int shots_fired{};
		math::vector2 punch{};
		float deg_per_pixel{};
	};

	struct recoil_runtime
	{
		math::vector2 old_punch{};
		math::vector2 residual{};
		int last_shots_fired{ -1 };
		bool has_old_punch{ false };
		bool suppress_mouse_this_tick{ false };
	};

	inline recoil_runtime g_recoil_runtime{};

	void reset_recoil_runtime( )
	{
		g_recoil_runtime.old_punch = {};
		g_recoil_runtime.residual = {};
		g_recoil_runtime.last_shots_fired = -1;
		g_recoil_runtime.has_old_punch = false;
	}

	[[nodiscard]] float clamp_recoil_strength( float strength )
	{
		return std::clamp( strength, 0.0f, 100.0f ) * 0.01f;
	}

	[[nodiscard]] bool read_recoil_snapshot( recoil_snapshot& out )
	{
		const auto pawn = systems::g_local.view_pawn( );
		if ( !pawn )
		{
			return false;
		}

		const auto health = g::memory.read<std::int32_t>( pawn + SCHEMA( "C_BaseEntity", "m_iHealth"_hash ) );
		if ( health <= 0 )
		{
			return false;
		}

		const auto life_state = g::memory.read<std::uint8_t>( pawn + SCHEMA( "C_BaseEntity", "m_lifeState"_hash ) );
		if ( life_state != 0 )
		{
			return false;
		}

		out.shots_fired = g::memory.read<int>( pawn + SCHEMA( "C_CSPlayerPawn", "m_iShotsFired"_hash ) );
		const auto aim_punch = g::memory.read<math::vector3>( pawn + SCHEMA( "C_CSPlayerPawn", "m_aimPunchAngle"_hash ) );
		out.punch = { aim_punch.x, aim_punch.y };

		const auto sensitivity = systems::g_convars.get<float>( CONVAR( "sensitivity"_hash ) );
		auto fov_adjust = g::memory.read<float>( pawn + SCHEMA( "C_BasePlayerPawn", "m_flFOVSensitivityAdjust"_hash ) );
		if ( !std::isfinite( fov_adjust ) || fov_adjust <= 0.0f )
		{
			fov_adjust = 1.0f;
		}

		constexpr auto m_yaw{ 0.022f };
		out.deg_per_pixel = sensitivity * m_yaw * fov_adjust;
		if ( !std::isfinite( out.deg_per_pixel ) || out.deg_per_pixel <= 0.0f )
		{
			out.deg_per_pixel = 0.0f;
			return false;
		}

		return std::isfinite( out.punch.x ) && std::isfinite( out.punch.y );
	}

	[[nodiscard]] math::vector2 scaled_recoil_punch( const recoil_snapshot& snapshot, const settings::combat::recoil_control_system& cfg )
	{
		const auto strength = clamp_recoil_strength( cfg.strength );
		return
		{
			snapshot.punch.x * strength,
			snapshot.punch.y * strength
		};
	}

	void apply_external_recoil_control( const settings::combat::recoil_control_system& cfg )
	{
		recoil_snapshot snapshot{};
		if ( !read_recoil_snapshot( snapshot ) )
		{
			reset_recoil_runtime( );
			return;
		}

		const auto start_bullet = std::clamp( cfg.start_bullet, 0, 5 );
		if ( snapshot.shots_fired <= start_bullet )
		{
			reset_recoil_runtime( );
			return;
		}

		const auto scaled_punch = scaled_recoil_punch( snapshot, cfg );
		const auto apply_mouse_delta = [ & ]( float delta_pitch, float delta_yaw )
			{
				const auto raw_move_x = ( delta_yaw * 2.0f ) / snapshot.deg_per_pixel + g_recoil_runtime.residual.x;
				const auto raw_move_y = -( delta_pitch * 2.0f ) / snapshot.deg_per_pixel + g_recoil_runtime.residual.y;

				if ( !std::isfinite( raw_move_x ) || !std::isfinite( raw_move_y ) )
				{
					reset_recoil_runtime( );
					return;
				}

				const auto move_x = static_cast< int >( std::lround( raw_move_x ) );
				const auto move_y = static_cast< int >( std::lround( raw_move_y ) );

				g_recoil_runtime.residual.x = raw_move_x - static_cast< float >( move_x );
				g_recoil_runtime.residual.y = raw_move_y - static_cast< float >( move_y );

				if ( move_x != 0 || move_y != 0 )
				{
					g::input.inject_mouse( move_x, move_y, input::move );
				}
			};
		if ( !g_recoil_runtime.has_old_punch || snapshot.shots_fired < g_recoil_runtime.last_shots_fired )
		{
			g_recoil_runtime.old_punch = scaled_punch;
			g_recoil_runtime.last_shots_fired = snapshot.shots_fired;
			g_recoil_runtime.has_old_punch = true;
			g_recoil_runtime.residual = {};
			return;
		}

		if ( snapshot.shots_fired == g_recoil_runtime.last_shots_fired )
		{
			return;
		}

		const auto delta_pitch = scaled_punch.x - g_recoil_runtime.old_punch.x;
		auto delta_yaw = scaled_punch.y - g_recoil_runtime.old_punch.y;

		// Lateral stabilizer tuned for sustained spray:
		// keep initial pull responsive, then progressively tighten horizontal correction.
		const auto spray_progress = std::clamp( snapshot.shots_fired - ( start_bullet + 1 ), 0, 20 );
		const auto sustained_progress = std::clamp( spray_progress - 3, 0, 12 );
		const auto yaw_dampen = 0.84f - static_cast< float >( sustained_progress ) * 0.015f;
		delta_yaw *= yaw_dampen;
		const auto yaw_deadzone = 0.0018f + static_cast< float >( sustained_progress ) * 0.00012f;
		if ( std::fabs( delta_yaw ) < yaw_deadzone )
		{
			delta_yaw = 0.0f;
			g_recoil_runtime.residual.x *= 0.55f;
		}
		const auto yaw_limit = 0.40f - static_cast< float >( sustained_progress ) * 0.01f;
		delta_yaw = std::clamp( delta_yaw, -yaw_limit, yaw_limit );

		apply_mouse_delta( delta_pitch, delta_yaw );

		g_recoil_runtime.old_punch = scaled_punch;
		g_recoil_runtime.last_shots_fired = snapshot.shots_fired;
		g_recoil_runtime.has_old_punch = true;
	}

	void apply_aimbot_recoil_compensation( math::vector3& desired_angles, const settings::combat::recoil_control_system& cfg, bool suppress_external_this_tick )
	{
		recoil_snapshot snapshot{};
		if ( !read_recoil_snapshot( snapshot ) )
		{
			reset_recoil_runtime( );
			return;
		}

		const auto start_bullet = std::clamp( cfg.start_bullet, 0, 5 );
		if ( snapshot.shots_fired <= start_bullet )
		{
			reset_recoil_runtime( );
			return;
		}

		const auto scaled_punch = scaled_recoil_punch( snapshot, cfg );

		desired_angles.x = std::clamp( desired_angles.x - scaled_punch.x * 2.0f, -89.0f, 89.0f );
		desired_angles.y = math::helpers::normalize_yaw( desired_angles.y - scaled_punch.y * 2.0f );

		g_recoil_runtime.old_punch = scaled_punch;
		g_recoil_runtime.last_shots_fired = snapshot.shots_fired;
		g_recoil_runtime.has_old_punch = true;
		g_recoil_runtime.residual = {};
		g_recoil_runtime.suppress_mouse_this_tick = suppress_external_this_tick;
	}

	[[nodiscard]] math::vector3 read_live_view_angles( const math::vector3& fallback )
	{
		if ( !g::offsets.view_angles )
		{
			return fallback;
		}

		const auto live = g::memory.read<math::vector3>( g::offsets.view_angles );
		if ( !std::isfinite( live.x ) || !std::isfinite( live.y ) )
		{
			return fallback;
		}

		return live;
	}

	[[nodiscard]] int early_autostop_hitbox_count( const settings::combat::aimbot& aimbot_cfg, int active_target_hitbox = -1 )
	{
		if ( aimbot_cfg.head_only )
		{
			// Head-only can be dynamically unlocked by lethal logic when a non-head
			// hitbox is currently selected.
			if ( active_target_hitbox >= 0 && active_target_hitbox != 0 )
			{
				return 2;
			}

			return 1;
		}

		auto mask = aimbot_cfg.all_hitboxes
			? settings::combat::aimbot::hitbox_all
			: aimbot_cfg.hitbox_mask;

		if ( mask == 0u )
		{
			mask = settings::combat::aimbot::hitbox_head;
		}

		// Keep behavior consistent with runtime selection rules.
		if ( ( mask & settings::combat::aimbot::hitbox_head ) != 0u )
		{
			mask &= ~settings::combat::aimbot::hitbox_neck;
		}

		if ( active_target_hitbox >= 0 )
		{
			const auto active_bit = [ ]( const int hitbox_index ) -> std::uint32_t
				{
					switch ( hitbox_index )
					{
					case 0:  return settings::combat::aimbot::hitbox_head;
					case 1:  return settings::combat::aimbot::hitbox_neck;
					case 6:  return settings::combat::aimbot::hitbox_upper_chest;
					case 5:  return settings::combat::aimbot::hitbox_chest_center;
					case 3:  return settings::combat::aimbot::hitbox_stomach;
					case 4:  return settings::combat::aimbot::hitbox_lower_torso;
					case 2:  return settings::combat::aimbot::hitbox_pelvis;
					case 7:  return settings::combat::aimbot::hitbox_left_shoulder;
					case 8:  return settings::combat::aimbot::hitbox_right_shoulder;
					case 9:  return settings::combat::aimbot::hitbox_left_upper_arm;
					case 10: return settings::combat::aimbot::hitbox_right_upper_arm;
					case 11: return settings::combat::aimbot::hitbox_left_hand;
					case 12: return settings::combat::aimbot::hitbox_right_hand;
					case 13: return settings::combat::aimbot::hitbox_left_hip;
					case 14: return settings::combat::aimbot::hitbox_right_hip;
					case 15: return settings::combat::aimbot::hitbox_left_knee;
					case 16: return settings::combat::aimbot::hitbox_right_knee;
					case 17: return settings::combat::aimbot::hitbox_left_foot;
					case 18: return settings::combat::aimbot::hitbox_right_foot;
					default: return 0u;
					}
				}( active_target_hitbox );

			// If lethal unlock picked a hitbox outside the configured mask, treat as multi-hitbox behavior.
			if ( active_bit != 0u && ( mask & active_bit ) == 0u )
			{
				return 2;
			}
		}

		auto count = 0;
		for ( auto bits = mask; bits != 0u; bits >>= 1u )
		{
			count += static_cast< int >( bits & 1u );
		}

		return ( std::max )( count, 1 );
	}

	[[nodiscard]] bool hitbox_matches_mask( int hitbox_index, std::uint32_t mask )
	{
		if ( mask == 0u )
		{
			mask = settings::combat::aimbot::hitbox_head;
		}

		const auto has = [ mask ]( std::uint32_t bit ) -> bool
			{
				return ( mask & bit ) != 0u;
			};

		// One menu option == one concrete CS2 hitbox index (no grouped regions).
		switch ( hitbox_index )
		{
		case 0:  // head
			return has( settings::combat::aimbot::hitbox_head );
		case 1:  // neck
			return has( settings::combat::aimbot::hitbox_neck );
		case 6:  // upper chest
			return has( settings::combat::aimbot::hitbox_upper_chest );
		case 5:  // chest
			return has( settings::combat::aimbot::hitbox_chest_center );
		case 3:  // stomach
			return has( settings::combat::aimbot::hitbox_stomach );
		case 4:  // lower torso
			return has( settings::combat::aimbot::hitbox_lower_torso );
		case 2:  // pelvis
			return has( settings::combat::aimbot::hitbox_pelvis );
		case 7:  // left shoulder
			return has( settings::combat::aimbot::hitbox_left_shoulder );
		case 8:  // right shoulder
			return has( settings::combat::aimbot::hitbox_right_shoulder );
		case 9:  // left upper arm
			return has( settings::combat::aimbot::hitbox_left_upper_arm );
		case 10: // right upper arm
			return has( settings::combat::aimbot::hitbox_right_upper_arm );
		case 11: // left hand
			return has( settings::combat::aimbot::hitbox_left_hand );
		case 12: // right hand
			return has( settings::combat::aimbot::hitbox_right_hand );
		case 13: // left hip
			return has( settings::combat::aimbot::hitbox_left_hip );
		case 14: // right hip
			return has( settings::combat::aimbot::hitbox_right_hip );
		case 15: // left knee
			return has( settings::combat::aimbot::hitbox_left_knee );
		case 16: // right knee
			return has( settings::combat::aimbot::hitbox_right_knee );
		case 17: // left foot
			return has( settings::combat::aimbot::hitbox_left_foot );
		case 18: // right foot
			return has( settings::combat::aimbot::hitbox_right_foot );
		default:
			return false;
		}
	}

	[[nodiscard]] std::uint32_t resolve_hitbox_bit( int hitbox_index, std::uint32_t mask )
	{
		if ( mask == 0u )
		{
			mask = settings::combat::aimbot::hitbox_head;
		}

		switch ( hitbox_index )
		{
		case 0:
			return settings::combat::aimbot::hitbox_head;
		case 1:
			return settings::combat::aimbot::hitbox_neck;
		case 6:
			return settings::combat::aimbot::hitbox_upper_chest;
		case 5:
			return settings::combat::aimbot::hitbox_chest_center;
		case 3:
			return settings::combat::aimbot::hitbox_stomach;
		case 4:
			return settings::combat::aimbot::hitbox_lower_torso;
		case 2:
			return settings::combat::aimbot::hitbox_pelvis;
		case 7:
			return settings::combat::aimbot::hitbox_left_shoulder;
		case 8:
			return settings::combat::aimbot::hitbox_right_shoulder;
		case 9:
			return settings::combat::aimbot::hitbox_left_upper_arm;
		case 10:
			return settings::combat::aimbot::hitbox_right_upper_arm;
		case 11:
			return settings::combat::aimbot::hitbox_left_hand;
		case 12:
			return settings::combat::aimbot::hitbox_right_hand;
		case 13:
			return settings::combat::aimbot::hitbox_left_hip;
		case 14:
			return settings::combat::aimbot::hitbox_right_hip;
		case 15:
			return settings::combat::aimbot::hitbox_left_knee;
		case 16:
			return settings::combat::aimbot::hitbox_right_knee;
		case 17:
			return settings::combat::aimbot::hitbox_left_foot;
		case 18:
			return settings::combat::aimbot::hitbox_right_foot;
		default:
			return 0u;
		}
	}

	[[nodiscard]] int fallback_bone_from_hitgroup( int hitgroup )
	{
		switch ( hitgroup )
		{
		case 1: return cstypes::head;
		case 8: return cstypes::neck;
		case 2: return cstypes::spine_2;      // chest
		case 3: return cstypes::spine_0;      // stomach / pelvis
		case 4: return cstypes::left_upper_arm;
		case 5: return cstypes::right_upper_arm;
		case 6: return cstypes::left_knee;
		case 7: return cstypes::right_knee;
		default: return -1;
		}
	}

	[[nodiscard]] int resolve_bone_for_hitbox(
		const systems::collector::player& player,
		int hitbox_index,
		int hitgroup,
		int fallback_bone = -1 )
	{
		for ( const auto& hb : player.hitboxes )
		{
			if ( hb.index == hitbox_index && hb.bone >= 0 )
			{
				return hb.bone;
			}
		}

		if ( fallback_bone >= 0 )
		{
			return fallback_bone;
		}

		return fallback_bone_from_hitgroup( hitgroup );
	}

	[[nodiscard]] int hitbox_priority_from_mask( std::uint32_t mask, std::uint32_t bit )
	{
		if ( mask == 0u )
		{
			mask = settings::combat::aimbot::hitbox_head;
		}

		constexpr std::array<std::uint32_t, 19> k_hitbox_priority_order
		{
			settings::combat::aimbot::hitbox_head,
			settings::combat::aimbot::hitbox_neck,
			settings::combat::aimbot::hitbox_upper_chest,
			settings::combat::aimbot::hitbox_chest_center,
			settings::combat::aimbot::hitbox_stomach,
			settings::combat::aimbot::hitbox_lower_torso,
			settings::combat::aimbot::hitbox_pelvis,
			settings::combat::aimbot::hitbox_left_shoulder,
			settings::combat::aimbot::hitbox_right_shoulder,
			settings::combat::aimbot::hitbox_left_upper_arm,
			settings::combat::aimbot::hitbox_right_upper_arm,
			settings::combat::aimbot::hitbox_left_hand,
			settings::combat::aimbot::hitbox_right_hand,
			settings::combat::aimbot::hitbox_left_hip,
			settings::combat::aimbot::hitbox_right_hip,
			settings::combat::aimbot::hitbox_left_knee,
			settings::combat::aimbot::hitbox_right_knee,
			settings::combat::aimbot::hitbox_left_foot,
			settings::combat::aimbot::hitbox_right_foot
		};

		auto rank{ 0 };
		for ( const auto ordered_bit : k_hitbox_priority_order )
		{
			if ( ( mask & ordered_bit ) == 0u )
			{
				continue;
			}

			if ( ordered_bit == bit )
			{
				return rank;
			}

			++rank;
		}

		return std::numeric_limits<int>::max( );
	}

}

namespace features::combat {

	void legit::on_render( )
	{
		const auto eye_pos = systems::g_view.origin( );
		const auto view_angles = systems::g_view.angles( );

		const auto& ctx = g_shared.ctx( );
		if ( !ctx.valid )
		{
			return;
		}

		const auto view_pawn = systems::g_local.view_pawn( );
		const auto view_alive = view_pawn
			&& g::memory.read<std::int32_t>( view_pawn + SCHEMA( "C_BaseEntity", "m_iHealth"_hash ) ) > 0
			&& g::memory.read<std::uint8_t>( view_pawn + SCHEMA( "C_BaseEntity", "m_lifeState"_hash ) ) == 0;
		if ( !view_alive )
		{
			this->m_fov_alpha.set_target( 0.0f );
			this->m_fov_alpha.update( );
			return;
		}

		const auto valid_weapon = cstypes::is_weapon_valid( ctx.weapon_type );
		const auto& cfg = resolve_combat_cfg( ctx.item_def_idx, ctx.weapon_type );

		this->m_fov_alpha.set_target( valid_weapon && cfg.aimbot.draw_fov && cfg.aimbot.enabled ? 1.0f : 0.0f );
		this->m_fov_alpha.update( );

		if ( this->m_fov_alpha.value( ) <= 0.01f )
		{
			return;
		}

		this->draw_fov( eye_pos, view_angles, cfg.aimbot );
	}

	void legit::tick( )
	{
		if ( !this->m_rng_seeded )
		{
			this->m_rng.seed( static_cast< int >( std::chrono::steady_clock::now( ).time_since_epoch( ).count( ) & 0x7fffffff ) );
			this->m_rng_seeded = true;
		}

		this->m_autostop_requested_tick = false;

		if ( this->m_trigger_held )
		{
			const auto& ctx = g_shared.ctx( );
			if ( !ctx.valid || ( !this->m_trigger_continuous_hold && ctx.current_time >= this->m_trigger_release_time ) )
			{
				g::input.inject_mouse( 0, 0, input::left_up );
				this->m_trigger_held = false;
				this->m_trigger_continuous_hold = false;
			}
		}

		if ( this->m_auto_scope_held )
		{
			const auto& scope_ctx = g_shared.ctx( );
			if ( !scope_ctx.valid || scope_ctx.current_time >= this->m_auto_scope_release_time )
			{
				g::input.inject_mouse( 0, 0, input::right_up );
				this->m_auto_scope_held = false;
			}
		}

		const auto& ctx = g_shared.ctx( );
		if ( !ctx.valid )
		{
			this->m_in_air_autostop_latched = false;
			this->release_autostop( );
			this->m_auto_scope_held = false;
			this->m_auto_scope_release_time = 0.0f;
			this->m_auto_scope_next_time = 0.0f;
			reset_recoil_runtime( );
			return;
		}

		const auto local_pawn = systems::g_local.view_pawn( );
		const auto local_view_alive = local_pawn
			&& g::memory.read<std::int32_t>( local_pawn + SCHEMA( "C_BaseEntity", "m_iHealth"_hash ) ) > 0
			&& g::memory.read<std::uint8_t>( local_pawn + SCHEMA( "C_BaseEntity", "m_lifeState"_hash ) ) == 0;
		if ( !local_view_alive )
		{
			this->m_in_air_autostop_latched = false;
			this->release_autostop( );
			if ( this->m_auto_scope_held )
			{
				g::input.inject_mouse( 0, 0, input::right_up );
				this->m_auto_scope_held = false;
			}
			this->m_auto_scope_release_time = 0.0f;
			this->m_auto_scope_next_time = 0.0f;

			if ( this->m_trigger_held )
			{
				g::input.inject_mouse( 0, 0, input::left_up );
				this->m_trigger_held = false;
				this->m_trigger_continuous_hold = false;
			}

			this->m_locked_target_pawn = 0;
			this->m_locked_target_hitbox = -1;
			reset_recoil_runtime( );
			return;
		}

		const auto valid_weapon = cstypes::is_weapon_valid( ctx.weapon_type );
		const auto is_legit_cfg = legit_mode_active( );
		const auto& cfg = resolve_combat_cfg( ctx.item_def_idx, ctx.weapon_type );
		constexpr std::uint16_t scout_item_def{ 40 };
		const auto in_air_autostop_active = cfg.aimbot.in_air_autostop && ctx.item_def_idx == scout_item_def;
		g_recoil_runtime.suppress_mouse_this_tick = false;
		if ( !cfg.recoil_control_system.enabled )
		{
			reset_recoil_runtime( );
		}

		if ( !valid_weapon )
		{
			this->m_in_air_autostop_latched = false;
			this->release_autostop( );
			if ( this->m_auto_scope_held )
			{
				g::input.inject_mouse( 0, 0, input::right_up );
				this->m_auto_scope_held = false;
			}
			this->m_auto_scope_release_time = 0.0f;
			this->m_auto_scope_next_time = 0.0f;
			reset_recoil_runtime( );
			return;
		}

		const auto eye_pos = systems::g_view.origin( );
		const auto view_angles = systems::g_view.angles( );
		const auto players = systems::g_collector.players( );
		const auto pawn = local_pawn;
		const auto velocity = pawn ? g::memory.read<math::vector3>( pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) ) : math::vector3{};
		const auto speed = velocity.length_2d( );
		const auto flags = pawn ? g::memory.read<std::uint32_t>( pawn + SCHEMA( "C_BaseEntity", "m_fFlags"_hash ) ) : 0u;
		const auto on_ground = ( flags & 1u ) != 0;
		const auto noclip_active = local_player_noclip( pawn );
		const auto is_falling = !on_ground && velocity.z < -0.01f;
		const auto fall_speed = std::fabs( velocity.z );
		constexpr auto in_air_fall_dead_zone_speed{ 95.0f };
		const auto passed_fall_dead_zone = is_falling && fall_speed > in_air_fall_dead_zone_speed;
		const auto legit_autostop_move_ready = is_legit_cfg && cfg.aimbot.autostop && cfg.triggerbot.enabled && pawn && on_ground && speed > 5.0f;
		if ( on_ground || !cfg.aimbot.autostop || !in_air_autostop_active || is_legit_cfg || ctx.is_reloading || passed_fall_dead_zone || noclip_active )
		{
			this->m_in_air_autostop_latched = false;
		}
		const auto active_aim_method = cfg.aimbot.type == settings::combat::aimbot::method_memory
			? settings::combat::aimbot::method_memory
			: settings::combat::aimbot::method_mouse_event;
		auto memory_aimbot_active{ false };

		if ( !ctx.is_reloading && ctx.weapon_ready )
		{
			const auto aimbot_key_held = cfg.aimbot.enabled && zui::keybind_active( cfg.aimbot.key );

			if ( !is_legit_cfg
				&& cfg.aimbot.autostop
				&& in_air_autostop_active
				&& pawn
				&& !on_ground
				&& !noclip_active
				&& !passed_fall_dead_zone )
			{
				auto should_autostop{ false };
				auto autostop_probe_cfg = cfg;

				// Semirage autostop should not depend on tight lock FOV:
				// use full angular scan (360 awareness).
				autostop_probe_cfg.aimbot.fov = 180.0f;
				autostop_probe_cfg.aimbot.head_only = false;
				autostop_probe_cfg.aimbot.all_hitboxes = true;
				autostop_probe_cfg.aimbot.hitbox_mask = settings::combat::aimbot::hitbox_all;
				autostop_probe_cfg.aimbot.visible_only = true;
				autostop_probe_cfg.aimbot.autowall = cfg.aimbot.autowall;

				// Dedicated probe for movement stopping; do not depend on current aimbot lock target.
				const auto autostop_target = this->select_target( eye_pos, view_angles, players, autostop_probe_cfg, 0u );

				if ( autostop_target.player )
				{
					const auto within_stop_dist = autostop_within_distance( eye_pos, *autostop_target.player );

					// Base autostop (without early) should only trigger on real fire condition:
					// visible hit (no autowall) or enough damage (autowall on).
					const auto autostop_min_damage = semirage_auto_mode_enabled( cfg )
						? semirage_auto_min_damage( eye_pos, *autostop_target.player )
						: cfg.aimbot.min_damage;
					const auto lethal_min_damage = resolve_lethal_min_damage( autostop_min_damage, autostop_target.player->health );
					const auto can_damage_now = cfg.aimbot.autowall
						? autostop_target.damage >= lethal_min_damage
						: autostop_target.hitbox >= 0;
					should_autostop = within_stop_dist && can_damage_now;

					if ( should_autostop && in_air_autostop_active && !on_ground )
					{
						this->m_in_air_autostop_latched = true;
					}
				}

				// In-air autostop should engage just slightly before fire-ready.
				// Keep lookahead minimal to avoid stopping too early.
				if ( !should_autostop && in_air_autostop_active && !on_ground )
				{
					const auto air_tick_interval{ 0.0078125f }; // 128 tick
					const auto speed_norm = std::clamp( ( speed - 80.0f ) / 260.0f, 0.0f, 1.0f );
					const auto lookahead_count = std::clamp(
						1 + static_cast< int >( std::round( speed_norm * 3.0f ) ),
						1,
						4 );
					const auto lookahead_step = 1;
					const auto lookahead_limit = lookahead_count * lookahead_step;

					for ( auto lookahead = lookahead_step; lookahead <= lookahead_limit; lookahead += lookahead_step )
					{
						const auto probe_pos = eye_pos + math::vector3
						{
							velocity.x * air_tick_interval * static_cast< float >( lookahead ),
							velocity.y * air_tick_interval * static_cast< float >( lookahead ),
							velocity.z * air_tick_interval * static_cast< float >( lookahead )
						};

						const auto in_air_target = this->select_target( probe_pos, view_angles, players, autostop_probe_cfg, 0u );
						if ( !in_air_target.player )
						{
							continue;
						}

						const auto probe_min_damage = semirage_auto_mode_enabled( cfg )
							? semirage_auto_min_damage( probe_pos, *in_air_target.player )
							: cfg.aimbot.min_damage;
						const auto probe_lethal_min_damage = resolve_lethal_min_damage( probe_min_damage, in_air_target.player->health );
						const auto can_damage_soon = cfg.aimbot.autowall
							? in_air_target.damage >= probe_lethal_min_damage
							: in_air_target.hitbox >= 0;
						const auto within_stop_dist = autostop_within_distance( probe_pos, *in_air_target.player );

						if ( !can_damage_soon || !within_stop_dist )
						{
							continue;
						}

						should_autostop = true;
						this->m_in_air_autostop_latched = true;
						break;
					}
				}

				// Keep in-air autostop engaged after first valid detection to avoid
				// one-tick probe flicker releasing movement mid-jump.
				if ( !should_autostop
					&& in_air_autostop_active
					&& !on_ground
					&& this->m_in_air_autostop_latched )
				{
					should_autostop = true;
				}

				// Probe a few lookahead snapshots and simulate the eventual stop point to trigger early wallbang stopping.
				// Early autostop is ground-only; in-air should use only the regular autostop path.
				if ( !should_autostop && on_ground && cfg.aimbot.autowall && cfg.aimbot.early_autostop )
				{
					constexpr auto tick_interval{ 0.01171875f }; // 25% less early-stop lookahead
					const auto selected_hitbox_count = early_autostop_hitbox_count(
						cfg.aimbot,
						autostop_target.hitbox );
					const std::array<int, 6> lookahead_ticks
					{
						1, 2, 3, 4, 5,
						selected_hitbox_count <= 1 ? 6 : 7
					};

					for ( const auto lookahead : lookahead_ticks )
					{
						const auto lookahead_pos = eye_pos + math::vector3
						{
							velocity.x * tick_interval * static_cast< float >( lookahead ),
							velocity.y * tick_interval * static_cast< float >( lookahead ),
							0.0f
						};

						const auto stop_probe_pos = g_shared.extrapolate_stop( lookahead_pos );
						const auto early_target = this->select_target( stop_probe_pos, view_angles, players, autostop_probe_cfg, 0u );
						const auto lethal_min_damage = early_target.player
							? resolve_lethal_min_damage(
								semirage_auto_mode_enabled( cfg )
									? semirage_auto_min_damage( stop_probe_pos, *early_target.player )
									: cfg.aimbot.min_damage,
								early_target.player->health )
							: cfg.aimbot.min_damage;
						const auto can_wallbang_after_stop = early_target.player && early_target.penetrated && early_target.damage >= lethal_min_damage;
						const auto within_stop_dist = early_target.player && autostop_within_distance( stop_probe_pos, *early_target.player );

						if ( !can_wallbang_after_stop || !within_stop_dist )
						{
							continue;
						}

						should_autostop = true;
						break;
					}
				}

				if ( should_autostop )
				{
					this->apply_autostop( cfg.aimbot.full_stop, in_air_autostop_active );
				}
				else if ( !in_air_autostop_active || on_ground )
				{
					this->m_in_air_autostop_latched = false;
				}
			}

			if ( cfg.aimbot.enabled )
			{
				const auto key_held = aimbot_key_held;
				if ( !key_held )
				{
					this->m_locked_target_pawn = 0;
					this->m_locked_target_hitbox = -1;
				}

				const auto preferred_pawn = key_held ? this->m_locked_target_pawn : 0u;
				auto target = this->select_target( eye_pos, view_angles, players, cfg, preferred_pawn );
				auto aim_target = target;

				if ( aim_target.player )
				{
					if ( key_held
						&& !is_legit_cfg
						&& cfg.aimbot.auto_scope
						&& weapon_supports_scope( ctx )
						&& auto_scope_needs_toggle( ctx )
						&& !this->m_auto_scope_held
						&& ctx.current_time >= this->m_auto_scope_next_time )
					{
						constexpr auto auto_scope_click_hold{ 0.015f };
						g::input.inject_mouse( 0, 0, input::right_down );
						this->m_auto_scope_held = true;
						this->m_auto_scope_release_time = ctx.current_time + auto_scope_click_hold;
						// No artificial inter-zoom delay: as soon as the first click is released,
						// the second zoom can be requested on the next eligible tick.
						this->m_auto_scope_next_time = this->m_auto_scope_release_time;
					}

					this->m_locked_target_pawn = aim_target.player->pawn;
					this->m_locked_target_hitbox = aim_target.hitbox;
					memory_aimbot_active = key_held && active_aim_method == settings::combat::aimbot::method_memory;
					this->aimbot( eye_pos, view_angles, aim_target, cfg.aimbot, cfg.recoil_control_system, active_aim_method );
				}
			}

			if ( cfg.triggerbot.enabled )
			{
				auto trigger_cfg = cfg.triggerbot;
				auto can_run_trigger{ true };
				if ( is_legit_cfg )
				{
					trigger_cfg.hitchance = 60.0f;

					if ( !pawn || !on_ground )
					{
						this->m_trigger_waiting = false;
						if ( this->m_trigger_held )
						{
							g::input.inject_mouse( 0, 0, input::left_up );
							this->m_trigger_held = false;
							this->m_trigger_continuous_hold = false;
						}
						can_run_trigger = false;
					}
				}
				else if ( cfg.aimbot.autostop && in_air_autostop_active && !on_ground && !noclip_active )
				{
					// In-air autostop requires instant trigger response.
					trigger_cfg.delay = 0;
				}

				if ( can_run_trigger )
				{
					if ( legit_autostop_move_ready )
					{
						const auto trigger_key_held = zui::keybind_active( trigger_cfg.key );
						if ( trigger_key_held )
						{
							const auto trigger_can_fire = [ & ]( const trigger_result& hit, const math::vector3& sample_pos ) -> bool
								{
									if ( !hit.player )
									{
										return false;
									}

									if ( hit.penetrated )
									{
										if ( !trigger_cfg.autowall )
										{
											return false;
										}

										const auto lethal_min_damage = resolve_lethal_min_damage( trigger_cfg.min_damage, hit.player->health );
										if ( hit.damage < lethal_min_damage )
										{
											return false;
										}
									}

									if ( !g_shared.is_sniper_accurate( ) )
									{
										return false;
									}

									if ( trigger_cfg.hitchance > 0.0f )
									{
										const auto required = trigger_cfg.hitchance * 0.01f;
										const auto hc = g_shared.calculate_hitchance( sample_pos, view_angles, *hit.player, hit.bones );
										if ( hc < required )
										{
											return false;
										}
									}

									return true;
								};

							const auto trigger_probe = this->trace_crosshair( eye_pos, view_angles, players, trigger_cfg );
							auto trigger_should_autostop = trigger_can_fire( trigger_probe, eye_pos );

							if ( !trigger_should_autostop && cfg.aimbot.early_autostop )
							{
								constexpr auto tick_interval{ 0.01171875f }; // 25% less early-stop lookahead
								const auto selected_hitbox_count = early_autostop_hitbox_count( cfg.aimbot, trigger_probe.hitbox );
								const std::array<int, 6> lookahead_ticks
								{
									1, 2, 3, 4, 5,
									selected_hitbox_count <= 1 ? 6 : 7
								};

								for ( const auto lookahead : lookahead_ticks )
								{
									const auto lookahead_pos = eye_pos + math::vector3
									{
										velocity.x * tick_interval * static_cast< float >( lookahead ),
										velocity.y * tick_interval * static_cast< float >( lookahead ),
										0.0f
									};

									const auto stop_probe_pos = g_shared.extrapolate_stop( lookahead_pos );
									const auto early_probe = this->trace_crosshair( stop_probe_pos, view_angles, players, trigger_cfg );
									if ( !trigger_can_fire( early_probe, stop_probe_pos ) )
									{
										continue;
									}

									trigger_should_autostop = true;
									break;
								}
							}

							if ( trigger_should_autostop )
							{
								this->apply_autostop( cfg.aimbot.full_stop, false );
							}
						}
					}

					this->triggerbot( eye_pos, view_angles, players, trigger_cfg );
				}
			}
		}
		
		if ( !is_legit_cfg
			&& cfg.aimbot.autostop
			&& in_air_autostop_active
			&& pawn
			&& !on_ground
			&& !noclip_active
			&& !ctx.is_reloading
			&& !passed_fall_dead_zone
			&& this->m_in_air_autostop_latched )
		{
			this->apply_autostop( cfg.aimbot.full_stop, true );
		}

		if ( cfg.recoil_control_system.enabled && !g_recoil_runtime.suppress_mouse_this_tick && !memory_aimbot_active )
		{
			apply_external_recoil_control( cfg.recoil_control_system );
		}

		this->release_autostop( );
	}

	legit::target legit::select_target( const math::vector3& eye_pos, const math::vector3& view_angles, const std::vector<systems::collector::player>& players, const settings::combat::group_config& cfg, std::uintptr_t preferred_pawn ) const
	{
		target best{};
		target preferred{};
		// FOV math is 0..180; allowing up to 180 enables full 360-aware target detection.
		const auto max_fov = std::clamp( cfg.aimbot.fov, 1.0f, 180.0f );
		best.fov = max_fov;
		preferred.fov = max_fov;
		constexpr auto tie_epsilon{ 0.05f };
		constexpr auto damage_tie_epsilon{ 0.5f };
		constexpr auto damage_fov_window{ 1.25f };
		constexpr auto stick_hysteresis{ 0.75f };

		for ( const auto& player : players )
		{
			if ( !systems::g_local.is_enemy( player.team ) )
			{
				continue;
			}

			if ( player.invulnerable || player.hitboxes.count <= 0 )
			{
				continue;
			}

			const auto bones = systems::g_bones.get( player.bone_cache );
			if ( !bones.is_valid( ) )
			{
				continue;
			}

			auto damage{ 0.0f };
			auto hitbox{ -1 };
			auto penetrated{ false };

			const auto aim_point = this->get_aim_point( eye_pos, view_angles, player, bones, cfg, damage, hitbox, penetrated );
			if ( hitbox < 0 )
			{
				continue;
			}

			const auto fov = this->get_fov( view_angles, eye_pos, aim_point );
			if ( fov > max_fov )
			{
				continue;
			}

			if ( preferred_pawn != 0 && player.pawn == preferred_pawn && fov <= preferred.fov )
			{
				preferred.player = &player;
				preferred.bones = bones;
				preferred.aim_point = aim_point;
				preferred.hitbox = hitbox;
				preferred.hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( hitbox );
				preferred.damage = damage;
				preferred.fov = fov;
				preferred.penetrated = penetrated;
			}

			auto candidate_better{ !best.player };
			if ( best.player )
			{
				const auto best_is_human = !best.player->is_bot;
				const auto candidate_is_human = !player.is_bot;

				if ( best_is_human != candidate_is_human )
				{
					candidate_better = candidate_is_human;
				}
				else
				{
					const auto fov_delta = fov - best.fov;
					const auto better_by_fov = fov_delta < -tie_epsilon;
					const auto within_tie_window = std::fabs( fov_delta ) <= tie_epsilon;
					const auto better_by_damage = damage > best.damage + damage_tie_epsilon;
					const auto comparable_fov = fov <= best.fov + damage_fov_window;

					if ( better_by_fov )
					{
						candidate_better = true;
					}
					else if ( within_tie_window )
					{
						if ( better_by_damage )
						{
							candidate_better = true;
						}
						else if ( std::fabs( damage - best.damage ) <= damage_tie_epsilon )
						{
							// Keep deterministic tie-breaker to avoid "first in list" behavior.
							candidate_better = fov < best.fov;
						}
					}
					else if ( cfg.aimbot.autowall && comparable_fov && better_by_damage )
					{
						// Allow a modest FOV concession when another target has clearly better damage.
						candidate_better = true;
					}
				}
			}

			if ( !candidate_better )
			{
				continue;
			}

			best.player = &player;
			best.bones = bones;
			best.aim_point = aim_point;
			best.hitbox = hitbox;
			best.hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( hitbox );
			best.damage = damage;
			best.fov = fov;
			best.penetrated = penetrated;
		}

		if ( preferred.player )
		{
			const auto preferred_is_human = !preferred.player->is_bot;
			const auto best_is_human = best.player && !best.player->is_bot;
			const auto best_damage_advantage = best.player ? ( best.damage - preferred.damage ) : 0.0f;
			constexpr auto lock_break_damage_advantage{ 5.0f };

			if ( !( best_is_human && !preferred_is_human )
				&& ( !best.player || preferred.fov <= best.fov + stick_hysteresis )
				&& best_damage_advantage <= lock_break_damage_advantage )
			{
				return preferred;
			}
		}

		return best;
	}

	math::vector3 legit::get_aim_point( const math::vector3& eye_pos, const math::vector3& view_angles, const systems::collector::player& player, const systems::bones::data& bones, const settings::combat::group_config& cfg, float& out_damage, int& out_hitbox, bool& out_penetrated ) const
	{
		out_hitbox = -1;
		const auto dynamic_min_damage = semirage_auto_mode_enabled( cfg )
			? semirage_auto_min_damage( eye_pos, player )
			: cfg.aimbot.min_damage;
		const auto safe_health = std::max( player.health, 1 );
		const auto lethal_unlock_enabled = dynamic_min_damage >= static_cast< float >( safe_health );
		const auto lethal_min_damage = resolve_lethal_min_damage( dynamic_min_damage, player.health );
		const auto enforce_min_damage = cfg.aimbot.visible_only && cfg.aimbot.autowall;
		const auto head_only_active = cfg.aimbot.head_only;
		const auto normalize_mask = [ ]( std::uint32_t mask ) -> std::uint32_t
			{
				if ( mask == 0u )
				{
					mask = settings::combat::aimbot::hitbox_head;
				}

				// Neck should only be considered when head is not selected.
				if ( ( mask & settings::combat::aimbot::hitbox_head ) != 0u )
				{
					mask &= ~settings::combat::aimbot::hitbox_neck;
				}

				return mask;
			};

		auto configured_mask = cfg.aimbot.all_hitboxes
			? settings::combat::aimbot::hitbox_all
			: cfg.aimbot.hitbox_mask;
		configured_mask = normalize_mask( configured_mask );

		struct scan_result
		{
			float eval_fov{ std::numeric_limits<float>::max( ) };
			int priority{ std::numeric_limits<int>::max( ) };
			math::vector3 pos{};
			float damage{ 0.0f };
			int hitbox{ -1 };
			bool penetrated{ false };
			bool meets_min_damage{ false };
		};

		const auto scan_with_mask = [ & ]( std::uint32_t scan_mask, bool force_head_only, bool disable_damage_bias ) -> scan_result
			{
				scan_result best{};

				for ( const auto& hb : player.hitboxes )
				{
					if ( hb.index < 0 || hb.bone < 0 )
					{
						continue;
					}

					const auto hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( hb.index );
					if ( force_head_only )
					{
						if ( hitgroup != 1 )
						{
							continue;
						}
					}
					else if ( !hitbox_matches_mask( hb.index, scan_mask ) )
					{
						continue;
					}

					const auto pos = bones.get_position( hb.bone );
					auto candidate_damage = 0.0f;
					auto candidate_hitbox = hb.index;
					auto candidate_penetrated = false;
					auto candidate_valid = false;
					auto candidate_bone = hb.bone;

					if ( !cfg.aimbot.visible_only )
					{
						candidate_damage = combat::g_shared.pen( ).get_max_damage( hitgroup, player.armor, player.has_helmet, player.team );
						candidate_valid = candidate_damage > 0.0f;
					}
					else
					{
						const auto trace = systems::g_bvh.trace_ray( eye_pos, pos );
						const auto visible = !trace.hit || trace.fraction > 0.97f;
						if ( cfg.aimbot.autowall )
						{
							shared::penetration::result pen_result{};
							if ( combat::g_shared.pen( ).run( eye_pos, pos, player, bones, pen_result ) )
							{
								if ( pen_result.damage > 0.0f )
								{
									// If target is not directly visible, require a true wall penetration.
									// This avoids selecting impossible points while still allowing autowall.
									if ( !visible && !pen_result.penetrated )
									{
										continue;
									}

									candidate_damage = pen_result.damage;
									candidate_hitbox = hb.index;
									candidate_penetrated = pen_result.penetrated;
									candidate_valid = true;
								}
							}
						}
						else
						{
							if ( !visible )
							{
								continue;
							}

							candidate_damage = combat::g_shared.pen( ).get_max_damage( hitgroup, player.armor, player.has_helmet, player.team );
							candidate_valid = candidate_damage > 0.0f;
						}
					}

					if ( !candidate_valid )
					{
						continue;
					}
					const auto candidate_meets_min_damage = !enforce_min_damage || candidate_damage >= lethal_min_damage;

					const auto candidate_hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( candidate_hitbox );
					candidate_bone = resolve_bone_for_hitbox( player, candidate_hitbox, candidate_hitgroup, candidate_bone );
					if ( candidate_bone < 0 )
					{
						continue;
					}

					const auto candidate_bit = resolve_hitbox_bit( candidate_hitbox, scan_mask );
					if ( candidate_bit == 0u )
					{
						continue;
					}
					const auto candidate_priority = hitbox_priority_from_mask( scan_mask, candidate_bit );
					const auto candidate_pos = bones.get_position( candidate_bone );
					const auto candidate_fov = this->get_fov( view_angles, eye_pos, candidate_pos );
					const auto eval_fov = ( player.pawn == this->m_locked_target_pawn && candidate_hitbox == this->m_locked_target_hitbox )
						? candidate_fov - 0.25f
						: candidate_fov;

					constexpr auto switch_hysteresis{ 0.06f };
					constexpr auto order_tie_window{ 0.35f };
					constexpr auto damage_tie_window{ 1.0f };
					constexpr auto damage_fov_window{ 0.8f };

					if ( best.hitbox >= 0 )
					{
						if ( candidate_meets_min_damage != best.meets_min_damage )
						{
							if ( !candidate_meets_min_damage )
							{
								continue;
							}
						}
						else
						{
							if ( enforce_min_damage && candidate_meets_min_damage && !disable_damage_bias )
							{
								const auto better_by_damage = candidate_damage > best.damage + damage_tie_window
									&& eval_fov <= best.eval_fov + damage_fov_window;
								if ( better_by_damage )
								{
									best.eval_fov = eval_fov;
									best.priority = candidate_priority;
									best.pos = candidate_pos;
									best.damage = candidate_damage;
									best.hitbox = candidate_hitbox;
									best.penetrated = candidate_penetrated;
									best.meets_min_damage = candidate_meets_min_damage;
									continue;
								}
							}

							const auto better_by_fov = eval_fov < best.eval_fov - switch_hysteresis;
							const auto within_tie_window = std::fabs( eval_fov - best.eval_fov ) <= order_tie_window;
							const auto better_by_order = within_tie_window && candidate_priority < best.priority;
							const auto better_same_order = within_tie_window && candidate_priority == best.priority && eval_fov < best.eval_fov;

							if ( !better_by_fov && !better_by_order && !better_same_order )
							{
								continue;
							}
						}
					}

					best.eval_fov = eval_fov;
					best.priority = candidate_priority;
					best.pos = candidate_pos;
					best.damage = candidate_damage;
					best.hitbox = candidate_hitbox;
					best.penetrated = candidate_penetrated;
					best.meets_min_damage = candidate_meets_min_damage;
				}

				return best;
			};

		const auto head_mask = normalize_mask( settings::combat::aimbot::hitbox_head );
		auto best = scan_with_mask(
			head_only_active ? head_mask : configured_mask,
			head_only_active,
			cfg.aimbot.all_hitboxes );

		// Lethal extension: only when slider is explicitly above target health
		// do we allow unlocking all hitboxes to secure a kill.
		if ( !cfg.aimbot.all_hitboxes && lethal_unlock_enabled && enforce_min_damage && !best.meets_min_damage )
		{
			const auto all_mask = normalize_mask( settings::combat::aimbot::hitbox_all );
			const auto unlocked = scan_with_mask( all_mask, false, true );
			if ( unlocked.hitbox >= 0 && unlocked.meets_min_damage )
			{
				best = unlocked;
			}
		}

		if ( best.hitbox < 0 )
		{
			return {};
		}

		out_damage = best.damage;
		out_hitbox = best.hitbox;
		out_penetrated = best.penetrated;
		return best.pos;
	}

	float legit::get_fov( const math::vector3& view_angles, const math::vector3& eye_pos, const math::vector3& target_pos ) const
	{
		return math::helpers::calculate_fov( view_angles, eye_pos, target_pos );
	}

	float legit::get_fov_radius( const math::vector3& eye_pos, const math::vector3& view_angles, float fov_degrees ) const
	{
		if ( fov_degrees <= 0.0f )
		{
			return 0.0f;
		}

		math::vector3 forward{};
		view_angles.to_directions( &forward, nullptr, nullptr );

		auto offset_angles = view_angles;
		offset_angles.x -= fov_degrees;

		math::vector3 offset_forward{};
		offset_angles.to_directions( &offset_forward, nullptr, nullptr );

		const auto center = systems::g_view.project( eye_pos + forward * 1000.0f );
		const auto edge = systems::g_view.project( eye_pos + offset_forward * 1000.0f );

		if ( !systems::g_view.projection_valid( center ) || !systems::g_view.projection_valid( edge ) )
		{
			return 0.0f;
		}

		const auto dx = edge.x - center.x;
		const auto dy = edge.y - center.y;

		return std::sqrtf( dx * dx + dy * dy );
	}

	void legit::draw_fov( const math::vector3& eye_pos, const math::vector3& view_angles, const settings::combat::aimbot& cfg )
	{
		const auto target_radius = this->get_fov_radius( eye_pos, view_angles, cfg.fov );
		const auto alpha = this->m_fov_alpha.value( );
		const auto radius = target_radius * alpha;

		if ( radius <= 0.5f )
		{
			return;
		}

		const auto [w, h] = zdraw::get_display_size( );
		const auto color = zdraw::rgba{ cfg.fov_color.r, cfg.fov_color.g, cfg.fov_color.b, static_cast< std::uint8_t >( alpha * 125.0f ) };
		constexpr auto fov_segments{ 90 };
		constexpr auto fov_thickness{ 1.25f };

		zdraw::circle( w * 0.5f, h * 0.5f, radius, color, fov_segments, fov_thickness );
	}

	void legit::aimbot( const math::vector3& eye_pos, const math::vector3& view_angles, const target& tgt, const settings::combat::aimbot& cfg, const settings::combat::recoil_control_system& recoil_cfg, int method )
	{
		if ( !zui::keybind_active( cfg.key ) )
		{
			this->m_aim_error = {};
			this->m_last_aim_target_pawn = 0;
			return;
		}

		const auto pawn = systems::g_local.view_pawn( );
		if ( !pawn )
		{
			return;
		}

		if ( this->m_last_aim_target_pawn != tgt.player->pawn )
		{
			this->m_aim_error = {};
			this->m_last_aim_target_pawn = tgt.player->pawn;
		}

		const auto freshest = systems::g_bones.get( tgt.player->bone_cache );
		if ( !freshest.is_valid( ) )
		{
			return;
		}

		const auto resolved_bone = resolve_bone_for_hitbox( *tgt.player, tgt.hitbox, tgt.hitgroup, cstypes::head );
		if ( resolved_bone < 0 )
		{
			return;
		}

		auto aim_point = freshest.get_position( resolved_bone );

		if ( cfg.predictive )
		{
			const auto velocity = g::memory.read<math::vector3>( tgt.player->pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );
			const auto prediction_time = g_shared.get_prediction_time( );

			aim_point = aim_point + velocity * prediction_time;
		}

		auto desired = math::helpers::calculate_angle( eye_pos, aim_point );
		if ( recoil_cfg.enabled )
		{
			const auto suppress_external = method == settings::combat::aimbot::method_memory;
			apply_aimbot_recoil_compensation( desired, recoil_cfg, suppress_external );
		}

		const auto min_smooth = settings::g_misc.m_safe_mode ? 1.0f : 0.0f;
		const auto smoothing = std::clamp( cfg.smoothing, min_smooth, 100.0f );

		auto current_view_angles = read_live_view_angles( view_angles );
		auto delta_x = desired.x - current_view_angles.x;
		auto delta_y = math::helpers::normalize_yaw( desired.y - current_view_angles.y );

		if ( method == settings::combat::aimbot::method_memory )
		{
			auto final_angles = desired;
			if ( smoothing > 1.0f )
			{
				const auto factor = smoothing;
				final_angles.x = current_view_angles.x + delta_x / factor;
				final_angles.y = current_view_angles.y + delta_y / factor;
			}

			final_angles.z = 0.0f;
			math::helpers::normalize_angles( final_angles );

			if ( g::offsets.view_angles && g::memory.write( g::offsets.view_angles, final_angles ) )
			{
				this->m_aim_error = {};
				return;
			}

			// Graceful fallback when write access is unavailable.
			method = settings::combat::aimbot::method_mouse_event;
			g_recoil_runtime.suppress_mouse_this_tick = false;
			current_view_angles = read_live_view_angles( view_angles );
			delta_x = desired.x - current_view_angles.x;
			delta_y = math::helpers::normalize_yaw( desired.y - current_view_angles.y );
		}

		const auto sensitivity = systems::g_convars.get<float>( CONVAR( "sensitivity"_hash ) );
		constexpr auto m_yaw{ 0.022f };
		auto fov_adjust = g::memory.read<float>( pawn + SCHEMA( "C_BasePlayerPawn", "m_flFOVSensitivityAdjust"_hash ) );
		if ( !std::isfinite( fov_adjust ) || fov_adjust <= 0.0f )
		{
			fov_adjust = 1.0f;
		}

		const auto deg_per_pixel = sensitivity * m_yaw * fov_adjust;
		if ( deg_per_pixel <= 0.0f )
		{
			return;
		}

		const auto smooth = smoothing;
		const auto smooth_norm = smooth * 0.01f;
		const auto smooth_divisor = 1.0f + smooth * 2.0f;
		auto spray_active{ false };
		auto mouse_multiplier = std::clamp( cfg.mouse_multiplier, 1.0f, 100.0f );
		if ( recoil_cfg.enabled )
		{
			recoil_snapshot spray_snapshot{};
			if ( read_recoil_snapshot( spray_snapshot ) )
			{
				const auto start_bullet = std::clamp( recoil_cfg.start_bullet, 0, 5 );
				spray_active = spray_snapshot.shots_fired > start_bullet;
				if ( spray_active )
				{
					// Match IRINEU behavior: keep multiplier bounded during spray so RCS + mouse-event do not fight.
					mouse_multiplier = std::min( mouse_multiplier, 6.0f );
				}
			}
		}
		const auto multiplier_scale = mouse_multiplier * 0.1f;

		auto remaining_x = delta_y / deg_per_pixel;
		auto remaining_y = delta_x / deg_per_pixel;
		const auto remaining_norm = std::sqrt( remaining_x * remaining_x + remaining_y * remaining_y );

		if ( !std::isfinite( remaining_norm ) || remaining_norm <= 0.0001f )
		{
			return;
		}

		// Anchor movement signs to actual screen-side of the target, same idea used in IRINEU mouse-event.
		const auto projected_aim = systems::g_view.project( aim_point );
		if ( systems::g_view.projection_valid( projected_aim ) )
		{
			const auto [w, h] = zdraw::get_display_size( );
			const auto screen_dx = projected_aim.x - w * 0.5f;
			const auto screen_dy = projected_aim.y - h * 0.5f;

			if ( std::isfinite( screen_dx ) && std::fabs( screen_dx ) > 0.01f )
			{
				// This path inverts X right before injection, so anchor with opposite sign here.
				remaining_x = std::copysign( std::fabs( remaining_x ), -screen_dx );
			}

			if ( !spray_active && std::isfinite( screen_dy ) && std::fabs( screen_dy ) > 0.01f )
			{
				// During spray keep pitch from pure angle delta so recoil compensation can drive descent.
				remaining_y = std::copysign( std::fabs( remaining_y ), screen_dy );
			}
		}

		auto move_x = ( remaining_x / smooth_divisor ) * multiplier_scale;
		auto move_y = ( remaining_y / smooth_divisor ) * multiplier_scale;

		if ( !std::isfinite( move_x ) || !std::isfinite( move_y ) )
		{
			return;
		}

		const auto min_step_base = remaining_norm > 2.0f
			? std::clamp( 0.90f - smooth_norm * 0.85f, 0.05f, 0.90f )
			: 0.0f;
		const auto min_step_scale = std::clamp( multiplier_scale / 1.45f, 0.0f, 7.0f );
		const auto min_step = min_step_base * min_step_scale;

		if ( min_step > 0.0f )
		{
			if ( std::fabs( move_x ) < min_step && std::fabs( remaining_x ) > min_step )
			{
				move_x = std::copysign( min_step, remaining_x );
			}

			if ( std::fabs( move_y ) < min_step && std::fabs( remaining_y ) > min_step )
			{
				move_y = std::copysign( min_step, remaining_y );
			}
		}

		if ( std::fabs( move_x ) > std::fabs( remaining_x ) )
		{
			move_x = remaining_x;
		}

		if ( std::fabs( move_y ) > std::fabs( remaining_y ) )
		{
			move_y = remaining_y;
		}

		// CS2 mouse-event direction: horizontal uses yaw delta sign, vertical uses pitch delta sign.
		move_x = -move_x;

		this->m_aim_error.x += move_x;
		this->m_aim_error.y += move_y;

		const auto dx = static_cast< int >( this->m_aim_error.x );
		const auto dy = static_cast< int >( this->m_aim_error.y );

		this->m_aim_error.x -= static_cast< float >( dx );
		this->m_aim_error.y -= static_cast< float >( dy );

		if ( dx != 0 || dy != 0 )
		{
			g::input.inject_mouse( dx, dy, input::move );
		}
	}

	legit::trigger_result legit::trace_crosshair( const math::vector3& eye_pos, const math::vector3& view_angles, const std::vector<systems::collector::player>& players, const settings::combat::triggerbot& cfg ) const
	{
		trigger_result result{};

		math::vector3 forward{};
		view_angles.to_directions( &forward, nullptr, nullptr );

		constexpr auto max_range{ 8192.0f };
		const auto end_pos = eye_pos + forward * max_range;

		const auto world_trace = systems::g_bvh.trace_ray( eye_pos, end_pos );
		auto best_dist_sq = max_range * max_range;
		const auto prediction_time = cfg.predictive ? g_shared.get_prediction_time( ) + static_cast< float >( cfg.delay ) * 0.001f : 0.0f;
		const auto should_replace = [ & ]( const systems::collector::player& candidate, float dist_sq )
			{
				if ( !result.player )
				{
					return true;
				}

				const auto current_is_human = !result.player->is_bot;
				const auto candidate_is_human = !candidate.is_bot;
				if ( current_is_human != candidate_is_human )
				{
					return candidate_is_human;
				}

				return dist_sq < best_dist_sq;
			};

		for ( const auto& player : players )
		{
			if ( !systems::g_local.is_enemy( player.team ) )
			{
				continue;
			}

			if ( player.invulnerable || player.hitboxes.count <= 0 )
			{
				continue;
			}

			const auto bones = systems::g_bones.get( player.bone_cache );
			if ( !bones.is_valid( ) )
			{
				continue;
			}

			math::vector3 velocity{};
			if ( cfg.predictive )
			{
				velocity = g::memory.read<math::vector3>( player.pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );
			}

			for ( const auto& hb : player.hitboxes )
			{
				if ( hb.index < 0 || hb.bone < 0 )
				{
					continue;
				}

				const auto raw_center = bones.get_position( hb.bone );
				const auto center = raw_center + velocity * prediction_time;
				const auto radius = hb.radius > 0.0f ? hb.radius : 3.5f;

				const auto oc = eye_pos - center;
				const auto a = forward.dot( forward );
				const auto b = 2.0f * oc.dot( forward );
				const auto c = oc.dot( oc ) - radius * radius;
				const auto discriminant = b * b - 4.0f * a * c;

				if ( discriminant < 0.0f )
				{
					continue;
				}

				const auto sqrt_d = std::sqrtf( discriminant );
				auto t = ( -b - sqrt_d ) / ( 2.0f * a );

				if ( t < 0.0f )
				{
					t = ( -b + sqrt_d ) / ( 2.0f * a );
				}

				if ( t < 0.0f || t > max_range )
				{
					continue;
				}

				const auto hit_pos = eye_pos + forward * t;
				const auto dist_sq = ( hit_pos - eye_pos ).length_sqr( );

				if ( !should_replace( player, dist_sq ) )
				{
					continue;
				}

				const auto vis_trace = systems::g_bvh.trace_ray( eye_pos, center );
				const auto visible = !vis_trace.hit || vis_trace.fraction > 0.97f;

				if ( cfg.autowall )
				{
					shared::penetration::result pen_result{};
					if ( combat::g_shared.pen( ).run( eye_pos, center, player, bones, pen_result ) )
					{
						const auto lethal_min_damage = resolve_lethal_min_damage( cfg.min_damage, player.health );
						if ( pen_result.damage >= lethal_min_damage )
						{
							best_dist_sq = dist_sq;
							result.player = &player;
							result.bones = bones;
							result.hitbox = pen_result.hitbox;
							result.hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( pen_result.hitbox );
							result.damage = pen_result.damage;
							result.penetrated = pen_result.penetrated;

							const auto resolved_bone = resolve_bone_for_hitbox( player, result.hitbox, result.hitgroup, hb.bone );
							result.point = resolved_bone >= 0 ? bones.get_position( resolved_bone ) : center;
						}
					}
				}
				else if ( visible )
				{
					const auto hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( hb.index );
					const auto damage = combat::g_shared.pen( ).get_max_damage( hitgroup, player.armor, player.has_helmet, player.team );

					best_dist_sq = dist_sq;
					result.player = &player;
					result.bones = bones;
					result.point = center;
					result.hitbox = hb.index;
					result.hitgroup = hitgroup;
					result.damage = damage;
					result.penetrated = false;
				}
			}
		}

		return result;
	}

	void legit::triggerbot( const math::vector3& eye_pos, const math::vector3& view_angles, const std::vector<systems::collector::player>& players, const settings::combat::triggerbot& cfg )
	{
		const auto& ctx = g_shared.ctx( );
		const auto auto_mode = !legit_mode_active( ) && cfg.auto_mode;
		auto trace_cfg = cfg;
		if ( auto_mode )
		{
			// Keep scan permissive; final fire gate applies per-target dynamic checks.
			trace_cfg.hitchance = 30.0f;
			trace_cfg.min_damage = 35.0f;
		}

		const auto manual_lmb_held = ( ::GetAsyncKeyState( VK_LBUTTON ) & 0x8000 ) != 0;
		const auto release_injected_trigger = [ & ]( )
			{
				if ( this->m_trigger_held )
				{
					g::input.inject_mouse( 0, 0, input::left_up );
				}

				this->m_trigger_held = false;
				this->m_trigger_continuous_hold = false;
			};

		if ( !zui::keybind_active( cfg.key ) )
		{
			this->m_trigger_waiting = false;
			release_injected_trigger( );
			return;
		}

		if ( !ctx.weapon_ready )
		{
			this->m_trigger_waiting = false;
			release_injected_trigger( );
			return;
		}

		auto result = this->trace_crosshair( eye_pos, view_angles, players, trace_cfg );

		const auto can_fire = [ & ]( const trigger_result& hit, const math::vector3& sample_pos, bool strict_accuracy ) -> bool
			{
				if ( !hit.player )
				{
					return false;
				}

				auto confirmed_damage = hit.damage;
				auto confirmed_penetrated = hit.penetrated;
				const auto effective_min_damage = auto_mode
					? semirage_auto_min_damage( sample_pos, *hit.player )
					: cfg.min_damage;

				if ( cfg.autowall )
				{
					shared::penetration::result confirm_pen{};
					if ( combat::g_shared.pen( ).run( sample_pos, hit.point, *hit.player, hit.bones, confirm_pen ) && confirm_pen.damage > 0.0f )
					{
						confirmed_damage = confirm_pen.damage;
						confirmed_penetrated = confirm_pen.penetrated;
					}

					const auto lethal_min_damage = resolve_lethal_min_damage( effective_min_damage, hit.player->health );
					if ( confirmed_damage < lethal_min_damage )
					{
						return false;
					}
				}

				if ( confirmed_penetrated )
				{
					if ( !cfg.autowall )
					{
						return false;
					}

					const auto lethal_min_damage = resolve_lethal_min_damage( effective_min_damage, hit.player->health );
					if ( confirmed_damage < lethal_min_damage )
					{
						return false;
					}
				}

				if ( strict_accuracy )
				{
					if ( !g_shared.is_sniper_accurate( ) )
					{
						return false;
					}

					const auto effective_hitchance = auto_mode
						? semirage_auto_hitchance( sample_pos, *hit.player )
						: cfg.hitchance;
					if ( effective_hitchance > 0.0f )
					{
						const auto required = effective_hitchance / 100.0f;
						const auto hc = g_shared.calculate_hitchance( sample_pos, view_angles, *hit.player, hit.bones );
						if ( hc < required )
						{
							return false;
						}
					}
				}

				return true;
			};

		const auto can_fire_now = can_fire( result, eye_pos, true );
		if ( !can_fire_now )
		{
			this->m_trigger_waiting = false;
			release_injected_trigger( );
			return;
		}

		// Predictive trigger can overshoot when opening an angle.
		// If autostop already detected/engaged this tick, require a second
		// confirmation on current (non-predicted) crosshair before firing.
		if ( cfg.predictive && this->m_autostop_requested_tick )
		{
			auto realtime_cfg = trace_cfg;
			realtime_cfg.predictive = false;
			const auto realtime_result = this->trace_crosshair( eye_pos, view_angles, players, realtime_cfg );
			if ( !can_fire( realtime_result, eye_pos, true ) )
			{
				this->m_trigger_waiting = false;
				release_injected_trigger( );
				return;
			}

			result = realtime_result;
		}

		// Manual click always has priority, but keep trigger timing primed so
		// there is no artificial delay right after the user releases LMB.
		if ( manual_lmb_held )
		{
			if ( !this->m_trigger_waiting )
			{
				this->m_trigger_waiting = true;
				this->m_trigger_delay_end = ctx.current_time;
			}
			release_injected_trigger( );
			return;
		}

		// Continuous spray for automatic weapons (except pistols/snipers) while target is in crosshair.
		const auto continuous_trigger = ctx.is_full_auto
			&& ctx.weapon_type != cstypes::pistol
			&& ctx.weapon_type != cstypes::sniper;

		if ( continuous_trigger )
		{
			this->m_trigger_waiting = false;
			if ( !this->m_trigger_held )
			{
				g::input.inject_mouse( 0, 0, input::left_down );
				this->m_trigger_held = true;
			}

			this->m_trigger_continuous_hold = true;
			return;
		}

		if ( this->m_trigger_held && this->m_trigger_continuous_hold )
		{
			release_injected_trigger( );
		}

		// Non-continuous path (semi-auto style).
		if ( this->m_trigger_held )
		{
			return;
		}

		this->m_trigger_continuous_hold = false;

		const auto now = ctx.current_time;

		if ( cfg.delay > 0 )
		{
			if ( !this->m_trigger_waiting )
			{
				this->m_trigger_waiting = true;
				this->m_trigger_delay_end = now + static_cast< float >( cfg.delay ) * 0.001f;
				return;
			}

			if ( now < this->m_trigger_delay_end )
			{
				return;
			}
		}
		this->m_trigger_waiting = false;

		const auto hold_ms = this->m_rng.random_float( 50.0f, 120.0f );

		g::input.inject_mouse( 0, 0, input::left_down );
		this->m_trigger_held = true;
		this->m_trigger_continuous_hold = false;
		this->m_trigger_release_time = now + hold_ms * 0.001f;
	}

	void legit::apply_autostop( bool full_stop, bool allow_in_air )
	{
		this->m_autostop_requested_tick = true;
		const auto release_shift = [ this ]( )
			{
				if ( this->m_autostop_shift_held )
				{
					g::input.inject_keyboard( VK_SHIFT, false );
					this->m_autostop_shift_held = false;
				}
			};

		const auto pawn = systems::g_local.view_pawn( );
		if ( !pawn )
		{
			release_shift( );
			return;
		}

		const auto flags = g::memory.read<std::uint32_t>( pawn + SCHEMA( "C_BaseEntity", "m_fFlags"_hash ) );
		const auto on_ground = ( flags & ( 1u << 0 ) ) != 0u;
		if ( !on_ground && !allow_in_air )
		{
			release_shift( );
			return;
		}

		const auto hold_shift = !on_ground && allow_in_air;
		if ( hold_shift )
		{
			if ( !this->m_autostop_shift_held )
			{
				g::input.inject_keyboard( VK_SHIFT, true );
				this->m_autostop_shift_held = true;
			}
		}
		else
		{
			release_shift( );
		}

		const auto velocity = g::memory.read<math::vector3>( pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );
		const auto speed_2d = velocity.length_2d( );
		const auto movement_services = g::memory.read<std::uintptr_t>( pawn + SCHEMA( "C_BasePlayerPawn", "m_pMovementServices"_hash ) );
		auto movement_max_speed{ 250.0f };
		if ( movement_services )
		{
			const auto read_max_speed = g::memory.read<float>( movement_services + SCHEMA( "CPlayer_MovementServices", "m_flMaxspeed"_hash ) );
			if ( std::isfinite( read_max_speed ) && read_max_speed > 1.0f )
			{
				movement_max_speed = read_max_speed;
			}
		}

		const auto in_air_autostop = !on_ground && allow_in_air;
		const auto abs_vel_z = std::fabs( velocity.z );
		constexpr auto apex_window_vel_z{ 60.0f };
		const auto apex_priority = in_air_autostop && abs_vel_z <= apex_window_vel_z;
		const auto phase2_speed = std::clamp( movement_max_speed * ( in_air_autostop ? 0.17f : 0.22f ), 12.0f, 44.0f );
		const auto strong_phase = speed_2d > phase2_speed || apex_priority;

		auto stop_threshold = std::clamp( movement_max_speed * 0.05f, 4.0f, 14.0f );
		if ( in_air_autostop )
		{
			// In air, don't release early like ground stop. Keep braking until almost zero.
			stop_threshold = apex_priority ? 0.01f : 0.05f;
		}
		if ( !full_stop && speed_2d <= stop_threshold )
		{
			if ( this->m_autostop_active && !this->m_autostop_full_stop )
			{
				for ( const auto key : this->m_autostop_keys )
				{
					g::input.inject_keyboard( key, false );
				}

				this->m_autostop_keys.clear( );
				this->m_autostop_active = false;
				this->m_autostop_full_stop = false;
			}
			release_shift( );
			return;
		}

		if ( full_stop )
		{
			if ( this->m_autostop_active && this->m_autostop_full_stop )
			{
				return;
			}

			if ( this->m_autostop_active && !this->m_autostop_full_stop )
			{
				for ( const auto key : this->m_autostop_keys )
				{
					g::input.inject_keyboard( key, false );
				}
			}

			this->m_autostop_keys.clear( );

			// Hard movement lock: hold both opposite directions so movement is canceled
			// even if the user keeps holding movement keys.
			this->m_autostop_keys.push_back( 'W' );
			this->m_autostop_keys.push_back( 'S' );
			this->m_autostop_keys.push_back( 'A' );
			this->m_autostop_keys.push_back( 'D' );

			for ( const auto key : this->m_autostop_keys )
			{
				g::input.inject_keyboard( key, false );
				g::input.inject_keyboard( key, true );
			}

			if ( !on_ground && allow_in_air )
			{
				set_autostop_movement_buttons( pawn, autostop_btn_mask );
			}

			this->m_autostop_active = true;
			this->m_autostop_full_stop = true;
			this->m_autostop_start = std::chrono::steady_clock::now( );
			return;
		}

		auto view_angles = systems::g_view.angles( );
		math::vector3 forward{};
		math::vector3 right{};
		view_angles.to_directions( &forward, &right, nullptr );

		forward.z = 0.0f;
		right.z = 0.0f;
		if ( forward.length_sqr( ) <= 0.0001f || right.length_sqr( ) <= 0.0001f )
		{
			release_shift( );
			return;
		}

		forward.normalize( );
		right.normalize( );

		const math::vector3 velocity_2d{ velocity.x, velocity.y, 0.0f };
		auto forward_speed = velocity_2d.dot( forward );
		auto side_speed = velocity_2d.dot( right );
		auto axis_deadzone = std::clamp( stop_threshold * 0.35f, 1.0f, 4.0f );
		if ( in_air_autostop )
		{
			// Apex-aware in-air stop: aggressive around apex, softer near settle.
			axis_deadzone = strong_phase ? 0.02f : 0.18f;
		}
		else if ( strong_phase )
		{
			axis_deadzone = std::max( 0.75f, axis_deadzone * 0.55f );
		}
		else
		{
			axis_deadzone = std::max( 1.25f, axis_deadzone );
		}

		std::vector<std::uint16_t> desired_keys{};
		if ( strong_phase )
		{
			// Phase 1 (strong): cut speed quickly by braking both axes when needed.
			if ( forward_speed > axis_deadzone )
			{
				desired_keys.push_back( 'S' );
			}
			else if ( forward_speed < -axis_deadzone )
			{
				desired_keys.push_back( 'W' );
			}

			if ( side_speed > axis_deadzone )
			{
				desired_keys.push_back( 'A' );
			}
			else if ( side_speed < -axis_deadzone )
			{
				desired_keys.push_back( 'D' );
			}
		}
		else
		{
			// Phase 2 (smooth): dominant-axis settle to avoid overshoot and micro-correction.
			const auto abs_forward_speed = std::fabs( forward_speed );
			const auto abs_side_speed = std::fabs( side_speed );
			if ( abs_forward_speed >= abs_side_speed )
			{
				if ( forward_speed > axis_deadzone )
				{
					desired_keys.push_back( 'S' );
				}
				else if ( forward_speed < -axis_deadzone )
				{
					desired_keys.push_back( 'W' );
				}
			}
			else
			{
				if ( side_speed > axis_deadzone )
				{
					desired_keys.push_back( 'A' );
				}
				else if ( side_speed < -axis_deadzone )
				{
					desired_keys.push_back( 'D' );
				}
			}
		}

		if ( desired_keys.empty( ) )
		{
			if ( !on_ground && allow_in_air )
			{
				set_autostop_movement_buttons( pawn, 0u );
			}

			if ( this->m_autostop_active && !this->m_autostop_full_stop )
			{
				for ( const auto key : this->m_autostop_keys )
				{
					g::input.inject_keyboard( key, false );
				}

				this->m_autostop_keys.clear( );
				this->m_autostop_active = false;
				this->m_autostop_full_stop = false;
			}
			release_shift( );
			return;
		}

		if ( !on_ground && allow_in_air )
		{
			const auto desired_mask = autostop_buttons_from_keys( desired_keys );
			set_autostop_movement_buttons( pawn, desired_mask );
		}

		const auto has_key = [ ]( const std::vector<std::uint16_t>& keys, const std::uint16_t key )
			{
				return std::find( keys.begin( ), keys.end( ), key ) != keys.end( );
			};

		// Normal autostop must override manual movement while active.
		// Reassert neutralization every tick so user held keys don't win.
		static constexpr std::array<std::uint16_t, 4> movement_keys{ 'W', 'A', 'S', 'D' };
		for ( const auto key : movement_keys )
		{
			g::input.inject_keyboard( key, false );
		}

		if ( this->m_autostop_active && !this->m_autostop_full_stop )
		{
			for ( const auto key : this->m_autostop_keys )
			{
				if ( !has_key( desired_keys, key ) )
				{
					g::input.inject_keyboard( key, false );
				}
			}

			for ( const auto key : desired_keys )
			{
				if ( !has_key( this->m_autostop_keys, key ) )
				{
					g::input.inject_keyboard( key, true );
				}
			}
		}
		else
		{
			if ( this->m_autostop_active )
			{
				for ( const auto key : this->m_autostop_keys )
				{
					g::input.inject_keyboard( key, false );
				}
			}

			for ( const auto key : desired_keys )
			{
				g::input.inject_keyboard( key, true );
			}
		}

		this->m_autostop_keys = std::move( desired_keys );
		this->m_autostop_active = true;
		this->m_autostop_full_stop = false;
		this->m_autostop_start = std::chrono::steady_clock::now( );
	}

	void legit::release_autostop( )
	{
		const auto release_shift = [ this ]( )
			{
				if ( this->m_autostop_shift_held )
				{
					g::input.inject_keyboard( VK_SHIFT, false );
					this->m_autostop_shift_held = false;
				}
			};

		if ( !this->m_autostop_active )
		{
			release_shift( );
			return;
		}

		const auto pawn = systems::g_local.view_pawn( );
		const auto clear_move_buttons = [ pawn ]( )
			{
				if ( pawn )
				{
					set_autostop_movement_buttons( pawn, 0u );
				}
			};

		if ( !this->m_autostop_requested_tick )
		{
			clear_move_buttons( );

			for ( const auto key : this->m_autostop_keys )
			{
				g::input.inject_keyboard( key, false );
			}

			this->m_autostop_keys.clear( );
			this->m_autostop_active = false;
			this->m_autostop_full_stop = false;
			release_shift( );
			return;
		}

		const auto velocity = pawn ? g::memory.read<math::vector3>( pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) ) : math::vector3{};
		const auto elapsed = std::chrono::duration<float>( std::chrono::steady_clock::now( ) - this->m_autostop_start ).count( );
		const auto speed_2d = velocity.length_2d( );

		if ( this->m_autostop_full_stop )
		{
			const auto release_speed = 0.75f;
			const auto min_hold = 0.45f;

			// Re-assert full lock every tick while active.
			for ( const auto key : this->m_autostop_keys )
			{
				g::input.inject_keyboard( key, true );
			}

			if ( pawn )
			{
				set_autostop_movement_buttons( pawn, autostop_btn_mask );
			}

			if ( speed_2d > release_speed && elapsed < min_hold )
			{
				return;
			}
		}
		else
		{
			if ( !pawn )
			{
				for ( const auto key : this->m_autostop_keys )
				{
					g::input.inject_keyboard( key, false );
				}

				this->m_autostop_keys.clear( );
				this->m_autostop_active = false;
				this->m_autostop_full_stop = false;
				this->m_in_air_autostop_latched = false;
				release_shift( );
				return;
			}

			const auto flags = g::memory.read<std::uint32_t>( pawn + SCHEMA( "C_BaseEntity", "m_fFlags"_hash ) );
			if ( !( flags & 1u ) )
			{
				if ( this->m_in_air_autostop_latched )
				{
					const auto release_speed_air = 0.05f;
					if ( speed_2d > release_speed_air )
					{
						static constexpr std::array<std::uint16_t, 4> movement_keys{ 'W', 'A', 'S', 'D' };
						for ( const auto key : movement_keys )
						{
							g::input.inject_keyboard( key, false );
						}

						set_autostop_movement_buttons( pawn, autostop_buttons_from_keys( this->m_autostop_keys ) );

						for ( const auto key : this->m_autostop_keys )
						{
							g::input.inject_keyboard( key, true );
						}
						return;
					}
				}

				for ( const auto key : this->m_autostop_keys )
				{
					g::input.inject_keyboard( key, false );
				}
				clear_move_buttons( );

				this->m_autostop_keys.clear( );
				this->m_autostop_active = false;
				this->m_autostop_full_stop = false;
				release_shift( );
				return;
			}

			const auto movement_services = g::memory.read<std::uintptr_t>( pawn + SCHEMA( "C_BasePlayerPawn", "m_pMovementServices"_hash ) );
			auto movement_max_speed{ 250.0f };
			if ( movement_services )
			{
				const auto read_max_speed = g::memory.read<float>( movement_services + SCHEMA( "CPlayer_MovementServices", "m_flMaxspeed"_hash ) );
				if ( std::isfinite( read_max_speed ) && read_max_speed > 1.0f )
				{
					movement_max_speed = read_max_speed;
				}
			}

			const auto release_speed = std::clamp( movement_max_speed * 0.05f, 4.0f, 14.0f );
			if ( speed_2d > release_speed )
			{
				// Keep overriding manual movement until we are actually near-zero speed.
				static constexpr std::array<std::uint16_t, 4> movement_keys{ 'W', 'A', 'S', 'D' };
				for ( const auto key : movement_keys )
				{
					g::input.inject_keyboard( key, false );
				}
				set_autostop_movement_buttons( pawn, autostop_buttons_from_keys( this->m_autostop_keys ) );

				for ( const auto key : this->m_autostop_keys )
				{
					g::input.inject_keyboard( key, true );
				}
				return;
			}
		}

	for ( const auto key : this->m_autostop_keys )
	{
		g::input.inject_keyboard( key, false );
	}
	clear_move_buttons( );

	this->m_autostop_keys.clear( );
	this->m_autostop_active = false;
		this->m_autostop_full_stop = false;
		release_shift( );
	}

} // namespace features::combat
