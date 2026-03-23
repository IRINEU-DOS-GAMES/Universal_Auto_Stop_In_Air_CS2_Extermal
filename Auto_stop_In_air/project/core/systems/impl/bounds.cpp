#include <stdafx.hpp>

namespace systems {

	bool bounds::data::is_valid( ) const
	{
		return this->min.x != 0xdead;
	}

	bounds::data bounds::get( const bones::data& bone_data ) const
	{
		auto screen_min = math::vector2{ std::numeric_limits<float>::max( ), std::numeric_limits<float>::max( ) };
		auto screen_max = math::vector2{ std::numeric_limits<float>::lowest( ), std::numeric_limits<float>::lowest( ) };
		auto one_point_is_valid{ false };

		constexpr auto width_offset{ 7.5f };
		constexpr auto height_offset{ 9.0f };

		for ( const auto& bones : bone_data.bones )
		{
			const auto& bone_pos = bones.position;
			if ( bone_pos.x == 0.0f && bone_pos.y == 0.0f && bone_pos.z == 0.0f )
			{
				continue;
			}

			const std::array<math::vector3, 9> expanded_points
			{
				bone_pos,
				bone_pos + math::vector3{ width_offset, 0.0f, 0.0f },
				bone_pos + math::vector3{ -width_offset, 0.0f, 0.0f },
				bone_pos + math::vector3{ 0.0f, width_offset, 0.0f },
				bone_pos + math::vector3{ 0.0f, -width_offset, 0.0f },
				bone_pos + math::vector3{ 0.0f, 0.0f, height_offset },
				bone_pos + math::vector3{ 0.0f, 0.0f, -height_offset },
				bone_pos + math::vector3{ width_offset, width_offset, 0.0f },
				bone_pos + math::vector3{ -width_offset, -width_offset, 0.0f }
			};

			for ( const auto& expanded_pos : expanded_points )
			{
				const auto position_projected = g_view.project( expanded_pos );
				if ( !g_view.projection_valid( position_projected ) )
				{
					continue;
				}

				one_point_is_valid = true;
				screen_min.x = std::min( screen_min.x, position_projected.x );
				screen_min.y = std::min( screen_min.y, position_projected.y );
				screen_max.x = std::max( screen_max.x, position_projected.x );
				screen_max.y = std::max( screen_max.y, position_projected.y );
			}
		}

		if ( !one_point_is_valid )
		{
			return { { 0xdead, 0xdead }, { } };
		}

		return { screen_min, screen_max };
	}

} // namespace systems