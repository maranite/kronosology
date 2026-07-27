/*
 * stg_unsol_msg_handler.h  -  CSTGUnsolMsgHandler, Eva's dispatcher for unsolicited
 * STGMessages arriving from OA.ko (Stage 6 breadth sweep, 2026-07-25).
 *
 * Real, non-zero-caller entry point confirmed by disassembly (not guessed): the one
 * constructor call site is inside `CEditor::CPanelIfcTask::CPanelIfcTask(CEditor
 * const&, PegScreen*)` (.text+0x0824b7e0, real disassembly at +0x924:
 * `call 0891c090 <CSTGUnsolMsgHandlerC1>`), which is itself invoked from
 * `CEditor::Setup()` (.text+0x08249b60, called by `CModuleManager::Setup()`'s
 * already-reconstructed per-module virtual dispatch, src/base/module_manager.cpp) --
 * i.e. this class sits directly on the (nominal) module-Setup boot path, not off in
 * unreached UI territory. `CEditor::CPanelIfcTask` itself is NOT reconstructed here
 * (its own constructor pulls in CTask/COutLinkMono, which belong to a concurrent
 * pass's CModule/CTask/CLevelManagerArray work -- see include/task_buffer.h's own
 * "CTask... genuinely out of scope" note, and this is itself a real, worth-flagging
 * correction to that pass's own "CTask::CTask() has no caller" verdict: it does have
 * one, right here) -- only the two CPanelIfcTask methods CSTGUnsolMsgHandler itself
 * calls (OnAnalogEvent/OnEncoderEvent) are declared below, as real-signature
 * call-contract externs, same convention as every other not-yet-owned class in this
 * project (ckernel.cpp's CTracer/CHostInterfaceBase).
 *
 * Real class layout (confirmed via CSTGUnsolMsgHandler::CSTGUnsolMsgHandler@0891c090.c,
 * cross-checked against CPanelIfcTask's own `malloc(0x98)` call site for the object --
 * 0x98 = 152 bytes, matching exactly):
 *   +0x00        vtable ptr (PTR__CSTGUnsolMsgHandler_08f75688) -- kept a plain raw
 *                void* (mVtbl), NOT a real C++ vtable, same "manual vtable swap, no
 *                `virtual` keyword" convention already established for COmegaPtrArray/
 *                CModule/CScheduler (see omega_ptr_array.h's own header comment) --
 *                a real C++ virtual method here would make the compiler synthesize
 *                its own vtable and fight the ctor's own manual `mVtbl = &PTR__...`
 *                assignment.
 *   +0x04        CEditor::CPanelIfcTask *mOwner  (ctor's own param_1)
 *   +0x08..+0x8f a real 17-entry unsolicited-message dispatch table, one entry per
 *                STGMessage subtype 0..16 (see HandleMessage() below), 8 bytes/entry:
 *                  +0  code* pfn   -- Itanium pointer-to-member-function "ptr" word
 *                  +4  int   adj   -- Itanium "adj" word (always 0 here -- every
 *                                     entry is a plain non-virtual function address,
 *                                     never the odd/vtable-offset encoding; HandleMessage()
 *                                     still contains the generic both-cases dispatch
 *                                     code a real ptr-to-member-function call compiles
 *                                     to, faithfully preserved even though the
 *                                     vtable-offset branch is dead given this ctor's
 *                                     own data)
 *   +0x90        int  mSentinel = 0xffffffff (purpose not traced -- never read by any
 *                method reconstructed here)
 *   +0x94        uint8_t mFlagsA = 0  (not read by any method reconstructed here)
 *   +0x95        uint8_t mForceSaveOnEnd = 0 (EndHandling()'s own end-of-song-edit
 *                save-and-shutdown gate -- see below)
 *
 * Message-subtype -> handler mapping (from the ctor's own literal table, index =
 * STGMessage's own offset+4 field per HandleMessage()'s dispatch -- a real, newly
 * confirmed fact about STGMessage's layout, not previously documented in
 * ustg_user_api.h's own opaque-STGMessage note):
 *   0 ControlMsgHandler        6  VoiceModelMsgHandler    12 CalibrationMsgHandler
 *   1 GlobalMsgHandler         7  EffectMgrMsgHandler     13 FrontPanelMsgHandler
 *   2 CombiMsgHandler          8  EffectSlotMsgHandler    14 HDRTrackMsgHandler
 *   3 ProgramSlotMsgHandler    9  EffectMsgHandler        15 KLMMsgHandler
 *   4 ProgramMsgHandler        10 TestControlMsgHandler   16 SetListMsgHandler
 *   5 PatchMsgHandler          11 ASKMsgHandler
 *
 * Real, worth-flagging quirk: the 5 slots typed `__cdecl(STGMessage*)` in
 * functions.csv (TestControlMsgHandler/ASKMsgHandler/CalibrationMsgHandler/
 * FrontPanelMsgHandler/KLMMsgHandler -- confirmed *static* member functions, no
 * implicit `this`) are still stored in, and called through, the exact same
 * two-argument (owner-adjusted-this, message) dispatch as the fifteen real
 * *instance* handlers. Under cdecl's right-to-left push order the callee's one
 * expected stack slot actually lands on the owner-pointer, not the message pointer --
 * real, harmless only because all five bodies are unconditional `return;` (see
 * below), not something this pass invented or "fixed".
 *
 * Tier A (faithful, transcribed below): ctor, both real destructor-shaped functions
 * (kept as plainly-named methods, not real C++ destructors -- see mVtbl note above),
 * HandleMessage() (the real dispatcher), EndHandling(), SendValueSlider(),
 * SendValueEncoder(), EnterGlobalObjectEdit(int), and 8 of the 17 table slots that
 * are genuinely, confirmed-by-reading-every-one empty `return;` bodies in the real
 * binary already (the 5 static ones above, each literally 1 byte in the real .text,
 * plus Initialize(CCombi*, CCombi*)/InitializeForSong(CCombi*, CCombi*)/
 * BeginHandling(), also 1-byte no-ops, also confirmed static/cdecl). These are real
 * facts about the shipped binary, not something this pass simplified.
 *
 * Tier A, batch 2 (2026-07-25 -- promoted from Tier B below): PatchMsgHandler (340B),
 * EffectMgrMsgHandler (541B), EffectMsgHandler (660B), HDRTrackMsgHandler (488B),
 * SetListMsgHandler (549B). All five share one real, repeated shape confirmed across
 * every one of them by direct disassembly: (1) an optional CStorage-current-selection
 * guard (skippable via the msg's own 0xffff/0xfffe "wildcard target" codes -- same
 * convention CSTGUnsolMsgHandler already establishes elsewhere), (2) a
 * `EditApi_vtbl+0x28` scope-id lookup ("ESProg"/"ESCombi"/"ESSong"/"ESSetList"/
 * "ESEffect"), (3) a small compile-time byte lookup table (a real local `static
 * const` table inside a *different*, not-reconstructed free function --
 * HandleHDRMsg/HandleEffectLFOParam/etc -- read directly out of the real binary's
 * .rodata via `readelf -l` VA->file-offset + raw byte read, not guessed, same
 * technique the ctor's own float constants used in batch 1) mapping the message's own
 * subtype/sub-index field to a (code, value) byte pair, and (4) the same
 * `EditApi_vtbl+0x30` "set param" dispatch already established by EndHandling()'s own
 * dead branch, now a real, unconditionally-exercised call. See
 * src/ipc/stg_unsol_msg_handler.cpp's own header comment for the raw table bytes and
 * the `EditApiGetScopeId`/`EditApiSendParamMsg` shared helpers factoring this repeated
 * shape. A REAL BUG was found and fixed alongside this: EditApi's own vtable
 * (`PTR__CEditApiInstance_08e85da8`, mains.cpp) was sized 6 slots, enough for
 * EndHandling()'s dead-branch-only +0x28/+0x2c reads but not for these five
 * handlers' unconditional +0x28/+0x30 dispatch (+0x30/4 = slot 12) -- bumped to 20,
 * see mains.cpp's own WORKAROUND #2 comment.
 *
 * Tier B (real signature, mostly not implemented): ControlMsgHandler (VoiceModelMsgHandler,
 * described alongside it below for context, was promoted to Tier A batch 8 -- see
 * stg_unsol_msg_handler.cpp's own header comment for the full case-by-case reconstruction).
 * ControlMsgHandler itself was RE-EXAMINED again 2026-07-27 (see its own header comment in
 * stg_unsol_msg_handler.cpp for the full finding) and turned out to have 6 of its 44 outer
 * subcodes (9, 10, 11, 16, 37, 38) genuinely tractable in isolation -- now real. The other
 * 38 stay Tier B; see below for why the majority is NOT separable the way
 * VoiceModelMsgHandler's cases were.
 * Both were RE-TRACED FROM SCRATCH (2026-07-26, `objdump -dr -M intel` against the real
 * ground-truth `Eva` binary, not just re-reading prior notes) specifically to check for
 * the "size is not depth" misdiagnosis pattern this same session caught 8 other times
 * elsewhere in the project (GCC auto-vectorized loops, IPA clones, inlined duplicate
 * dispatch, etc.). Real sizes corrected along the way -- both were previously undercounted
 * by reading Ghidra's own function-size label instead of the real next-symbol gap, same
 * class of error this file's own EffectSlotMsgHandler header comment already flagged once:
 *   - ControlMsgHandler: real .text 0x0891ac70..0x0891c090 = 5152 bytes (was documented as
 *     4886B).
 *   - VoiceModelMsgHandler: real .text 0x08917100..0x08917ad0 = 2512 bytes (was documented
 *     as 2487B).
 *
 * ControlMsgHandler -- CONFIRMED genuinely deep, not a misdiagnosis. A full call-target
 * survey of the real disassembly (`objdump | grep call | sort | uniq -c`) shows 18 distinct
 * out-of-scope subsystems, not a repeated mechanical shape: CMMI::GetInstance() (18x),
 * CEditor::IsSwitchPressed() (14x), CControlSurface::GetInstance()/MoveKnobFader()/
 * PressPlayMuteSwitch()/PressControlSwitch()/PressSelectSwitch()/ResetPerfSwitch(),
 * CHelpManager::ShowHelpPage() (3 overloads, 12x total), CZ::CZ() + CFileOperation::
 * NotifyOnKSCFileChanges() (the same out-of-scope container as CBatchDiskMan/CEditClient
 * elsewhere in this project), CDiskUtil::BeginDiskWork()/EndDiskWork()/
 * SetSwitchStateFlags(), CModeManager::ChangePage(), CSmplModeMgr::aftersampleedit(),
 * CMMI::OpenDialog(CPegForm*,...)/OpenProcessBoxWithoutBar() and
 * CFormDlogGlobalAutoPowerOffWarning::Open()/Close() (real Peg-toolkit CForm dialogs --
 * permanently out of scope per this project's own UI-fidelity boundary), CFormPianoP4Main::
 * SetupPianoType(), CFormCancelOK ctor, CFormGlobal6::ReloadFromSTG(), CSmplMemManager::
 * updateramsize(), CMSDSInfo::Refresh(), CSTGUtil::Dump(), raw HAL_DisableInterrupts()/
 * HAL_EnableInterrupts() critical-section pairs (4x each, real hardware interrupt-mask
 * manipulation, not the kernel-module RTAI shim this project already treats as a dropped
 * no-op elsewhere -- e.g. HAL_DisableInterrupts @0891aef8, HAL_EnableInterrupts @0891af0b,
 * CMMI::OpenDialog @0891af64, CFormDlogGlobalAutoPowerOffWarning::Open @0891b0f0,
 * CHelpManager::ShowHelpPage(EAnalogDeviceCode) @0891b295). This is real front-panel
 * button/switch semantic dispatch spanning nearly every out-of-scope UI/control-surface
 * subsystem this project has already independently flagged elsewhere as permanently
 * deferred (Peg toolkit, CZ, CStorage, CModeManager) -- not tractable in isolation, and
 * not a productive target even for a dedicated follow-up batch.
 *
 * UPDATE (2026-07-27): re-applied this project's own "size/depth verdict can be right for
 * the whole while a sub-piece is still tractable" lens (already validated on CJobStack's
 * and CEditClient's ctor/dtor) to ControlMsgHandler specifically. Its outer dispatch is a
 * real 44-entry jump table (same shape as VoiceModelMsgHandler's own JT1), but a full
 * jump/call-target-vs-owning-case-range audit of the real disassembly found GCC has
 * extensively CROSS-JUMPED/tail-merged this particular switch: one ~2KB physical region
 * (0x0891b890..0891c090) is entered directly by ~20 of the 44 subcodes (including both of
 * subcode 6's and 7's own nested sub-jump-tables) and jumps back out into several other
 * subcodes' own code ranges -- a single interconnected CFG hub carrying essentially all 18
 * out-of-scope calls above, not 44 independent leaves. This makes the existing verdict
 * MORE precise, not weaker: most of the 44 subcodes cannot be reconstructed one at a time
 * without reconstructing the shared hub, which is exactly the permanently-deferred material.
 * Outside that hub, exactly 6 subcodes are provably self-contained (zero crossjump either
 * direction) and depend only on already-real targets -- promoted to Tier A. See
 * stg_unsol_msg_handler.cpp's own ControlMsgHandler header comment for the full per-subcode
 * evidence (real .rodata button-code tables, the shared OnButtonEvent tail block, and the
 * one real out-of-bounds-table-read hazard found and deliberately not reproduced).
 *
 * VoiceModelMsgHandler -- PROMOTED TO TIER A (batch 8, 2026-07-27, full follow-up pass on
 * the scaffolding below -- both real jump tables fully case-traced, all real .rodata
 * tables read directly, see stg_unsol_msg_handler.cpp's own header comment for the
 * complete case-by-case reconstruction and exact per-case evidence). RE-CHARACTERIZED
 * (2026-07-26, before that follow-up pass) as genuinely more mechanical than previously
 * documented. A full call-target survey shows its ENTIRE
 * external dependency surface is just 3 already-modeled call targets: `memcpy` (1x),
 * `SetWithoutUpdatingSTG()` (7x, already stubbed in this file, see below), and
 * `CStorage::GetInstance()` (1x, already partially modeled in this file via
 * `CStorageSingleton`/`CPrograms`). Every other real subsystem call (`Api`+0x94 assert,
 * `EditApi`+0x28 scope lookup) is dispatched through vtable globals this file already owns
 * (`Api` @0x930a1f4, `EditApi` @0x930aae4 -- see EndHandling()/EditApiGetScopeId() above).
 * Its overall guard shape is CONFIRMED IDENTICAL to already-reconstructed PatchMsgHandler's
 * own: `(*(uint*)(p+0xc) == CStorage::sm_ucCurrentProg && target == DAT_0af30549) ||
 * target == 0xfffe || target == 0xffff` (real addresses 0xaf30548/0xaf30549, both already
 * declared globals in this file), THEN gated on `(DAT_0af0df1e & 7)` -- PatchMsgHandler
 * bails outright unless this equals 3; VoiceModelMsgHandler instead branches into two
 * different bodies depending on it (==3 selects a deep path, else a 17-way mechanical
 * jump-table dispatch), a real structural variant built from the exact same already-known
 * primitives, not a new dependency.
 *   - The MECHANICAL majority (all of JT1's 17 entries at real .rodata 0x08f1bac0, indexed
 *     by subindex+1 for subindex in {0xffff}u[0,0xf], plus JT2's 6 entries at 0x08f1bb04
 *     for the `(DAT_0af0df1e&7)==3` path's own subindex in [0,5]) is the same
 *     EditApiGetScopeId()-then-byte-table-then-`SetWithoutUpdatingSTG()` shape as this
 *     file's other Tier-A handlers, using only 3 real scope-name strings observed at their
 *     real addresses: "ESProg" (0x8e79800), "ESSampling" (0x8e7987c), and one NEW name not
 *     seen elsewhere in this project, "ESMOSS" (0x8e79862) -- confirms the deep leaf below
 *     is a real, separate edit-server scope, not a guess.
 *   - The ONE genuine deep leaf: reached only when `(DAT_0af0df1e&7)==3` AND a per-slot
 *     "type" byte (read from a static array at 0xaf0e049, indexed by the message's own raw
 *     `value` field * 0x41c -- a real, not-yet-modeled algorithm-descriptor array) is in
 *     [2,9] AND the subindex sub-field is >5. That single path (real call at .text
 *     0x08917209, `call CStorage::GetInstance()` then `[eax+0x48]`-vtable dispatch through
 *     a bounds-checked array of further-vtabled objects, `[obj+0x18]`/`[obj+0x1c]`, plus a
 *     `memcpy`) is confirmed via a real nearby file-path string
 *     (`../../../../../OPOS/Projects/x2100/Modules/Storage/MOSSAlgorithm...`, real address
 *     0x8f25dc4, used as the 2nd `Api`+0x94 assert's own file argument) to be a genuine
 *     "MOSS algorithm" voice-model database dispatch -- a real, currently entirely
 *     unmodeled class hierarchy (matches the header's original "CSTGMultisampleBankUUIDBase"
 *     naming), and IS out of scope for this pass, same as `CToneAdjustTool::
 *     ConvertParamToLinear` elsewhere in this file (a stub returning a fixed sentinel would
 *     be the natural way to make this branch safely bail rather than model the real
 *     algorithm database).
 * Scaffolding above (jump tables + guard shape + external dependency inventory, all
 * addresses real, not guessed) for what was then a dedicated follow-up batch. That
 * follow-up (batch 8, 2026-07-27) is now DONE: all 2 stacked jump tables' ~16 distinct
 * case bodies were traced byte-exact, including the several genuinely bespoke
 * non-table-driven code/value computations flagged above -- see
 * stg_unsol_msg_handler.cpp's own header comment for the complete reconstruction. Only
 * the one MOSS-algorithm leaf remains unimplemented, as a precisely-scoped Tier-B stub.
 *
 * EffectSlotMsgHandler (real 1856B, .text 0x08917cd0..0x08918410 -- Ghidra's own
 * "size=1796" label undercounts by ~60 bytes of trailing out-of-line branch targets,
 * confirmed by disassembling straight through to the next function's own entry with
 * no gap) -- promoted to Tier A (2026-07-25, follow-up batch). Previously deferred
 * for a different reason than the six above: same EditApi/CStorage/local-byte-table
 * shape as the five already promoted, but a 15-way switch on the message's own +0x18
 * "sub-index" nested inside the outer scope/kind resolution, plus a reused stack
 * buffer (Ghidra's `local_2c`) whose several partial-width writes (`_0_1_`/
 * `_1_3_<<8`/CONCAT31 idioms) originally read as a possible genuine uninitialized-
 * stack-garbage hazard. **That concern is RESOLVED, not routed around**: byte-by-byte
 * disassembly of every one of local_2c's write sites (not just the decompile) shows
 * each one writes only byte 0 to a fully-determined value (0, 1, or a message-field
 * byte), and every one of those call sites ALSO passes `len=1` to EditApi's own
 * +0x30 "set param" call -- the real callee never reads local_2c's other 3 bytes at
 * any of those sites, so Ghidra's "upper 3 bytes carried forward from before" framing
 * is just how its SSA view expresses "byte 0 written, rest untouched", not evidence
 * of a live hazard. Reconstructed below as a plain byte scalar per message, not a
 * 4-byte buffer -- no behavioral loss versus the real binary. The one case (idx==3)
 * that copies a real 4-byte message field into the buffer is a normal, fully-defined
 * int copy with no garbage involved. Full switch/jump-table structure (15 entries at
 * real .rodata 0x08f1bb1c) cross-checked instruction-by-instruction against the
 * decompile, including confirming case 9 and case 10/11/12 are genuinely different
 * jump targets (0x08918080 vs 0x08918070) despite superficially similar decompiled
 * bodies -- an initial raw table read of this pass's own miscounted the file offset
 * and had to be corrected by an independent `objdump -s` cross-check.
 *
 * All three of idx==2 (default-group special case), idx==0xb, and idx==3 use the
 * exact same `EditApiSendParamMsg` shape already established (flag always 1,
 * lastEditMessage always 0x500c) -- reuse the shared helper. Only the true "default"
 * tail (every other idx, via a `CSWTCH_231`-int[9]-table flag lookup keyed on the
 * message's own +2 `eSTGMidiSource` field, real bytes `{4,1,2,1,1,3,1,4,2}` at
 * 0x08f1c460) diverges from the shared helper's shape: its `flag` argument is
 * variable (not always 1) and `CEditor::lastEditMessage` is `(flag==3) + 0x500c`,
 * not unconditionally `0x500c` -- written inline in this one method rather than
 * bent into the shared helper, same "don't generalize a helper for one outlier"
 * precedent EndHandling() already set.
 *
 * GlobalMsgHandler (real 2012B, .text 0x08918b50..0x08919360) -- promoted to Tier A
 * (2026-07-26). Its own real subtype switch (msg's own +8 field, 0..4) covers
 * global-param / drumkit-param / wave-sequence-param / two flag-diff cases, each
 * hand-verified against `objdump -dr -M intel` (not just the decompile, which turned
 * out accurate everywhere EXCEPT its own `CSWTCH_231[code+0x21]` table-index framing
 * -- Ghidra invented its own array base; the real one, direct-indexed, is
 * 0x08f1c481, see stg_unsol_msg_handler.cpp's own header comment). Its one
 * out-of-scope dependency is `SetWithoutUpdatingSTG()`, a real internal-linkage
 * (`static`) free function GCC IPA-cloned into a 4-argument runtime shape (scope/
 * code/value/payload in EAX/EDX/ECX/stack at both real call sites, confirmed by
 * register tracing -- its full mangled signature has a 5th `EEditSource` parameter
 * this clone never materializes) -- stubbed, return value unused at both call sites
 * (each the last statement in its case block, immediately before an early return).
 *
 * Two genuinely asymmetric restore-guard shapes exist here, NOT collapsed into
 * EditApiSendParamMsg()'s uniform "live check right before the call" shape (which
 * every OTHER branch below genuinely does have and does reuse):
 *   - case 0's own subtype-0x26 sub-case: the begin-restore guard before the shared
 *     final `setParam` call uses a snapshotted `iVar8` (0 by default; if
 *     s_eNowRestoreSeqParameters was set, re-read fresh immediately after that
 *     sub-case's own inner setParam+endRestore cycle, not at the point of the final
 *     check itself) -- the matching END-restore guard after the final call, by
 *     contrast, is a fresh live re-read. Confirmed at the real 0x08918ee7 vs
 *     0x08918d7f instructions.
 *   - case 2's own subtype-0x14-field==0x20 sub-case: a real `goto LAB_089192c8`
 *     (real address 0x089192c8) from the "non-negative" branch back into the top of
 *     the "negative" branch's own body, AFTER that sub-case's own inner
 *     setParam+endRestore cycle -- so the re-entered code's own begin-restore check
 *     can fire a SECOND, independent beginRestore() even though one already ran
 *     earlier in the same invocation. Transcribed with a real C++ goto/label pair,
 *     same precedent as edit_server.cpp's own goto preservation.
 * Both are dead given s_eNowRestoreSeqParameters's own always-0 status throughout
 * this reconstruction, but preserved exactly rather than hand-waved, per this
 * project's own "faithful even when currently dead" convention.
 *
 * Reference-vs-pointer parameter shape for every method below is taken from
 * symbols.csv's own demangled names, not functions.csv's ABI-level (pointer-only)
 * view -- e.g. real mangling is `CSTGUnsolMsgHandler::ControlMsgHandler(STGMessage
 * const&)`, so that one alone takes `const STGMessage&`; every other STGMessage-
 * taking method (including the 5 static no-ops) takes a plain non-const
 * `STGMessage&`.
 */

#ifndef STG_UNSOL_MSG_HANDLER_H
#define STG_UNSOL_MSG_HANDLER_H

#include "ustg_user_api.h"   /* struct STGMessage (opaque) */
#include "panel_ifc_task.h"  /* CEditor::CPanelIfcTask (forward-usable pointer type) */

#include <stdint.h>

/* Forward-only -- not reconstructed here, see CTask/COutLinkMono note above. Only
 * ever touched as an opaque pointer by CSTGUnsolMsgHandler's own Initialize()/
 * InitializeForSong() (both real, confirmed-empty no-ops, see above).
 */
class CCombi;

/* CSTGUnsolMsgHandler's own SendValueSlider()/SendValueEncoder()/HandleMessage()/
 * EndHandling() forward analog-slider and rotary-encoder values into two
 * CPanelIfcTask methods this pass does not otherwise reconstruct. Declared here as
 * real-signature call-contract externs (not implemented) -- see this header's own
 * top comment. SAnalogEvt/SEncoderEvt sizes are confirmed exactly from the stack
 * layout each real caller builds (see .cpp), not guessed.
 */
/* Real namespace confirmed straight from `nm -C` (not guessed): both events are
 * `CPanelOut::SAnalogEvt`/`CPanelOut::SEncoderEvt`, NOT nested under CEditor at all
 * -- `CPanelOut` itself is a whole other not-reconstructed class (front-panel LED/
 * analog output), only its two nested event-payload structs are needed here.
 */
namespace CPanelOut {

struct SAnalogEvt {
	int32_t type;   /* always 0x19 (25) at every real call site in this class */
	int16_t value;  /* 0..1023 (see HandleMessage()'s own float scale, .cpp) */
};

/* Real size is 8 bytes (two stack dwords) at every call site, but only byte 0 is
 * ever assigned a real value -- bytes 1..3 are the original binary's own
 * uninitialized-stack-garbage read (EndHandling()'s own local_18._1_3_), and the
 * trailing dword is always zeroed. Preserved as found: this reconstruction zero-
 * initializes the padding (deterministic, unlike the original) rather than
 * reproducing genuine UB, since nothing downstream is confirmed to read it anyway.
 */
struct SEncoderEvt {
	uint8_t value;
	uint8_t reserved[3];
	int32_t zero;
};

} /* namespace CPanelOut */

/* CEditor::CPanelIfcTask::OnAnalogEvent(CPanelOut::SAnalogEvt const*) / OnEncoderEvent
 * (.text+0x0824be00 / 0x0824bdb0, 403/79 bytes) -- real signatures confirmed via
 * `nm -C`. Declared as genuine member-function additions to panel_ifc_task.h's
 * existing CPanelIfcTask class (Tier B, not implemented) rather than as free
 * functions bound via an asm() label: a real non-static C++ member declared on the
 * class gets the correct implicit-`this`-as-first-argument call shape for free
 * (matching what Ghidra calls "__thiscall" for this SysV/Itanium binary) with zero
 * extra machinery, whereas a free function would need careful hand-verified calling-
 * convention matching to avoid a real ABI mismatch.
 */

class CSTGUnsolMsgHandler {
public:
	/* .text+0x0891c090, 297 bytes. Builds the 17-entry dispatch table described
	 * above; see .cpp for the full literal table transcription.
	 */
	CSTGUnsolMsgHandler(CEditor::CPanelIfcTask *owner);

	/* .text+0x089b9e30, 11 bytes. Real "complete-object destructor" shape: just
	 * resets the vtable pointer. Named plainly (not `~CSTGUnsolMsgHandler()`)
	 * per this file's own mVtbl convention note above.
	 */
	void ResetVTable();

	/* .text+0x089b9e40, 39 bytes. Real "deleting destructor" shape: resets the
	 * vtable pointer (same as above) then frees `this`. Real disassembly
	 * brackets the free() in HAL_Disable/EnableInterrupts (dropped here, same
	 * documented no-op-in-userspace convention as ckernel.cpp).
	 */
	void DeletingDtor();

	/* .text+0x089162e0, 168 bytes. The real dispatcher: reads STGMessage's own
	 * offset+4 field as a 0..16 subtype index, calls the matching table-slot
	 * handler via the real Itanium ptr-to-member-function encoding (see header
	 * comment), then -- if a pending slider value was consumed by that handler
	 * (sNowValueSlider transitions nonzero->zero) -- forwards the scaled value
	 * to CPanelIfcTask::OnAnalogEvent(). STGMessage stays opaque; only the
	 * offset+4 subtype-index fact is asserted here, consistent with
	 * ustg_user_api.h's existing "confirm one field, don't retype the whole
	 * struct" convention.
	 */
	void HandleMessage(STGMessage &msg);

	/* .text+0x0891c290, 283 bytes. Flushes any still-pending slider/encoder
	 * value, then -- if mForceSaveOnEnd is set -- queries EditApi (real vtable
	 * slots +0x28/+0x2c, both undecoded -- see .cpp) for an "ESSong" scope flag
	 * and, if that flag comes back false, saves the random seed, syncs, sleeps
	 * 3 seconds, and force-shuts-down. Real, faithfully transcribed; dead code
	 * given this pass's own data (mForceSaveOnEnd is never set to nonzero by
	 * anything reconstructed here -- EnterGlobalObjectEdit() only ever touches
	 * the *global* s_bIsInGlobalObjectEdit, a different flag).
	 */
	void EndHandling();

	/* .text+0x0891c1f0/0x0891c240, 71/66 bytes. Unconditionally (Slider) /
	 * conditionally on sEncoderValue!=0 (Encoder) forward the pending value to
	 * CPanelIfcTask, same scale-and-clear shape as HandleMessage()'s own tail.
	 */
	void SendValueSlider();
	void SendValueEncoder();

	/* .text+0x0891c3c0, 10 bytes. `s_bIsInGlobalObjectEdit = enable;` -- real,
	 * trivial.
	 */
	void EnterGlobalObjectEdit(int enable);

	/* Real, confirmed-empty in the shipped binary (each 1 byte, `return;` with
	 * no body), and confirmed *static* (cdecl, no implicit `this` in
	 * functions.csv) -- not simplified by this pass, see header comment.
	 */
	static void Initialize(CCombi *a, CCombi *b);
	static void InitializeForSong(CCombi *a, CCombi *b);
	static void BeginHandling();
	static void TestControlMsgHandler(STGMessage &msg);
	static void ASKMsgHandler(STGMessage &msg);
	static void CalibrationMsgHandler(STGMessage &msg);
	static void FrontPanelMsgHandler(STGMessage &msg);
	static void KLMMsgHandler(STGMessage &msg);

	/* Tier B -- real signature only, genuinely deep per-subsystem processing,
	 * see header comment for size/call-target survey. Not implemented.
	 */
	void ControlMsgHandler(const STGMessage &msg);

	/* Tier A, batch 8 (2026-07-27) -- real body, see header/.cpp for the full
	 * from-scratch re-trace (promoted from Tier B; two real jump tables fully
	 * decoded, ~90% mechanical EditApi/SetWithoutUpdatingSTG dispatch, with
	 * exactly ONE genuine deep leaf -- a CStorage::GetInstance()-based "MOSS
	 * algorithm" voice-model-database dispatch -- left as a precisely-scoped
	 * Tier-B stub, VoiceModelMossAlgorithmDispatch() in the .cpp).
	 */
	void VoiceModelMsgHandler(STGMessage &msg);

	/* Tier A, batch 2 (2026-07-25) -- real bodies, see header comment. */
	void PatchMsgHandler(STGMessage &msg);
	void EffectMgrMsgHandler(STGMessage &msg);
	void EffectMsgHandler(STGMessage &msg);
	void HDRTrackMsgHandler(STGMessage &msg);
	void SetListMsgHandler(STGMessage &msg);
	/* Tier A, batch 3 (2026-07-25) -- real body, see header comment (formerly
	 * deferred for goto/switch complexity + a buffer-reuse concern now resolved).
	 */
	void EffectSlotMsgHandler(STGMessage &msg);
	/* Tier A, batch 4 (2026-07-26) -- real body, see header comment (formerly
	 * deferred for a `SetWithoutUpdatingSTG()` dependency, now a safely-stubbed
	 * out-of-scope leaf, plus two genuinely asymmetric restore-guard shapes now
	 * resolved by direct register tracing rather than hand-waved).
	 */
	void GlobalMsgHandler(const STGMessage &msg);
	/* Tier A, batch 5 (2026-07-26) -- real body, see header comment (promoted from
	 * Tier B; new real dependencies CMMI/CModeManager/CKGMsgProcessor, all stubbed
	 * file-local in the .cpp, same convention as SetWithoutUpdatingSTG()).
	 */
	void ProgramSlotMsgHandler(STGMessage &msg);
	/* Tier A, batch 6 (2026-07-26) -- real body, see header comment (promoted from
	 * Tier B; new real dependencies HandleProgToneAdjustParam/CMMI/CModeManager,
	 * all stubbed file-local, same convention as SetWithoutUpdatingSTG()).
	 */
	void ProgramMsgHandler(STGMessage &msg);
	/* Tier A, batch 7 (2026-07-26) -- real body, see stg_unsol_msg_handler.cpp's own
	 * header comment (promoted from Tier B; real 7-way jump table whose physical
	 * .text layout groups guard/table code BY SCOPE STRING across cases rather than
	 * by case number -- hand-verified via `objdump -dr -M intel` case-by-case,
	 * tracing forward from each case's own real `GetScopeId` call site rather than
	 * trusting physical address proximity. New real dependencies CStorage::
	 * GetInstance()/CPrograms::GetProgramPointer/CPrograms::IsCopyableBank/
	 * CToneAdjustTool::ConvertParamToLinear, all stubbed file-local --
	 * ConvertParamToLinear's return VALUE is genuinely consumed by one branch, not
	 * just gated; stubbed to a fixed sentinel, same convention as
	 * SetWithoutUpdatingSTG()/IsOnTimbreProgramEditInContext()'s hardcoded false).
	 */
	void CombiMsgHandler(STGMessage &msg);

	/* Real layout is {code* fn; int32 adj} per HandleMessage()'s own decompile --
	 * kept public/raw (not a real C++ pointer-to-member) so the ctor and
	 * HandleMessage() can both build/walk it with plain pointer arithmetic,
	 * matching the original's own explicit encoding checks instruction-for-
	 * instruction rather than trusting the host compiler to reproduce the
	 * identical dispatch a real ptr-to-member-function call would generate.
	 */
	struct Slot {
		void *pfn;
		int32_t adj;
	};

	/* Test-only seam (verify/test_stg_unsol_msg_handler.cpp) -- peeks the private
	 * dispatch table/owner-pointer the real class has no public accessor for.
	 * Same convention as ustg_user_api.h's own UstgUserApiTestHooks friend.
	 */
	friend struct StgUnsolMsgHandlerTestHooks;

private:
	void *mVtbl;
	CEditor::CPanelIfcTask *mOwner;
	Slot mTable[17];
	int32_t mSentinel;
	uint8_t mFlagsA;
	uint8_t mForceSaveOnEnd;

	static CSTGUnsolMsgHandler *sInstance;

	/* Shared EditApi vtable-dispatch helpers used by the five Tier A batch-2
	 * handlers (PatchMsgHandler/EffectMgrMsgHandler/EffectMsgHandler/
	 * HDRTrackMsgHandler/SetListMsgHandler) -- private static (touch no
	 * instance state, but need friend access into USTGUserAPI::
	 * mNowStopMessaging, hence members of this class rather than free
	 * functions). See src/ipc/stg_unsol_msg_handler.cpp's own header comment.
	 */
	static unsigned char EditApiGetScopeId(const char *name);
	static void EditApiSendParamMsg(unsigned char scope, unsigned char code, unsigned char value,
	                                 void *payload, int len, int flag);

	/* VoiceModelMsgHandler's own two dispatch entry points (Tier A batch 8,
	 * 2026-07-27) -- private static (need EditApiGetScopeId's access, touch no
	 * instance state) so the ~20 pure-math per-case helpers in the .cpp can stay
	 * plain free functions, same split this file already uses for
	 * EditApiGetScopeId/EditApiSendParamMsg vs. CombiMsgHandlerGuardPass(). See
	 * stg_unsol_msg_handler.cpp's own header comment for the real control flow
	 * these two implement.
	 */
	static void VoiceModelMainDispatch(unsigned char *p);
	static void VoiceModelWildcardDispatch(unsigned char *p);

	/* VoiceModelMsgHandler's subindex==13 case body (0x08917592) -- the one JT1
	 * case confirmed (by direct disassembly) to call EditApiGetScopeId()
	 * UNCONDITIONALLY at its own top, before any bound/guard check, unlike every
	 * other case (which check their own bound first and only fetch scope if it
	 * passes -- modeled as free-function Compute*() helpers in the .cpp instead).
	 * Needs class access for that unconditional fetch, hence its own private
	 * static member rather than a free function. See stg_unsol_msg_handler.cpp's
	 * own header comment.
	 */
	static void VoiceModelS13(unsigned char *p);
};

#endif /* STG_UNSOL_MSG_HANDLER_H */
