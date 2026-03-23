#include <stdafx.hpp>

namespace detail {

	struct rgb
	{
		int r, g, b;

		static rgb lerp( const rgb& a, const rgb& b, float t )
		{
			return {
				a.r + static_cast< int >( ( b.r - a.r ) * t ),
				a.g + static_cast< int >( ( b.g - a.g ) * t ),
				a.b + static_cast< int >( ( b.b - a.b ) * t )
			};
		}
	};

	static constexpr auto k_reset{ "\033[0m" };
	static constexpr auto k_dim{ "\033[38;2;90;95;110m" };
	static constexpr auto k_text{ "\033[38;2;195;200;215m" };
	static constexpr auto k_bold{ "\033[1m" };

	static std::string colorize( char c, const rgb& col )
	{
		return std::format( "\033[38;2;{};{};{}m{}", col.r, col.g, col.b, c );
	}

	static std::string gradient_text( std::string_view text, const rgb& from, const rgb& to )
	{
		std::string result;
		result.reserve( text.size( ) * 20 );

		const auto len = text.size( );
		for ( std::size_t i = 0; i < len; ++i )
		{
			const auto t = len > 1 ? static_cast< float >( i ) / static_cast< float >( len - 1 ) : 0.0f;
			result += colorize( text[ i ], rgb::lerp( from, to, t ) );
		}

		return result;
	}

	static constexpr rgb k_accent_light{ 140, 150, 235 };
	static constexpr rgb k_accent_dark{ 75, 80, 180 };

	static constexpr rgb k_error_light{ 255, 130, 120 };
	static constexpr rgb k_error_dark{ 180, 40, 40 };

	static constexpr rgb k_warn_light{ 255, 210, 120 };
	static constexpr rgb k_warn_dark{ 200, 150, 50 };

	static constexpr rgb k_success_light{ 120, 230, 160 };
	static constexpr rgb k_success_dark{ 45, 160, 90 };

} // namespace detail

bool console::initialize( std::string_view title )
{
	this->m_handle = ::GetStdHandle( STD_OUTPUT_HANDLE );
	this->m_input_handle = ::GetStdHandle( STD_INPUT_HANDLE );

	DWORD mode{};
	if ( ::GetConsoleMode( this->m_handle, &mode ) )
	{
		::SetConsoleMode( this->m_handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING );
	}

	CONSOLE_FONT_INFOEX cfi{ .cbSize = sizeof( CONSOLE_FONT_INFOEX ) };
	if ( ::GetCurrentConsoleFontEx( this->m_handle, FALSE, &cfi ) )
	{
		wcscpy_s( cfi.FaceName, L"Consolas" );
		cfi.dwFontSize = { 0, 16 };
		::SetCurrentConsoleFontEx( this->m_handle, FALSE, &cfi );
	}

	CONSOLE_SCREEN_BUFFER_INFOEX csbi{ .cbSize = sizeof( CONSOLE_SCREEN_BUFFER_INFOEX ) };
	if ( ::GetConsoleScreenBufferInfoEx( this->m_handle, &csbi ) )
	{
		csbi.ColorTable[ 0 ] = RGB( 15, 16, 22 );
		csbi.ColorTable[ 7 ] = RGB( 185, 190, 210 );
		++csbi.srWindow.Bottom;
		++csbi.srWindow.Right;
		::SetConsoleScreenBufferInfoEx( this->m_handle, &csbi );
	}

	const auto hwnd = ::GetConsoleWindow( );
	if ( hwnd )
	{
		auto style = ::GetWindowLongW( hwnd, GWL_STYLE );
		style &= ~( WS_MAXIMIZEBOX | WS_SIZEBOX );
		::SetWindowLongW( hwnd, GWL_STYLE, style );

		::SetWindowLongW( hwnd, GWL_EXSTYLE, ::GetWindowLongW( hwnd, GWL_EXSTYLE ) | WS_EX_LAYERED );
		::SetLayeredWindowAttributes( hwnd, 0, 242, LWA_ALPHA );

		SMALL_RECT window_size{ 0, 0, 88, 29 };
		::SetConsoleWindowInfo( this->m_handle, TRUE, &window_size );

		COORD buffer_size{ 89, 9999 };
		::SetConsoleScreenBufferSize( this->m_handle, buffer_size );
	}

	CONSOLE_CURSOR_INFO cursor{ .dwSize = 1, .bVisible = FALSE };
	::SetConsoleCursorInfo( this->m_handle, &cursor );

	DWORD input_mode{};
	if ( ::GetConsoleMode( this->m_input_handle, &input_mode ) )
	{
		::SetConsoleMode( this->m_input_handle, input_mode | ENABLE_QUICK_EDIT_MODE | ENABLE_MOUSE_INPUT );
	}

	::FlushConsoleInputBuffer( this->m_input_handle );
	::SetConsoleTitleA( title.data( ) );

	std::printf( "\n" );
	this->print( "console initialized." );

	return this->m_handle != 0;
}

void console::emit( level lvl, const std::string& message ) const
{
	detail::rgb grad_from, grad_to, bracket, body;

	switch ( lvl )
	{
	case level::error:
		grad_from = detail::k_error_light;
		grad_to = detail::k_error_dark;
		bracket = detail::k_error_dark;
		body = detail::k_error_light;
		break;
	case level::warn:
		grad_from = detail::k_warn_light;
		grad_to = detail::k_warn_dark;
		bracket = detail::k_warn_dark;
		body = detail::k_warn_light;
		break;
	case level::success:
		grad_from = detail::k_success_light;
		grad_to = detail::k_success_dark;
		bracket = detail::k_success_dark;
		body = detail::k_success_light;
		break;
	default:
		grad_from = detail::k_accent_light;
		grad_to = detail::k_accent_dark;
		bracket = { 90, 95, 130 };
		body = { 195, 200, 215 };
		break;
	}

	const auto body_col = std::format( "\033[38;2;{};{};{}m", body.r, body.g, body.b );
	const auto label = detail::gradient_text( "By: IRINEU", grad_from, grad_to );

	std::printf( "  %s%s%s - %s%s%s\n", detail::k_dim, label.c_str( ), detail::k_reset, body_col.c_str( ), message.c_str( ), detail::k_reset );
}

void console::wait_and_exit( ) const
{
	const auto err_col = std::format( "\033[38;2;{};{};{}m", detail::k_error_light.r, detail::k_error_light.g, detail::k_error_light.b );

	std::printf( "\n  %s%s press enter to exit...%s", detail::k_dim, err_col.c_str( ), detail::k_reset );
	std::getchar( );
	std::exit( 1 );
}
