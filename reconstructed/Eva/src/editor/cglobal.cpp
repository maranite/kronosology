/* cglobal.cpp - see include/cglobal.h for provenance. Every method here is a plain,
 * ground-truth-confirmed forward to the embedded CSpecialFuncCCMap sub-object.
 */

#include "cglobal.h"

void CGlobal::GetMappingName(ESpecialCCMapFunction fn, const char *&out) const
{
	mSpecialFuncCCMap.GetMappingName(fn, out);
}

#define DEFINE_FORWARD_SCALAR(Name)                                                    \
	void CGlobal::Get##Name##MIDIChannel(unsigned char *out) const                     \
	{                                                                                    \
		mSpecialFuncCCMap.Get##Name##MIDIChannel(out);                                 \
	}                                                                                    \
	void CGlobal::Set##Name##MIDIChannel(const char *in)                               \
	{                                                                                    \
		mSpecialFuncCCMap.Set##Name##MIDIChannel(in);                                  \
	}                                                                                    \
	void CGlobal::Get##Name##CCAssign(char *out) const                                 \
	{                                                                                    \
		mSpecialFuncCCMap.Get##Name##CCAssign(out);                                    \
	}                                                                                    \
	void CGlobal::Set##Name##CCAssign(const char *in)                                  \
	{                                                                                    \
		mSpecialFuncCCMap.Set##Name##CCAssign(in);                                     \
	}

#define DEFINE_FORWARD_ARRAY(Name)                                                      \
	void CGlobal::Get##Name##MIDIChannel(unsigned idx, unsigned char *out) const       \
	{                                                                                    \
		mSpecialFuncCCMap.Get##Name##MIDIChannel(idx, out);                            \
	}                                                                                    \
	void CGlobal::Set##Name##MIDIChannel(unsigned idx, const unsigned char *in)        \
	{                                                                                    \
		mSpecialFuncCCMap.Set##Name##MIDIChannel(idx, in);                             \
	}                                                                                    \
	void CGlobal::Get##Name##CCAssign(unsigned idx, char *out) const                   \
	{                                                                                    \
		mSpecialFuncCCMap.Get##Name##CCAssign(idx, out);                               \
	}                                                                                    \
	void CGlobal::Set##Name##CCAssign(unsigned idx, const char *in)                    \
	{                                                                                    \
		mSpecialFuncCCMap.Set##Name##CCAssign(idx, in);                                \
	}

DEFINE_FORWARD_SCALAR(ProgramUp)
DEFINE_FORWARD_SCALAR(ProgramDown)
DEFINE_FORWARD_SCALAR(SongStart)
DEFINE_FORWARD_SCALAR(SongPunch)
DEFINE_FORWARD_SCALAR(TapTempo)
DEFINE_FORWARD_SCALAR(OctaveUp)
DEFINE_FORWARD_SCALAR(OctaveDown)
DEFINE_FORWARD_SCALAR(RibbonLock)
DEFINE_FORWARD_ARRAY(RTKnobFunc)
DEFINE_FORWARD_ARRAY(PadFunc)
DEFINE_FORWARD_SCALAR(JSXLock)
DEFINE_FORWARD_SCALAR(JSYLock)
DEFINE_FORWARD_SCALAR(JSPYLock)
DEFINE_FORWARD_SCALAR(JSMYLock)
DEFINE_FORWARD_SCALAR(JSXRibLock)
DEFINE_FORWARD_SCALAR(JSYRibLock)
DEFINE_FORWARD_SCALAR(JSPYRibLock)
DEFINE_FORWARD_SCALAR(JSMYRibLock)
DEFINE_FORWARD_SCALAR(SW1Func)
DEFINE_FORWARD_SCALAR(SW2Func)
DEFINE_FORWARD_SCALAR(IncFunc)
DEFINE_FORWARD_SCALAR(DecFunc)
DEFINE_FORWARD_SCALAR(ChordSw)
DEFINE_FORWARD_SCALAR(DTrackEnable)
DEFINE_FORWARD_SCALAR(AftertouchLock)

#undef DEFINE_FORWARD_SCALAR
#undef DEFINE_FORWARD_ARRAY
