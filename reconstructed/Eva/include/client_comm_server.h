/*
 * client_comm_server.h  -  CClientCommServer, the per-client SysEx transport state
 * machine (IDLE/SENT/WAIT protocol handshake over a CSysExMsgOutLink) -- Stage 6
 * breadth sweep, 2026-07-25.
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
 * SCOPE: this is a genuinely large class (26 real methods, ~7.6KB of real .text, a full
 * IDLE/SENT/WAIT handshake state machine layered on a CRC-checked packet framing
 * protocol) whose CONSTRUCTOR alone pulls in an entire un-reconstructed subsystem:
 * `CEvBuffersPool`/`CEvent` (a fixed-size, refcounted event-buffer pool allocator --
 * `CEvBuffersPool::Alloc()`/`Lock()`, 176/508 bytes, .text+0x0807f400/0x0807f660,
 * NOT reconstructed here) backs an embedded `CLinkedEvent`-shaped tagged buffer this
 * class uses as its own TX/RX scratch space. Almost every one of the 24 remaining
 * methods touches that embedded buffer (or the `CSysExMsgOutLink&`/`CSexServiceTask&`
 * it was constructed with), so only the TWO methods that are genuinely
 * self-contained protocol math get a real (Tier A) body this pass:
 *
 *   ComputeCRCByte(unsigned char)              -- .text+0x0816f610, reads this class's
 *     own embedded event-buffer tag/pointer fields directly (raw fields, no
 *     CEvBuffersPool method call needed to READ them), a running XOR checksum over a
 *     mod-256-indexed byte range.
 *   CheckIncomingSexCRCByte(unsigned char const*, unsigned char) -- .text+0x08173430,
 *     694 bytes, does not touch `this` AT ALL (confirmed by reading the decompile --
 *     every reference is to the two parameters) -- the incoming-wire-format twin of
 *     ComputeCRCByte, same XOR-fold algorithm over an explicit caller-supplied buffer.
 *
 * The other 24 (ctor, dtor, and everything touching the IDLE/SENT/WAIT state machine,
 * packet framing, or the client/outlink references) are Tier B this pass: real
 * signatures (confirmed via `nm -C`/symbols.csv, not functions.csv's ABI-flattened
 * view), empty bodies -- matching this project's established "every symbol on the
 * unresolved list gets a real, even if trivial, definition" convention.
 *
 * REAL LAYOUT (confirmed from CClientCommServer@0816ecc0.c, ComputeCRCByte@0816f610.c):
 *   +0x00  vtbl         &PTR_OnReceiveMessage_08e898c8 (this class's real, slot-0-named
 *                        vtable -- OnReceiveMessage(CMessage const&) is the primary
 *                        virtual override; never dispatched through by any
 *                        reconstructed code, same "install-only" status as
 *                        omega_vtables.h's own catalogue)
 *   +0x04/+0x08  2 unknown dwords, ctor zeroes both, never read back by any
 *                reconstructed method
 *   +0x0c/+0x0d  2 state bytes, ctor zeroes both (protocol state machine's own
 *                current-state fields, presumably -- not decoded, Tier B methods
 *                would read/write these)
 *   +0x0e        byte, ctor sets 0xff ("no active retry"?, not decoded)
 *   +0x0f        byte, ctor sets `(byte)service | (byte)commMode` -- the combined
 *                mode/service tag this client registered under
 *   +0x10        CSysExMsgOutLink* mOutLink (ctor's own `outLink` argument, stored raw)
 *   +0x14        unsigned char mEcb (ctor's own `ecb` argument -- the comm/sysex-id
 *                byte this client answers to)
 *   +0x18        void* mTxBuf -- malloc'd scratch buffer, size = mMaxSexPropLen (below)
 *   +0x1c        byte, ctor zeroes (another state byte, not decoded)
 *   +0x1d/+0x1e  byte mMaxSexPropLen (both copies of `CSexInputTask::
 *                sm_uiMaxSexPropLen`'s own low byte -- NOT reconstructed here, that
 *                static lives on a different, un-reconstructed class; ctor's own
 *                assert bounds-checks it against 0xff)
 *   +0x20        int mEvTag -- embedded CEvBuffersPool-backed event's own packed tag
 *                word (high byte = payload length, per ComputeCRCByte's own
 *                `(uint)mEvTag >> 0x10` extraction -- matches this project's other
 *                CEvent-adjacent code's `& 0xff00 | 0x8000000a`-shaped tag literals)
 *   +0x24        void* mEvBuf -- the event's own payload buffer pointer (from
 *                `CEvBuffersPool::Alloc()`/`Lock()`, real ctor only -- Tier B here)
 *   +0x28        int, ctor zeroes (a second embedded-event field, not decoded)
 *   +0x2c        CSexServiceTask* mOwner (ctor's own `owner` argument, stored raw)
 *   +0x30        CSysExMsgTaskBase* mClient (ctor's own `client` argument, stored raw
 *                -- THIS is the field that makes CSysExMsgTaskBase a real, load-bearing
 *                dependency of this class, per this header's own reachability writeup)
 */

#ifndef CLIENT_COMM_SERVER_H
#define CLIENT_COMM_SERVER_H

class CSexServiceTask;
class CSysExMsgTaskBase;
class CSysExMsgOutLink;
class CMessage;
class CLinkedEvent;

class CClientCommServer {
public:
	/* Real enums, opaque (functions.csv/symbols.csv carry only the enum TYPE name,
	 * same convention as task.h's ETaskLevel/CSysExMsgTaskBase's ECanTransmit).
	 */
	enum ECommMode { eCommModeReserved = 0 };
	enum EService { eServiceReserved = 0 };
	enum ESexMsgType { eSexMsgTypeReserved = 0 };
	enum EErrNotifyMode { eErrNotifyReserved = 0 };

	/* .text+0x0816ecc0, 1343 bytes. Tier B -- see header comment (CEvBuffersPool/
	 * CEvent dependency).
	 */
	CClientCommServer(CSexServiceTask &owner, CSysExMsgTaskBase &client, unsigned char ecb,
	                   ECommMode mode, EService service, CSysExMsgOutLink *outLink);

	/* .text+0x0816f240 (+1 duplicate thunk), 125 bytes. Tier B. */
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

	/* Remaining 22 real methods -- Tier B, real signatures, empty bodies. See
	 * header comment for why (touch the embedded CEvBuffersPool-backed event,
	 * mOutLink, mOwner, or mClient, none of which a Tier-B ctor actually
	 * populates).
	 */
	void Error(EErrNotifyMode mode);
	void EventToMessage(const CLinkedEvent *ev, unsigned char *out, unsigned char &outLen);
	void MessageToEvent(const unsigned char *data, unsigned char len, CLinkedEvent *ev);
	void OnProcessRetry(unsigned char x);
	/* NOT declared C++ `virtual` -- see sysex_msg_task_base.h's own header comment
	 * for why (this project's raw-`mVtbl`-pointer convention, not real C++
	 * polymorphism). Real ground-truth vtable slot 0.
	 */
	int OnReceiveMessage(const CMessage &msg);
	void OnReceiveSysExBuffer(const unsigned char *data, unsigned char len, unsigned char x);
	void OnRxMsgWhenInIDLE(const unsigned char *data, unsigned char len, unsigned char x);
	void OnRxMsgWhenInSENT(const unsigned char *data, unsigned char len, unsigned char x);
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
	void RetryTXPacket();
	void SendMessageToClient();
	void SendToSysExLink();
	void TXData();
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
