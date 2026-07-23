/*
 * eva_main.cpp  -  _start / main / Ouch, Eva's real boot path.
 *
 * Transcribed from the Ghidra decompile export:
 *   main  Decomp/EVA_Decomp/eva_export/functions/main@0804cd50.c
 *   Ouch  Decomp/EVA_Decomp/eva_export/functions/Ouch@0804cd10.c
 * _start is the standard glibc CRT entry (__libc_start_main(main, ...)) -- not
 * reproduced here, the real toolchain's own crt1.o provides it once this links
 * against a matching libc (see README.md's "Linking / build-ABI status").
 *
 * See README.md's "Boot path (Stage 1)" section for the full call-by-call writeup
 * this was transcribed from, including the argv[0]-basename app-mode quirk and the
 * CCommDriver::getInstance overload-ordering hazard.
 */

#include "comm_driver.h"
#include "lcd_control.h"
#include "omega_interface.h"
#include "panel_ifc_task.h"
#include "ustg_user_api.h"

#include <cstdio>
#include <cstring>
#include <csignal>
#include <sched.h>
#include <pthread.h>

/* Real global, set by the argv[0]-basename check below; read by the "else" branch
 * to avoid re-setting hardware margins if somehow re-entered with mode already set.
 */
static int s_eAppMode = 0;

/* Real handler (.text+0x0804cd10, 17 bytes) -- installed via signal(SIGINT, Ouch)
 * after COmegaInterface::Init() returns (see README.md point 8 for why that ordering
 * is preserved as found, not "fixed"). Only sets a real global latch, does no work
 * itself -- some other code (not yet traced) presumably polls s_bIsFinished.
 */
static volatile char s_bIsFinished = 0;

extern "C" void Ouch(int /*signum*/)
{
	if (s_bIsFinished == 0)
		s_bIsFinished = 1;
}

int main(int argc, char **argv, char **envp)
{
	/* Real code zeroes a 128-byte (0x20 dwords) cpu_set_t-shaped buffer by hand
	 * before calling sched_setaffinity -- reproduced with the real libc type
	 * instead of a raw byte array, functionally identical.
	 */
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(2, &cpuset);
	puts("Eva will run on CPU 2");
	sched_setaffinity(0, sizeof(cpuset), &cpuset);

	/* Return value genuinely ignored by the real binary -- not a decompiler
	 * omission, faithfully preserved (see README.md point 2).
	 */
	USTGUserAPI::Connect();
	USTGAPILCDControl::LoadStoredSettings();

	fflush(stdout);

	sigset_t blocked;
	sigemptyset(&blocked);
	sigaddset(&blocked, SIGTERM);
	sigaddset(&blocked, SIGCHLD);
	sigaddset(&blocked, SIGRTMIN + 8); /* real signal 0x28 */
	pthread_sigmask(SIG_BLOCK, &blocked, 0);

	/* argv[0]-basename app-mode detection. The real disassembly is a manually
	 * inlined byte-compare loop (a decompiler artifact of an inlined strcmp/
	 * strncmp); reproduced here with real strcmp() calls against the exact same
	 * literals and iteration counts (7 bytes = "EvaSim\0", 11 bytes =
	 * "EvaSimSVGA\0"), which is functionally identical -- see README.md's
	 * "app-mode detection" note for the full byte-count derivation. Naming the
	 * staged VM binary anything other than EvaSim/EvaSimSVGA/Eva changes which
	 * branch fires below.
	 */
	const char *slash = strrchr(argv[0], '/');
	const char *basename = slash ? slash + 1 : argv[0];

	if (strcmp(basename, "EvaSim") == 0) {
		s_eAppMode = 1;
	} else if (strcmp(basename, "EvaSimSVGA") == 0) {
		s_eAppMode = 2;
	} else if (s_eAppMode == 0) {
		/* Real hardware path -- real touch-panel calibration margins, only
		 * set here, on real hardware (argv[0] basename "Eva"), matching
		 * neither simulator name.
		 */
		CEditor::CPanelIfcTask::SetMargin(CEditor::CPanelIfcTask::kMargin0, 10);
		CEditor::CPanelIfcTask::SetMargin(CEditor::CPanelIfcTask::kMargin1, 12);
		CEditor::CPanelIfcTask::SetMargin(CEditor::CPanelIfcTask::kMargin2, 5);
		CEditor::CPanelIfcTask::SetMargin(CEditor::CPanelIfcTask::kMargin3, 7);
	}

	/* Real constructing overload -- the only place the CCommDriver singleton is
	 * ever created (see comm_driver.h).
	 */
	CCommDriver::getInstance(argv);

	puts("begin omega init");
	Omega.Init(0);
	puts("end omega init");

	/* Installed after Init() returns -- see README.md point 8; preserved as
	 * found, not moved earlier.
	 */
	signal(SIGINT, Ouch);

	puts("Start closing");
	Omega.Close();
	puts("End closing");

	return 0;
}
