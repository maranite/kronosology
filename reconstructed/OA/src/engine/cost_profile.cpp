// SPDX-License-Identifier: GPL-2.0
/*
 * cost_profile.cpp  -  CCostProfile::CCostProfile(). See
 * oa_setup_global_resources.h's own file comment for the full
 * ground-truthing detail (base class discovery, the confirmed
 * unrolled-zero + 198-entry-array structure, and the cross-check
 * against setup_global_resources.cpp's own already-confirmed
 * `::operator new(0x12a0)` allocation size).
 *
 * `CStartupFile::CStartupFile(const char*)`/`~CStartupFile()` are left
 * as confirmed-real, deliberately deferred externs -- NOT defined
 * here, matching this project's established pattern for a base class
 * whose own body is out of scope for the derived class's
 * reconstruction (e.g. CEmergencyStealer for CLoadBalancer, sec
 * 10.59).
 */

#include "oa_setup_global_resources.h"

/* Real vtable data symbol (`_ZTV12CCostProfile`), referenced via the
 * `extern "C"` byte-array trick established in sec 10.58
 * (CSTGRecordEvent): lets this file point at the confirmed-real symbol
 * without needing to fully define its contents (none of CCostProfile's
 * own virtual methods are reconstructed yet). */
extern "C" unsigned char _ZTV12CCostProfile[];

CCostProfile *CCostProfile::sInstance;

CCostProfile::CCostProfile() : CStartupFile("CostProfile")
{
	/* Standard Itanium "+8 to skip offset-to-top/RTTI" convention,
	 * confirmed via the real relocation's own addend. */
	_vtablePtr = _ZTV12CCostProfile + 8;

	for (unsigned int i = 0; i < sizeof(_unrecovered_zeroed); i++)
		_unrecovered_zeroed[i] = 0;

	for (unsigned int i = 0; i < CCOSTPROFILE_ENTRY_COUNT; i++) {
		/* entries[i]._unaccounted0 deliberately left untouched --
		 * confirmed real: the constructor never writes it, before
		 * or during this loop. */
		entries[i].field4 = 0;
		entries[i].field8 = 0;
		entries[i].fieldC = 0;
		entries[i].field10 = 0;
	}

	sInstance = this;
}
