#include <stdafx.hpp>

namespace systems {

	bool bones::data::is_valid( ) const
	{
		const auto& root = this->bones[ 0 ].position;
		return root.x != 0.0f || root.y != 0.0f || root.z != 0.0f;
	}

	math::vector3 bones::data::get_position( std::uint32_t id ) const
	{
		const auto index = static_cast< std::size_t >( id );
		if ( index >= this->bones.size( ) )
		{
			return {};
		}

		return this->bones[ index ].position;
	}

	math::quaternion bones::data::get_rotation( std::uint32_t id ) const
	{
		const auto index = static_cast< std::size_t >( id );
		if ( index >= this->bones.size( ) )
		{
			return {};
		}

		return this->bones[ index ].rotation;
	}

	bones::data bones::get( std::uintptr_t bone_cache ) const
	{
		struct alignas( 16 ) bone_data
		{
			math::vector3 position;
			float scale;
			math::quaternion rotation;
		};

		std::array<bone_data, 128> raw{};
		if ( !g::memory.read( bone_cache, raw.data( ), sizeof( bone_data ) * 128 ) )
		{
			return {};
		}

		data result{};
		for ( std::size_t i = 0; i < result.bones.size( ); ++i )
		{
			result.bones[ i ].position = raw[ i ].position;
			result.bones[ i ].rotation = raw[ i ].rotation;
		}

		return result;
	}

} // namespace systems