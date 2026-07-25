/*
 * app_mode.cpp  -  see include/app_mode.h.
 *
 * s_eAppMode  Decomp/EVA_Decomp/eva_export/functions/main@0804cd50.c (the write
 *             sites; app-mode detection from argv[0]'s basename, see
 *             src/init/eva_main.cpp / README.md's Stage 1 writeup).
 * Eva_IsSimulation      Decomp/EVA_Decomp/eva_export/functions/Eva_IsSimulation@0804cd30.c
 * Eva_IsSimulationSVGA  Decomp/EVA_Decomp/eva_export/functions/Eva_IsSimulationSVGA@0804cd40.c
 *
 * Both accessors are real, trivial (13 bytes each): `return s_eAppMode == N;`.
 * Split into their own TU (Stage 6 breadth sweep, 2026-07-25) rather than
 * living in eva_main.cpp alongside main() -- see app_mode.h's own header
 * comment for why.
 */

#include "app_mode.h"

int s_eAppMode = 0;

extern "C" bool Eva_IsSimulation(void)
{
	return s_eAppMode == 1;
}

extern "C" bool Eva_IsSimulationSVGA(void)
{
	return s_eAppMode == 2;
}
