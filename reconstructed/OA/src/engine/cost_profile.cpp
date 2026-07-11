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

/*
 * CStartupFile::Load()  --  faithfully reconstructed from OA_322.ko
 * @.text+0x458d0 (149 bytes). Real control flow:
 *     TString<256> path = this->Path();                 // via CFileFolder root
 *     if (!CFileStream::Exists(path)) return 9;          // startup file absent
 *     CFileStream fs(path, eFileOpenAttr(0), eEndianness(0));
 *     int rc = this->LoadData(fs);   // virtual slot 4 -> CCostProfile::LoadData
 *     fs.~CFileStream();
 *     return rc;
 *
 * VM/bring-up divergence (2026-07-10): the CFileStream / CFileFolder /
 * TString<256> file-I/O subsystem is not reconstructed in this build and no
 * "CostProfile" startup file exists in the VM image, so the real file-absent
 * branch (return 9, no stream opened, LoadData never dispatched) is taken
 * unconditionally -- the same "absent resource -> honest no-op" gate used for
 * the AT88 auth chip (atmel_setup.cpp). On real hardware, with the file
 * present, execution falls through to LoadData unchanged.
 *
 * -mregparm=3: `this` arrives in EAX; returns int in EAX. Declared with the
 * real mangled name so the vtable below can point a slot straight at it.
 */
extern "C" __attribute__((regparm(3))) int _ZN12CStartupFile4LoadEv(void *self)
{
	(void)self;
	return 9;	/* eFileResult: CostProfile startup file not present */
}

/*
 * A correctly-shaped CCostProfile vtable that we control.
 *
 * ROOT CAUSE of the old marker-8 wild-call: the reconstructed header
 * (oa_setup_global_resources.h) declares only `virtual ~CStartupFile()` --
 * it does NOT declare Load/Save/LoadData/SaveData as virtual. So the vtable
 * GCC auto-synthesizes for CCostProfile (`_ZTV12CCostProfile`) contains only
 * the two destructor slots; `_vtablePtr[2]` (slot 2, read+called at
 * setup_global_resources.cpp step 10) therefore indexes PAST GCC's short
 * vtable into adjacent .rodata (observed once as 0x203a414f, the ASCII of a
 * nearby log string). That is a genuine latent wild-call bug of the same
 * class as the AllocEj/AllocEm mismatch -- it only avoided a crash because
 * the stray bytes happened to decode < PAGE_OFFSET and a runtime guard
 * skipped the dispatch; a different link layout could have jumped into
 * garbage on real hardware too.
 *
 * Rather than fight GCC's auto-emission (declaring the extra virtuals would
 * force definitions for all of them and reshape every CStartupFile-derived
 * class), we point `_vtablePtr` at this hand-built table whose layout
 * matches the real _ZTV12CCostProfile relocations
 * (OA_322.ko .rel.rodata._ZTV12CCostProfile), Itanium C++ ABI:
 *   [0] offset-to-top = 0
 *   [1] RTTI          = 0        (class built -fno-rtti)
 *   [2] ~CCostProfile           (D1)  -- not reached at load (singleton)
 *   [3] ~CCostProfile [deleting](D0)  -- not reached at load
 *   [4] CStartupFile::Load             <-- the slot dispatched at load
 *   [5] CStartupFile::Save(bool)       -- not reached at load
 *   [6] CCostProfile::LoadData(CFileStream&) -- only if file present
 *   [7] CCostProfile::SaveData(CFileStream&) -- not reached at load
 * Only Load() is reconstructed; the remaining slots are null. None of them
 * is dispatched on the module-load path (the singleton is never destroyed;
 * Save/SaveData never run at load; LoadData is gated behind the file-present
 * branch that Load() reports as "absent" in the VM), so a null there can
 * never be called during load -- and, unlike the old short auto-vtable, is
 * an honest, deterministic "not reconstructed" rather than a wild pointer.
 * (GCC still emits its own short, weak _ZTV12CCostProfile, but nothing
 * references it now.)
 */
typedef void (*oa_vfn)(void);
static const oa_vfn kCCostProfileVtbl[8] = {
	0, 0,					/* offset-to-top, RTTI            */
	0, 0,					/* [2] D1, [3] D0                 */
	(oa_vfn)&_ZN12CStartupFile4LoadEv,	/* [4] slot 2: CStartupFile::Load */
	0, 0, 0,				/* [5] Save, [6] LoadData, [7] SaveData */
};

CCostProfile *CCostProfile::sInstance;

CCostProfile::CCostProfile() : CStartupFile("CostProfile")
{
	/* Standard Itanium "+8 to skip offset-to-top/RTTI" convention,
	 * confirmed via the real relocation's own addend. Point at our
	 * correctly-shaped table (above), so &[2] == byte offset 8 and slot 2
	 * (_vtablePtr[2]) is a valid pointer to CStartupFile::Load, not the
	 * out-of-bounds read past GCC's short auto-synthesized vtable. */
	_vtablePtr = (void *)&kCCostProfileVtbl[2];

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
