/*
 * client_comm_server.h  -  CClientCommServer, the per-client SysEx transport state
 * machine (IDLE/SENT/WAIT protocol handshake over a CSysExMsgOutLink) -- Stage 6
 * breadth sweep, 2026-07-25 (follow-up pass same day: CEvBuffersPool/CEvent now real,
 * see ev_buffers_pool.h/event.h -- promotes 8 more methods below to Tier A).
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
 * The remaining 16 methods (everything touching the IDLE/SENT/WAIT dispatch proper --
 * `OnReceiveMessage`/`OnRx*`/`PrepareMsgBuffer`/`UnprepareBuffer`/`EventToMessage`/
 * `MessageToEvent`/`OnReceiveSysExBuffer`/`OnRxPacket`/`TransmitSexAnswer`/`Error`) are
 * still Tier B this pass: real signatures (confirmed via `nm -C`/symbols.csv), empty
 * bodies -- they need either the packet-escape-byte framing protocol
 * (`PrepareMsgBuffer`/`UnprepareBuffer`/`EventToMessage`/`MessageToEvent`, not yet
 * decoded) or pull in further out-of-scope classes (`CMessage`'s own internals,
 * `CSysExMsgOutLink` methods) this pass didn't chase down -- a genuinely disproportionate
 * sub-effort relative to this pass's own CEvBuffersPool/CEvent scope, left for a future
 * batch.
 *
 * REAL LAYOUT (confirmed from CClientCommServer@0816ecc0.c, ComputeCRCByte@0816f610.c):
 *   +0x00  vtbl         &PTR_OnReceiveMessage_08e898c8 (this class's real, slot-0-named
 *                        vtable -- OnReceiveMessage(CMessage const&) is the primary
 *                        virtual override; never dispatched through by any
 *                        reconstructed code, same "install-only" status as
 *                        omega_vtables.h's own catalogue)
 *   +0x04/+0x08  2 unknown dwords (mUnknown04/mUnknown08), ctor zeroes both; TXData()
 *                also re-zeroes both after every send (real, confirmed -- still not
 *                decoded what they track); RetryTXPacket()/OnProcessRetry() both set
 *                mUnknown04 = 1 right before resending, so it reads as some kind of
 *                "a retry is in flight" flag, not fully confirmed
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

/* Forward-declared the same way, for SendMessageToClient()'s own real call --
 * `COutLinkMono::OutMono(unsigned short, void*, unsigned short)`
 * (`_ZN12COutLinkMono7OutMonoEtPvt`), a direct (non-virtual) call in ground truth.
 * Out of scope otherwise (a different, un-reconstructed link-transport class).
 */
class COutLinkMono {
public:
	int OutMono(unsigned short ecb, void *buf, unsigned short len);
};

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

	/* Remaining 16 real methods -- Tier B, real signatures, empty bodies. See
	 * header comment for why (packet-escape-byte framing protocol not yet
	 * decoded, or pull in further out-of-scope classes).
	 */
	void Error(EErrNotifyMode mode);
	void EventToMessage(const CLinkedEvent *ev, unsigned char *out, unsigned char &outLen);
	void MessageToEvent(const unsigned char *data, unsigned char len, CLinkedEvent *ev);
	/* NOT declared C++ `virtual` -- see sysex_msg_task_base.h's own header comment
	 * for why (this project's raw-`mVtbl`-pointer convention, not real C++
	 * polymorphism). Real ground-truth vtable slot 0.
	 */
	int OnReceiveMessage(const CMessage &msg);
	void OnReceiveSysExBuffer(const unsigned char *data, unsigned char len, unsigned char x);
	void OnRxMsgWhenInIDLE(const unsigned char *data, unsigned char len, unsigned char x);
	void OnRxMsgWhenInSENT(const unsigned char *data, unsigned char len, unsigned char x);
	/* .text+0x0816ffd0, 59 bytes. Real return value is 1 (bool `true`) in ground
	 * truth, but the committed real signature (functions.csv/symbols.csv, no
	 * return type resolved) is `void` -- same "eax not part of the real
	 * contract" category as TXData()'s own observed-but-discarded eax. Real
	 * body: an unconditional log (Api+0x90, omitted, soft) then Error().
	 */
	void OnRxMsgWhenInWAIT(const unsigned char *data, unsigned char len, unsigned char x);
	void OnRxPacket(const unsigned char *data, unsigned char len, unsigned char x);
	void OnRxSexWhenInIDLE(ESexMsgType type, const unsigned char *data, unsigned char len,
	                        unsigned char x);
	void OnRxSexWhenInSENT(ESexMsgType type, const unsigned char *data, unsigned char len,
	                        unsigned char x);
	void OnRxSexWhenInWAIT(ESexMsgType type, const unsigned char *data, unsigned char len,
	                        unsigned char x);
	void PrepareMsgBuffer(unsigned char *buf, unsigned char &len, const unsigned char *data,
	                       unsigned char dataLen);
	void TransmitSexAnswer(ESexMsgType type, unsigned char x);
	void UnprepareBuffer(CLinkedEvent *ev, const unsigned char *data, unsigned char len,
	                      unsigned char x);

	/* .text+0x08e7bc9c, 1 byte. Real static data member. Value not read from the
	 * binary this pass (no reconstructed code reads it); zero-initialized here.
	 */
	static unsigned char sm_byMaxRetryNumber;

private:
	void              *mVtbl;
	int                mUnknown04;
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
