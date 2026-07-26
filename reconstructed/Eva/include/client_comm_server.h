/*
 * client_comm_server.h  -  CClientCommServer, the per-client SysEx transport state
 * machine (IDLE/SENT/WAIT protocol handshake over a CSysExMsgOutLink) -- Stage 6
 * breadth sweep, 2026-07-25 (follow-up pass same day: CEvBuffersPool/CEvent now real,
 * see ev_buffers_pool.h/event.h -- promotes 8 more methods below to Tier A; SECOND
 * follow-up pass same day promotes 4 more of the remaining 16 -- OnRxPacket(),
 * OnReceiveSysExBuffer(), OnRxSexWhenInWAIT(), TransmitSexAnswer() -- see their own
 * comments below and the corrected `mState` field entry in the layout comment).
 *
 * THIRD FOLLOW-UP PASS (same day, 23/26 Tier A now -- only `OnReceiveMessage()`
 * stays Tier B): `PrepareMsgBuffer()`, `UnprepareBuffer()`, `EventToMessage()`,
 * `MessageToEvent()`, `Error()`, `OnRxSexWhenInIDLE()`, `OnRxSexWhenInSENT()`,
 * `OnRxMsgWhenInIDLE()`, and `OnRxMsgWhenInSENT()` are now all real -- transcribed
 * from the Ghidra decompile export (`Decomp/EVA_Decomp/eva_export/functions`,
 * ground truth per PLAN.md) rather than hand-traced raw `objdump` this time -- the
 * byte-packing loops are hand-unrolled to a degree (7-way nested `if`) that made a
 * Ghidra-decompile cross-check the safer source of truth than re-deriving bit
 * positions from `objdump -M intel` alone. The prior pass's own prediction that
 * the message-side/sex-side IDLE/SENT pairs would still be blocked by `Error()`'s
 * own unreconstructed body turned out to be OVERLY conservative: once
 * `PrepareMsgBuffer()`/`UnprepareBuffer()`/`Error()` were real, a fresh dependency
 * check on all 4 found ZERO remaining `CMessage`/`CSexMatrix`-shaped calls in any
 * of them (confirmed by grepping each decompile's own call list) -- only
 * `OnReceiveMessage()` itself remains genuinely blocked, since `CMessage` is the
 * primary parameter type it cannot avoid touching.
 *
 * THE WIRE FORMAT (this pass's biggest finding): `PrepareMsgBuffer()`/
 * `UnprepareBuffer()` are a matched DECODE/ENCODE pair for a MIDI-SysEx-style
 * 8-to-7-bit-safe framing, but with the flag byte and its 7 payload bytes ordered
 * flag-FIRST (not flag-last): a "group" is 1 flag byte followed by up to 7 payload
 * bytes, and `PrepareMsgBuffer()` reconstructs output byte `i` (i=0..6, 0-based
 * within the group) as `(group[i+1] & 0x7f) | (((flag >> (6-i)) & 1) << 7)` --
 * i.e. flag bit6 restores payload1's bit7, bit5 restores payload2's, ... bit0
 * restores payload7's. Confirmed bit-for-bit against the Ghidra decompile's own
 * literal shift/mask constants for all 7 positions (`bVar2*2&0x80`, `*4&0x80`,
 * `*8&0x80`, `(bVar2&0xf8)<<4` truncated-to-byte, `*0x20&0x80`, `(bVar2&2)<<6`,
 * `bVar2<<7` truncated-to-byte -- each one independently verified to isolate
 * exactly bit 6,5,4,3,2,1,0 of the flag byte respectively). `UnprepareBuffer()` is
 * the exact inverse: builds the flag byte by left-shifting in each payload byte's
 * own bit7 as it's consumed, then (for a partial trailing group of `k<7` bytes)
 * left-justifies the accumulated `k`-bit value by an extra `(7-k)` shift so short
 * trailing groups still land in the same bit positions a full group would use.
 * Both directions are reconstructed here as a clean loop (not a transliteration of
 * the 7-deep nested-if/goto shape either function's own real disassembly has) --
 * safe because the per-position bit formula was verified from the DECOMPILE's own
 * explicit values, not inferred from the loop shape.
 *
 * `EventToMessage()`/`MessageToEvent()` are the CLinkedEvent<->raw-message
 * counterparts, layering a fixed 6-byte SysEx header (`F0 <CSexInputTask::
 * sm_byKorgID=0x42> <device, top bit clear> <CSexInputTask::sm_byKorgItalyID=0x60>
 * <mEcb> 0x01>`, values confirmed by a direct raw-byte read of
 * `Decomp/EVA_Decomp/Eva`'s `.rodata+0x8e7bce5..0x8e7bce7`) plus a trailing 0xF7
 * (SysEx end byte) around a `PrepareMsgBuffer()`/`UnprepareBuffer()` payload.
 * `EventToMessage()`'s own header-validation checks are REAL and enforcing (a
 * genuine `return` with no log on mismatch, unlike this file's usual soft
 * asserts) -- confirmed by reading the decompile's own bare `return 0;` with zero
 * diagnostic call. `MessageToEvent()` allocates its own fresh event buffer
 * directly via `CEvBuffersPool::Alloc()` (NOT through this class's own ctor-built
 * `mEvBuf`/`mEvTag` -- it operates on a caller-supplied `CLinkedEvent*`, same as
 * `UnprepareBuffer()`), and its final step overwrites the event tag's own
 * "unknown middle byte" (bits 8-15, the byte every OTHER method in this file only
 * ever preserves via `& 0xff00`) with a raw byte read from `mClient` (a
 * `CSysExMsgTaskBase*`) at offset +0x8c -- an out-of-scope field one byte past
 * where `sysex_msg_task_base.h`'s own reconstruction currently ends (`mOutLink`
 * at +0x88, 4 bytes, ending the class's currently-modeled layout at exactly
 * +0x8c) -- see that method's own `.cpp` comment for the resulting caveat (this
 * one read is against real, but currently unmodeled, memory in a sibling class
 * this file does not own).
 *
 * `Error()` -- real, ~1900 bytes, genuinely a 3-way dispatch on `mode`'s bits 0/1
 * as the SCOPE section below already predicted, but almost ENTIRELY the SAME
 * "reacquire mEvBuf lock, mutate, release, [maybe TransmitSysEx()], reacquire,
 * release" idiom already established by `TXData()`/`TransmitSexAnswer()` above,
 * repeated (not de-duplicated in ground truth either) up to 3 times depending on
 * which mode bits are set. Mode bit 0 (`& 1`) triggers a raw vtable-slot dispatch
 * on `mClient` (slot +0x18, index 6) -- reproduced as a raw pointer-arithmetic
 * call (matching every `Api`-vtable call site's own style in this file) rather
 * than including `sysex_msg_task_base.h`, keeping this file decoupled from that
 * class's own separate reconstruction effort. Mode bit 1 (`& 2`) writes a type=4
 * "error" marker byte into the event buffer at a fixed offset
 * (`CSexInputTask::sm_uiSexPropHeaderLen`, the SAME real value of 5 already used
 * here as `kSexPacketOverhead`) and sends it via `TransmitSysEx()`. Every mode
 * combination converges on the SAME final reset: `mUnknown0e=0xff`,
 * `mState0d=0`, `mState=0` (IDLE), `mUnknown08=0`.
 *
 * `OnRxSexWhenInIDLE()`/`OnRxSexWhenInSENT()` and `OnRxMsgWhenInIDLE()`/
 * `OnRxMsgWhenInSENT()` -- the remaining 4 members of the IDLE/SENT/WAIT dispatch
 * family (`OnRxSexWhenInWAIT()`/`OnRxMsgWhenInWAIT()` were already real from the
 * prior 2 passes) -- turned out to have ZERO remaining `CMessage`/`CSexMatrix`
 * dependency once checked fresh against this pass's own newly-real helpers, so all
 * 4 are reconstructed here too:
 *   `OnRxSexWhenInIDLE(type,...)` -- type 0 tail-calls `OnRxPacket()` unchanged;
 *     type 1 either builds+sends a raw echo through `mOutLink->OutMono()` directly
 *     (when `mModeService` bit 0x20, the checksum-framing bit, is CLEAR -- ground
 *     truth genuinely reads `OutMono()`'s own return value here, unlike
 *     `SendMessageToClient()`'s own discard, but this method's own committed
 *     `void` return -- symbols.csv has no resolved type -- discards it in turn)
 *     or falls to the shared `Error(0)` tail (bit 0x20 SET, an out-of-protocol
 *     "raw data when checksums are on" condition); types 2/3/default all take the
 *     same soft-log-only + `Error(0)` shared tail; type 4 is the ONE case with a
 *     different `Error()` argument (`Error(1)`).
 *   `OnRxSexWhenInSENT(type,...)` -- types 0/1 both do a real "reset to IDLE" (the
 *     SAME `mUnknown0e=0xff`/`mState0d=0`/`mState=0`/`mUnknown08=0` reset `Error()`
 *     itself performs) then a genuine tail-jmp into `OnRxSexWhenInIDLE()` with the
 *     SAME `type`/`data`/`len`/`x` this method itself received (confirmed via
 *     `objdump` -- ground truth's own `jmp` target reuses the caller's original
 *     argument registers/stack slots unchanged, not re-derived); type 2 (ack)
 *     resets to IDLE and returns if `data[0]==mState0c`, else falls to a shared
 *     `Error(0)` "give up" tail; type 3 (retry) resends via `TransmitSysEx()` and
 *     bumps the retry counter if `data[0]==mState0c && mState0d<5`, else the same
 *     `Error(0)` tail; type 4 and out-of-range default are BOTH genuine no-ops (no
 *     log, no `Error()` call at all, confirmed by reading the fallthrough -- a
 *     real, different tail from every other case in this method). One documented
 *     dead-code simplification: ground truth's own `if (mUnknown08==1)
 *     mUnknown08=0;` at the top of cases 0/1/2 is provably dead (every control
 *     path in those 3 cases unconditionally overwrites `mUnknown08` again before
 *     returning, whether via this method's own reset or `Error()`'s own shared
 *     tail) and is omitted; the SAME check in case 3's SUCCESS path is NOT dead
 *     (it is the ONLY write to `mUnknown08` on that path) and is kept.
 *   `OnRxMsgWhenInIDLE(data,len,x)` -- packs `data`/`len` into this class's OWN
 *     embedded event (via `UnprepareBuffer(reinterpret_cast<CLinkedEvent*>(&mEvTag),
 *     ...)`, starting at a fixed offset depending on `mModeService` bit 0x20:
 *     `kSexPacketOverhead+1` (6) if clear, `+2` (7) if set) and overwrites the
 *     tag's own "unknown middle byte" with `x` (same idiom `MessageToEvent()`
 *     uses). If bit 0x20 is SET (checksum framing requested): backfills a 2-byte
 *     mini-header at offsets `headerOff-2`/`headerOff-1` (`[0, mState0c+1 mod
 *     0x80]` -- `mState0c` is incremented here as a real, observable side effect),
 *     appends a running `ComputeCRCByte()` checksum byte right after the packed
 *     payload, transitions to SENT (`mState=1`), and sends -- this is the message
 *     side of the SAME ack/retry state machine `OnRxSexWhenInSENT()` drives. If
 *     bit 0x20 is CLEAR: backfills a single fixed type=1 byte at `headerOff-1`, no
 *     checksum, sends fire-and-forget, then does the FULL reset back to IDLE
 *     (same 4-field reset as `Error()`'s own shared tail) since nothing is
 *     expected back.
 *   `OnRxMsgWhenInSENT(data,len,x)` -- much simpler: sets `mUnknown08=1`
 *     (unconditional, no dead-code caveat here), one reacquire+release cycle
 *     (idiom), then a genuine tail-jmp into `OnRxMsgWhenInIDLE()` with the SAME
 *     `data`/`len`/`x` unchanged (same confirmed-via-`objdump` pattern as
 *     `OnRxSexWhenInSENT()`'s own cases 0/1 above).
 *
 * Still deferred after this pass (1/26 Tier B): `OnReceiveMessage(const CMessage&)`
 * only -- `CMessage` itself remains completely out of scope (forward-declared only,
 * no field layout established), and this is the one method whose own primary
 * parameter type IS `CMessage`, so it cannot be reconstructed without that class.
 *
 * GROUND TRUTH REACHABILITY (the actual point of this pass, correcting Stage 6 batch
 * 6's own "no confirmed caller found in a quick check, lower confidence" verdict on
 * this exact class): the real caller chain is genuine and boot-path-adjacent, fully
 * confirmed by direct `objdump -dr` call-site tracing this pass, not guessed:
 *
 *   CKernel::InitUserLayer() (ckernel.cpp, already real)
 *     -> CConfigManager::SetupSysex() (.text+0x08056b90, config_manager.cpp, UPGRADED
 *        to Tier A this pass -- was an empty Tier-B stub)
 *     -> SysExApi->RegisterMessageClient(name, ecb, mode, service) -- a REAL VIRTUAL
 *        call through the `SysExApi` global (mains.cpp's own already-real
 *        `void *SysExApi` singleton pointer, vtable slot +0x30) for every
 *        non-"skip"-flagged entry in `CConfigManager::sm_ptSysExModuleInfo`'s own
 *        table (currently a zero-initialized placeholder -- see config_info.cpp --
 *        so this pass's own reconstruction executes zero real registrations today,
 *        same "real code, currently a no-op pending real config data" category as
 *        `CModuleManager::AddModule()` before `mains.cpp` populated `mModules`)
 *     -> tail-jumps to `CSexServiceTask::RegisterMessageClient()` (.text+0x08179420,
 *        NOT reconstructed here -- see below)
 *     -> constructs `CClientCommServer` (.text+0x0816ecc0) with a `CSysExMsgTaskBase&`
 *        obtained via a by-name module lookup (Api vtable slot +0x64, RTTI-checked
 *        against `CSysExMsgTaskBase::SysName`).
 *
 * `CSexServiceTask::RegisterMessageClient()` itself is genuinely reachable too (it is
 * the ONLY real caller of `CClientCommServer::CClientCommServer` found anywhere in the
 * 3.5M-line disassembly -- exhaustively checked: zero direct callers of any of
 * `CSysExApiInstance`'s dozen public methods exist anywhere except this one virtual
 * path) but is NOT reconstructed this pass -- it is 0x31f (799) bytes of real
 * `CSysExMsgOutLink` construction, `CTask::Add(COutLink*)`, and `CEvent`-buffer
 * bookkeeping, i.e. it sits on the SAME out-of-scope dependency chain this class's own
 * ctor does (see below). Reconstructing it would not change what this pass proves --
 * `CClientCommServer` is real and reachable regardless of whether its own single real
 * caller is itself reconstructed, same "ground-truth reachable is the bar, not this
 * reconstruction's own current call graph" principle task.h's own CTask writeup
 * already established this pass.
 *
 * SCOPE (updated, follow-up pass same day): this is a genuinely large class (26 real
 * methods, ~7.6KB of real .text, a full IDLE/SENT/WAIT handshake state machine layered
 * on a CRC-checked packet framing protocol). The original pass found its CONSTRUCTOR
 * alone pulled in an entire un-reconstructed subsystem -- `CEvBuffersPool`/`CEvent`, a
 * fixed-size refcounted slab allocator backing an embedded `CLinkedEvent`-shaped tagged
 * buffer this class uses as its own TX scratch space. That subsystem is now real (see
 * `ev_buffers_pool.h`/`event.h`, reconstructed from `CEvBuffersPool::
 * {CEvBuffersPool,Alloc,Free,Lock}`/`CEvent::~CEvent`'s own disassembly), which unblocks
 * the ctor/dtor and 6 more leaf methods -- 10/26 Tier A total now:
 *
 * SECOND FOLLOW-UP PASS (same day, 14/26 Tier A now): reconstructed
 * `OnRxSexWhenInWAIT()`, `OnReceiveSysExBuffer()`, `OnRxPacket()`,
 * `TransmitSexAnswer()` directly from `objdump -dr -M intel` (Decomp/EVA_Decomp/Eva).
 * This pass's single biggest finding: what the ORIGINAL pass called `mUnknown04`
 * ("some kind of retry-in-flight flag, not fully confirmed") is actually the class's
 * own top-level protocol state (IDLE=0/SENT=1/WAIT=2) that selects which
 * `OnRx{Msg,Sex}WhenIn{IDLE,SENT,WAIT}` family member runs next -- confirmed
 * independently in TWO different functions (`OnReceiveSysExBuffer()`'s own 3-way
 * `cmp ecx,1`/`cmp ecx,2`/`test ecx,ecx` dispatch into `OnRxSexWhenInSENT`/
 * `OnRxSexWhenInWAIT`/`OnRxSexWhenInIDLE`, and `OnRxPacket()`'s own
 * `mState = 2` on a short/malformed packet and `mState = 0` after a clean
 * receive-and-answer cycle) -- renamed `mUnknown04` -> `mState` throughout, correcting
 * the original pass's own lower-confidence guess (`RetryTXPacket()`/`OnProcessRetry()`
 * setting it to 1 before resending is still consistent with the corrected meaning:
 * a retry transitions the state machine back to SENT, awaiting a fresh ack).
 * `mUnknown08` is unaffected by this finding -- still not decoded, still zeroed
 * alongside `mState` in the same 3 places.
 *
 * Still deferred (12/26 Tier B) after this pass, each for a distinct, examined
 * reason -- NOT a blanket "didn't get to it":
 *   `PrepareMsgBuffer()`/`UnprepareBuffer()` -- confirmed (by reading the "raw
 *     passthrough" fast path taken when `mModeService` bit 0x10 is clear: a plain
 *     `memcpy`) that the bit-0x10-SET path is a REAL, non-trivial byte-packing
 *     transform, not just framing -- a hand-unrolled per-byte-offset(0..6) loop
 *     building an MSB-accumulator byte alongside 7 payload bytes with `and 0x7f`/`or`
 *     patterns consistent with a MIDI-style 7-bytes-plus-1-MSB-byte 8-to-7-bit-safe
 *     encoding, but NOT verified bit-exact this pass -- a wrong guess here would
 *     silently corrupt real wire framing, so left undecoded rather than guessed.
 *     `EventToMessage()`/`MessageToEvent()` presumably wrap the same transform in the
 *     other direction -- deferred alongside for the same reason, not separately
 *     examined this pass.
 *   `Error()` -- real, ~2KB (0816f830-0816ffd0), but almost entirely a 3-way
 *     dispatch (on `mode`'s bits 0/1) of the SAME "reacquire mEvBuf, reset
 *     mState0c/mUnknown1c, maybe TransmitSysEx() again" idiom already established by
 *     `TXData()`/`TransmitSexAnswer()` below, glued together with heavy shared-tail
 *     jump reuse across all 3 branches -- mechanically de-duplicating this correctly
 *     under time budget without silently flipping a branch polarity (this file's own
 *     established risk, see `event.h`'s sign-bit bug-class note) was judged
 *     disproportionate to its payoff versus the 4 methods promoted this pass; every
 *     caller of `Error()` reconstructed so far treats it as an opaque "handle the
 *     error and reset" leaf, which is exactly how it's declared (Tier B, real
 *     signature, empty body).
 *   `OnReceiveMessage(const CMessage&)` -- the primary vtable-slot-0 entry point,
 *     but `CMessage` itself is a completely out-of-scope class (forward-declared
 *     only, no field layout established) -- cannot safely read its members.
 *   `OnRxMsgWhenInIDLE()`/`OnRxMsgWhenInSENT()` (575/157 lines) and
 *     `OnRxSexWhenInIDLE()`/`OnRxSexWhenInSENT()` (161/381 lines) -- the other 4
 *     members of the IDLE/SENT/WAIT dispatch family (`OnRxSexWhenInWAIT` above is
 *     the one member of this family this pass DID reconstruct); each calls
 *     `PrepareMsgBuffer()`/`UnprepareBuffer()`/`Error()` heavily, so full
 *     reconstruction has the same "framing transform not yet verified" and
 *     "Error()'s exact side effects not yet reconstructed" dependencies as above --
 *     left for a future batch alongside those two.
 *
 *   ComputeCRCByte(unsigned char) / CheckIncomingSexCRCByte(unsigned char const*,
 *     unsigned char) -- unchanged from the original pass, see below.
 *   CClientCommServer(...) (ctor, .text+0x0816ecc0, 1343 bytes) / ~CClientCommServer()
 *     (dtor, .text+0x0816f240, 125 bytes) -- now real: malloc's mTxBuf, then
 *     Alloc()s+Lock()s the embedded event buffer (dtor mirrors CEvent::~CEvent()'s own
 *     sign-of-mEvTag ownership check, inlined against the embedded fields rather than
 *     a real CEvent member -- see field layout below). `CSexInputTask::
 *     sm_uiMaxSexPropLen`'s real value is still not reconstructed (CSexInputTask itself
 *     is a different, out-of-scope class) -- a placeholder constant
 *     (`kMaxSexPropLenPlaceholder`, the real ctor's own assert upper bound, 0xff) feeds
 *     both the malloc() size and the Alloc() size, same "real code, placeholder input"
 *     category as `CConfigManager::sm_ptSysExModuleInfo`'s own zero-initialized table.
 *   SendMessageToClient() (.text+0x0816f2c0, 169 bytes) -- real: calls
 *     `COutLinkMono::OutMono(mOutLink, mEcb, mTxBuf, mUnknown1c)` unconditionally (a
 *     real soft-assert on `mModeService`'s own bit 0x02 gates nothing -- both sides of
 *     that branch converge on the same call, confirmed by reading both targets).
 *   SendToSysExLink() / RetryTXPacket() (.text+0x0816f370/+0x0816f3a0, 39/46 bytes) --
 *     both real, both just `mOwner->TransmitSysEx(&embedded-event-as-CLinkedEvent*,
 *     mEcb)` (RetryTXPacket also sets `mUnknown04 = 1` first).
 *   TXData() (.text+0x0816f3d0, 562 bytes) -- real: same TransmitSysEx() call, then
 *     (since `mEvTag` is provably always the ctor's own negative
 *     `0x8000000a`-shaped tag by construction, a real, non-collapsed conditional here
 *     since a not-yet-reconstructed caller could in principle leave it non-negative)
 *     re-acquires the embedded buffer via the SAME clear-lock-bit-then-Lock() sequence
 *     the ctor uses.
 *   OnProcessRetry(unsigned char) (.text+0x08170010, 124 bytes) -- real: if the state
 *     byte doesn't match the expected value, or the retry counter already exceeds 4,
 *     calls `Error(eErrNotifyReserved)` and returns (a real soft trace-log call, Api
 *     vtable slot 0x90, omitted -- non-enforcing); otherwise increments the retry
 *     counter and resends via `TransmitSysEx()`.
 *
 * `CSexServiceTask::TransmitSysEx(CLinkedEvent*, unsigned char)` itself is NOT
 * reconstructed (out of scope, same class this header's own reachability writeup
 * already deferred) -- forward-declared here with its real mangled signature so these
 * 6 methods link against the real symbol name.
 *
 * SECOND FOLLOW-UP PASS, 4 more real (14/26 Tier A total) -- see the SCOPE section
 * above for the full `mState` finding and the specific reasons the other 12 are still
 * deferred:
 *   OnRxSexWhenInWAIT(type, data, len, x) (.text+0x08172860, 313 bytes) -- real: a
 *     5-entry jump table on `type` (0..4, read from `.rodata+0x8e7bc60`, confirmed by
 *     direct file read): type 0 tail-calls `OnRxPacket(data, len, x)` unchanged (the
 *     "this is just a normal packet" case); types 1/2/3 log (soft) then
 *     `Error(static_cast<EErrNotifyMode>(0))`; type 4 logs then
 *     `Error(static_cast<EErrNotifyMode>(1))`; type >4 (out of range) logs and returns
 *     WITHOUT calling Error() at all -- confirmed by reading the fallthrough, a
 *     genuinely different tail from the 1/2/3/4 cases.
 *   OnReceiveSysExBuffer(data, len, x) (.text+0x08173210, 543 bytes) -- real: after a
 *     soft NULL-data assert (logs and continues anyway on data==NULL, same
 *     "non-enforcing" convention as everywhere else in this file) and a soft
 *     minimum-length assert (`len <= 6`, `.rodata+0x8e7bcb0`'s own value, confirmed
 *     5), verifies `data[global-1] == mEcb` (soft, logs on mismatch but continues),
 *     then a REAL, enforcing bounds check (`len > .rodata+0x8e7bcb4` = 255 -> silent
 *     early return, no log) before dispatching on `mState` (0/1/2) into
 *     `OnRxSexWhenInIDLE`/`OnRxSexWhenInSENT`/`OnRxSexWhenInWAIT` with
 *     `type = data[5]`, `payload = data+6`, `len = original_len-7`, `x` unchanged --
 *     any other `mState` value logs (soft) and returns without dispatching.
 *   OnRxPacket(data, len, x) (.text+0x08172320, 1408 bytes) -- real: gates on
 *     `mModeService` bit 0x20 and `len != 0` (both REAL early-return checks, unlike
 *     most of this file's soft asserts); soft-checks `data[0]` against `mUnknown0e`
 *     (the "last-accepted tag byte" sentinel, 0xff = "none yet"); a packet with
 *     `len <= 2` or a failing running-XOR checksum over `data[1 .. len-2]` (seeded
 *     with `len-1`, ground truth is an SSE2-vectorized reduction + Duff's-device
 *     tail, collapsed here to one clean scalar loop -- XOR is commutative/
 *     associative so this is bit-identical regardless of reduction order, same
 *     license as `ComputeCRCByte()`'s own collapse) both take the SAME "resync" path
 *     (`mState = 2`, `mUnknown0e = data[0]`, `TransmitSexAnswer(3, data[0])`); a
 *     passing checksum calls the not-yet-decoded `PrepareMsgBuffer()` to build an
 *     echo into `mTxBuf+1` (this method's own real bytes ARE reconstructed even
 *     though the helper it calls is still a Tier-B stub -- same "real code, opaque
 *     helper" layering already established for `CConfigManager`), sets
 *     `mState = 0`, calls `TransmitSexAnswer(2, data[0])`, then unconditionally
 *     (2 more soft, non-gating asserts on `mModeService` bit 0x2 and `mOutLink !=
 *     NULL` confirmed to gate nothing -- reused as a call to the already-real
 *     `SendMessageToClient()`) sends `mTxBuf` and zeroes `mUnknown1c`.
 *   TransmitSexAnswer(type, x) (.text+0x08170090, 928 bytes) -- real: reacquires
 *     `mEvBuf` (clear lock bit + `Lock()`, the ctor's own established idiom), writes
 *     `mEvBuf[5] = type`, `mEvBuf[6] = x` (a fixed 7-byte short-answer format sharing
 *     the SAME embedded-event buffer `SendToSysExLink()`/`TXData()` use, NOT
 *     `mTxBuf`), sets the event's packed length field to 7
 *     (`mEvTag |= 7 << 16`), calls `mOwner->TransmitSysEx()`, then reacquires and
 *     immediately releases the lock bit again (leaving the buffer available for the
 *     next user, matching the "exclusive lock held only transiently during mutation"
 *     convention this whole class follows). A soft assert that `type` is 2 or 3
 *     gates nothing -- the write happens unconditionally regardless of `type`'s
 *     actual value, confirmed by reading every branch target.
 *
 * (At the time of THIS specific pass, 12 methods were still Tier B; later follow-up
 * passes the same day closed all but one -- see line ~141 above for the current,
 * final status: only `OnReceiveMessage(const CMessage&)` remains Tier B.)
 *
 * REAL LAYOUT (confirmed from CClientCommServer@0816ecc0.c, ComputeCRCByte@0816f610.c):
 *   +0x00  vtbl         &PTR_OnReceiveMessage_08e898c8 (this class's real, slot-0-named
 *                        vtable -- OnReceiveMessage(CMessage const&) is the primary
 *                        virtual override; never dispatched through by any
 *                        reconstructed code, same "install-only" status as
 *                        omega_vtables.h's own catalogue)
 *   +0x04        mState (int) -- the class's own top-level protocol state:
 *                0=IDLE, 1=SENT, 2=WAIT (selects which `OnRx{Msg,Sex}WhenIn*`
 *                member handles the next receive). RENAMED from the original pass's
 *                `mUnknown04` ("some kind of retry-in-flight flag, not fully
 *                confirmed") -- see this header's own SCOPE section for the 2
 *                independent confirmations this pass found. Ctor zeroes it (IDLE);
 *                TXData() also re-zeroes it after every send (now understood: a
 *                completed send-and-ack cycle returns to IDLE); RetryTXPacket()/
 *                OnProcessRetry() set it to 1 (SENT) right before resending, and
 *                OnRxPacket() sets it to 2 (WAIT) on a short/bad-checksum packet or
 *                back to 0 (IDLE) after a clean receive-and-answer cycle.
 *   +0x08        mUnknown08 (int), ctor zeroes; TXData() also re-zeroes it after
 *                every send -- still not decoded what this one tracks, the `mState`
 *                finding above doesn't extend to it (checked, no caller this pass
 *                reconstructed reads it).
 *   +0x0c/+0x0d  2 state bytes (mState0c/mState0d), ctor zeroes both (protocol state
 *                machine's own current-state field + retry counter -- OnProcessRetry()
 *                confirms mState0c is compared against an expected-state argument and
 *                mState0d is a retry count capped at 4, both real now)
 *   +0x0e        byte (mUnknown0e), ctor sets 0xff ("no active retry"?, not decoded);
 *                TXData() resets it to 0xff after every send too
 *   +0x0f        byte mModeService, ctor sets `(byte)service | (byte)commMode` -- the
 *                combined mode/service tag this client registered under
 *   +0x10        CSysExMsgOutLink* mOutLink (ctor's own `outLink` argument, stored raw)
 *   +0x14        unsigned char mEcb (ctor's own `ecb` argument -- the comm/sysex-id
 *                byte this client answers to)
 *   +0x18        void* mTxBuf -- malloc'd scratch buffer, size = mMaxSexPropLen (below)
 *   +0x1c        byte mUnknown1c, ctor zeroes; TXData() re-zeroes it after every send
 *   +0x1d/+0x1e  byte mMaxSexPropLen (both copies of `CSexInputTask::
 *                sm_uiMaxSexPropLen`'s own low byte -- NOT reconstructed here, that
 *                static lives on a different, un-reconstructed class; ctor's own
 *                assert bounds-checks it against 0xff, reused here as a placeholder
 *                value, see ctor comment in the .cpp)
 *   +0x20        int mEvTag -- embedded CEvBuffersPool-backed event's own packed tag
 *                word (bits 16-23 = payload length, per ComputeCRCByte's own
 *                `(uint)mEvTag >> 0x10` extraction; bit 31 = "owns a pool buffer",
 *                same convention as `CEvent`'s own tag, see event.h) laid out, together
 *                with +0x24/+0x28 below, exactly as an embedded `CLinkedEvent` --
 *                `SendToSysExLink()`/`TXData()`/`RetryTXPacket()`/`OnProcessRetry()`
 *                all `reinterpret_cast<CLinkedEvent*>(&mEvTag)` this same address range
 *                when calling `CSexServiceTask::TransmitSysEx()`.
 *   +0x24        void* mEvBuf -- the event's own payload buffer pointer, now real
 *                (`CEvBuffersPool::Alloc()`/`Lock()`, see ev_buffers_pool.h/event.h)
 *   +0x28        int mUnknown28, ctor zeroes (the embedded CLinkedEvent's own `mNext`
 *                slot -- always zero here since this event is never queued anywhere)
 *   +0x2c        CSexServiceTask* mOwner (ctor's own `owner` argument, stored raw)
 *   +0x30        CSysExMsgTaskBase* mClient (ctor's own `client` argument, stored raw
 *                -- THIS is the field that makes CSysExMsgTaskBase a real, load-bearing
 *                dependency of this class, per this header's own reachability writeup)
 */

#ifndef CLIENT_COMM_SERVER_H
#define CLIENT_COMM_SERVER_H

class CSysExMsgTaskBase;
class CSysExMsgOutLink;
class CMessage;
class CLinkedEvent;

/* Forward-declared with only the one real method this class calls --
 * CSexServiceTask itself is out of scope this pass (see header comment). Real mangled
 * signature: `_ZN15CSexServiceTask13TransmitSysExEP12CLinkedEventh`. Return type is
 * genuinely unresolved (functions.csv/symbols.csv both carry no return type for it) --
 * every real caller's own `eax` after the call is either unused or copied into a
 * value the caller's own declared-void signature then discards (same "eax not part of
 * the real contract" pattern already established for `TXData()` itself), so `int` here
 * is a safe placeholder that doesn't change any observable behavior.
 */
class CSexServiceTask {
public:
	int TransmitSysEx(CLinkedEvent *ev, unsigned char ecb);
};

/* COutLinkMono is now a REAL reconstructed class (out_link.h, Eva CSysExMsgClientOutLink
 * follow-up pass, 2026-07-25) -- SendMessageToClient()'s own `COutLinkMono::OutMono()`
 * call (a direct, non-virtual call in ground truth, unaffected by this upgrade) now
 * resolves to the real implementation. See out_link.h for the full writeup; only
 * forward-declared here, the .cpp includes out_link.h for the real definition.
 */
class COutLinkMono;

class CClientCommServer {
public:
	/* Real enums, opaque (functions.csv/symbols.csv carry only the enum TYPE name,
	 * same convention as task.h's ETaskLevel/CSysExMsgTaskBase's ECanTransmit).
	 */
	enum ECommMode { eCommModeReserved = 0 };
	enum EService { eServiceReserved = 0 };
	enum ESexMsgType { eSexMsgTypeReserved = 0 };
	enum EErrNotifyMode { eErrNotifyReserved = 0 };

	/* .text+0x0816ecc0, 1343 bytes. Tier A -- see header comment (now real, using
	 * CEvBuffersPool/CEvent).
	 */
	CClientCommServer(CSexServiceTask &owner, CSysExMsgTaskBase &client, unsigned char ecb,
	                   ECommMode mode, EService service, CSysExMsgOutLink *outLink);

	/* .text+0x0816f240 (+1 duplicate thunk), 125 bytes. Tier A. */
	~CClientCommServer();

	/* .text+0x08173430, 694 bytes. Tier A -- pure, does not touch `this` at all
	 * (confirmed by reading the decompile). Running XOR checksum over
	 * `data[0..len-1]`, mod-256 indexed (matches ComputeCRCByte's own indexing
	 * convention, just over an explicit caller buffer instead of the embedded
	 * event). Real early-out: `len < 2` returns false unconditionally.
	 */
	bool CheckIncomingSexCRCByte(const unsigned char *data, unsigned char len) const;

	/* .text+0x0816f610, 520 bytes. Tier A -- see header comment. Real body asserts
	 * (via two paired tag-byte checks the ORIGINAL debug build encoded as
	 * `Api`-vtable-slot-0x94 assertion calls) that `mEvTag`'s own tag byte reads
	 * 0x0a both times -- both asserts are omitted here (this pass's KAT drives
	 * `mEvTag`/`mEvBuf` directly via the friend hook below, always with a
	 * consistent tag, so the real assert's failure path is unreachable by
	 * construction, same "dead in practice" call this pass made for
	 * CSysExMsgTaskBase::Exec(CMessage&)).
	 */
	unsigned char ComputeCRCByte(unsigned char startIndex) const;

	/* .text+0x08170010, 124 bytes. Tier A -- see header comment. If mState0c
	 * doesn't match `expectedState`, or the retry counter (mState0d) already
	 * exceeds 4, logs (Api+0x90, omitted, soft) and calls Error(); otherwise
	 * bumps the retry counter and resends via mOwner->TransmitSysEx().
	 */
	void OnProcessRetry(unsigned char expectedState);

	/* .text+0x0816f3a0, 46 bytes. Tier A -- see header comment. */
	void RetryTXPacket();

	/* .text+0x0816f2c0, 169 bytes. Tier A -- see header comment. */
	void SendMessageToClient();

	/* .text+0x0816f370, 39 bytes. Tier A -- see header comment. */
	void SendToSysExLink();

	/* .text+0x0816f3d0, 562 bytes. Tier A -- see header comment. */
	void TXData();

	/* .text+0x08172860, 313 bytes. Tier A -- see header comment (second follow-up
	 * pass). 5-entry jump table on `type`; type 0 tail-calls OnRxPacket() unchanged,
	 * types 1-4 log (soft) then Error(), type>4 logs and returns without calling
	 * Error() at all.
	 */
	void OnRxSexWhenInWAIT(ESexMsgType type, const unsigned char *data, unsigned char len,
	                        unsigned char x);

	/* .text+0x08173210, 543 bytes. Tier A -- see header comment (second follow-up
	 * pass). Soft NULL/ecb-mismatch/min-length asserts, one REAL enforcing bounds
	 * check (len > 255 -> silent early return), then dispatches on mState into
	 * OnRxSexWhenIn{IDLE,SENT,WAIT}.
	 */
	void OnReceiveSysExBuffer(const unsigned char *data, unsigned char len, unsigned char x);

	/* .text+0x08172320, 1408 bytes. Tier A -- see header comment (second follow-up
	 * pass). Two REAL gating checks (mModeService bit 0x20, len!=0), a running-XOR
	 * checksum (collapsed from an SSE2-vectorized reduction, see header comment),
	 * dispatch to a resync path (TransmitSexAnswer(3,...)) or a success path that
	 * calls the still-Tier-B PrepareMsgBuffer() to build an echo, then
	 * TransmitSexAnswer(2,...) and a real send via SendMessageToClient().
	 */
	void OnRxPacket(const unsigned char *data, unsigned char len, unsigned char x);

	/* .text+0x08170090, 928 bytes. Tier A -- see header comment (second follow-up
	 * pass). Reacquires mEvBuf, writes a fixed 7-byte short-answer format into it
	 * (mEvBuf[5]=type, mEvBuf[6]=x), sets the event's length field to 7, sends via
	 * mOwner->TransmitSysEx(), then releases the lock again.
	 */
	void TransmitSexAnswer(ESexMsgType type, unsigned char x);

	/* .text+0x081706d0, 794 bytes. Tier A -- third follow-up pass, see header
	 * comment for the full DECODE-direction wire-format writeup. Real return
	 * value (bool success) is discarded by every real caller traced so far
	 * (confirmed at OnRxPacket()'s own call site) -- committed as void.
	 */
	void PrepareMsgBuffer(unsigned char *buf, unsigned char &len, const unsigned char *data,
	                       unsigned char dataLen);

	/* .text+0x08170a00, 2763 bytes. Tier A -- third follow-up pass, ENCODE
	 * direction (see header comment). Operates on a caller-supplied
	 * `CLinkedEvent*`, not this object's own mEvTag/mEvBuf -- accessed via raw
	 * offset-0/offset-4 pointer arithmetic (same layout `event.h`'s own
	 * CEvent/CLinkedEvent establishes), not a real CEvent member access.
	 */
	void UnprepareBuffer(CLinkedEvent *ev, const unsigned char *data, unsigned char len,
	                      unsigned char x);

	/* .text+0x081736f0, 611 bytes. Tier A -- third follow-up pass. REAL,
	 * enforcing 6-byte SysEx header validation (genuine `return` with no log on
	 * mismatch -- F0/KorgID/<device>/KorgItalyID/mEcb/0x01), then forwards the
	 * payload to PrepareMsgBuffer(). Real return value (bool) discarded, same
	 * convention as PrepareMsgBuffer() itself.
	 */
	void EventToMessage(const CLinkedEvent *ev, unsigned char *out, unsigned char &outLen);

	/* .text+0x08173970, 1791 bytes. Tier A -- third follow-up pass. Allocates a
	 * FRESH event buffer (CEvBuffersPool::Alloc(), not this object's own
	 * mEvTag/mEvBuf), writes the fixed 6-byte SysEx header + UnprepareBuffer()'s
	 * packed payload + a trailing 0xF7, then overwrites the tag's own "unknown
	 * middle byte" with a raw byte read from `mClient` at offset +0x8c -- see
	 * header comment and this method's own `.cpp` comment for the resulting
	 * "reads real but currently-unmodeled sibling-class memory" caveat.
	 */
	void MessageToEvent(const unsigned char *data, unsigned char len, CLinkedEvent *ev);

	/* .text+0x0816f830, 1902 bytes. Tier A -- third follow-up pass, see header
	 * comment for the full 3-way mode-bit dispatch writeup.
	 */
	void Error(EErrNotifyMode mode);

	/* .text+0x08172990, 582 bytes. Tier A -- third follow-up pass, see header
	 * comment. type 0 tail-calls OnRxPacket(); type 1 builds+sends a raw echo
	 * (mModeService bit 0x20 clear) or falls to Error(0) (bit 0x20 set); types
	 * 2/3/default -> Error(0); type 4 -> Error(1) (the one case with a different
	 * argument).
	 */
	void OnRxSexWhenInIDLE(ESexMsgType type, const unsigned char *data, unsigned char len,
	                        unsigned char x);

	/* .text+0x08172bf0, 1516 bytes. Tier A -- third follow-up pass, see header
	 * comment. Types 0/1: real IDLE reset + tail-jmp into OnRxSexWhenInIDLE()
	 * with the SAME arguments. Type 2 (ack): reset-to-IDLE on data[0]==mState0c,
	 * else Error(0). Type 3 (retry): resend + bump retry counter on
	 * data[0]==mState0c && mState0d<5, else Error(0). Type 4 / default: genuine
	 * no-ops (no log, no Error() call).
	 */
	void OnRxSexWhenInSENT(ESexMsgType type, const unsigned char *data, unsigned char len,
	                        unsigned char x);

	/* .text+0x08171510, 2159 bytes. Tier A -- third follow-up pass, see header
	 * comment. Packs data/len into this object's OWN embedded event via
	 * UnprepareBuffer(), then either (mModeService bit 0x20 set) appends a
	 * sequence+checksum byte pair and transitions to SENT awaiting an ack/retry,
	 * or (bit 0x20 clear) sends a fixed fire-and-forget marker and resets
	 * straight back to IDLE.
	 */
	void OnRxMsgWhenInIDLE(const unsigned char *data, unsigned char len, unsigned char x);

	/* .text+0x08171db0, 591 bytes. Tier A -- third follow-up pass, see header
	 * comment. Sets mUnknown08=1, one reacquire+release cycle, then a genuine
	 * tail-jmp into OnRxMsgWhenInIDLE() with the SAME arguments unchanged.
	 */
	void OnRxMsgWhenInSENT(const unsigned char *data, unsigned char len, unsigned char x);

	/* .text+0x0816ffd0, 59 bytes. Real return value is 1 (bool `true`) in ground
	 * truth, but the committed real signature (functions.csv/symbols.csv, no
	 * return type resolved) is `void` -- same "eax not part of the real
	 * contract" category as TXData()'s own observed-but-discarded eax. Real
	 * body: an unconditional log (Api+0x90, omitted, soft) then Error().
	 */
	void OnRxMsgWhenInWAIT(const unsigned char *data, unsigned char len, unsigned char x);

	/* Only remaining Tier B method (real signature, empty body) -- CMessage
	 * itself is completely out of scope (forward-declared only), and this is
	 * this class's own primary CMessage-typed entry point. NOT declared C++
	 * `virtual` -- see sysex_msg_task_base.h's own header comment for why (this
	 * project's raw-`mVtbl`-pointer convention, not real C++ polymorphism). Real
	 * ground-truth vtable slot 0.
	 */
	int OnReceiveMessage(const CMessage &msg);

	/* .text+0x08e7bc9c, 1 byte. Real static data member. Value not read from the
	 * binary this pass (no reconstructed code reads it); zero-initialized here.
	 */
	static unsigned char sm_byMaxRetryNumber;

private:
	void              *mVtbl;
	int                mState;    /* was mUnknown04, see header comment */
	int                mUnknown08;
	unsigned char      mState0c;
	unsigned char      mState0d;
	unsigned char      mUnknown0e;
	unsigned char      mModeService;
	CSysExMsgOutLink  *mOutLink;
	unsigned char      mEcb;
	void              *mTxBuf;
	unsigned char      mUnknown1c;
	unsigned char      mMaxSexPropLen1d;
	unsigned char      mMaxSexPropLen1e;
	int                mEvTag;
	void              *mEvBuf;
	int                mUnknown28;
	CSexServiceTask   *mOwner;
	CSysExMsgTaskBase *mClient;

	/* Friend accessor for verify/test_client_comm_server.cpp -- pokes mEvTag/mEvBuf
	 * directly so ComputeCRCByte() can be exercised without a real (Tier-B, does
	 * not populate them) constructor having run. Same convention as
	 * comm_driver.h's CommDriverTestHooks/module.h's ModuleTestHooks.
	 */
	friend struct ClientCommServerTestHooks;
};

#endif /* CLIENT_COMM_SERVER_H */
