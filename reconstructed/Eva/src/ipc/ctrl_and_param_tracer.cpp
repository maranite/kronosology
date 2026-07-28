/*
 * ctrl_and_param_tracer.cpp  -  CCtrlAndParamTracer. See ctrl_and_param_tracer.h for
 * the full derivation.
 */

#include "ctrl_and_param_tracer.h"

/* ---- construction ----------------------------------------------------------------*/

CCtrlAndParamTracer::CCtrlAndParamTracer()
	: CControllerTracer(), mRpnParams(0, eRPN), mNrpnParams(0, eNRPN), mLastUpdated(0)
{
}

CCtrlAndParamTracer::CCtrlAndParamTracer(unsigned char channel)
	: CControllerTracer(channel), mRpnParams(channel, eRPN), mNrpnParams(channel, eNRPN),
	  mLastUpdated(0)
{
}

CCtrlAndParamTracer::CCtrlAndParamTracer(const CCtrlAndParamTracer &other)
	: CControllerTracer(other), mRpnParams(other.mRpnParams), mNrpnParams(other.mNrpnParams),
	  mLastUpdated(0)
{
	if (other.mLastUpdated == &other.mRpnParams)
		mLastUpdated = &mRpnParams;
	else if (other.mLastUpdated == &other.mNrpnParams)
		mLastUpdated = &mNrpnParams;
}

CCtrlAndParamTracer &CCtrlAndParamTracer::operator=(const CCtrlAndParamTracer &other)
{
	if (this == &other)
		return *this;

	CControllerTracer::operator=(other); /* compiler-generated: plain field/array copy */
	mRpnParams = other.mRpnParams;
	mNrpnParams = other.mNrpnParams;

	if (other.mLastUpdated == &other.mRpnParams)
		mLastUpdated = &mRpnParams;
	else if (other.mLastUpdated == &other.mNrpnParams)
		mLastUpdated = &mNrpnParams;
	else
		mLastUpdated = 0;

	return *this;
}

/* ---- controller/parameter-number dispatch -----------------------------------------*/

void CCtrlAndParamTracer::UpdateCtrl(unsigned char ctrlNum, unsigned char value)
{
	CControllerTracer::UpdateCtrl(ctrlNum, value);

	switch (ctrlNum) {
	case 6: /* Data Entry MSB */
		if (mLastUpdated)
			mLastUpdated->SetDataMSB(value);
		break;

	case 0x26: /* Data Entry LSB */
		if (mLastUpdated)
			mLastUpdated->SetDataLSB(value);
		break;

	case 0x60: /* Data Increment */
		if (mLastUpdated)
			mLastUpdated->DataInc();
		break;

	case 0x61: /* Data Decrement */
		if (mLastUpdated)
			mLastUpdated->DataDec();
		break;

	case 0x62: { /* NRPN Parameter-Number LSB */
		unsigned char oldMSB = mNrpnParams.mCurAddr.b0;
		mNrpnParams.mCurAddr.b1 = value;
		if (oldMSB < 0x80 && value < 0x80 && !(oldMSB == 0x7f && value == 0x7f))
			mLastUpdated = &mNrpnParams;
		break;
	}
	case 0x63: { /* NRPN Parameter-Number MSB */
		unsigned char oldLSB = mNrpnParams.mCurAddr.b1;
		mNrpnParams.mCurAddr.b0 = value;
		if (value < 0x80 && oldLSB < 0x80 && !(value == 0x7f && oldLSB == 0x7f))
			mLastUpdated = &mNrpnParams;
		break;
	}
	case 0x64: { /* RPN Parameter-Number LSB */
		unsigned char oldMSB = mRpnParams.mCurAddr.b0;
		mRpnParams.mCurAddr.b1 = value;
		if (oldMSB < 0x80 && value < 0x80 && !(oldMSB == 0x7f && value == 0x7f))
			mLastUpdated = &mRpnParams;
		break;
	}
	case 0x65: { /* RPN Parameter-Number MSB */
		unsigned char oldLSB = mRpnParams.mCurAddr.b1;
		mRpnParams.mCurAddr.b0 = value;
		if (value < 0x80 && oldLSB < 0x80 && !(value == 0x7f && oldLSB == 0x7f))
			mLastUpdated = &mRpnParams;
		break;
	}
	default:
		break;
	}
}

void CCtrlAndParamTracer::Reset()
{
	CControllerTracer::Reset();
	mRpnParams.Reset();
	mNrpnParams.Reset();
	mLastUpdated = 0;
}

void CCtrlAndParamTracer::InitAfterDefaultCtor(unsigned char channel)
{
	CControllerTracer::InitAfterDefaultCtor(channel);
	mRpnParams.InitAfterDefaultCtor(channel, eRPN);
	mNrpnParams.InitAfterDefaultCtor(channel, eNRPN);
}

/* ---- parameter-changed append ------------------------------------------------------*/

int CCtrlAndParamTracer::AppendAllParams(CLinkedEvent *&cursor) const
{
	/* See file header: real order depends on mLastUpdated but the resulting SET of
	 * messages is identical either way. */
	int total = mRpnParams.AppendAllParams(cursor);
	total += mNrpnParams.AppendAllParams(cursor);
	return total;
}

int CCtrlAndParamTracer::AppendParams(CLinkedEvent *&cursor, const SBytePair *rpnAddrList,
                                       const SBytePair *nrpnAddrList) const
{
	int total = mRpnParams.AppendParamsDontCareAddr(cursor, rpnAddrList);
	total += mNrpnParams.AppendParamsDontCareAddr(cursor, nrpnAddrList);
	return total;
}
