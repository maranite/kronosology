// SPDX-License-Identifier: GPL-2.0
/*
 * atmel_primitives.cpp  -  cm_GetRandomBytes()/cm_SetChallengeParams():
 * two of the small CryptoMemory dongle primitives
 * SetupAtmelForAuthorizations() (atmel_setup.cpp) calls into. See
 * oa_atmel.h for the full symbol-name mapping (this project's
 * descriptive `cm_*`/`nv2ac_*` aliases for OA_real.ko's own obfuscated
 * real names).
 *
 * cm_GetRandomBytes @ .text+0x4f6730 (real name `sdflkjsvnd2s`, 15
 * bytes): confirmed a pure tail-forwarder to the real kernel
 * `get_random_bytes(buf, len)` -- its ONLY instructions besides
 * prologue/epilogue are the call itself (single relocation,
 * R_386_PC32 get_random_bytes). regparm(3) eax=buf/edx=len pass
 * straight through unchanged.
 *
 * cm_SetChallengeParams @ .text+0x4f61a0 (real name `sdflkjsvnd2a`, 18
 * bytes): stores the three GPA Diffie-Hellman decimal-string pointers
 * (regparm(3): eax=p, edx=q, ecx=g, matching this project's own
 * already-declared header signature `(p, q, g)`) into three .bss
 * globals for cm_ComputeChallenge (still a stub) to read later -- no
 * GMP-context setup, no validation, no dereference of the strings
 * themselves, just three raw pointer caches. Real ground-truth memory
 * layout is p/g/q, NOT p/q/g (confirmed via the three distinct .bss
 * offsets 0x5ca148/0x5ca14c/0x5ca150 written from eax/ecx/edx
 * respectively) -- a real but behaviorally inert quirk (nothing in
 * this project reads these three globals by raw relative offset;
 * cm_ComputeChallenge isn't reconstructed yet either way), preserved
 * here only as a comment, not as a packed struct. Exposed as
 * non-static globals so a host KAT can read them back directly.
 */

#include "oa_atmel.h"

extern "C" void get_random_bytes(void *buf, unsigned int len);

void cm_GetRandomBytes(unsigned char *buf, int len)
{
	get_random_bytes(buf, (unsigned int)len);
}

extern "C" const char *gGpaChallengeP;
extern "C" const char *gGpaChallengeG;
extern "C" const char *gGpaChallengeQ;
const char *gGpaChallengeP;
const char *gGpaChallengeG;
const char *gGpaChallengeQ;

void cm_SetChallengeParams(const char *p, const char *q, const char *g)
{
	gGpaChallengeP = p;
	gGpaChallengeG = g;
	gGpaChallengeQ = q;
}
