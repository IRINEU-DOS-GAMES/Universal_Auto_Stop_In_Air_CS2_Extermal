#include <stdafx.hpp>

bool offsets::initialize( )
{
	// Synced with cs2-dumper output (offsets.hpp), generated on 2026-03-19 07:39:12 UTC.
	constexpr std::uintptr_t dwCSGOInput = 0x2319FC0;
	constexpr std::uintptr_t dwEntityList = 0x24AF268;
	constexpr std::uintptr_t dwLocalPlayerController = 0x22F4188;
	constexpr std::uintptr_t dwLocalPlayerPawn = 0x2069B50;
	constexpr std::uintptr_t dwGlobalVars = 0x205E5C0;
	constexpr std::uintptr_t dwViewAngles = 0x231A648;
	constexpr std::uintptr_t dwViewMatrix = 0x230FF20;

	csgo_input = g::modules.client + dwCSGOInput;
	entity_list = g::modules.client + dwEntityList;
	local_player_controller = g::modules.client + dwLocalPlayerController;
	local_player_pawn = g::modules.client + dwLocalPlayerPawn;
	global_vars = g::modules.client + dwGlobalVars;
	view_angles = g::modules.client + dwViewAngles;
	view_matrix = g::modules.client + dwViewMatrix;

	return csgo_input && entity_list && local_player_controller && local_player_pawn && global_vars && view_angles && view_matrix;
}
