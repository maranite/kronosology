// SPDX-License-Identifier: GPL-2.0
/*
 * oa_rtparm_family.h  -  the KARMA "RTParm" free-function family:
 * ~99 plain-C/regparm(3) functions (plus 2 small classes,
 * RTParmNameManager/RTParmShortNameGroup) operating on RTParm/RTParmFunction/
 * RTParmEdit records and the GE/PE dispatch tables (see oa_rtparm_ge_table.h/
 * oa_rtparm_pe_table.h -- same overall KARMA-startup domain, natural
 * continuation of that work).
 *
 * Scope: the 56 smallest/most mechanical members (8..402 bytes) fully
 * traced bottom-up in the first pass, PLUS the 7 members that pass
 * deliberately deferred (LimitRTParmEditValues{,Row}, UpdateRTParmIfSame_GE,
 * GetRTParmModAndID, RTParmShortNameGroup::GetRTParmShortNameStringPtr,
 * DoRTParmMultiEnable{PE,GE}), reconstructed in a dedicated follow-up pass
 * (2026-07-29) after a full independent re-derivation of each one's real
 * control flow against fresh `objdump -dr -M intel` disassembly. Every one
 * of the 7 turned out tractable once fully traced -- none needed a real
 * simplification/guess; see each function's own header comment in
 * src/engine/rtparm_family.cpp for the derivation. Two were genuinely
 * simpler than the first pass's own risk assessment suggested once fully
 * unwound: LimitRTParmEditValuesRow collapses to a plain independent
 * `clamp(f4,lo,hi); clamp(f6,lo,hi); clamp(f0,min(f4,f6),max(f4,f6))`
 * (ground truth's own branchy structure is a compiler if-conversion
 * artifact of exactly that, not a genuinely different algorithm), and
 * UpdateRTParmIfSame_GE's "two different re-entry labels" turned out to be
 * two compiler-duplicated copies of the SAME loop-continue code (one
 * reached via the self-skip path, one via the post-match path) -- a plain
 * C `for`/`continue` loop reproduces both faithfully. Three more, however,
 * really are as dense as first assessed and are transcribed close to
 * label-for-label rather than restructured, to avoid a silent behavior
 * change: GetRTParmModAndID (a two-level range search collapsed into two
 * uniform sub-loops after full derivation, but the derivation itself
 * needed care), and DoRTParmMultiEnable{PE,GE} (kept as literal nested
 * loops matching ground truth's own iteration bounds/pointer strides,
 * including two real NEW callees this pass found: `Do_KM_rtp_val_out_pe`
 * (distinct from the already-declared `KM_rtp_val_out_pe`) and
 * `IsRTParmFunctionSameGE`, both declared `extern` below, `pending`).
 *
 * The family's largest members (AssignRTParmFunction_Drm 8294B,
 * RevertRTSceneBuffers* 8095B, DoRTParmMultiLogicPE 4556B,
 * AssignRTParmFunction_Rhy 4115B, IsRTParmFunctionSameGE 3907B,
 * RTParmNameManager::GetRTParmNameString 3643B, AssignRTParmFunction_Rpt
 * 1995B, DoDynRTParms 2095B, CreateRTParmInfoString 1682B,
 * AssignRTParmFunction_Phs 1605B, RTParmNameManager::RTParmNameManager()
 * 1437B, GES_AssignRTParm 1370B, DoRTParmFunction 1357B,
 * LimitRTParmPairGE 1266B, AssignRTParmFunction 1195B,
 * AssignRTParmFunction_Rif 1073B, DoRTParmMultiLogicGE 1042B,
 * AssignRTParmGE 936B, AssignRTParmPE 913B, UpdateRTParmPairIfAssignedGE
 * 889B, AssignRTParmFunction_GM 869B, RTParmUpdateTemplate 845B,
 * RTParmNameManager::Initialize() 767B, SaveVirtualRTParms 704B,
 * RestoreVirtualRTParms 691B, SetRTParmCurValueAtOffset 559B,
 * AssignRTParmFunction_Wav 544B, UpdateRTParmPairIfAssignedPE 497B,
 * AssignRTParmFunction_{Pan,Vel,Clu,Nte,Dur} 479B each, SetRTParmMinMax
 * 478B, ScaleRTParmValue 476B, UpdateRTParmIfSame_PE 748B) were surveyed
 * (top-of-cluster spot-check confirmed via `nm -C -S`, and
 * AssignRTParmFunction_Drm's own body traced far enough to confirm it is
 * a real, in-scope jump-table dispatcher over `GenMod`/`GenEffect`
 * territory, NOT an exotic/unmodeled-library dependency) but not
 * attempted this pass -- all still `pending`, declared `extern` below
 * wherever a reconstructed function calls one of them, matching this
 * project's standing convention for a not-yet-reconstructed sibling (see
 * oa_rtparm_ge_table.h's own 313 RT_* callee declarations).
 * (*) RevertRTSceneBuffers does not literally contain the substring
 * "RTParm" and was NOT part of the awk-filtered 99-function/62318-byte
 * count this pass's survey used -- it is closely related in spirit (same
 * KARMA-scene-buffer domain) but a separate, not-yet-surveyed function.
 *
 * Ground truth: real ELF at `/home/share/docs/ASM Docs/OA.ko/OA.ko`.
 * Manifest/Ghidra-offset citations below use this project's established
 * `.text+0x` convention (raw ELF address + a per-region translation
 * confirmed via `nm -C -S` name+size matching, NOT a single global
 * additive constant -- a same-day gotcha this pass re-confirmed: the
 * `manifest_addr - raw_addr` delta that held for the main
 * 0x52xxxx-0x57xxxx RTParm cluster (0x16130) does NOT hold for
 * CKGParamEdit::GetRTParmBufferSelectId / CKGSysExBuffer::
 * StoreRTParmBySeq / CKGSysExBuffer::SendParamsDependOnRTParm, which live
 * in an entirely different .text region (~0x34xxxx-0x3axxxx) -- always
 * re-derive per-function via `nm -C -S` name+size, never reuse a prior
 * function's delta.
 *
 * Data model: this family operates on GenEffect/GenMod/Performance/
 * RTParm/RTParmFunction/RTParmEdit/EditBuffer/RTParmFunctionTable, NONE of
 * which have an independently reconstructed named layout anywhere in this
 * project yet (only the opaque `GenEffect_pub` stand-in exists, used
 * elsewhere for pure pass-through pointers that are never dereferenced --
 * a different, narrower case). Every confirmed field access below is
 * therefore done via raw `(unsigned char *)ptr + offset` arithmetic with
 * an inline comment citing the confirmed offset, matching this project's
 * dominant convention for not-yet-fully-modeled KARMA types (see
 * CSTGMidiOutPort's own packed-offset fields, oa_engine_init.h) rather
 * than inventing named struct layouts this pass cannot independently
 * verify in full.
 *
 * Confirmed structural facts (from this pass's own disassembly, used
 * across multiple functions below -- NOT independently exhaustive struct
 * layouts):
 *   - GenEffect: a 32-entry, 8-byte-stride RTParm array at +0x8cc
 *     (GetRTParmIDFromGE, GetRTParmAssigned_GE).
 *   - Performance: a 32-entry, 8-byte-stride RTParm array at +0x26a
 *     (GetRTParmIDFromPE), interleaved further out at +0x272/+0x27a/...
 *     +0x2a2 with 8 such 8-entry-wide "pair" groups used by
 *     IsRTParmPairAssignedPE/GetRTParmAssigned_PE (see those functions'
 *     own bodies -- NOT re-derived as a single named array here); also a
 *     32*8-entry RTParmEdit-shaped array at +0x2ea (current) / +0x6ea
 *     (compare), stride 8, indexed `module*32+ge` (GetRTParmEditGE,
 *     CopyRTParmEditTo{OtherBuffer,Module,Master}).
 *   - GenMod: a single "module index" byte at +0x0 (UpdateRTParmIfSame_GE,
 *     found this pass), a GenEffect* at +0xc (used by
 *     UpdateRTParmIfSame_GE) and two 32-entry, 0x28-byte-stride
 *     RTParmFunctionTable-shaped "current"/"compare" buffers at +0x86a4 /
 *     +0x8ba4 (ResetRTParmGELastVal, CopyRTParmFunctionToOtherBuffer) --
 *     RTParmBufferSelect (0/1) selects between them.
 *   - The gKS 0x280c region (below, `module*0x9cc+0x280c`, stride 8,
 *     32 slots/module) re-confirmed this pass (LimitRTParmEditValuesRow,
 *     GetRTParmModAndID, DoRTParmMultiEnableGE) to hold real RTParm-shaped
 *     records: `type` (+0), `subId` (+1), a "mask" byte (+2, already used
 *     by IsRTParmPairAssignedGE's own `d & e[2]` check) -- consistent with,
 *     not a correction of, the existing "RTParmFunctionTable-shaped" note
 *     below (that labels the table's own 0x28-stride *destination*
 *     buffers the mask/type bytes here get copied into, e.g.
 *     SetRTParmMultiBackupGE's `dst[i*0x28+0x20] = src[i*8+2]`).
 *   - gKS (`CSTGGlobal`-adjacent KARMA scene blob, .bss, 0x402a8 bytes):
 *     holds, per GE module slot (stride 0x9d10, 8 modules), a "current"
 *     RTParmFunctionTable-shaped 32-entry/0x28-stride buffer at
 *     module_base+0x1f73c and a "compare" one at module_base+0x1fc3c
 *     (GetRTParmFunctionGE/SetRTParmMultiBackupGE), plus assorted smaller
 *     per-module byte/short tables at module_base+0x1f762 (last-value
 *     cache, ResetRTParmGELastValIndControl) and a fixed (non-module-
 *     indexed) region at 0x280c.. (RTParmFunctionTable-shaped,
 *     SetRTParmMultiBackupGE/IsRTParmPairAssignedGE) and 0x8ce0 (a single
 *     shared "X2100 shared-mem" pointer-array pointer, dereferenced as
 *     `shmemBase[module*4+4]`, X2100ShMem_Update{Master,Module}RTParmEdit
 *     /CopyRTParmEditTo{Module,Master}).
 *   - RTParm: `unsigned char type` (+0), `unsigned char subId` (+1);
 *     `type` selects a GE-domain (0-15) or PE-domain (0-6) descriptor
 *     table, `subId` further selects a 0x20-byte-stride record within it
 *     (GetRTParmDescriptor{,GE,PE}). The leading WORD (+0/+1 read
 *     together) also serves as a direct lookup key into
 *     gRTParmFunctionTable_{GE,PE}'s own `index` field
 *     (GetRTParmFunctionTableEntry_{GE,PE}(kind=1, rtParm)).
 *   - RTParmFunction: a 4-byte "value source" pointer (+0), a 4-byte
 *     handler function pointer (+4, the field AssignRTParmFunction_Xxx/
 *     AdjustRTParmFunction{GE,PE} write/compare), a "value kind" tag byte
 *     (+8, 0..0x27, selects a bitfield-extraction shape -- see
 *     GetRTParmCurValue), plus +0xb/+0xc fields read by
 *     GetScaledRTParmValue (meaning not independently determined this
 *     pass).
 *   - RTParmEdit: 8 bytes, 3 confirmed 16-bit fields at +0/+4/+6
 *     (ByteSwapRTParmEdit byte-swaps exactly these three; +2 is untouched
 *     padding or a non-byte-order-sensitive field).
 *   - RTParmFunctionTable (the shared 0x28-byte-stride record type used
 *     by gRTParmFunctionTable_{GE,PE} AND GenMod's/gKS's per-module
 *     buffers): +0x4 kind tag, +0x10+i*4 a per-sub-index byte offset
 *     table (GetRTParmCurValueFromOffset), +0x20/+0x24 seen written by
 *     several callers, meaning not independently determined this pass.
 *
 * Two small classes reconstructed here (both real, both distinct from
 * RTParmNameManager -- see that class's own not-yet-reconstructed ctor/
 * Initialize/GetRTParmNameString, declared `extern` below):
 *   - RTParmShortNameGroup: ctor + 2 small accessors (SetRTParmShortName-
 *     StringPtr/SetProductArrays); GetRTParmShortNameStringPtr stays
 *     declared-only (see deferral list above).
 *   - RTParmNameManager::SetPrependCCInfo(): the ONE member of that
 *     class reconstructed this pass; the class itself stays otherwise
 *     opaque (ctor/Initialize/GetRTParmNameString all still `pending`,
 *     declared here only so callers can link against them).
 */

#ifndef OA_RTPARM_FAMILY_H
#define OA_RTPARM_FAMILY_H

#include "oa_rtparm_ge_table.h"   /* RTParmBufferSelect */

/* ---- opaque KARMA record types (see header comment: none of these have
 * an independently reconstructed named layout in this project yet) ---- */
struct GenEffect;
struct GenMod;
struct Performance;
struct RTParm;
struct RTParmFunction;
struct RTParmEdit;
struct RTParmFunctionTable;
struct EditBuffer;
struct DynMidi;
/* CSKParameterChangeMessage -- real class (confirmed via
 * `_ZN25CSKParameterChangeMessage8SetValueEi`), own layout out of scope,
 * only the one already-relied-on method declared. */
struct CSKParameterChangeMessage {
	void SetValue(int value) __attribute__((regparm(3)));
};

/* CSysExBuffer -- real base class (confirmed via
 * `_ZN12CSysExBuffer16SendSysExMassageEPh`, called through a
 * CKGSysExBuffer `this` in SendParamsDependOnRTParm's own real body) --
 * own layout entirely out of scope, only the one already-relied-on method
 * declared, matching this project's established minimal-base-class
 * convention (e.g. CCostProfile : public CStartupFile). */
struct CSysExBuffer {
	void SendSysExMassage(unsigned char *msg) __attribute__((regparm(3)));
};

/* CKGSysExBuffer : public CSysExBuffer -- real class, real inheritance
 * (see CSysExBuffer's own comment). Only the 2 members reconstructed this
 * pass are declared; own layout otherwise out of scope. */
struct CKGSysExBuffer : public CSysExBuffer {
	void StoreRTParmBySeq(int seq, int offset) __attribute__((regparm(3)));
	void SendParamsDependOnRTParm(CSKParameterChangeMessage *msg) __attribute__((regparm(3)));
};
/* RTParm_pub -- a SEPARATE opaque type from RTParm (confirmed distinct
 * mangled name, `10RTParm_pub` vs `6RTParm`), same "_pub" opaque-view
 * naming idiom already established elsewhere in this project
 * (`GenEffect_pub`). Used only by `KM_rtp_val_out_pe`'s own real
 * signature -- never dereferenced by any function reconstructed here. */
struct RTParm_pub;

/* kRTParmFunctionTableEntryIndexType -- real mangled type name
 * (`34kRTParmFunctionTableEntryIndexType`) confirmed via
 * GetRTParmFunctionTableEntry_{GE,PE}'s own symbols; only 2 concrete
 * values (0, 1) observed at real call sites, exact enumerator names
 * unconfirmed. */
enum kRTParmFunctionTableEntryIndexType {
	kRTParmFunctionTableEntryIndexType_0 = 0,
	kRTParmFunctionTableEntryIndexType_1 = 1,
};

/* RTParmNameProductID -- real mangled type name (`19RTParmNameProductID`)
 * confirmed via RTParmNameManager::SetPrependCCInfo/
 * RTParmShortNameGroup::SetProductArrays's own symbols; used only as an
 * array index (stride 3 in SetPrependCCInfo, stride 4 in
 * SetProductArrays) -- real enumerator count/names unconfirmed. */
enum RTParmNameProductID {
	kRTParmNameProductID_0 = 0,
};

/* gKS -- the KARMA scene/module-state blob this whole family reads and
 * writes via raw offsets (see header comment). .bss, 0x402a8 bytes
 * confirmed via `nm -C -S`. */
extern unsigned char gKS[0x402a8];

/* The 23 RTParm_menu_{ge,pe}_xxx descriptor tables (.rodata; uniquely
 * `RTParm_menu_ge_wav` is .data (`D`), i.e. writable -- confirmed via nm,
 * not modeled specially here since nothing in this pass's own functions
 * writes to it). Sizes confirmed via `nm -C -S`. */
extern unsigned char RTParm_menu_ge_off[0x20];
extern unsigned char RTParm_menu_ge_ge[0x80];
extern unsigned char RTParm_menu_ge_rif[0x200];
extern unsigned char RTParm_menu_ge_phs[0x520];
extern unsigned char RTParm_menu_ge_rhy[0x1e0];
extern unsigned char RTParm_menu_ge_dur[0x120];
extern unsigned char RTParm_menu_ge_nte[0x160];
extern unsigned char RTParm_menu_ge_clu[0xa0];
extern unsigned char RTParm_menu_ge_vel[0x180];
extern unsigned char RTParm_menu_ge_pan[0x200];
extern unsigned char RTParm_menu_ge_wav[0x540];
extern unsigned char RTParm_menu_ge_env[0x3c0];
extern unsigned char RTParm_menu_ge_rpt[0x360];
extern unsigned char RTParm_menu_ge_bnd[0x260];
extern unsigned char RTParm_menu_ge_drm[0x5c0];
extern unsigned char RTParm_menu_ge_dix[0x280];
extern unsigned char RTParm_menu_pe_off[0x20];
extern unsigned char RTParm_menu_pe_pe[0x20];
extern unsigned char RTParm_menu_pe_mix[0x80];
extern unsigned char RTParm_menu_pe_ctl[0x180];
extern unsigned char RTParm_menu_pe_trg[0x1e0];
extern unsigned char RTParm_menu_pe_key[0x140];
extern unsigned char RTParm_menu_pe_rsd[0x80];

/* ---- not-yet-reconstructed callees this family's reconstructed members
 * call (real, all `pending`) ---- */
extern "C++" {
unsigned long GetFirstOnBit(unsigned long mask, unsigned char width) __attribute__((regparm(3)));
void AssignRTParmGE(unsigned char module, GenEffect *ge, RTParm *rtParm, RTParmFunction *rtf,
                     const RTParmFunctionTable *table, unsigned char a, unsigned char b) __attribute__((regparm(3)));
void AssignRTParmPE(RTParm *rtParm, RTParmFunction *rtf, const RTParmFunctionTable *table,
                     unsigned char a, unsigned char b) __attribute__((regparm(3)));
short ScaleRTParmValue(RTParmEdit *rtd, unsigned char value, unsigned char useCompare) __attribute__((regparm(3)));
void KM_rtp_val_out_pe(RTParm_pub *rtParm, unsigned char a, unsigned char b) __attribute__((regparm(3)));
void Do_KM_rtp_update_name(unsigned char a, unsigned char b) __attribute__((regparm(3)));
void Do_KM_rtp_update_all_names(unsigned char a, unsigned long mask) __attribute__((regparm(3)));
/* Do_KM_rtp_val_out_pe -- a SEPARATE real symbol from KM_rtp_val_out_pe
 * above (confirmed distinct mangled name, `_Z20Do_KM_rtp_val_out_peP6RTParmhh`
 * vs the RTParm_pub-taking one) -- real callee of UpdateRTParmIfSame_GE,
 * found this pass. */
void Do_KM_rtp_val_out_pe(RTParm *rtParm, unsigned char a, unsigned char b) __attribute__((regparm(3)));
/* IsRTParmFunctionSameGE -- real, large (3907B, surveyed-not-attempted)
 * sibling of the already-reconstructed IsRTParmFunctionSamePE; real
 * callee of DoRTParmMultiEnableGE, found this pass. */
bool IsRTParmFunctionSameGE(unsigned char kind, unsigned char idx, unsigned char b, unsigned char c) __attribute__((regparm(3)));
/* CountOnBits -- real, not-yet-reconstructed (`_Z11CountOnBitsmh`); real
 * callee of RTParmShortNameGroup::GetRTParmShortNameStringPtr, found this
 * pass. */
unsigned long CountOnBits(unsigned long mask, unsigned char width) __attribute__((regparm(3)));
}

/* RTParmNameManager -- real class (see header comment). Only
 * SetPrependCCInfo() has a body in this pass; ctor/Initialize/
 * GetRTParmNameString stay declared-only (real, `pending`). */
class RTParmNameManager {
public:
	RTParmNameManager();
	void Initialize();
	void GetRTParmNameString(GenEffect *ge, RTParm *rtParm, char *buf, bool flag);
	void SetPrependCCInfo(RTParmNameProductID product, bool a, bool b) __attribute__((regparm(3)));
};
extern RTParmNameManager *gRTParmNameManagerPtr;

/* RTParmShortNameGroup -- real class, own layout not independently
 * confirmed beyond the offsets its own reconstructed methods touch.
 * GetRTParmShortNameStringPtr deferred (see header comment). */
class RTParmShortNameGroup {
public:
	RTParmShortNameGroup() __attribute__((regparm(3)));
	void SetRTParmShortNameStringPtr(const char (*str)[24], unsigned short a, unsigned short b) __attribute__((regparm(3)));
	void SetProductArrays(RTParmNameProductID product, unsigned short a, unsigned short b) __attribute__((regparm(3)));
	/* Reconstructed this pass. Ground truth's own return register (eax
	 * at the end of the function) holds a raw pointer into the 24-byte-
	 * wide string table this-> +0x0 points to (or 0/NULL) -- NOT an
	 * `unsigned short` as previously (incorrectly) declared here; fixed
	 * as part of implementing the body, since the mangled symbol itself
	 * never encodes a C++ return type and the prior placeholder was
	 * simply wrong. Also adds the missing `regparm(3)` -- ground truth
	 * passes `this`/`product`/`useAlt` in eax/edx/ecx and `a` on the
	 * stack, matching every other method on this class. */
	const char *GetRTParmShortNameStringPtr(RTParmNameProductID product, unsigned char useAlt, unsigned char a) __attribute__((regparm(3)));
	/* Confirmed minimum real extent: the ctor zeroes +0x0..+0x17 (24
	 * bytes: a 4-byte pointer field, then ten 2-byte fields) -- real
	 * total size may be larger (unconfirmed), this is a lower bound, not
	 * an asserted exact `sizeof`. Declared as a plain byte blob (not
	 * named sub-fields) matching this project's established convention
	 * for a not-fully-modeled type; also gives the class a real,
	 * non-empty size so host KAT code can stack-allocate one safely. */
	unsigned char _data[0x18];
};

/* ---- 48 of the first pass's 56 reconstructed members (the other 8 are
 * class methods declared on RTParmShortNameGroup/RTParmNameManager/
 * CKGSysExBuffer above and CKGParamEdit::GetRTParmBufferSelectId in
 * oa_ckg_module_param_msg_handler.h), in ascending ground-truth
 * size order (matches src/engine/rtparm_family.cpp's own layout). The
 * follow-up pass's own 6 free-function members are declared further below,
 * after this block. ---- */
extern "C++" {

void ResetDynRTParmWindow(unsigned char *p) __attribute__((regparm(3)));
unsigned int GetRTParmIDFromGE(GenEffect *ge, RTParm *rtParm) __attribute__((regparm(3)));
unsigned int GetRTParmIDFromPE(Performance *perf, RTParm *rtParm) __attribute__((regparm(3)));
void ResetRTParmGELastVal(GenMod *genMod) __attribute__((regparm(3)));
void ByteSwapRTParm(RTParm *rtParm) __attribute__((regparm(3)));
void CreateRTParmNameString(GenEffect *ge, RTParm *rtParm, char *buf, unsigned char flag) __attribute__((regparm(3)));
unsigned char GetRTParmGroupItems(unsigned char type, unsigned char mode) __attribute__((regparm(3)));
RTParmFunctionTable *GetRTParmFunctionGE(unsigned char module, unsigned char ge, RTParmBufferSelect sel) __attribute__((regparm(3)));
RTParmEdit *GetRTParmEditGE(Performance *perf, RTParmBufferSelect sel, unsigned char module, unsigned char ge) __attribute__((regparm(3)));
void ByteSwapRTParmEdit(RTParmEdit *rtd) __attribute__((regparm(3)));
void AdjustRTParmFunctionGE(RTParmFunction *rtf) __attribute__((regparm(3)));
short GetScaledRTParmValue(RTParmEdit *rtd, RTParmFunction *rtf, unsigned char value) __attribute__((regparm(3)));
RTParmFunctionTable *GetRTParmFunctionTableEntry_PE(kRTParmFunctionTableEntryIndexType kind, void *key) __attribute__((regparm(3)));
void X2100ShMem_UpdateMasterRTParmEdit(Performance *perf, unsigned char module, unsigned char ge) __attribute__((regparm(3)));
void X2100ShMem_UpdateModuleRTParmEdit(Performance *perf, unsigned char module, unsigned char ge) __attribute__((regparm(3)));
RTParmFunctionTable *GetRTParmFunctionTableEntry_GE(kRTParmFunctionTableEntryIndexType kind, void *key) __attribute__((regparm(3)));
RTParm *GetRTParmAssigned_GE(GenEffect *ge, unsigned char a, unsigned char b, unsigned char mask) __attribute__((regparm(3)));
void ResetRTParmPELastVal() __attribute__((regparm(3)));
void AssignRTParmFunction_PE(RTParm *rtParm, RTParmFunction *rtf) __attribute__((regparm(3)));
void CopyRTParmFunctionToOtherBuffer(GenMod *genMod, RTParmBufferSelect sel, unsigned char idx) __attribute__((regparm(3)));
void AdjustRTParmFunctionPE(RTParmFunction *rtf) __attribute__((regparm(3)));
void SetRTParmMultiBackupGE(unsigned char module, RTParmBufferSelect sel) __attribute__((regparm(3)));
void CopyRTParmEditToOtherBuffer(Performance *perf, RTParmBufferSelect sel, unsigned char a, unsigned char b) __attribute__((regparm(3)));
void AssignRTParmFunction_Key(RTParm *rtParm, RTParmFunction *rtf) __attribute__((regparm(3)));
void AssignRTParmFunction_Ctl(RTParm *rtParm, RTParmFunction *rtf) __attribute__((regparm(3)));
void SetRTParmMultiBackupPE() __attribute__((regparm(3)));
void AssignRTParmFunction_Mix(RTParm *rtParm, RTParmFunction *rtf) __attribute__((regparm(3)));
bool IsRTParmTemplateRestoreType(RTParm *rtParm) __attribute__((regparm(3)));
void AssignRTParmFunction_Dix(unsigned char module, GenEffect *ge, RTParm *rtParm, RTParmFunction *rtf) __attribute__((regparm(3)));
void AssignRTParmFunction_Bnd(unsigned char module, GenEffect *ge, RTParm *rtParm, RTParmFunction *rtf) __attribute__((regparm(3)));
void AssignRTParmFunction_GE(unsigned char module, GenEffect *ge, RTParm *rtParm, RTParmFunction *rtf) __attribute__((regparm(3)));
void CopyRTParmEditToModule(Performance *perf, unsigned char a, unsigned char b) __attribute__((regparm(3)));
void ResetRTParmGELastValIndControl(unsigned char module, unsigned char control, RTParmBufferSelect sel) __attribute__((regparm(3)));
void SetRTParmWasEdited(unsigned char a, unsigned char b, bool clearAll) __attribute__((regparm(3)));
void CopyRTParmEditToMaster(Performance *perf, unsigned char a, unsigned char b) __attribute__((regparm(3)));
void AssignRTParmFunction_Env(unsigned char module, GenEffect *ge, RTParm *rtParm, RTParmFunction *rtf) __attribute__((regparm(3)));
unsigned char *GetRTParmDescriptorPE(unsigned char module, unsigned char idx) __attribute__((regparm(3)));
unsigned char *GetRTParmDescriptor(RTParm *rtParm, unsigned char mode) __attribute__((regparm(3)));
void AssignRTParmFunction_Trg(RTParm *rtParm, RTParmFunction *rtf) __attribute__((regparm(3)));
unsigned char IsRTParmPairAssignedGE(unsigned char a, unsigned char b, unsigned char c, unsigned char d) __attribute__((regparm(3)));
void UpdateRTParmName(unsigned char a, unsigned char b, unsigned char c, unsigned char d) __attribute__((regparm(3)));
void AssignRTParmFunction_Rsd(RTParm *rtParm, RTParmFunction *rtf) __attribute__((regparm(3)));
void LimitRTParmPairPE(unsigned char a, RTParm *rtParm, RTParmEdit *rtd) __attribute__((regparm(3)));
int GetRTParmCurValue(RTParmFunction *rtf) __attribute__((regparm(3)));
unsigned char *GetRTParmDescriptorGE(unsigned char module, unsigned char idx) __attribute__((regparm(3)));
bool IsRTParmFunctionSamePE(unsigned char kind, unsigned char a, unsigned char b, unsigned char c) __attribute__((regparm(3)));
int GetRTParmCurValueFromOffset(const RTParmFunctionTable *table, void *base, unsigned char idx) __attribute__((regparm(3)));
unsigned char IsRTParmPairAssignedPE(unsigned char a, unsigned char b, unsigned char c) __attribute__((regparm(3)));
unsigned char *GetRTParmAssigned_PE(Performance *perf, unsigned char a, unsigned char b, unsigned char c) __attribute__((regparm(3)));

/* ---- the 6 (of 7) free-function members deferred from the first pass,
 * reconstructed in the follow-up pass documented above (the 7th,
 * RTParmShortNameGroup::GetRTParmShortNameStringPtr, is declared in-class
 * above) ---- */
unsigned int GetRTParmModAndID(RTParm *rtParm, unsigned char *out) __attribute__((regparm(3)));
bool LimitRTParmEditValues(EditBuffer *eb) __attribute__((regparm(3)));
bool LimitRTParmEditValuesRow(EditBuffer *eb, unsigned char module, unsigned char ge, RTParmBufferSelect sel) __attribute__((regparm(3)));
void UpdateRTParmIfSame_GE(GenMod *genMod, RTParm *rtParm, RTParmEdit *rtd, RTParmBufferSelect sel) __attribute__((regparm(3)));
void DoRTParmMultiEnablePE() __attribute__((regparm(3)));
void DoRTParmMultiEnableGE(unsigned char module, RTParmBufferSelect sel) __attribute__((regparm(3)));

} /* extern "C++" */

#endif /* OA_RTPARM_FAMILY_H */
