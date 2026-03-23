#include <stdafx.hpp>

namespace animation {

	void tween::start( float from, float to, float duration, easing ease )
	{
		this->m_from = from;
		this->m_to = to;
		this->m_value = from;
		this->m_duration = duration;
		this->m_elapsed = 0.0f;
		this->m_easing = ease;
		this->m_finished = false;
	}

	void tween::update( )
	{
		if ( this->m_finished )
		{
			return;
		}

		const auto dt = zdraw::get_delta_time( );

		this->m_elapsed += dt;

		if ( this->m_elapsed >= this->m_duration )
		{
			this->m_value = this->m_to;
			this->m_finished = true;
			return;
		}

		const auto t = this->apply_easing( this->m_elapsed / this->m_duration );
		this->m_value = this->m_from + ( this->m_to - this->m_from ) * t;
	}

	void tween::reset( )
	{
		this->m_value = this->m_from;
		this->m_elapsed = 0.0f;
		this->m_finished = true;
	}

	float tween::apply_easing( float t ) const
	{
		switch ( this->m_easing )
		{
		case easing::ease_in:
			return t * t;

		case easing::ease_out:
			return 1.0f - ( 1.0f - t ) * ( 1.0f - t );

		case easing::ease_in_out:
			return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow( -2.0f * t + 2.0f, 2.0f ) * 0.5f;

		default:
			return t;
		}
	}

	void tween2d::start( float from_x, float from_y, float to_x, float to_y, float duration, easing ease )
	{
		this->m_x.start( from_x, to_x, duration, ease );
		this->m_y.start( from_y, to_y, duration, ease );
	}

	void tween2d::update( )
	{
		this->m_x.update( );
		this->m_y.update( );
	}

	void tween2d::reset( )
	{
		this->m_x.reset( );
		this->m_y.reset( );
	}

	// -------------------------------- //

	void spring::set_target( float target )
	{
		this->m_target = target;
	}

	void spring::update( )
	{
		const auto dt = zdraw::get_delta_time( );

		const auto diff = this->m_target - this->m_value;
		const auto accel = diff * this->m_stiffness - this->m_velocity * this->m_damping;

		this->m_velocity += accel * dt;
		this->m_value += this->m_velocity * dt;
	}

	bool spring::settled( ) const
	{
		return std::abs( this->m_target - this->m_value ) < 0.001f && std::abs( this->m_velocity ) < 0.001f;
	}

	void spring::snap( float value )
	{
		this->m_value = value;
		this->m_target = value;
		this->m_velocity = 0.0f;
	}

	void spring2d::set_target( float x, float y )
	{
		this->m_x.set_target( x );
		this->m_y.set_target( y );
	}

	void spring2d::update( )
	{
		this->m_x.update( );
		this->m_y.update( );
	}

	void spring2d::set_stiffness( float stiffness )
	{
		this->m_x.set_stiffness( stiffness );
		this->m_y.set_stiffness( stiffness );
	}

	void spring2d::set_damping( float damping )
	{
		this->m_x.set_damping( damping );
		this->m_y.set_damping( damping );
	}

	void spring2d::snap( float x, float y )
	{
		this->m_x.snap( x );
		this->m_y.snap( y );
	}

	// -------------------------------- //

	void progress::set( float target, float duration )
	{
		this->m_tween.start( this->m_tween.value( ), target, duration, easing::ease_out );
		this->m_target = target;
	}

	void progress::update( )
	{
		this->m_tween.update( );
	}

	// -------------------------------- //

	void fade::fade_in( float duration )
	{
		this->m_tween.start( this->m_tween.value( ), 1.0f, duration, easing::ease_out );
		this->m_alpha_target = 1.0f;
	}

	void fade::fade_out( float duration )
	{
		this->m_tween.start( this->m_tween.value( ), 0.0f, duration, easing::ease_out );
		this->m_alpha_target = 0.0f;
	}

	void fade::update( )
	{
		this->m_tween.update( );
	}

} // namespace animation