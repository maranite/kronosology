// SPDX-License-Identifier: GPL-2.0
/*
 * sysex_objects_ge_region.h  -  CSysExGlobal/CSysExKarmaGE/
 * CSysExGETemplate/CSysExRegion: 4 more siblings of the
 * CSysEx.../CSysExObjectBase family (sysex_objects.h/sysex_object_names.h/
 * sysex_control_objects.h), each with the common GetStorageId/
 * GetVersion/GetObjectSize/GetObjectSizeForExport trio (rounds 39-40)
 * plus the extended GetNumBanks/HasDigests/GetSysExBankId/GetNumOfObject
 * trio (round 41's CSysExKarmaGEInfo shape).
 *
 * FOUND 2026-07-29 (round 42, solo -- session-wide 200-subagent dispatch
 * cap hit, see PROJECT_BRAIN/status.md); a fresh manifest survey of this
 * family's remaining siblings. Confirmed via
 * /home/share/Decomp/EVA_Decomp/eva_export's own per-function decompiles.
 *
 * Per-class real literal constants (GetStorageId/GetNumBanks/HasDigests/
 * GetVersion/GetObjectSize/GetObjectSizeForExport):
 *   CSysExGlobal:     5 / 1 / 1 / 2 / 0x6084 / 0x6084
 *   CSysExKarmaGE:    6 / 0xc / 1 / 0 / 0x9ec / 0x9f0
 *   CSysExGETemplate: 7 / 4  / 1 / 0 / 0x10580 / 0x10584
 *   CSysExRegion:     0xb / 1 / 1 / 1 / 0x130 / 0x130
 * `GetSysExBankId(int)` is an identity passthrough for all 4 EXCEPT
 * CSysExGlobal/CSysExRegion, which always return 0 (`this`/arg unused).
 * `GetNumOfObject(int)` is a trivial literal for all 4 (1 / 0x80 / 1 /
 * 10000).
 *
 * `GetObjectPointer(int, int)` -- all 4 real and reconstructed:
 *   CSysExGlobal:     discards both `CStorage::GetInstance()`/
 *     `GetGlobal()` call results (real declared return type IS `void`,
 *     same genuine-discard shape as CSysExSongControl::GetObjectPointer,
 *     round 41) -- `CStorage` modeled as a minimal no-op stand-in,
 *     storage_converter_ext_stubs.h.
 *   CSysExKarmaGE:    `CKGUtil::GetUserGE(param_1) + param_2*0x9ec`.
 *   CSysExGETemplate: `CKGUtil::GetUserKarmaTemplate(param_1) +
 *     param_2*0x10580`.
 *   CSysExRegion:     `(param_2<10000 ? param_2 : 0)*0x130 +
 *     CKGUtil::sm_poRegionHolder` -- a real, confirmed static member
 *     (base of a 10000-entry region array), added to `sysex_control_
 *     objects.h`'s `CKGUtil` this round.
 *
 * `GetTotalSizeForExport(int, int)` -- only CSysExRegion's own version
 * is reconstructed: a genuine, fully concrete (no "could not recover"
 * warning) 8x-unrolled loop counting `0x130` for every region in
 * `[0,10000)` whose byte at `sm_poRegionHolder + 0x18 + i*0x130` is
 * nonzero ("active" flag). CSysExGlobal's own version is deliberately
 * NOT reconstructed -- Ghidra's own decompile flags "Could not recover
 * jumptable, too many branches" through an unconfirmed vtable slot on
 * `param_1` (a caller-supplied, non-`this` pointer), same unresolvable
 * shape as round 40's 4 deferred `GetNumObjectsForDigest` overrides.
 * CSysExKarmaGE/CSysExGETemplate's own versions are ALSO deliberately
 * NOT reconstructed for a DIFFERENT reason: their real bodies (both
 * byte-identical 374-byte compiled bodies) are fully concrete, no
 * decompiler warning, but perform a real 2-call-per-item virtual
 * dispatch through THIS class's own vtable at raw byte offsets 0x38/
 * 0x1c summing `GetXAtIndex(i) * GetXSize()` over `[param_1,param_2]`
 * -- resolving which named methods occupy those slots needs the full
 * base-class vtable interface reconstructed first (out of scope this
 * round, same class of deferral as OA.ko's `CFileStream::
 * SetPositionBeginning`, round 49).
 *
 * `GetNumObjectsForDigest(int)` deliberately NOT reconstructed for
 * CSysExKarmaGE/CSysExGETemplate/CSysExRegion -- same genuinely
 * unresolvable vtable-slot-0x38 indirect call as round 40's family
 * (Ghidra's own "Could not recover jumptable" warning). CSysExGlobal's
 * OWN version IS a trivial literal (`return 1`) and IS reconstructed.
 *
 * Same shared-vtable / non-virtual-dtor finding as every prior sibling
 * family re-confirmed for all 4 classes here too (no per-class vtable
 * object exists -- ground truth's own dtors all reset to the SAME
 * `PTR__CSysExObjectBase_08f7a908`).
 */

#ifndef SYSEX_OBJECTS_GE_REGION_H
#define SYSEX_OBJECTS_GE_REGION_H

class CSysExGlobal {
public:
	~CSysExGlobal() {}
	unsigned int GetStorageId() const;
	unsigned int GetNumBanks() const;
	unsigned int HasDigests() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
	int GetSysExBankId(int param1) const;
	unsigned int GetNumOfObject(int param1) const;
	unsigned int GetNumObjectsForDigest(int param1) const;
	/* GetObjectPointer's own real return type is void (see header
	 * comment) -- ground truth genuinely discards both CStorage
	 * accessor calls' results. */
	void GetObjectPointer(int param1, int param2) const;
};

class CSysExKarmaGE {
public:
	~CSysExKarmaGE() {}
	unsigned int GetStorageId() const;
	unsigned int GetNumBanks() const;
	unsigned int HasDigests() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
	int GetSysExBankId(int param1) const;
	unsigned int GetNumOfObject(int param1) const;
	int GetObjectPointer(int param1, int param2) const;
};

class CSysExGETemplate {
public:
	~CSysExGETemplate() {}
	unsigned int GetStorageId() const;
	unsigned int GetNumBanks() const;
	unsigned int HasDigests() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
	int GetSysExBankId(int param1) const;
	unsigned int GetNumOfObject(int param1) const;
	int GetObjectPointer(int param1, int param2) const;
};

class CSysExRegion {
public:
	~CSysExRegion() {}
	unsigned int GetStorageId() const;
	unsigned int GetNumBanks() const;
	unsigned int HasDigests() const;
	unsigned int GetVersion() const;
	unsigned int GetObjectSize() const;
	unsigned int GetObjectSizeForExport() const;
	int GetSysExBankId(int param1) const;
	unsigned int GetNumOfObject(int param1) const;
	int GetObjectPointer(int param1, int param2) const;
	int GetTotalSizeForExport(int param1, int param2) const;
};

#endif // SYSEX_OBJECTS_GE_REGION_H
