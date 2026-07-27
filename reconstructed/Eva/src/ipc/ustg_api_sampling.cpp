/*
 * ustg_api_sampling.cpp  -  see include/ustg_api_sampling.h.
 *
 * Transcribed field-by-field from `objdump -dr -M intel` (Decomp/EVA_Decomp/Eva,
 * .text+0x08e230d0/0x08e24870/0x08e24970/0x08e24a80).
 */

#include "ustg_api_sampling.h"

#include "ustg_user_api.h"

void *USTGAPISampling::SharedScratch()
{
	if (!USTGUserAPI::mFrontPanelStatusAddress)
		return 0;
	return static_cast<char *>(USTGUserAPI::mFrontPanelStatusAddress) + 0xd34;
}

namespace {

/* Common 24-byte header shape for all 4 primitives: len=0x18, subtype=1, type=1,
 * subcode=0xc. Payload dword roles vary per function -- see each caller below.
 */
struct SamplingSimpleMsg {
	unsigned short length; /* 0x18 */
	unsigned short subtype; /* 1 */
	unsigned int type;      /* 1 */
	unsigned int subcode;   /* 0xc */
	unsigned int payload0;
	unsigned int payload1;
	unsigned int payload2;
};

/* Same 8x-poll loop shape as ustg_api_wrappers.cpp's WaitForDumpSubcodeEcho(), but
 * this family's real ack condition is a fixed sentinel (response subcode == 9),
 * not an echo of the sent subcode -- kept as its own helper rather than reusing
 * WaitForDumpSubcodeEcho() to avoid conflating the two real, different conditions.
 */
bool PollForAck9(char *buf, unsigned bufSize)
{
	for (int i = 0; i < 8; ++i) {
		if (USTGUserAPI::ReadMessage(buf, bufSize) != 0)
			break;
	}
	SamplingSimpleMsg *msg = reinterpret_cast<SamplingSimpleMsg *>(buf);
	return msg->subcode == 9;
}

} // namespace

bool USTGAPISampling::SendSimpleMessage(int opcode, int arg2, int arg3)
{
	SamplingSimpleMsg msg;
	msg.length = 0x18;
	msg.subtype = 1;
	msg.type = 1;
	msg.subcode = 0xc;
	msg.payload0 = static_cast<unsigned int>(arg3);
	msg.payload1 = static_cast<unsigned int>(opcode);
	msg.payload2 = static_cast<unsigned int>(arg2);

	return USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg));
}

bool USTGAPISampling::ReceiveSimpleMessage(int opcode, unsigned long &out)
{
	SamplingSimpleMsg msg;
	msg.length = 0x18;
	msg.subtype = 1;
	msg.type = 1;
	msg.subcode = 0xc;
	msg.payload0 = 0;
	msg.payload1 = static_cast<unsigned int>(opcode);
	msg.payload2 = 0;

	if (!USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg))) {
		out = msg.payload2;
		return false;
	}

	bool ack = PollForAck9(reinterpret_cast<char *>(&msg), sizeof(msg));
	out = msg.payload2;
	return ack;
}

bool USTGAPISampling::ReceiveMessage(char *buf, int opcode, int arg3, int arg4)
{
	SamplingSimpleMsg *msg = reinterpret_cast<SamplingSimpleMsg *>(buf);
	msg->length = 0x18;
	msg->subtype = 1;
	msg->type = 1;
	msg->subcode = 0xc;
	msg->payload0 = static_cast<unsigned int>(arg3);
	msg->payload1 = static_cast<unsigned int>(opcode);
	msg->payload2 = static_cast<unsigned int>(arg4);

	if (!USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(buf)))
		return false;

	return PollForAck9(buf, sizeof(SamplingSimpleMsg));
}
