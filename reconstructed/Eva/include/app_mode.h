/*
 * app_mode.h  -  s_eAppMode / Eva_IsSimulation() / Eva_IsSimulationSVGA().
 *
 * Real global + 2 real accessor functions (.text+0x0804cd30/0x0804cd40, 13
 * bytes each: `return s_eAppMode == N;`), defined in src/init/app_mode.cpp --
 * deliberately its OWN translation unit, not eva_main.cpp (which owns main()
 * and is excluded from every verify/ binary's link per the Makefile's
 * `$(filter-out objs/init/eva_main.o,$(OBJ))` rule; keeping these here instead
 * lets every verify/ KAT link against the real accessors like any other
 * reconstructed TU, no special-casing needed).
 *
 * s_eAppMode itself is set exactly once, from argv[0]'s basename, by main()'s
 * own app-mode-detection step in eva_main.cpp (see README.md's Stage 1
 * writeup) -- kept as a plain extern global (not wrapped in a setter) to match
 * this project's established convention for this class of real raw global
 * (s_bIsFinished/s_bRunning etc.).
 *
 * Confirmed real callers of the 2 accessors (Stage 6 breadth sweep,
 * 2026-07-25): only CCommDriver::setupfifoname() (this project's own
 * reconstruction, src/ipc/comm_driver.cpp) and CESGlobalTask::
 * SetLCDCalibration() (real, but Peg/CForm UI -- out of scope, not
 * reconstructed; see README.md's Stage-6 survey note).
 */

#ifndef APP_MODE_H
#define APP_MODE_H

extern int s_eAppMode;

extern "C" {
bool Eva_IsSimulation(void);
bool Eva_IsSimulationSVGA(void);
}

#endif /* APP_MODE_H */
