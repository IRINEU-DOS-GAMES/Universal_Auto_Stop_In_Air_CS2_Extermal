#pragma once

namespace animation {

	enum class easing : std::uint8_t
	{
		linear,
		ease_in,
		ease_out,
		ease_in_out
	};

	class tween
	{
	public:
		void start( float from, float to, float duration, easing ease = easing::ease_out );
		void update( );

		[[nodiscard]] float value( ) const { return this->m_value; }
		[[nodiscard]] bool finished( ) const { return this->m_finished; }

		void reset( );

	private:
		[[nodiscard]] float apply_easing( float t ) const;

		float m_from{ 0.0f };
		float m_to{ 0.0f };
		float m_value{ 0.0f };
		float m_duration{ 0.0f };
		float m_elapsed{ 0.0f };
		easing m_easing{ easing::linear };
		bool m_finished{ true };
	};

	class tween2d
	{
	public:
		void start( float from_x, float from_y, float to_x, float to_y, float duration, easing ease = easing::ease_out );
		void update( );

		[[nodiscard]] float x( ) const { return this->m_x.value( ); }
		[[nodiscard]] float y( ) const { return this->m_y.value( ); }
		[[nodiscard]] bool finished( ) const { return this->m_x.finished( ) && this->m_y.finished( ); }

		void reset( );

	private:
		tween m_x{};
		tween m_y{};
	};

	class spring
	{
	public:
		void set_target( float target );
		void update( );

		[[nodiscard]] float value( ) const { return this->m_value; }
		[[nodiscard]] bool settled( ) const;

		void set_stiffness( float stiffness ) { this->m_stiffness = stiffness; }
		void set_damping( float damping ) { this->m_damping = damping; }
		void snap( float value );

	private:
		float m_value{ 0.0f };
		float m_velocity{ 0.0f };
		float m_target{ 0.0f };
		float m_stiffness{ 200.0f };
		float m_damping{ 20.0f };
	};

	class spring2d
	{
	public:
		void set_target( float x, float y );
		void update( );

		[[nodiscard]] float x( ) const { return this->m_x.value( ); }
		[[nodiscard]] float y( ) const { return this->m_y.value( ); }
		[[nodiscard]] bool settled( ) const { return this->m_x.settled( ) && this->m_y.settled( ); }

		void set_stiffness( float stiffness );
		void set_damping( float damping );
		void snap( float x, float y );

	private:
		spring m_x{};
		spring m_y{};
	};

	class progress
	{
	public:
		void set( float target, float duration = 0.3f );
		void update( );

		[[nodiscard]] float value( ) const { return this->m_tween.value( ); }
		[[nodiscard]] float target( ) const { return this->m_target; }
		[[nodiscard]] bool finished( ) const { return this->m_tween.finished( ); }

	private:
		tween m_tween{};
		float m_target{ 0.0f };
	};

	class fade
	{
	public:
		void fade_in( float duration = 0.2f );
		void fade_out( float duration = 0.2f );
		void update( );

		[[nodiscard]] float alpha( ) const { return this->m_tween.value( ); }
		[[nodiscard]] bool visible( ) const { return this->m_alpha_target > 0.0f || !this->m_tween.finished( ); }
		[[nodiscard]] bool finished( ) const { return this->m_tween.finished( ); }

	private:
		tween m_tween{};
		float m_alpha_target{ 0.0f };
	};

} // namespace animation