#pragma once

class modules
{
public:
	bool initialize( );

	std::uintptr_t client{ 0 };
	std::uintptr_t engine2{ 0 };
	std::uintptr_t tier0{ 0 };
	std::uintptr_t schemasystem{ 0 };
	std::uintptr_t vphysics2{ 0 };
};