/*
 * client_comm_server.h  -  CClientCommServer, the per-client SysEx transport state
 * machine (IDLE/SENT/WAIT protocol handshake over a CSysExMsgOutLink) -- Stage 6
 * breadth sweep, 2026-07-25 (follow-up pass same day: CEvBuffersPool/CEvent now real,
 * see ev_buffers_pool.h/event.h -- promotes 8 more methods below to Tier A; SECOND
 * follow-up pass same day promotes 4 more of the remaining 16 -- OnRxPacket(),
 * OnReceiveSysExBuffer(), OnRxSexWhenInWAIT(), TransmitSexAnswer() -- see their own
 * comments below and the corrected `mState` field entry in the layout comment).
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
 * The remaining 12 methods are still Tier B this pass: real signatures (confirmed via
 * `nm -C`/symbols.csv), empty bodies -- see the SCOPE section above for the specific,
 * examined reason for each (not a blanket "didn't get to it").
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

	/* Remaining 12 real methods -- Tier B, real signatures, empty bodies. See
	 * header comment for the specific, examined reason each one is still deferred
	 * (packet-escape-byte framing protocol not yet decoded bit-exact, Error()'s
	 * heavy shared-tail duplication, or CMessage's own internals being out of
	 * scope).
	 */
	void Error(EErrNotifyMode mode);
	void EventToMessage(const CLinkedEvent *ev, unsigned char *out, unsigned char &outLen);
	void MessageToEvent(const unsigned char *data, unsigned char len, CLinkedEvent *ev);
	/* NOT declared C++ `virtual` -- see sysex_msg_task_base.h's own header comment
	 * for why (this project's raw-`mVtbl`-pointer convention, not real C++
	 * polymorphism). Real ground-truth vtable slot 0.
	 */
	int OnReceiveMessage(const CMessage &msg);
	void OnRxMsgWhenInIDLE(const unsigned char *data, unsigned char len, unsigned char x);
	void OnRxMsgWhenInSENT(const unsigned char *data, unsigned char len, unsigned char x);
	/* .text+0x0816ffd0, 59 bytes. Real return value is 1 (bool `true`) in ground
	 * truth, but the committed real signature (functions.csv/symbols.csv, no
	 * return type resolved) is `void` -- same "eax not part of the real
	 * contract" category as TXData()'s own observed-but-discarded eax. Real
	 * body: an unconditional log (Api+0x90, omitted, soft) then Error().
	 */
	void OnRxMsgWhenInWAIT(const unsigned char *data, unsigned char len, unsigned char x);
	void OnRxSexWhenInIDLE(ESexMsgType type, const unsigned char *data, unsigned char len,
	                        unsigned char x);
	void OnRxSexWhenInSENT(ESexMsgType type, const unsigned char *data, unsigned char len,
	                        unsigned char x);
	void PrepareMsgBuffer(unsigned char *buf, unsigned char &len, const unsigned char *data,
	                       unsigned char dataLen);
	void UnprepareBuffer(CLinkedEvent *ev, const unsigned char *data, unsigned char len,
	                      unsigned char x);

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
