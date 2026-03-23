#pragma once

class input
{
public:
	bool initialize( );
	void inject_mouse( int x, int y, std::uint8_t buttons ) const;
	void inject_keyboard( std::uint16_t key, bool pressed ) const;

	enum mouse_buttons : std::uint8_t
	{
		none = 0,
		left_down = 1 << 0,
		left_up = 1 << 1,
		right_down = 1 << 2,
		right_up = 1 << 3,
		move = 1 << 4
	};

private:
	struct mouse_info_t
	{
		POINT pt;
		unsigned long data;
		unsigned long flags;
		unsigned long time;
		std::uintptr_t extra_info;
	};

	struct keyboard_info_t
	{
		std::uint16_t vk;
		std::uint16_t scan;
		unsigned long flags;
		unsigned long time;
		std::uintptr_t extra_info;
	};

	using fn_inject_mouse = BOOL( NTAPI* )( mouse_info_t*, int );
	using fn_inject_keyboard = BOOL( NTAPI* )( keyboard_info_t*, int );

	fn_inject_mouse m_inject_mouse{};
	fn_inject_keyboard m_inject_keyboard{};
};