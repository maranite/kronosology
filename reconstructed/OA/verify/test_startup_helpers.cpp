// SPDX-License-Identifier: GPL-2.0
/*
 * test_startup_helpers.cpp  -  host-side known-answer tests for the four
 * init_module()-call-chain helpers reconstructed in
 * src/init/startup_helpers.cpp (sec 10.179):
 *   init_cpp_support(), GetInstalledRAM(), IncProgressBar(),
 *   SetInstalledOptions(int).
 *
 * Links only src/init/startup_helpers.cpp. Provides the two link-time
 * dependencies that real file references but does not define itself:
 *   - STGAPIFrontPanelStatus::sInstance storage (every other panel-
 *     touching test defines its own copy identically -- see
 *     test_engine_startup_bits2.cpp:83 etc.);
 *   - a COmapNKS4_IncProgressBar() mock (the real symbol is external,
 *     from OmapNKS4Module.ko) that counts its invocations.
 * gInstalledRAM's storage comes from startup_helpers.cpp itself.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "oa_setup_global_resources.h"   /* STGAPIFrontPanelStatus, GetInstalledRAM, IncProgressBar */
#include "oa_init.h"                      /* init_cpp_support, SetInstalledOptions */

static int g_fail;

static void check_eq(const char *label, unsigned long got, unsigned long want)
{
	if (got == want) {
		printf("  ok    %-52s 0x%lx\n", label, got);
		return;
	}
	printf("  FAIL  %-52s got=0x%lx want=0x%lx\n", label, got, want);
	g_fail++;
}

/* ---- link-time dependencies ---- */
unsigned char *STGAPIFrontPanelStatus::sInstance;

static int g_progressTicks;
extern "C" void COmapNKS4_IncProgressBar(void) { g_progressTicks++; }

/* gInstalledRAM is defined in startup_helpers.cpp; drive it from here. */
extern "C" unsigned int gInstalledRAM;

int main(void)
{
	printf("init_module startup-helpers known-answer test\n");
	printf("=============================================\n");

	printf("[1] init_cpp_support() is a confirmed no-op (bare ret)\n");
	init_cpp_support();          /* must not crash / must do nothing */
	printf("  ok    init_cpp_support() returned cleanly\n");

	printf("[2] GetInstalledRAM() returns the .bss+0x20 global verbatim\n");
	gInstalledRAM = 0;
	check_eq("GetInstalledRAM() when global==0", GetInstalledRAM(), 0);
	gInstalledRAM = 0x40000000;  /* 1 GiB in bytes */
	check_eq("GetInstalledRAM() when global==1GiB", GetInstalledRAM(), 0x40000000);
	gInstalledRAM = 0x12345678;
	check_eq("GetInstalledRAM() passes value through unmodified",
		 GetInstalledRAM(), 0x12345678);

	printf("[3] IncProgressBar() forwards to COmapNKS4_IncProgressBar()\n");
	g_progressTicks = 0;
	IncProgressBar();
	check_eq("one IncProgressBar() -> one external tick", g_progressTicks, 1);
	IncProgressBar();
	IncProgressBar();
	check_eq("three total IncProgressBar() calls -> three ticks",
		 g_progressTicks, 3);

	printf("[4] SetInstalledOptions(): OR low byte into sInstance+0x1090\n");
	{
		unsigned char *panel = (unsigned char *)calloc(1, STGAPI_FRONTPANEL_SIZE);
		STGAPIFrontPanelStatus::sInstance = panel;

		SetInstalledOptions(0x20);
		check_eq("first SetInstalledOptions(0x20) sets bit 0x20",
			 panel[STGAPI_OFF_INSTALLED_OPTIONS], 0x20);

		SetInstalledOptions(0x10);
		check_eq("SetInstalledOptions(0x10) ORs in -> 0x30",
			 panel[STGAPI_OFF_INSTALLED_OPTIONS], 0x30);

		SetInstalledOptions(0x20);  /* already set -> stays 0x30 */
		check_eq("re-OR of an already-set bit is idempotent",
			 panel[STGAPI_OFF_INSTALLED_OPTIONS], 0x30);

		/* Only the LOW BYTE of the argument participates, and only the
		 * single target byte is touched -- the neighbouring 0x1091
		 * (STGAPI_OFF_CAL_MARKER) must remain untouched. */
		panel[STGAPI_OFF_INSTALLED_OPTIONS] = 0;
		SetInstalledOptions(0x1FF);
		check_eq("SetInstalledOptions(0x1FF) uses low byte only -> 0xFF",
			 panel[STGAPI_OFF_INSTALLED_OPTIONS], 0xFF);
		check_eq("adjacent byte 0x1091 untouched by the byte-wide OR",
			 panel[STGAPI_OFF_CAL_MARKER], 0x00);

		free(panel);
	}

	printf("[5] SetInstalledOptions() tolerates a NULL sInstance\n");
	STGAPIFrontPanelStatus::sInstance = 0;
	SetInstalledOptions(0x20);   /* guarded -> must not dereference NULL */
	printf("  ok    SetInstalledOptions() with NULL sInstance did not crash\n");

	printf("\n%s (%d failed checks)\n",
	       g_fail ? "SOME CHECKS FAILED" : "all checks passed", g_fail);
	return g_fail ? 1 : 0;
}
