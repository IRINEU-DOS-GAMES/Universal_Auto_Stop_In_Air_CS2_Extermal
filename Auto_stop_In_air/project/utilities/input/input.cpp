#include <stdafx.hpp>

bool input::initialize( )
{
	const auto win32u = GetModuleHandleA( "win32u.dll" );
	if ( !win32u )
	{
		return false;
	}

	this->m_inject_mouse = reinterpret_cast< fn_inject_mouse >( GetProcAddress( win32u, "NtUserInjectMouseInput" ) );
	this->m_inject_keyboard = reinterpret_cast< fn_inject_keyboard >( GetProcAddress( win32u, "NtUserInjectKeyboardInput" ) );

	return this->m_inject_mouse && this->m_inject_keyboard;
}

void input::inject_mouse( int x, int y, std::uint8_t buttons ) const
{
	if ( !this->m_inject_mouse )
	{
		return;
	}

	unsigned long flags{ 0 };

	if ( buttons & mouse_buttons::move )
	{
		flags |= MOUSEEVENTF_MOVE;
	}

	if ( buttons & mouse_buttons::left_down )
	{
		flags |= MOUSEEVENTF_LEFTDOWN;
	}

	if ( buttons & mouse_buttons::left_up )
	{
		flags |= MOUSEEVENTF_LEFTUP;
	}

	if ( buttons & mouse_buttons::right_down )
	{
		flags |= MOUSEEVENTF_RIGHTDOWN;
	}

	if ( buttons & mouse_buttons::right_up )
	{
		flags |= MOUSEEVENTF_RIGHTUP;
	}

	mouse_info_t info{};
	info.pt.x = x;
	info.pt.y = y;
	info.data = 0;
	info.flags = flags;
	info.time = 0;
	info.extra_info = 0;

	this->m_inject_mouse( &info, 1 );
}

void input::inject_keyboard( std::uint16_t key, bool pressed ) const
{
	if ( !this->m_inject_keyboard )
	{
		return;
	}

	keyboard_info_t info{};
	info.vk = key;
	info.scan = static_cast< std::uint16_t >( MapVirtualKeyW( key, MAPVK_VK_TO_VSC ) );
	info.flags = pressed ? 0 : KEYEVENTF_KEYUP;
	info.time = 0;
	info.extra_info = 0;

	this->m_inject_keyboard( &info, 1 );
}