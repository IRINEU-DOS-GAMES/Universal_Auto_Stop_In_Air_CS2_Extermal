#pragma once

#include <string>

namespace settings {

	struct menu_controls
	{
		int open_key{ VK_INSERT };
		int exit_key{ VK_END };
	};

	struct combat
	{
		struct aimbot
		{
			enum hitbox : std::uint32_t
			{
				hitbox_head = 1u << 0,            // index 0
				hitbox_neck = 1u << 1,            // index 1
				hitbox_upper_chest = 1u << 2,     // index 6
				hitbox_chest_center = 1u << 3,    // index 5
				hitbox_stomach = 1u << 4,         // index 3
				hitbox_pelvis = 1u << 5,          // index 2
				hitbox_lower_torso = 1u << 18,    // index 4
				hitbox_left_shoulder = 1u << 6,   // index 7
				hitbox_right_shoulder = 1u << 7,  // index 8
				hitbox_left_upper_arm = 1u << 8,  // index 9
				hitbox_right_upper_arm = 1u << 9, // index 10
				hitbox_left_hand = 1u << 10,      // index 11
				hitbox_right_hand = 1u << 11,     // index 12
				hitbox_left_hip = 1u << 12,       // index 13
				hitbox_right_hip = 1u << 13,      // index 14
				hitbox_left_knee = 1u << 14,      // index 15
				hitbox_right_knee = 1u << 15,     // index 16
				hitbox_left_foot = 1u << 16,      // index 17
				hitbox_right_foot = 1u << 17,     // index 18

				// Backward-compatible aliases for existing code paths.
				hitbox_chest = hitbox_upper_chest,
				hitbox_left_arm = hitbox_left_upper_arm,
				hitbox_right_arm = hitbox_right_upper_arm,
				hitbox_left_leg = hitbox_left_knee,
				hitbox_right_leg = hitbox_right_knee,

				hitbox_all =
					hitbox_head | hitbox_neck | hitbox_upper_chest | hitbox_chest_center | hitbox_stomach | hitbox_pelvis | hitbox_lower_torso
					| hitbox_left_shoulder | hitbox_right_shoulder
					| hitbox_left_upper_arm | hitbox_right_upper_arm | hitbox_left_hand | hitbox_right_hand
					| hitbox_left_hip | hitbox_right_hip | hitbox_left_knee | hitbox_right_knee
					| hitbox_left_foot | hitbox_right_foot
			};

			enum method : int
			{
				method_mouse_event = 0,
				method_memory = 2
			};

			bool enabled{ true };
			int key{ VK_XBUTTON2 };
			int type{ method_mouse_event };

			float fov{ 5.0f };
			float smoothing{ 5.0f };
			float mouse_multiplier{ 10.0f };
			std::uint32_t hitbox_mask{ hitbox_head | hitbox_neck | hitbox_upper_chest | hitbox_stomach };

			bool autowall{ true };
			float min_damage{ 90.0f };

			bool head_only{ false };
			bool all_hitboxes{ false };
			bool visible_only{ true };

			bool draw_fov{ true };
			zdraw::rgba fov_color{ 225, 225, 225, 125 };
			bool auto_scope{ false };

			bool predictive{ true };
			bool autostop{ true };
			bool early_autostop{ true };
			bool in_air_autostop{ false };
			bool full_stop{ false };
		};

		struct triggerbot
		{
			bool enabled{ true };
			int key{ VK_XBUTTON2 };
			bool auto_mode{ false };

			float hitchance{ 75.0f };
			int delay{ 10 };

			bool autowall{ true };
			float min_damage{ 90.0f };

			bool autostop{ false };
			bool early_autostop{ false };

			bool predictive{ true };
		};

		struct recoil_control_system
		{
			bool enabled{ false };
			float strength{ 100.0f };
			int start_bullet{ 1 };
		};

		struct group_config
		{
			aimbot aimbot{};
			triggerbot triggerbot{};
			recoil_control_system recoil_control_system{};
		};

		static constexpr std::uint32_t k_group_count{ 6 };
		static constexpr std::array<std::uint32_t, k_group_count> k_group_weapon_types
		{
			cstypes::pistol,
			cstypes::smg,
			cstypes::rifle,
			cstypes::shotgun,
			cstypes::sniper,
			cstypes::lmg
		};
		static constexpr std::array<const char*, k_group_count> k_group_names
		{
			"Pistol",
			"Smg",
			"Rifle",
			"Shotgun",
			"Sniper",
			"Lmg"
		};

		struct weapon_entry
		{
			std::uint16_t item_def_idx{};
			std::uint32_t weapon_type{};
			const char* name{};
		};

		static constexpr std::array<weapon_entry, 34> k_weapon_entries
		{ {
			{ 1,  cstypes::pistol,  "Deagle" },
			{ 2,  cstypes::pistol,  "Dual berettas" },
			{ 3,  cstypes::pistol,  "Five-seven" },
			{ 4,  cstypes::pistol,  "Glock-18" },
			{ 30, cstypes::pistol,  "Tec-9" },
			{ 32, cstypes::pistol,  "P2000" },
			{ 36, cstypes::pistol,  "P250" },
			{ 61, cstypes::pistol,  "USP-S" },
			{ 63, cstypes::pistol,  "CZ75-auto" },
			{ 64, cstypes::pistol,  "R8 revolver" },

			{ 17, cstypes::smg,     "Mac-10" },
			{ 19, cstypes::smg,     "P90" },
			{ 23, cstypes::smg,     "Mp5-sd" },
			{ 24, cstypes::smg,     "Ump-45" },
			{ 26, cstypes::smg,     "PP-bizon" },
			{ 33, cstypes::smg,     "Mp7" },
			{ 34, cstypes::smg,     "Mp9" },

			{ 7,  cstypes::rifle,   "Ak-47" },
			{ 8,  cstypes::rifle,   "Aug" },
			{ 10, cstypes::rifle,   "Famas" },
			{ 13, cstypes::rifle,   "Galil ar" },
			{ 16, cstypes::rifle,   "M4a4" },
			{ 39, cstypes::rifle,   "SG553" },
			{ 60, cstypes::rifle,   "M4a1-S" },

			{ 25, cstypes::shotgun, "Xm1014" },
			{ 27, cstypes::shotgun, "Mag-7" },
			{ 29, cstypes::shotgun, "Sawed-off" },
			{ 35, cstypes::shotgun, "Nova" },

			{ 9,  cstypes::sniper,  "Awp" },
			{ 11, cstypes::sniper,  "G3sg1" },
			{ 38, cstypes::sniper,  "Scar-20" },
			{ 40, cstypes::sniper,  "Ssg-08" },

			{ 14, cstypes::lmg,     "M249" },
			{ 28, cstypes::lmg,     "Negev" },
		} };

		static constexpr std::size_t k_weapon_entry_count{ k_weapon_entries.size( ) };

		std::array<group_config, k_group_count> groups{};
		std::array<group_config, k_weapon_entry_count> weapon_groups{};

		[[nodiscard]] static std::int32_t find_weapon_index( std::uint16_t item_def_idx )
		{
			for ( std::int32_t i = 0; i < static_cast< std::int32_t >( k_weapon_entry_count ); ++i )
			{
				if ( k_weapon_entries[ static_cast< std::size_t >( i ) ].item_def_idx == item_def_idx )
				{
					return i;
				}
			}

			return -1;
		}

		group_config& get( std::uint32_t weapon_type )
		{
			const auto idx = weapon_type - cstypes::pistol;
			return this->groups[ idx < k_group_count ? idx : 2 ];
		}

		const group_config& get( std::uint32_t weapon_type ) const
		{
			const auto idx = weapon_type - cstypes::pistol;
			return this->groups[ idx < k_group_count ? idx : 2 ];
		}

		group_config& get_for_weapon( std::uint16_t item_def_idx, std::uint32_t weapon_type )
		{
			const auto idx = find_weapon_index( item_def_idx );
			if ( idx >= 0 )
			{
				return this->weapon_groups[ static_cast< std::size_t >( idx ) ];
			}

			return this->get( weapon_type );
		}

		const group_config& get_for_weapon( std::uint16_t item_def_idx, std::uint32_t weapon_type ) const
		{
			const auto idx = find_weapon_index( item_def_idx );
			if ( idx >= 0 )
			{
				return this->weapon_groups[ static_cast< std::size_t >( idx ) ];
			}

			return this->get( weapon_type );
		}

		[[nodiscard]] const group_config& resolve_runtime_cfg( std::uint16_t item_def_idx, std::uint32_t weapon_type ) const
		{
			const auto idx = find_weapon_index( item_def_idx );
			if ( idx >= 0 )
			{
				const auto& weapon_cfg = this->weapon_groups[ static_cast< std::size_t >( idx ) ];
				if ( weapon_cfg.aimbot.enabled || weapon_cfg.triggerbot.enabled || weapon_cfg.recoil_control_system.enabled )
				{
					return weapon_cfg;
				}
			}

			return this->get( weapon_type );
		}
	};

	struct combat_method_values
	{
		struct aimbot_values
		{
			float fov_mouse_event{ 5.0f };
			float fov_memory{ 5.0f };
			float smoothing_mouse_event{ 5.0f };
			float smoothing_memory{ 5.0f };
		};

		std::array<aimbot_values, combat::k_group_count> groups{};
		std::array<aimbot_values, combat::k_weapon_entry_count> weapon_groups{};

		aimbot_values& get( std::uint32_t weapon_type )
		{
			const auto idx = weapon_type - cstypes::pistol;
			return this->groups[ idx < combat::k_group_count ? idx : 2 ];
		}

		const aimbot_values& get( std::uint32_t weapon_type ) const
		{
			const auto idx = weapon_type - cstypes::pistol;
			return this->groups[ idx < combat::k_group_count ? idx : 2 ];
		}
	};

	struct esp
	{
		struct player
		{
			bool enabled{ true };

			struct spotted
			{
				bool enabled{ false };
			} m_spotted{};

			struct radar
			{
				enum class mode0 : std::uint8_t { external = 0, force_memory = 1 };

				bool enabled{ false };
				mode0 mode{ mode0::force_memory };
			} m_radar{};

			struct sound
			{
				enum class mode0 : std::uint8_t { mode_2d, mode_3d };

				bool enabled{ false };
				mode0 mode{ mode0::mode_2d };
				bool dynamic{ true };

				zdraw::rgba visible_color{ 255, 240, 150, 235 };
				zdraw::rgba hidden_color{ 255, 220, 90, 190 };
			} m_sound{};

			struct box
			{
				enum class style0 : std::uint8_t { full, cornered };

				bool enabled{ true };
				style0 style{ style0::cornered };
				bool fill{ true };
				bool outline{ true };
				float corner_length{ 10.0f };

				zdraw::rgba visible_color{ 140, 150, 235, 255 };
				zdraw::rgba occluded_color{ 110, 115, 170, 180 };
			} m_box{};

			struct skeleton
			{
				bool enabled{ true };
				float thickness{ 1.0f };

				zdraw::rgba visible_color{ 170, 175, 220, 255 };
				zdraw::rgba occluded_color{ 130, 135, 180, 180 };
			} m_skeleton{};

			struct hitboxes
			{
				bool enabled{ false };

				zdraw::rgba visible_color{ 150, 160, 240, 10 };
				zdraw::rgba occluded_color{ 115, 120, 185, 10 };

				bool fill{ true };
				bool outline{ true };
			} m_hitboxes{};

			struct health_bar
			{
				enum class position : std::uint8_t { left, top, bottom };

				bool enabled{ true };
				position position{ position::left };
				bool outline{ true };
				bool gradient{ true };
				bool show_value{ true };

				zdraw::rgba full_color{ 140, 150, 235, 255 };
				zdraw::rgba low_color{ 75, 80, 180, 255 };
				zdraw::rgba background_color{ 15, 16, 22, 150 };
				zdraw::rgba outline_color{ 15, 16, 22, 255 };
				zdraw::rgba text_color{ 195, 200, 215, 255 };
			} m_health_bar{};

			struct ammo_bar
			{
				enum class position : std::uint8_t { left, top, bottom };

				bool enabled{ true };
				position position{ position::bottom };
				bool outline{ true };
				bool gradient{ true };
				bool show_value{ false };

				zdraw::rgba full_color{ 140, 150, 235, 255 };
				zdraw::rgba low_color{ 75, 80, 180, 255 };
				zdraw::rgba background_color{ 15, 16, 22, 150 };
				zdraw::rgba outline_color{ 15, 16, 22, 255 };
				zdraw::rgba text_color{ 195, 200, 215, 255 };
			} m_ammo_bar{};

			struct info_flags
			{
				enum flag : std::uint16_t
				{
					none = 0,
					money = 1 << 0,
					armor = 1 << 1,
					kit = 1 << 2,
					scoped = 1 << 3,
					defusing = 1 << 4,
					flashed = 1 << 5,
					ping = 1 << 6,
					distance = 1 << 7,
					bomb = 1 << 8
				};

				bool enabled{ true };
				std::uint16_t flags{ flag::money | flag::armor | flag::kit | flag::scoped | flag::defusing | flag::flashed | flag::ping };

				zdraw::rgba money_color{ 120, 230, 160, 255 };
				zdraw::rgba armor_color{ 195, 200, 215, 255 };
				zdraw::rgba kit_color{ 140, 150, 235, 255 };
				zdraw::rgba scoped_color{ 195, 200, 215, 255 };
				zdraw::rgba defusing_color{ 140, 150, 235, 255 };
				zdraw::rgba flashed_color{ 255, 210, 120, 255 };
				zdraw::rgba distance_color{ 90, 95, 130, 255 };
				zdraw::rgba bomb_color{ 255, 185, 100, 255 };

				[[nodiscard]] bool has( flag f ) const { return this->flags & f; }
			} m_info_flags{};

			struct name
			{
				bool enabled{ true };
				zdraw::rgba color{ 195, 200, 215, 230 };
			} m_name{};

			struct weapon
			{
				enum class display_type : std::uint8_t { text, icon, text_and_icon };

				bool enabled{ true };
				display_type display{ display_type::icon };

				zdraw::rgba text_color{ 195, 200, 215, 210 };
				zdraw::rgba icon_color{ 195, 200, 215, 230 };
			} m_weapon{};
		} m_player{};

		struct item
		{
			bool enabled{ true };
			float max_distance{ 40.0f };

			struct icon
			{
				bool enabled{ true };
				zdraw::rgba color{ 195, 200, 215, 200 };
			} m_icon{};

			struct name
			{
				bool enabled{ false };
				zdraw::rgba color{ 195, 200, 215, 180 };
			} m_name{};

			struct ammo
			{
				bool enabled{ true };
				zdraw::rgba color{ 140, 150, 235, 200 };
				zdraw::rgba empty_color{ 180, 80, 80, 200 };
			} m_ammo{};

			struct filters
			{
				bool rifles{ true };
				bool smgs{ true };
				bool shotguns{ true };
				bool snipers{ true };
				bool pistols{ true };
				bool heavy{ true };
				bool grenades{ true };
				bool utility{ true };
			} m_filters{};
		} m_item{};

		struct projectile
		{
			bool enabled{ true };

			bool show_icon{ true };
			bool show_name{ true };
			bool show_timer_bar{ true };
			bool show_inferno_bounds{ true };
			bool show_bomb{ true };

			zdraw::rgba default_color{ 195, 200, 215, 200 };
			zdraw::rgba color_he{ 220, 150, 150, 220 };
			zdraw::rgba color_flash{ 230, 220, 150, 220 };
			zdraw::rgba color_smoke{ 160, 200, 180, 220 };
			zdraw::rgba color_molotov{ 220, 170, 130, 220 };
			zdraw::rgba color_decoy{ 170, 175, 200, 200 };
			zdraw::rgba color_bomb{ 255, 185, 100, 230 };

			zdraw::rgba timer_high_color{ 140, 150, 235, 255 };
			zdraw::rgba timer_low_color{ 220, 100, 100, 255 };
			zdraw::rgba bar_background{ 15, 16, 22, 150 };
		} m_projectile{};
	};

	struct misc
	{
		bool m_safe_mode{ true };

		struct grenades
		{
			bool enabled{ true };

			zdraw::rgba line_color{ 170, 175, 220, 200 };
			float line_thickness{ 2.0f };
			bool line_gradient{ true };

			bool show_bounces{ true };
			zdraw::rgba bounce_color{ 195, 200, 215, 255 };
			float bounce_size{ 2.0f };

			zdraw::rgba detonate_color{ 140, 150, 235, 255 };
			float detonate_size{ 4.0f };

			bool per_type_colors{ false };
			zdraw::rgba color_he{ 190, 140, 140, 200 };
			zdraw::rgba color_flash{ 200, 195, 150, 200 };
			zdraw::rgba color_smoke{ 150, 185, 165, 200 };
			zdraw::rgba color_molotov{ 195, 155, 130, 200 };
			zdraw::rgba color_decoy{ 160, 165, 185, 200 };

			bool local_only{ true };
			float fade_duration{ 0.3f };
		} m_grenades{};

		struct sniper_crosshair
		{
			bool enabled{ false };
			zdraw::rgba color{ 235, 235, 235, 220 };
			float size{ 8.0f };
			float thickness{ 1.4f };
		} m_sniper_crosshair{};

		struct recoil_crosshair
		{
			bool enabled{ false };
			zdraw::rgba color{ 235, 235, 235, 220 };
			float size{ 5.0f };
			float thickness{ 1.3f };
		} m_recoil_crosshair{};

		struct rgb_crosshair
		{
			bool enabled{ false };
			float speed{ 2.0f };
			float size{ 5.0f };
			float thickness{ 1.3f };
		} m_rgb_crosshair{};

		struct anti_flash
		{
			bool enabled{ false };
			float overlay_alpha{ 0.0f };
		} m_anti_flash{};

		struct oof_arrows
		{
			bool enabled{ false };
			float radius_scale{ 0.42f };
			float size{ 12.0f };
			float thickness{ 1.4f };
			zdraw::rgba color{ 235, 235, 235, 225 };
		} m_oof_arrows{};

		struct hit_marker
		{
			bool enabled{ false };
			float duration{ 0.25f };
			zdraw::rgba color{ 235, 235, 235, 255 };
		} m_hit_marker{};

		struct china_hat
		{
			bool enabled{ false };
			float radius{ 8.5f };
			float height{ 7.5f };
			zdraw::rgba color{ 140, 150, 235, 215 };
		} m_china_hat{};

		struct night_mode
		{
			bool enabled{ false };
			float darkness{ 0.35f };
		} m_night_mode{};

		struct knife_switch
		{
			bool enabled{ false };
		} m_knife_switch{};

		struct bomb_timer
		{
			bool enabled{ false };
			float scale{ 1.0f };
			float pos_x{ 18.0f };
			float pos_y{ 96.0f };
		} m_bomb_timer{};

		struct auto_pistol
		{
			bool enabled{ false };
		} m_auto_pistol{};

		struct auto_knife
		{
			bool enabled{ false };
		} m_auto_knife{};

		struct auto_tazer
		{
			bool enabled{ false };
		} m_auto_tazer{};

		struct auto_nade
		{
			bool enabled{ false };
		} m_auto_nade{};

		struct anti_afk
		{
			bool enabled{ false };
			float interval_s{ 30.0f };
		} m_anti_afk{};

		struct quick_stop
		{
			bool enabled{ false };
		} m_quick_stop{};

		struct spectator_list
		{
			bool enabled{ false };
			float pos_x{ 18.0f };
			float pos_y{ 250.0f };
		} m_spectator_list{};

		struct fov_changer
		{
			bool enabled{ false };
			int value{ 90 };
		} m_fov_changer{};

		struct remove_scope
		{
			enum mode0 : int
			{
				none = 0,
				remove_zoom = 1 << 0,
				remove_scope_overlay = 1 << 1,
				remove_headshot_punch = 1 << 2,
				fullbright = 1 << 3,
				all = remove_zoom
					| remove_scope_overlay
					| remove_headshot_punch
					| fullbright
			};

			int mode{ none };
		} m_remove_scope{};

		struct jump_throw
		{
			int key{ 0x51 }; // Q
		} m_jump_throw{};

		struct drop_bomb
		{
			int key{ 0 };
		} m_drop_bomb{};

		struct chat_spammer
		{
			bool enabled{ false };
			std::string text{ "" };
		} m_chat_spammer{};

		struct kill_sound
		{
			bool enabled{ false };
			std::string file_name{ "" };
		} m_kill_sound{};

		struct hit_sound
		{
			bool enabled{ false };
			std::string file_name{ "" };
		} m_hit_sound{};

		struct kill_say
		{
			bool enabled{ false };
			std::string text{ "" };
		} m_kill_say{};
	};

	inline combat g_legit_bot = [ ]
		{
			combat cfg{};
			for ( auto& group : cfg.groups )
			{
				group.aimbot.enabled = false;
				group.triggerbot.enabled = false;
				group.aimbot.head_only = false;
				group.aimbot.all_hitboxes = false;
				group.aimbot.autowall = false;
				group.aimbot.type = combat::aimbot::method_mouse_event;
				group.triggerbot.autowall = false;
				group.triggerbot.delay = 50;
			}

			for ( std::size_t i = 0; i < combat::k_weapon_entry_count; ++i )
			{
				const auto weapon_type = combat::k_weapon_entries[ i ].weapon_type;
				cfg.weapon_groups[ i ] = cfg.get( weapon_type );
			}

			return cfg;
		}( );
	inline combat g_combat = [ ]
		{
			combat cfg{};
			for ( auto& group : cfg.groups )
			{
				group.aimbot.enabled = false;
				group.triggerbot.enabled = false;
				group.aimbot.type = combat::aimbot::method_memory;
			}

			for ( std::size_t i = 0; i < combat::k_weapon_entry_count; ++i )
			{
				const auto weapon_type = combat::k_weapon_entries[ i ].weapon_type;
				cfg.weapon_groups[ i ] = cfg.get( weapon_type );
			}

			return cfg;
		}( );
	inline esp g_esp{};
	inline misc g_misc{};
	inline menu_controls g_menu_controls{};
	inline combat_method_values g_legit_bot_method_values{};
	inline combat_method_values g_combat_method_values{};

} // namespace settings
