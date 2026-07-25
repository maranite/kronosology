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
 * Remaining 16 methods are still Tier B (empty, real signature) -- see header comment
 * for why.
 */

#include "client_comm_server.h"
#include "event.h"

#include <cstdlib>

/* CSexServiceTask::TransmitSysEx()/COutLinkMono::OutMono() are real ground-truth
 * methods belonging to two entirely separate, out-of-scope classes (see
 * client_comm_server.h's own forward-declarations) -- NOT reconstructed here. This
 * file's own new Tier-A methods below (SendToSysExLink()/RetryTXPacket()/TXData()/
 * OnProcessRetry()/SendMessageToClient()) are the first real callers of either
 * anywhere in this project, which means this project's own verify Makefile (every
 * `verify/test_*.cpp` binary links the FULL reconstructed object set, not just the
 * objects it needs) would otherwise fail to link every OTHER verify binary the
 * moment this file's object is included. These two definitions are minimal
 * linkage-only stubs -- non-static so every verify binary resolves them, with a
 * pair of test-observable call counters `test_client_comm_server.cpp` reads via
 * `extern` -- NOT an attempt to model real CSexServiceTask/COutLinkMono behavior,
 * same spirit as this project's established "every symbol on the unresolved list
 * gets a real, even if trivial, definition" convention, just applied here to two
 * external callees' single real method each instead of a whole not-yet-reached
 * class.
 */
int g_ccsTestTransmitSysExCalls = 0;
unsigned char g_ccsTestLastTransmitEcb = 0;
int CSexServiceTask::TransmitSysEx(CLinkedEvent *, unsigned char ecb)
{
	g_ccsTestTransmitSysExCalls++;
	g_ccsTestLastTransmitEcb = ecb;
	return 0;
}

int g_ccsTestOutMonoCalls = 0;
int COutLinkMono::OutMono(unsigned short, void *, unsigned short)
{
	g_ccsTestOutMonoCalls++;
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
	: mVtbl(0), mUnknown04(0), mUnknown08(0), mState0c(0), mState0d(0), mUnknown0e(0xff),
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
void CClientCommServer::OnReceiveSysExBuffer(const unsigned char *, unsigned char, unsigned char) {}
void CClientCommServer::OnRxMsgWhenInIDLE(const unsigned char *, unsigned char, unsigned char) {}
void CClientCommServer::OnRxMsgWhenInSENT(const unsigned char *, unsigned char, unsigned char) {}
void CClientCommServer::OnRxPacket(const unsigned char *, unsigned char, unsigned char) {}
void CClientCommServer::OnRxSexWhenInIDLE(ESexMsgType, const unsigned char *, unsigned char,
                                            unsigned char) {}
void CClientCommServer::OnRxSexWhenInSENT(ESexMsgType, const unsigned char *, unsigned char,
                                            unsigned char) {}
void CClientCommServer::OnRxSexWhenInWAIT(ESexMsgType, const unsigned char *, unsigned char,
                                            unsigned char) {}
void CClientCommServer::PrepareMsgBuffer(unsigned char *, unsigned char &, const unsigned char *,
                                           unsigned char) {}
void CClientCommServer::TransmitSexAnswer(ESexMsgType, unsigned char) {}
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
	mUnknown04 = 1;
	mOwner->TransmitSysEx(reinterpret_cast<CLinkedEvent *>(&mEvTag), mEcb);
}

/* .text+0x0816f3a0, 46 bytes. */
void CClientCommServer::RetryTXPacket()
{
	mUnknown04 = 1;
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
 * is a direct (non-virtual) call against `mOutLink` reinterpreted as a
 * `COutLinkMono*` -- ground truth's own call site never goes through any vtable
 * here, regardless of `mOutLink`'s own declared static type
 * (`CSysExMsgOutLink*`, established by an earlier pass); `COutLinkMono` itself is
 * out of scope (forward-declared with just this one real method, mangled
 * `_ZN12COutLinkMono7OutMonoEtPvt`).
 */
void CClientCommServer::SendMessageToClient()
{
	reinterpret_cast<COutLinkMono *>(mOutLink)->OutMono(mEcb, mTxBuf, mUnknown1c);
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
	mUnknown04 = 0;
	mUnknown08 = 0;
}
