#include <stdafx.hpp>

bool modules::initialize( )
{
	this->client = g::memory.get_module( "client.dll" );
	if ( !this->client )
	{
		return false;
	}

	this->engine2 = g::memory.get_module( "engine2.dll" );
	if ( !this->engine2 )
	{
		return false;
	}

	this->tier0 = g::memory.get_module( "tier0.dll" );
	if ( !this->tier0 )
	{
		return false;
	}

	this->schemasystem = g::memory.get_module( "schemasystem.dll" );
	if ( !this->schemasystem )
	{
		return false;
	}

	this->vphysics2 = g::memory.get_module( "vphysics2.dll" );
	if ( !this->vphysics2 )
	{
		return false;
	}

	g::console.print( "modules initialized." );

	return true;
}