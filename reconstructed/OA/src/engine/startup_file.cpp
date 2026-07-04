// SPDX-License-Identifier: GPL-2.0
/*
 * startup_file.cpp  -  CStartupFile::CStartupFile(const char*)/
 * ~CStartupFile() (sec 10.148). See oa_setup_global_resources.h's own
 * class comment for the full ground-truthing detail (CCostProfile's
 * confirmed inheritance, the +0x4 field's later life as a genuine float
 * once CCostProfile's own vtable-slot-2 call overwrites it).
 *
 * Ground-truthed via objdump -dr against OA_real.ko:
 *   CStartupFile::CStartupFile(char const*)  .text+0x457c0, 10 bytes
 *   CStartupFile::~CStartupFile()            .text+0x45790,  7 bytes
 * (both C1/C2 and D1/D2 fold to these same two addresses respectively --
 * confirmed via nm, no virtual bases).
 *
 * Confirmed real ctor body (regparm(3): this=EAX, name=EDX -- confirmed
 * directly from CCostProfile::CCostProfile()'s own call site, which
 * loads EDX with the literal "CostProfile" string BEFORE calling this
 * constructor with `this` still in EAX): `*this = &_ZTV12CStartupFile[8
 * bytes in]` (the standard Itanium "+8 to skip offset-to-top/RTTI"
 * convention), then `*(this+4) = name` -- a RAW POINTER STORE, not a
 * meaningful float, despite `_field4`'s own declared type. This is not a
 * contradiction: CCostProfile::CCostProfile() (already reconstructed,
 * cost_profile.cpp) immediately follows this base constructor with a
 * real vtable-slot-2 dispatch that overwrites `_field4` with a genuine
 * float BEFORE anything (CSTGCPUInfo::Update(), setup_global_resources.
 * cpp) ever reads it -- see oa_setup_global_resources.h's own header
 * comment on that call chain. This constructor's own transient write is
 * reproduced via an explicit same-width (4 bytes on both the real 32-bit
 * target and this host build, since `_field4` is a `float`) raw pointer
 * store through an `unsigned int*` reinterpretation of `&_field4` -- NOT
 * a native pointer store, which would be 8 bytes on this project's
 * 64-bit host verify build and overrun the field (the same host/target
 * pointer-width hazard flagged throughout this project, e.g. sec
 * 10.142/10.143). No `<cstring>`/memcpy here -- this project's own
 * established convention avoids it in freestanding-built files (see
 * cpu_affinity.cpp/global_ctor.cpp/setup_global_resources.cpp's own
 * identical notes): it doesn't resolve under this project's `-m32
 * -ffreestanding` host codegen-check flags on this host's multilib
 * setup (missing 32-bit `bits/c++config.h`), confirmed by hitting that
 * exact build error while reconstructing this file. The same
 * "type-punned pointer" strict-aliasing pattern is already used
 * elsewhere in this codebase (e.g. global.cpp's own `UpdateHeadroom`/
 * `UpdateMasterTune`), just via a raw pointer cast instead of memcpy.
 *
 * Confirmed real dtor body: resets the vtable pointer back to this
 * class's own (`&_ZTV12CStartupFile[8 bytes in]`) and nothing else -- no
 * field destruction, matching the class's own single non-pointer-owning
 * `_field4`.
 */

#include "oa_setup_global_resources.h"

/* Real vtable data symbol (`_ZTV12CStartupFile`), referenced via the
 * `extern "C"` byte-array trick established sec 10.58/10.60
 * (CSTGRecordEvent/CCostProfile): `~CStartupFile()`'s own out-of-line
 * definition below is this class's Itanium "key function", so the
 * compiler emits the real vtable data for us in this very translation
 * unit -- no separate byte-array definition needed, just this
 * declaration to reference it by its real mangled name. */
extern "C" unsigned char _ZTV12CStartupFile[];

CStartupFile::CStartupFile(const char *name)
{
	_vtablePtr = _ZTV12CStartupFile + 8;

	*(unsigned int *)&_field4 = (unsigned int)(unsigned long)name;
}

CStartupFile::~CStartupFile()
{
	_vtablePtr = _ZTV12CStartupFile + 8;
}
