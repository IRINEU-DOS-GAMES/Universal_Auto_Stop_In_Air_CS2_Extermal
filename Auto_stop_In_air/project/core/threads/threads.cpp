#include <stdafx.hpp>

#include <timeapi.h>
#pragma comment( lib, "winmm.lib" )

namespace threads {

	void game( )
	{
		std::string last_map{};
		auto next_bvh_retry = std::chrono::steady_clock::time_point{};
		auto bvh_retry_attempts{ 0 };

		std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );

		while ( true )
		{
			systems::g_local.update( );

			if ( systems::g_local.valid( ) )
			{
				systems::g_entities.refresh( );
				systems::g_collector.run( );

				const auto global_vars = g::memory.read<std::uintptr_t>( g::offsets.global_vars );
				if ( global_vars )
				{
					const auto map_ptr = g::memory.read<std::uintptr_t>( global_vars + 0x188 );
					const auto current_map = map_ptr ? g::memory.read_string( map_ptr ) : std::string{};

					if ( !current_map.empty( ) && current_map != "<empty>" )
					{
						if ( current_map != last_map )
						{
							g::console.print( "map change: {} -> {}", last_map.empty( ) ? "none" : last_map, current_map );
							last_map = current_map;
							systems::g_bvh.clear( );
							bvh_retry_attempts = 0;
							next_bvh_retry = std::chrono::steady_clock::now( );
						}

						const auto now = std::chrono::steady_clock::now( );
						if ( !systems::g_bvh.valid( ) && now >= next_bvh_retry )
						{
							++bvh_retry_attempts;
							if ( bvh_retry_attempts == 1 )
							{
								g::console.print( "parsing bvh for {}...", current_map );
							}

							systems::g_bvh.parse( );
							const auto tri_count = systems::g_bvh.count( );
							if ( tri_count > 0 )
							{
								g::console.success( "bvh parsed. triangles: {}", tri_count );
							}
							else
							{
								if ( bvh_retry_attempts == 1 || ( bvh_retry_attempts % 5 ) == 0 )
								{
									g::console.warn( "bvh parse for {} returned 0 triangles (attempt {}). retrying...", current_map, bvh_retry_attempts );
								}

								next_bvh_retry = now + std::chrono::milliseconds( 750 );
							}
						}
					}
				}
			}
			else
			{
				if ( !last_map.empty( ) )
				{
					last_map = {};
					systems::g_bvh.clear( );
					bvh_retry_attempts = 0;
					next_bvh_retry = {};
				}
			}

			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
		}
	}

	void combat( )
	{
		timeBeginPeriod( 1 );

		constexpr auto target_tps{ 128 };
		constexpr auto tick_interval = std::chrono::nanoseconds( 1'000'000'000 / target_tps );
		auto next_tick = std::chrono::steady_clock::now( );

		while ( true )
		{
			if ( systems::g_local.valid( ) && systems::g_bvh.valid( ) )
			{
				systems::g_view.update( );
				features::combat::g_shared.tick( );
				features::combat::g_legit.tick( );
			}

			next_tick += tick_interval;

			const auto now = std::chrono::steady_clock::now( );
			if ( next_tick < now )
			{
				next_tick = now;
				continue;
			}

			std::this_thread::sleep_until( next_tick - std::chrono::milliseconds( 1 ) );

			while ( std::chrono::steady_clock::now( ) < next_tick )
			{
				_mm_pause( );
			}
		}
	}

} // namespace threads
