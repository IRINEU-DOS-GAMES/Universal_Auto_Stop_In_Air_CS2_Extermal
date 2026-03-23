#pragma once

namespace features {

	namespace combat {

		class legit
		{
		public:
			void on_render( );
			void tick( );

		private:
			struct target
			{
				const systems::collector::player* player{};
				systems::bones::data bones{};
				math::vector3 aim_point{};
				int hitbox{ -1 };
				int hitgroup{ -1 };
				float damage{};
				float fov{};
				bool penetrated{};
			};

			[[nodiscard]] target select_target( const math::vector3& eye_pos, const math::vector3& view_angles, const std::vector<systems::collector::player>& players, const settings::combat::group_config& cfg, std::uintptr_t preferred_pawn = 0 ) const;
			[[nodiscard]] math::vector3 get_aim_point( const math::vector3& eye_pos, const math::vector3& view_angles, const systems::collector::player& player, const systems::bones::data& bones, const settings::combat::group_config& cfg, float& out_damage, int& out_hitbox, bool& out_penetrated ) const;

			[[nodiscard]] float get_fov( const math::vector3& view_angles, const math::vector3& eye_pos, const math::vector3& target_pos ) const;
			[[nodiscard]] float get_fov_radius( const math::vector3& eye_pos, const math::vector3& view_angles, float fov_degrees ) const;

			void draw_fov( const math::vector3& eye_pos, const math::vector3& view_angles, const settings::combat::aimbot& cfg );
			void aimbot( const math::vector3& eye_pos, const math::vector3& view_angles, const target& tgt, const settings::combat::aimbot& cfg, const settings::combat::recoil_control_system& recoil_cfg, int method );

			struct trigger_result
			{
				const systems::collector::player* player{};
				systems::bones::data bones{};
				math::vector3 point{};
				int hitbox{ -1 };
				int hitgroup{ -1 };
				float damage{};
				bool penetrated{};
			};

			[[nodiscard]] trigger_result trace_crosshair( const math::vector3& eye_pos, const math::vector3& view_angles, const std::vector<systems::collector::player>& players, const settings::combat::triggerbot& cfg ) const;
			void triggerbot( const math::vector3& eye_pos, const math::vector3& view_angles, const std::vector<systems::collector::player>& players, const settings::combat::triggerbot& cfg );

			void apply_autostop( bool full_stop, bool allow_in_air );
			void release_autostop( );

			animation::spring m_fov_alpha{};
			random::valve_rng m_rng{};
			bool m_rng_seeded{ false };

			math::vector2 m_aim_error{};
			std::uintptr_t m_last_aim_target_pawn{};
			std::uintptr_t m_locked_target_pawn{};
			int m_locked_target_hitbox{ -1 };

			float m_trigger_delay_end{ 0.0f };
			bool m_trigger_waiting{ false };
			bool m_trigger_held{ false };
			bool m_trigger_continuous_hold{ false };
			float m_trigger_release_time{ 0.0f };
			bool m_auto_scope_held{ false };
			float m_auto_scope_release_time{ 0.0f };
			float m_auto_scope_next_time{ 0.0f };

			bool m_autostop_active{ false };
			bool m_autostop_full_stop{ false };
			bool m_autostop_requested_tick{ false };
			bool m_in_air_autostop_latched{ false };
			bool m_autostop_shift_held{ false };
			std::chrono::steady_clock::time_point m_autostop_start{};
			std::vector<std::uint16_t> m_autostop_keys{};
		};

		class shared
		{
		public:
			struct context
			{
				std::uintptr_t weapon;
				std::uintptr_t weapon_vdata;
				std::uint32_t weapon_type;
				std::uint16_t item_def_idx;
				int num_bullets;
				float accuracy_penalty;
				float inaccuracy;
				float spread;
				float recoil_index;
				bool is_reloading;
				bool is_full_auto;
				bool is_scoped;
				bool weapon_ready;
				float current_time;
				float cycle_time;
				float last_shot_time;
				bool valid;
			};

			class penetration
			{
			public:
				struct weapon_data
				{
					float damage;
					float penetration;
					float range_modifier;
					float range;
					float armor_ratio;
					float headshot_multiplier;
				};

				struct result
				{
					float damage;
					int hitbox;
					bool penetrated;
				};

				void prepare( std::uintptr_t weapon_vdata, std::uintptr_t weapon );

				[[nodiscard]] bool run( const math::vector3& start, const math::vector3& end, const systems::collector::player& target, const systems::bones::data& bones, result& out ) const;
				[[nodiscard]] float get_max_damage( int hitgroup, int target_armor, bool has_helmet, int target_team ) const;
				[[nodiscard]] const weapon_data& get_weapon_data( ) const { return this->m_weapon_data; }

			private:
				weapon_data m_weapon_data{};
			};

			void tick( );

			[[nodiscard]] const context& ctx( ) const { return this->m_ctx; }
			[[nodiscard]] const penetration& pen( ) const { return this->m_pen; }

			[[nodiscard]] float calculate_hitchance( const math::vector3& eye_pos, const math::vector3& aim_angle, const systems::collector::player& target, const systems::bones::data& bones ) const;
			[[nodiscard]] std::uint32_t get_spread_seed( const math::vector3& angles, int tick ) const;
			[[nodiscard]] math::vector2 calculate_spread( int seed, float accuracy, float spread, float recoil_index, int item_def_idx, int num_bullets ) const;
			[[nodiscard]] math::vector3 extrapolate_stop( const math::vector3& pos ) const;
			[[nodiscard]] bool is_sniper_accurate( ) const;
			[[nodiscard]] float get_prediction_time( ) const;

		private:
			[[nodiscard]] float get_spread( std::uintptr_t weapon_vdata ) const;
			[[nodiscard]] float get_inaccuracy( std::uintptr_t pawn, std::uintptr_t weapon, std::uintptr_t weapon_vdata, const math::vector3& eye_angles, float accuracy_penalty ) const;

			context m_ctx{};
			penetration m_pen{};
			mutable std::shared_mutex m_ctx_mutex{};
		};

		inline legit g_legit{};
		inline shared g_shared{};

	} // namespace combat

} // namespace features
