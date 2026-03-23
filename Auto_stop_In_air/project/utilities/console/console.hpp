#pragma once

class console
{
public:
	bool initialize( std::string_view title );

	template <typename... args_t>
	void print( std::format_string<args_t...> fmt, args_t&&... args ) const
	{
		this->emit( level::info, std::format( fmt, std::forward<args_t>( args )... ) );
	}

	template <typename... args_t>
	[[noreturn]] void error( std::format_string<args_t...> fmt, args_t&&... args ) const
	{
		this->emit( level::error, std::format( fmt, std::forward<args_t>( args )... ) );
		this->wait_and_exit( );
	}

	template <typename... args_t>
	void warn( std::format_string<args_t...> fmt, args_t&&... args ) const
	{
		this->emit( level::warn, std::format( fmt, std::forward<args_t>( args )... ) );
	}

	template <typename... args_t>
	void success( std::format_string<args_t...> fmt, args_t&&... args ) const
	{
		this->emit( level::success, std::format( fmt, std::forward<args_t>( args )... ) );
	}

private:
	void* m_handle{};
	void* m_input_handle{};

	enum class level { info, warn, error, success };

	void emit( level lvl, const std::string& message ) const;
	[[noreturn]] void wait_and_exit( ) const;
};