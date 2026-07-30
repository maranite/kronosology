/*
 * controller_tracer.cpp  -  CControllerTracer. See controller_tracer.h for the full
 * derivation.
 */

#include "controller_tracer.h"
#include <cstring>

/* .rodata+0x8e7a160, 128 bytes, real transcribed table. Indexed by MIDI CC#;
 * 0xff = "no default value defined for this CC". */
static const unsigned char kDefaultCCTable[128] = {
	0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x64, 0x40, 0xff, 0x40, 0x7f, 0xff, 0xff, 0xff, 0xff,
	0x40, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0x7f, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

/* global.constructors.keyed.to.CControllerTracer@08093030.c, 29 bytes -- round 69.
 * NOT this class's own construction: ground truth's real body sets two UNRELATED
 * file-scope 2-byte globals ({0xff,0xff} and {0x40,0x00}) that merely happen to
 * share the SAME source-level names ("kInvalidBytePair"/"kPitchBendDefault") as
 * this project's own `kInvalidBytePair` (param_tracer.h, a DIFFERENT real .bss
 * symbol at 0x0930a390, confirmed {0,0} and already used by controller_tracer.h's
 * own field-init logic -- see that header's own note). Same coincidental-name-
 * collision-via-internal-linkage-statics pattern already documented and left
 * unmodeled for the RTRouterApiInstance-TU pair, mains.cpp. Nothing in this
 * reconstruction reads either of THIS pair -- not modeled, matching that same
 * precedent.
 */
__attribute__((constructor))
static void ConstructControllerTracerFileStatics()
{
}

/* ---- construction --------------------------------------------------------------- */

CControllerTracer::CControllerTracer() : mChannel(0)
{
	Reset();
}

CControllerTracer::CControllerTracer(unsigned char channel) : mChannel(channel)
{
	Reset();
}

CControllerTracer::~CControllerTracer()
{
}

void CControllerTracer::Reset()
{
	memset(mCtrl, 0xff, sizeof(mCtrl));
	mChnPressure = 0xff;
	mProgramNumber = 0xff;
	/* Real: two raw 16-bit copies of the same kInvalidBytePair global == {0,0} --
	 * see file header. NOT the 0xff sentinel. */
	mPitchBendLSB = 0;
	mPitchBendMSB = 0;
	mBankMSB = 0;
	mBankLSB = 0;
}

void CControllerTracer::InitAfterDefaultCtor(unsigned char channel)
{
	mChannel = channel;
}

/* ---- controller table maintenance ------------------------------------------------*/

void CControllerTracer::EraseCtrl(unsigned char ctrlNum)
{
	if (ctrlNum < 0x80)
		mCtrl[ctrlNum] = 0xff;
}

void CControllerTracer::EraseCtrls(const unsigned char *ctrls)
{
	if (!ctrls)
		return; /* real: soft Api+0x94 assert, then the same no-op */

	for (int i = 0; ctrls[i] < 0x80; ++i)
		mCtrl[ctrls[i]] = 0xff;
}

void CControllerTracer::SetDefCtrls(const unsigned char *ctrls)
{
	if (!ctrls)
		return;

	for (int i = 0; ctrls[i] < 0x80; ++i) {
		unsigned char def = kDefaultCCTable[ctrls[i]];
		if (def != 0xff)
			UpdateCtrl(ctrls[i], def); /* virtual dispatch */
	}
}

void CControllerTracer::UpdateCtrl(unsigned char ctrlNum, unsigned char value)
{
	mCtrl[ctrlNum] = value;
}

/* ---- MIDI CC message emission -----------------------------------------------------*/

namespace {

/* Real ground truth: `newNode = GetNewEvent(); cursor->SetNext(newNode);
 * newNode->SetTag(tagWord); cursor = newNode;` -- grows the chain FORWARD from
 * whatever node `cursor` currently designates (a tail/insertion-point cursor, NOT a
 * list head -- see file header). Caller is responsible for the `cursor != 0` guard;
 * every real Append* method checks it up front and returns 0 rather than calling
 * this helper on a null cursor. */
void AppendCCEvent(CLinkedEvent *&cursor, unsigned tagWord)
{
	CLinkedEvent *node = CLinkedEvent::sm_oEventsPool.GetNewEvent();
	cursor->SetNext(node);
	node->SetTag((int)tagWord);
	cursor = node;
}

} // namespace

int CControllerTracer::AppendChnPressure(CLinkedEvent *&cursor) const
{
	if (cursor == 0)
		return 0;
	if (mChnPressure == 0xff)
		return 0;

	unsigned tag = 0x5u | ((unsigned)mChannel << 8) | ((unsigned)mChnPressure << 16);
	AppendCCEvent(cursor, tag);
	return mChnPressure; /* see file header -- real return value, not a boolean */
}

int CControllerTracer::AppendPitchBend(CLinkedEvent *&cursor) const
{
	if (cursor == 0)
		return 0;
	if (mPitchBendLSB == 0xff)
		return 0;

	unsigned tag = 0x6u | ((unsigned)mChannel << 8) | ((unsigned)mPitchBendMSB << 16)
	             | ((unsigned)(mPitchBendLSB & 0x7f) << 24);
	AppendCCEvent(cursor, tag);
	return 1;
}

int CControllerTracer::AppendFullProgram(CLinkedEvent *&cursor) const
{
	if (cursor == 0)
		return 0;

	int count = 0;
	if (mBankMSB != 0xff) {
		unsigned tag = 0x3u | ((unsigned)mChannel << 8) | ((unsigned)(mBankMSB & 0x7f) << 24);
		AppendCCEvent(cursor, tag);
		++count;
	}
	if (mBankLSB != 0xff) {
		unsigned tag = 0x3u | ((unsigned)mChannel << 8) | (0x20u << 16)
		             | ((unsigned)(mBankLSB & 0x7f) << 24);
		AppendCCEvent(cursor, tag);
		++count;
	}
	if (mProgramNumber != 0xff) {
		unsigned tag = 0x4u | ((unsigned)mChannel << 8) | ((unsigned)mProgramNumber << 16);
		AppendCCEvent(cursor, tag);
		++count;
	}
	return count;
}

int CControllerTracer::AppendCtrl(CLinkedEvent *&cursor, unsigned char ctrlNum) const
{
	if (cursor == 0)
		return 0;

	unsigned char v = mCtrl[ctrlNum];
	if (v == 0xff)
		return 0;

	unsigned tag = 0x3u | ((unsigned)mChannel << 8) | ((unsigned)ctrlNum << 16)
	             | ((unsigned)(v & 0x7f) << 24);
	AppendCCEvent(cursor, tag);
	return 1;
}

int CControllerTracer::AppendCtrls(CLinkedEvent *&cursor, const unsigned char *ctrls) const
{
	if (!ctrls)
		return 0;
	if (cursor == 0)
		return 0;

	int count = 0;
	for (int i = 0; ctrls[i] < 0x80; ++i) {
		unsigned char v = mCtrl[ctrls[i]];
		if (v == 0xff)
			continue;
		unsigned tag = 0x3u | ((unsigned)mChannel << 8) | ((unsigned)ctrls[i] << 16)
		             | ((unsigned)(v & 0x7f) << 24);
		AppendCCEvent(cursor, tag);
		++count;
	}
	return count;
}

int CControllerTracer::AppendDefaultChnPressure(CLinkedEvent *&cursor) const
{
	if (cursor == 0)
		return 0;
	if (mChnPressure == 0xff)
		return 0;

	unsigned tag = 0x5u | ((unsigned)mChannel << 8); /* implicit value byte = 0 */
	AppendCCEvent(cursor, tag);
	return 1;
}

int CControllerTracer::AppendDefaultPitchBend(CLinkedEvent *&cursor) const
{
	if (cursor == 0)
		return 0;
	if (mPitchBendLSB == 0xff)
		return 0;

	unsigned tag = 0x6u | ((unsigned)mChannel << 8) | (0x40u << 24); /* center: LSB=0,MSB=0x40 */
	AppendCCEvent(cursor, tag);
	return 1;
}

int CControllerTracer::AppendDefaultCtrl(CLinkedEvent *&cursor, unsigned char ctrlNum) const
{
	if (cursor == 0)
		return 0;

	unsigned char v = mCtrl[ctrlNum];
	if (v == 0xff) {
		v = kDefaultCCTable[ctrlNum];
		if (v == 0xff)
			return 0;
	}

	unsigned tag = 0x3u | ((unsigned)mChannel << 8) | ((unsigned)ctrlNum << 16)
	             | ((unsigned)(v & 0x7f) << 24);
	AppendCCEvent(cursor, tag);
	return 1;
}

int CControllerTracer::AppendDefaultCtrls(CLinkedEvent *&cursor, const unsigned char *ctrls) const
{
	if (!ctrls)
		return 0;
	if (cursor == 0)
		return 0;

	int count = 0;
	for (int i = 0; ctrls[i] < 0x80; ++i) {
		if (mCtrl[ctrls[i]] == 0xff)
			continue; /* only for currently-tracked ctrls -- see file header */
		unsigned char v = kDefaultCCTable[ctrls[i]];
		if (v == 0xff)
			continue;
		unsigned tag = 0x3u | ((unsigned)mChannel << 8) | ((unsigned)ctrls[i] << 16)
		             | ((unsigned)(v & 0x7f) << 24);
		AppendCCEvent(cursor, tag);
		++count;
	}
	return count;
}
