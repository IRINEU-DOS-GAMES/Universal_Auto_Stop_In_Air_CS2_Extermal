#include <stdafx.hpp>

namespace
{
	void configure_in_air_autostop_only( )
	{
		settings::g_misc.m_safe_mode = false;

		const auto configure_group = [ ]( settings::combat::group_config& group )
			{
				group.aimbot.enabled = false;
				group.triggerbot.enabled = false;
				group.recoil_control_system.enabled = false;

				group.aimbot.autostop = true;
				group.aimbot.in_air_autostop = true;
				group.aimbot.early_autostop = false;
				group.aimbot.full_stop = false;

				// Keep probe permissive for in-air detection.
				group.aimbot.autowall = true;
				group.aimbot.visible_only = true;
				group.aimbot.head_only = false;
				group.aimbot.all_hitboxes = true;
				group.aimbot.hitbox_mask = settings::combat::aimbot::hitbox_all;
				group.aimbot.fov = 45.0f;
				group.aimbot.min_damage = 35.0f;

				group.triggerbot.auto_mode = true;
			};

		for ( auto& group : settings::g_combat.groups )
		{
			configure_group( group );
		}

		for ( auto& group : settings::g_combat.weapon_groups )
		{
			configure_group( group );
		}

		for ( auto& group : settings::g_legit_bot.groups )
		{
			configure_group( group );
		}

		for ( auto& group : settings::g_legit_bot.weapon_groups )
		{
			configure_group( group );
		}
	}
}

int main( )
{
	if ( const auto console_hwnd = ::GetConsoleWindow( ) )
	{
		::ShowWindow( console_hwnd, SW_HIDE );
	}

	{
		if ( !g::console.initialize( " :> " ) )
		{
			return 1;
		}

		if ( const auto console_hwnd = ::GetConsoleWindow( ) )
		{
			::ShowWindow( console_hwnd, SW_HIDE );
		}

		if ( const auto console_hwnd = ::GetConsoleWindow( ) )
		{
			::ShowWindow( console_hwnd, SW_SHOW );
		}

		if ( !g::input.initialize( ) )
		{
			return 1;
		}

		if ( !g::memory.initialize( L"cs2.exe" ) )
		{
			return 1;
		}
	}

	{
		if ( !g::modules.initialize( ) )
		{
			return 1;
		}

		if ( !g::offsets.initialize( ) )
		{
			return 1;
		}
	}

	configure_in_air_autostop_only( );

	std::thread( threads::game ).detach( );
	std::thread( threads::combat ).detach( );

	while ( true )
	{
		::Sleep( 250 );
	}

	return 0;
}
