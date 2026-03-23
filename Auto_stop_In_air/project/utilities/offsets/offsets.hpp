#pragma once

class offsets
{
public:
	bool initialize( );

	std::uintptr_t csgo_input{ 0 };
	std::uintptr_t entity_list{ 0 };
	std::uintptr_t local_player_controller{ 0 };
	std::uintptr_t local_player_pawn{ 0 };
	std::uintptr_t global_vars{ 0 };
	std::uintptr_t view_angles{ 0 };
	std::uintptr_t view_matrix{ 0 };
};
