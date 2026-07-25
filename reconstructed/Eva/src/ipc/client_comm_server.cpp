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
 * Every other method is Tier B (empty, real signature) -- see header comment for why.
 */

#include "client_comm_server.h"

unsigned char CClientCommServer::sm_byMaxRetryNumber = 0;

CClientCommServer::CClientCommServer(CSexServiceTask &owner, CSysExMsgTaskBase &client,
                                       unsigned char ecb, ECommMode mode, EService service,
                                       CSysExMsgOutLink *outLink)
	: mVtbl(0), mUnknown04(0), mUnknown08(0), mState0c(0), mState0d(0), mUnknown0e(0xff),
	  mModeService(static_cast<unsigned char>((unsigned char)service | (unsigned char)mode)),
	  mOutLink(outLink), mEcb(ecb), mTxBuf(0), mUnknown1c(0), mMaxSexPropLen1d(0),
	  mMaxSexPropLen1e(0), mEvTag(0), mEvBuf(0), mUnknown28(0), mOwner(&owner), mClient(&client)
{
	/* Real ctor also mallocs mTxBuf (size = CSexInputTask::sm_uiMaxSexPropLen's own
	 * low byte, a static this pass does not reconstruct) and allocates+locks an
	 * embedded CEvBuffersPool-backed event for mEvTag/mEvBuf -- NOT modeled, see
	 * header comment. mTxBuf/mEvTag/mEvBuf stay at their zero-initialized values.
	 */
}

CClientCommServer::~CClientCommServer() {}

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

void CClientCommServer::Error(EErrNotifyMode) {}
void CClientCommServer::EventToMessage(const CLinkedEvent *, unsigned char *, unsigned char &) {}
void CClientCommServer::MessageToEvent(const unsigned char *, unsigned char, CLinkedEvent *) {}
void CClientCommServer::OnProcessRetry(unsigned char) {}
int CClientCommServer::OnReceiveMessage(const CMessage &) { return 0; }
void CClientCommServer::OnReceiveSysExBuffer(const unsigned char *, unsigned char, unsigned char) {}
void CClientCommServer::OnRxMsgWhenInIDLE(const unsigned char *, unsigned char, unsigned char) {}
void CClientCommServer::OnRxMsgWhenInSENT(const unsigned char *, unsigned char, unsigned char) {}
void CClientCommServer::OnRxMsgWhenInWAIT(const unsigned char *, unsigned char, unsigned char) {}
void CClientCommServer::OnRxPacket(const unsigned char *, unsigned char, unsigned char) {}
void CClientCommServer::OnRxSexWhenInIDLE(ESexMsgType, const unsigned char *, unsigned char,
                                            unsigned char) {}
void CClientCommServer::OnRxSexWhenInSENT(ESexMsgType, const unsigned char *, unsigned char,
                                            unsigned char) {}
void CClientCommServer::OnRxSexWhenInWAIT(ESexMsgType, const unsigned char *, unsigned char,
                                            unsigned char) {}
void CClientCommServer::PrepareMsgBuffer(unsigned char *, unsigned char &, const unsigned char *,
                                           unsigned char) {}
void CClientCommServer::RetryTXPacket() {}
void CClientCommServer::SendMessageToClient() {}
void CClientCommServer::SendToSysExLink() {}
void CClientCommServer::TXData() {}
void CClientCommServer::TransmitSexAnswer(ESexMsgType, unsigned char) {}
void CClientCommServer::UnprepareBuffer(CLinkedEvent *, const unsigned char *, unsigned char,
                                          unsigned char) {}
