/*
 * client_comm_server.cpp  -  see include/client_comm_server.h.
 *
 * ComputeCRCByte()/CheckIncomingSexCRCByte() transcribed for real (Tier A) from
 * Decomp/EVA_Decomp/eva_export/functions/{ComputeCRCByte@0816f610,
 * CheckIncomingSexCRCByte@08173430}.c -- both are a running XOR checksum over a byte
 * range, GCC-unrolled 8-at-a-time (ComputeCRCByte) / 16-at-a-time-SIMD-then-8-at-a-time
 * (CheckIncomingSexCRCByte) in the real binary, collapsed to a single clean loop here
 * (same license as omega_ptr_array.cpp's own Duff's-device collapses -- verified
 * index-by-index against each real decompile while writing this). Natural 8-bit
 * `unsigned char` wraparound in the loop index reproduces the real byte-register
 * wraparound the compiled unrolled version relies on; no special-cased modulo needed.
 *
 * Ctor/dtor and 6 more leaf methods transcribed directly from `objdump -dr -M intel`
 * (Decomp/EVA_Decomp/Eva) in the same follow-up pass that made CEvBuffersPool/CEvent
 * real -- see header comment for the full accounting. Every real "soft assert" call
 * site (Api vtable slot 0x90/0x94) is omitted throughout, matching this file's own
 * already-established ComputeCRCByte()/CheckIncomingSexCRCByte() precedent: every one
 * of them logs a diagnostic and then falls straight through to the same code that
 * would have run anyway, confirmed by reading each branch target, not assumed.
 *
 * Second follow-up pass, same day: OnRxSexWhenInWAIT()/OnReceiveSysExBuffer()/
 * OnRxPacket()/TransmitSexAnswer() transcribed the same way (objdump -dr -M intel,
 * Decomp/EVA_Decomp/Eva). This pass renamed mUnknown04 -> mState (see header
 * comment for the 2-function confirmation this class's own top-level protocol
 * state, not a "retry in flight" flag as the original pass guessed). All 4 still
 * call into not-yet-reconstructed helpers (Error()/PrepareMsgBuffer()) for parts of
 * their own real control flow -- those calls are kept as real calls against the
 * still-Tier-B stub bodies declared below, same "real code, opaque helper" layering
 * already established elsewhere in this project (CConfigManager, etc).
 *
 * Remaining 12 methods are still Tier B (empty, real signature) -- see header comment
 * for the specific, examined reason each one is deferred.
 */

#include "client_comm_server.h"
#include "event.h"
#include "out_link.h"

#include <cstdlib>

namespace {

/* .rodata+0x8e7bcb0 / +0x8e7bcb4 in ground truth -- two small integer constants
 * OnReceiveSysExBuffer()/OnRxPacket()/TransmitSexAnswer() all read directly rather
 * than through any named symbol. Confirmed by reading the raw bytes at those
 * addresses in Decomp/EVA_Decomp/Eva (5 and 255 respectively). kSexPacketOverhead
 * is the fixed header size a sysex payload buffer carries before its own type byte
 * (address+ecb+len-shaped preamble, exact field breakdown not decoded); the second
 * is the same 0xff bound already used as kMaxSexPropLenPlaceholder above, reused
 * here under its own name since this call site doesn't go through mMaxSexPropLen1e.
 */
const unsigned char kSexPacketOverhead = 5;
const unsigned char kSexMaxLen = 0xff;

} // namespace

/* CSexServiceTask::TransmitSysEx() is a real ground-truth method belonging to a
 * genuinely out-of-scope class (see client_comm_server.h's own forward-declaration --
 * its only real dependency, CSexInputTask::TransmitSysEx(), is a 1374-byte CSexMatrix
 * routing engine, a disproportionate sub-effort of its own). NOT reconstructed here.
 * This file's own new Tier-A methods (SendToSysExLink()/RetryTXPacket()/TXData()/
 * OnProcessRetry()) are the first real callers of it anywhere in this project, which
 * means this project's own verify Makefile (every `verify/test_*.cpp` binary links
 * the FULL reconstructed object set, not just the objects it needs) would otherwise
 * fail to link every OTHER verify binary the moment this file's object is included.
 * This definition is a minimal linkage-only stub -- non-static so every verify binary
 * resolves it, with a test-observable call counter `test_client_comm_server.cpp` reads
 * via `extern` -- NOT an attempt to model real CSexServiceTask behavior, same spirit
 * as this project's established "every symbol on the unresolved list gets a real,
 * even if trivial, definition" convention.
 *
 * `COutLinkMono::OutMono()` itself is now a REAL reconstructed method (out_link.h/
 * .cpp, Eva CSysExMsgClientOutLink follow-up pass, 2026-07-25) -- its own stub
 * definition that used to live here is gone; `SendMessageToClient()` below now calls
 * straight through to the real implementation via `mOutLink`'s own real
 * `CSysExMsgOutLink -> COutLinkMono` inheritance chain.
 */
int g_ccsTestTransmitSysExCalls = 0;
unsigned char g_ccsTestLastTransmitEcb = 0;
int CSexServiceTask::TransmitSysEx(CLinkedEvent *, unsigned char ecb)
{
	g_ccsTestTransmitSysExCalls++;
	g_ccsTestLastTransmitEcb = ecb;
	return 0;
}

namespace {

/* CSexInputTask::sm_uiMaxSexPropLen's real low-byte value -- CSexInputTask itself is a
 * different, un-reconstructed class (out of scope this pass). Placeholder chosen at
 * the real ctor's own assert upper bound (0xff), same "real code, placeholder input"
 * category as CConfigManager::sm_ptSysExModuleInfo's own zero-initialized table
 * (config_info.cpp).
 */
const unsigned char kMaxSexPropLenPlaceholder = 0xff;

} // namespace

unsigned char CClientCommServer::sm_byMaxRetryNumber = 0;

CClientCommServer::CClientCommServer(CSexServiceTask &owner, CSysExMsgTaskBase &client,
                                       unsigned char ecb, ECommMode mode, EService service,
                                       CSysExMsgOutLink *outLink)
	: mVtbl(0), mState(0), mUnknown08(0), mState0c(0), mState0d(0), mUnknown0e(0xff),
	  mModeService(static_cast<unsigned char>((unsigned char)service | (unsigned char)mode)),
	  mOutLink(outLink), mEcb(ecb), mTxBuf(0), mUnknown1c(0),
	  mMaxSexPropLen1d(kMaxSexPropLenPlaceholder), mMaxSexPropLen1e(kMaxSexPropLenPlaceholder),
	  mEvTag(0x8000000a), mEvBuf(0), mUnknown28(0), mOwner(&owner), mClient(&client)
{
	/* Real ecb-range assert and mode/service-shape asserts (Api+0x94) omitted here,
	 * see file header comment.
	 */

	mTxBuf = malloc(mMaxSexPropLen1e);
	/* Real OOM soft-assert omitted; mTxBuf stays NULL on failure, matching ground
	 * truth (nothing reconstructed dereferences it before a real send path
	 * populates it).
	 */

	mEvBuf = CEvent::sm_oEvBuffersPool.Alloc(mMaxSexPropLen1e);
	if (mEvBuf) {
		/* Ground truth pokes the chunk header directly here rather than through
		 * a CEvBuffersPool method -- see ev_buffers_pool.h's own header comment
		 * on the raw, non-encapsulated chunk-header protocol. Clears the "just
		 * allocated" state bit so the Lock() call below takes its real
		 * refcount-based path rather than re-marking an already-fresh chunk.
		 */
		*(static_cast<unsigned char *>(mEvBuf) - 3) &= 0x7f;
	}
	mEvBuf = CEvent::sm_oEvBuffersPool.Lock(mEvBuf);
	/* mEvTag stays 0x8000000a throughout the ctor -- ground truth's own real
	 * branch on its sign (mirrored in TXData() as a genuine, non-collapsed
	 * conditional) is provably always-taken here since nothing changes mEvTag
	 * between its initialization above and this point; reproduced as
	 * unconditional code rather than a locally-dead if/else.
	 */
}

CClientCommServer::~CClientCommServer()
{
	free(mTxBuf);
	if (mEvTag < 0)
		CEvent::sm_oEvBuffersPool.Free(mEvBuf);
}

bool CClientCommServer::CheckIncomingSexCRCByte(const unsigned char *data, unsigned char len) const
{
	if (len < 2)
		return false;

	unsigned char crc = data[0];
	for (unsigned char i = 1; i < len; i = static_cast<unsigned char>(i + 1)) {
		crc ^= data[i];
	}
	return crc == 0;
}

unsigned char CClientCommServer::ComputeCRCByte(unsigned char startIndex) const
{
	unsigned char len = static_cast<unsigned char>(static_cast<unsigned int>(mEvTag) >> 16);
	const unsigned char *buf = static_cast<const unsigned char *>(mEvBuf);

	unsigned char crc = buf[startIndex];
	for (unsigned char i = static_cast<unsigned char>(startIndex + 1); i < len;
	     i = static_cast<unsigned char>(i + 1)) {
		crc ^= buf[i];
	}
	return crc;
}

/* --- Tier B: still empty, real signatures only -- see header comment. -------- */

void CClientCommServer::Error(EErrNotifyMode) {}
void CClientCommServer::EventToMessage(const CLinkedEvent *, unsigned char *, unsigned char &) {}
void CClientCommServer::MessageToEvent(const unsigned char *, unsigned char, CLinkedEvent *) {}
int CClientCommServer::OnReceiveMessage(const CMessage &) { return 0; }
void CClientCommServer::OnRxMsgWhenInIDLE(const unsigned char *, unsigned char, unsigned char) {}
void CClientCommServer::OnRxMsgWhenInSENT(const unsigned char *, unsigned char, unsigned char) {}
void CClientCommServer::OnRxSexWhenInIDLE(ESexMsgType, const unsigned char *, unsigned char,
                                            unsigned char) {}
void CClientCommServer::OnRxSexWhenInSENT(ESexMsgType, const unsigned char *, unsigned char,
                                            unsigned char) {}
void CClientCommServer::PrepareMsgBuffer(unsigned char *, unsigned char &, const unsigned char *,
                                           unsigned char) {}
void CClientCommServer::UnprepareBuffer(CLinkedEvent *, const unsigned char *, unsigned char,
                                          unsigned char) {}

/* --- Tier A: real bodies, this follow-up pass. -------------------------------- */

/* .text+0x0816ffd0, 59 bytes. Real ground truth: an unconditional trace log
 * (Api+0x90, "unexpected message while WAITing"-shaped, omitted -- soft, see file
 * header comment) then Error(eErrNotifyReserved); ground truth's own eax==1 on
 * return is not part of this method's committed (void) signature, see header.
 */
void CClientCommServer::OnRxMsgWhenInWAIT(const unsigned char *, unsigned char, unsigned char)
{
	Error(eErrNotifyReserved);
}

/* .text+0x08170010, 124 bytes. */
void CClientCommServer::OnProcessRetry(unsigned char expectedState)
{
	if (mState0c != expectedState || mState0d > 4) {
		/* Real trace log (Api+0x90) omitted, see file header comment. */
		Error(eErrNotifyReserved);
		return;
	}
	mState0d++;
	mState = 1;
	mOwner->TransmitSysEx(reinterpret_cast<CLinkedEvent *>(&mEvTag), mEcb);
}

/* .text+0x0816f3a0, 46 bytes. */
void CClientCommServer::RetryTXPacket()
{
	mState = 1;
	mOwner->TransmitSysEx(reinterpret_cast<CLinkedEvent *>(&mEvTag), mEcb);
}

/* .text+0x0816f370, 39 bytes. */
void CClientCommServer::SendToSysExLink()
{
	mOwner->TransmitSysEx(reinterpret_cast<CLinkedEvent *>(&mEvTag), mEcb);
}

/* .text+0x0816f2c0, 169 bytes. Real ground truth: a soft, non-enforcing assert on
 * `mModeService`'s own bit 0x02 (Api+0x94, omitted, see file header comment) gates
 * nothing -- both sides of that branch converge on the same
 * `COutLinkMono::OutMono()` call, confirmed by reading both targets. The real call
 * is a direct (non-virtual) call against `mOutLink`'s own `COutLinkMono` base
 * subobject -- ground truth's own call site never goes through any vtable here,
 * matching `CSysExMsgOutLink`'s real (single, non-diamond) inheritance from
 * `COutLinkMono` (out_link.h), so no cast is needed: `mOutLink->OutMono(...)` reaches
 * the exact same base-class method ground truth's own direct call reaches. Real
 * return value (an error code) is discarded here, matching ground truth's own
 * `void`-looking call site (nothing reads `eax` afterward).
 */
void CClientCommServer::SendMessageToClient()
{
	mOutLink->OutMono(mEcb, mTxBuf, mUnknown1c);
}

/* .text+0x0816f3d0, 562 bytes. */
void CClientCommServer::TXData()
{
	mOwner->TransmitSysEx(reinterpret_cast<CLinkedEvent *>(&mEvTag), mEcb);
	mState0c = 0;
	mUnknown1c = 0;

	/* Real, non-collapsed conditional -- unlike the ctor's own always-taken copy
	 * of this same sequence, mEvTag's sign here depends on whatever a
	 * not-yet-reconstructed caller (e.g. OnRxPacket()) may have left it as, so
	 * this branch is kept live rather than assumed.
	 */
	if (mEvTag < 0) {
		/* Real tag-byte soft-assert omitted, see file header comment. */
		if (mEvBuf)
			*(static_cast<unsigned char *>(mEvBuf) - 3) &= 0x7f;
		mEvTag = static_cast<int>((static_cast<unsigned>(mEvTag) & 0xff00u) | 0x8000000au);
		mEvBuf = CEvent::sm_oEvBuffersPool.Lock(mEvBuf);
	}

	mUnknown0e = 0xff;
	mState0d = 0;
	mState = 0;
	mUnknown08 = 0;
}

/* --- Tier A: real bodies, SECOND follow-up pass. ------------------------------ */

/* .text+0x08170090, 928 bytes. Real ground truth reacquires mEvBuf (clear lock bit
 * + CEvBuffersPool::Lock(), the ctor's own established idiom) both before writing
 * and again after transmitting, with a soft tag-byte assert at each reacquire point
 * (omitted, see file header comment). Reproduced directly rather than collapsed,
 * since -- unlike the ctor's provably-always-taken copy of this sequence -- this
 * class's own convention is "exclusive lock held only transiently during mutation",
 * and this method is the only one that both writes AND immediately re-releases
 * within a single call.
 */
void CClientCommServer::TransmitSexAnswer(ESexMsgType type, unsigned char x)
{
	if (mEvBuf)
		*(static_cast<unsigned char *>(mEvBuf) - 3) &= 0x7f;
	mEvTag = static_cast<int>((static_cast<unsigned>(mEvTag) & 0xff00u) | 0x8000000au);
	mEvBuf = CEvent::sm_oEvBuffersPool.Lock(mEvBuf);

	/* Real soft assert that `type` is 2 or 3 (Api+0x94) gates nothing -- the write
	 * below happens unconditionally regardless of `type`'s actual value, confirmed
	 * by reading every branch target.
	 */
	if (mEvBuf) {
		unsigned char *buf = static_cast<unsigned char *>(mEvBuf);
		buf[5] = static_cast<unsigned char>(type);
		buf[6] = x;
	}
	/* Real ground truth: `mEvTag |= (7 << 16)` -- a plain OR, not a
	 * clear-then-set, but safe/equivalent here since the tag's own length byte
	 * (bits 16-23) is always 0 at this point (just forced by the `& 0xff00`
	 * above), so OR-ing in 7 is the same as setting it.
	 */
	mEvTag = static_cast<int>(mEvTag | 0x00070000);

	mOwner->TransmitSysEx(reinterpret_cast<CLinkedEvent *>(&mEvTag), mEcb);

	if (mEvBuf)
		*(static_cast<unsigned char *>(mEvBuf) - 3) &= 0x7f;
	mEvTag = static_cast<int>((static_cast<unsigned>(mEvTag) & 0xff00u) | 0x8000000au);
	mEvBuf = CEvent::sm_oEvBuffersPool.Lock(mEvBuf);

	/* Real ground truth releases the lock bit again right before returning,
	 * leaving the buffer available for the next user (same convention as above).
	 */
	if (mEvBuf)
		*(static_cast<unsigned char *>(mEvBuf) - 3) &= 0x7f;
}

/* .text+0x08172860, 313 bytes. Real ground truth: a 5-entry jump table on `type`
 * read from `.rodata+0x8e7bc60` (confirmed by direct file read of
 * Decomp/EVA_Decomp/Eva: {0x8172930, 0x8172960, 0x8172978, 0x81728d0, 0x8172900} for
 * type 0..4, mapped below to the branch each address led to). Every non-zero-type
 * branch's own diagnostic log call (Api+0x94, a different format string per case)
 * is omitted (soft, see file header comment); the out-of-range (type>4) default
 * ALSO just logs (Api+0x94) and returns -- WITHOUT calling Error(), a genuinely
 * different tail from the type 1-4 cases, confirmed by reading the fallthrough.
 */
void CClientCommServer::OnRxSexWhenInWAIT(ESexMsgType type, const unsigned char *data,
                                            unsigned char len, unsigned char x)
{
	switch (static_cast<unsigned int>(type)) {
	case 0:
		/* Real ground truth is a genuine tail jmp back into OnRxPacket() with
		 * the same `this`/data/len/x -- reproduced here as a normal call since
		 * the observable behavior is identical (this method does nothing after
		 * the tail jmp in ground truth either).
		 */
		OnRxPacket(data, len, x);
		return;
	case 1:
	case 2:
	case 3:
		Error(static_cast<EErrNotifyMode>(0));
		return;
	case 4:
		Error(static_cast<EErrNotifyMode>(1));
		return;
	default:
		/* type > 4: soft log only, no Error() call -- see comment above. */
		return;
	}
}

/* .text+0x08173210, 543 bytes. Real ground truth: a soft NULL-data assert (logs and
 * continues anyway even when data==NULL -- same non-enforcing convention as
 * everywhere else in this file, reproduced faithfully: the length/ecb checks below
 * still run against a possibly-NULL `data` in that case, matching ground truth's own
 * behavior), a soft minimum-length assert (len <= kSexPacketOverhead+1, logs but
 * continues), a soft mEcb-mismatch assert (data[kSexPacketOverhead-1] should equal
 * mEcb, logs but continues), then a REAL enforcing bounds check (len >
 * kSexMaxLen -> silent early return, no log) before dispatching on `mState`.
 */
void CClientCommServer::OnReceiveSysExBuffer(const unsigned char *data, unsigned char len,
                                               unsigned char x)
{
	/* Soft asserts on data==NULL / len<=overhead+1 / data[overhead-1]!=mEcb
	 * omitted (Api+0x94, non-enforcing, see file header comment) -- ground truth
	 * continues into the same code below regardless.
	 */
	if (len > kSexMaxLen)
		return;

	unsigned char payloadLen = static_cast<unsigned char>(len - kSexPacketOverhead - 2);
	const unsigned char *payload = data + kSexPacketOverhead + 1;
	unsigned char type = data[kSexPacketOverhead];

	switch (mState) {
	case 0: /* IDLE */
		OnRxSexWhenInIDLE(static_cast<ESexMsgType>(type), payload, payloadLen, x);
		return;
	case 1: /* SENT */
		OnRxSexWhenInSENT(static_cast<ESexMsgType>(type), payload, payloadLen, x);
		return;
	case 2: /* WAIT */
		OnRxSexWhenInWAIT(static_cast<ESexMsgType>(type), payload, payloadLen, x);
		return;
	default:
		/* Real ground truth: soft log (Api+0x94) then return without
		 * dispatching -- an invalid mState value.
		 */
		return;
	}
}

/* .text+0x08172320, 1408 bytes. Real ground truth: 2 REAL gating checks
 * (mModeService bit 0x20 clear, or len==0, both call Error(0) and return), a soft
 * tag-byte assert against mUnknown0e (0xff = "none yet", logs+errors+returns on a
 * genuine mismatch -- this one IS enforcing, confirmed by reading the target: it
 * calls Error() and returns, unlike this file's usual "log and continue" asserts),
 * then either a "too short"/"bad checksum" resync path or a full decode-and-answer
 * success path.
 */
void CClientCommServer::OnRxPacket(const unsigned char *data, unsigned char len, unsigned char x)
{
	if (!(mModeService & 0x20)) {
		/* Real soft log (Api+0x90) then Error(0), returns. */
		Error(static_cast<EErrNotifyMode>(0));
		return;
	}
	if (len == 0) {
		Error(static_cast<EErrNotifyMode>(0));
		return;
	}

	unsigned char tag = data[0];

	if (mUnknown0e != 0xff && tag != mUnknown0e) {
		/* Real soft log (Api+0x90) then Error(0), returns -- a genuine
		 * enforcing check despite the "soft log" framing, confirmed by reading
		 * the branch target.
		 */
		Error(static_cast<EErrNotifyMode>(0));
		return;
	}

	if (len <= 2) {
		/* Too short to carry a checksum-verifiable payload -- resync. */
		mState = 2; /* WAIT */
		mUnknown0e = tag;
		TransmitSexAnswer(static_cast<ESexMsgType>(3), tag);
		return;
	}

	/* Running XOR checksum over data[1 .. len-2], seeded with (len-1) -- ground
	 * truth computes this with an SSE2-vectorized reduction + Duff's-device tail,
	 * collapsed here to a single clean loop (XOR is commutative/associative, so
	 * this is bit-identical regardless of reduction order, same license as
	 * ComputeCRCByte()'s own collapse).
	 */
	unsigned char crc = static_cast<unsigned char>(len - 1);
	for (unsigned char i = 1; i <= static_cast<unsigned char>(len - 2);
	     i = static_cast<unsigned char>(i + 1)) {
		crc ^= data[i];
	}

	if (crc != 0) {
		/* Checksum mismatch -- same resync path as the too-short case above. */
		mState = 2; /* WAIT */
		mUnknown0e = tag;
		TransmitSexAnswer(static_cast<ESexMsgType>(3), tag);
		return;
	}

	/* Checksum OK: decode the payload into mTxBuf (leading byte = x, the rest via
	 * the still-Tier-B PrepareMsgBuffer()), answer with type 2, then send.
	 */
	unsigned char newLen = static_cast<unsigned char>(len - 2);
	/* Real ground truth stores unconditionally, no mTxBuf NULL check -- matches
	 * the ctor's own comment that mTxBuf can stay NULL on malloc() OOM and
	 * nothing reconstructed guards against it before this, the first real
	 * dereference; reproduced faithfully (a real, inherited latent crash risk
	 * on OOM, not this reconstruction's own bug).
	 */
	static_cast<unsigned char *>(mTxBuf)[0] = x;

	/* mUnknown1c is reused here as PrepareMsgBuffer()'s in/out length parameter:
	 * IN = remaining capacity (mMaxSexPropLen1d - 1, since mTxBuf[0] is already
	 * spoken for), OUT = the actual encoded length PrepareMsgBuffer() wrote.
	 */
	mUnknown1c = static_cast<unsigned char>(mMaxSexPropLen1d - 1);
	PrepareMsgBuffer(static_cast<unsigned char *>(mTxBuf) + 1, mUnknown1c, data + 1, newLen);
	mUnknown1c = static_cast<unsigned char>(mUnknown1c + 1); /* account for mTxBuf[0] */

	mState = 0; /* IDLE */
	TransmitSexAnswer(static_cast<ESexMsgType>(2), tag);

	/* Real ground truth's own soft asserts on mModeService bit 0x2 and
	 * mOutLink!=NULL gate nothing -- both converge on the same
	 * COutLinkMono::OutMono() call regardless, confirmed by reading every
	 * branch target (same pattern as SendMessageToClient() itself). Reused
	 * here as a call to the already-real SendMessageToClient(), followed by
	 * the same mUnknown1c reset ground truth performs after.
	 */
	SendMessageToClient();
	mUnknown1c = 0;
}
