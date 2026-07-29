// SPDX-License-Identifier: GPL-2.0
/*
 * control_surface.cpp  -  CControlSurface, round 49 batch (solo).
 * See include/control_surface.h for the full derivation of every offset
 * used below.
 */
#include "control_surface.h"
#include <cstring>

// Host/target pointer-width convention (this project's established
// ToU32/FromU32 idiom, e.g. OA.ko's src/init/file_io.cpp) -- the real
// 32-bit target stores these as plain 4-byte fields; on a 64-bit host
// FromU32 zero-extends rather than truncating.
static unsigned char *FromU32(unsigned int v) { return (unsigned char *)(unsigned long)v; }

// CSWTCH_497: real .rodata LED-code table, GetModeLEDCode's own `< 9`
// bounds check confirms 9 valid entries; content not independently
// confirmed.
extern "C" unsigned int CSWTCH_497[9] = {0};
// CSWTCH_500: real .rodata LED-code table, GetKarmaModuleSelectLEDCode's
// own `< 4` bounds check confirms 4 valid entries; content not
// independently confirmed.
extern "C" unsigned int CSWTCH_500[4] = {0};

// Real static/global object CControlSurface::GetSongForReset() returns
// the address of -- opaque, content/type not independently confirmed
// beyond size: SetSongForReset() (round 50) copies 0xcc5 (3269) DWORDs
// into it, confirming the true size (13,076 bytes), NOT a single scalar
// as round 49 assumed.
extern "C" unsigned int g_oSongForReset[0xcc5] = {0};

// s_akbyAreaForIFX: real .rodata byte table, indexed by the same +0x128
// "current slot" index field GetSoloSelected (round 49) already uses --
// content and true size not independently confirmed, sized to a safe,
// generous upper bound.
extern "C" unsigned char s_akbyAreaForIFX[64] = {0};

void CControlSurface::EditKarmaPadVelocityMode(int) { }
void CControlSurface::UpdateKnobFaderLED() { }
void CControlSurface::UpdateKnobFaderLED(int) { }
void CControlSurface::UpdateKnobFaderLED(int, unsigned short) { }
void CControlSurface::PressPlayMuteSwitchInExternal(int, int) { }
void CControlSurface::PressSelectSwitchInExternal(int, int) { }
void CControlSurface::PressPlayMuteSwitchInGraphicEQ(int, int) { }
void CControlSurface::PressSelectSwitchInGraphicEQ(int, int) { }
void CControlSurface::MoveKnobInGraphicEQ(int) { }

unsigned int *CControlSurface::GetSongForReset()
{
	return g_oSongForReset;
}

void CControlSurface::InitializeSelectSwitchInGraphicEQ()
{
	((unsigned char *)this)[0x11c] = 0;
}

void CControlSurface::InitializeSelectSwitchInExternal()
{
	unsigned char *base = (unsigned char *)this;
	base[0x11c] = base[0x265];
}

void CControlSurface::InitializePlayMuteSwitchInGraphicEQ()
{
	unsigned char *base = (unsigned char *)this;
	base[0x11a] = 0;
	base[0x11b] = 0;
}

void CControlSurface::InitializePlayMuteSwitchInExternal()
{
	unsigned char *base = (unsigned char *)this;
	base[0x11b] = 0;
	base[0x11a] = (unsigned char)(*(unsigned short *)(base + 0x264));
}

void CControlSurface::EditMasterVolume(int volume)
{
	*(int *)(((unsigned char *)this) + 0x10c) = volume;
}

unsigned int CControlSurface::GetSoloSelected() const
{
	unsigned char *base = (unsigned char *)this;
	int idx = *(int *)(base + 0x128);
	unsigned int ptr = *(unsigned int *)(base + idx * 4 + 0x7ec);
	return *(unsigned int *)(FromU32(ptr) + 0x24);
}

void CControlSurface::InitializeModKnob()
{
	unsigned char *base = (unsigned char *)this;
	base[0x7d0] = 0x40;
	base[0x7d1] = 0x40;
	base[0x7d2] = 0x40;
	base[0x7d3] = 0x40;
	base[0x7d4] = 0x40;
	base[0x7d5] = 0x40;
	base[0x7d6] = 0x40;
	base[0x7d7] = 0x40;
}

unsigned int CControlSurface::GetExternalSwitch(int bitIndex) const
{
	unsigned char *base = (unsigned char *)this;
	unsigned short field = *(unsigned short *)(base + 0x264);
	return ((unsigned int)field >> (bitIndex & 0x1f)) & 1;
}

unsigned int CControlSurface::GetExternalKnobFaderMax(char arg1) const
{
	return (arg1 != -1) ? 0x7f : 0;
}

unsigned int CControlSurface::GetExternalKnob(int index) const
{
	unsigned char *base = (unsigned char *)this;
	return *(unsigned int *)(base + (index + 0x15) * 0x10 + 0xc);
}

unsigned int CControlSurface::GetExternalSlider(int index) const
{
	unsigned char *base = (unsigned char *)this;
	return *(unsigned int *)(base + (index + 0x1d) * 0x10 + 0xc);
}

void CControlSurface::EditRTKnob(int index, int value)
{
	unsigned char *base = (unsigned char *)this;
	if (*(int *)(base + 0x138) != 5)
		return;
	*(int *)(base + index * 0x10 + 0xc) = value;
}

void CControlSurface::EditSetListEQBandLevel(unsigned int index, int value)
{
	unsigned char *base = (unsigned char *)this;
	if (*(int *)(base + 0x138) != 8)
		return;
	*(int *)(base + index * 0x10 + 0x8c) = value;
}

void CControlSurface::EditAudioVolume(int index, int value)
{
	unsigned char *base = (unsigned char *)this;
	if (*(int *)(base + 0x138) == 7 && (unsigned int)index < 8) {
		*(int *)(base + index * 0x10 + 0x8c) = value;
	}
}

void CControlSurface::EditAudioTrackVolume(int index, int value)
{
	unsigned char *base = (unsigned char *)this;
	int mode = *(int *)(base + 0x138);
	if (mode != 2) {
		if (mode != 3)
			return;
		index = index - 8;
	}
	if ((unsigned int)index > 7)
		return;
	*(int *)(base + index * 0x10 + 0x8c) = value;
}

void CControlSurface::EditVolume(int index, int value)
{
	unsigned char *base = (unsigned char *)this;
	int mode = *(int *)(base + 0x138);
	if (mode != 0) {
		if (mode != 1)
			return;
		index = index - 8;
	}
	if ((unsigned int)index > 7)
		return;
	*(int *)(base + index * 0x10 + 0x8c) = value;
}

void CControlSurface::EditKnobFaderForCustomMod(unsigned int arg1, int knobFader, int value)
{
	unsigned char *base = (unsigned char *)this;
	if (*(int *)(base + 0x138) != 6)
		return;
	if (arg1 != 0xffffffffu) {
		unsigned int ptr = *(unsigned int *)(base + 0x140);
		unsigned char selector = *(FromU32(ptr) + 1);
		if (arg1 != selector)
			return;
	}
	*(unsigned int *)(base + knobFader * 0x10 + 0xc) = (unsigned int)value;
}

unsigned int CControlSurface::GetModeLEDCode(int mode) const
{
	if ((unsigned int)mode < 9)
		return CSWTCH_497[mode];
	return 0x4d;
}

unsigned int CControlSurface::GetKarmaModuleSelectLEDCode(int index) const
{
	if ((unsigned int)index < 4)
		return CSWTCH_500[index];
	return 0x37;
}

bool CControlSurface::IsUseGlobalAudio() const
{
	unsigned char *base = (unsigned char *)this;
	if (*(int *)(base + 0x808) == 0) {
		unsigned int ptr = *(unsigned int *)(base + 0x140);
		return (*(FromU32(ptr) + 3) & 1) != 0;
	}
	return true;
}

void CControlSurface::SetSongForReset(const void *song)
{
	memcpy(g_oSongForReset, song, sizeof(g_oSongForReset));
}

void CControlSurface::SetBackupMode(int mode)
{
	unsigned char *base = (unsigned char *)this;
	if ((unsigned int)mode >= 8)
		return;
	unsigned int bit = 1u << (mode & 0x1f);
	if ((bit & 0x8c) != 0) {
		base[0x119] = (unsigned char)mode;
	} else if ((bit & 3) != 0) {
		base[0x118] = (unsigned char)mode;
	}
}

bool CControlSurface::ShouldSetupFaderAsReverse(int knobFader, int algorithm, unsigned char value) const
{
	if ((unsigned int)(knobFader - 8) >= 9 || algorithm != 3)
		return false;
	return ((unsigned char)(value - 0x23) < 5) || (value < 0x1a);
}

void CControlSurface::EditAudioChannelStripKnob(unsigned int arg1, int value, int knobFader)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int ptr = *(unsigned int *)(base + 0x140);
	unsigned char *p = FromU32(ptr);
	if (*(signed char *)p < 0)
		return;
	if (*(int *)(base + 0x138) != 7)
		return;
	if (arg1 != 0xffffffffu && arg1 != (unsigned int)(p[2] & 0xf))
		return;
	if (knobFader == 0x11)
		return;
	*(int *)(base + knobFader * 0x10 + 0xc) = value;
}

void CControlSurface::EditIFXSend1(int arg1, int value)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int ptr = *(unsigned int *)(base + 0x140);
	if (*(signed char *)FromU32(ptr) < 0)
		return;
	unsigned int mode = *(unsigned int *)(base + 0x138);
	if (mode != 7 && mode > 1)
		return;
	int idx = *(int *)(base + 0x128);
	unsigned int rhs = (unsigned int)s_akbyAreaForIFX[idx] + (unsigned int)arg1;
	if ((unsigned int)base[0x72] == rhs) {
		*(int *)(base + 0x6c) = value;
	}
}

void CControlSurface::EditIFXSend2(int arg1, int value)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int ptr = *(unsigned int *)(base + 0x140);
	if (*(signed char *)FromU32(ptr) < 0)
		return;
	unsigned int mode = *(unsigned int *)(base + 0x138);
	if (mode != 7 && mode > 1)
		return;
	int idx = *(int *)(base + 0x128);
	unsigned int rhs = (unsigned int)s_akbyAreaForIFX[idx] + (unsigned int)arg1;
	if ((unsigned int)base[0x82] == rhs) {
		*(int *)(base + 0x7c) = value;
	}
}

void CControlSurface::EditExternalKnob(int index, int value)
{
	unsigned char *base = (unsigned char *)this;
	if (*(int *)(base + 0x138) != 4)
		return;
	int off = index * 0x10;
	*(int *)(base + off + 0xc) = value;
	*(unsigned int *)(base + off + 0x154) = *(unsigned int *)(base + off + 4);
	*(unsigned int *)(base + off + 0x158) = *(unsigned int *)(base + off + 8);
	*(unsigned int *)(base + off + 0x15c) = *(unsigned int *)(base + off + 0xc);
	*(unsigned int *)(base + off + 0x160) = *(unsigned int *)(base + off + 0x10);
}

void CControlSurface::EditExternalSlider(int index, int value)
{
	unsigned char *base = (unsigned char *)this;
	if (*(int *)(base + 0x138) != 4)
		return;
	int off = index * 0x10;
	*(int *)(base + off + 0x8c) = value;
	*(unsigned int *)(base + off + 0x1d4) = *(unsigned int *)(base + off + 0x84);
	*(unsigned int *)(base + off + 0x1d8) = *(unsigned int *)(base + off + 0x88);
	*(unsigned int *)(base + off + 0x1dc) = *(unsigned int *)(base + off + 0x8c);
	*(unsigned int *)(base + off + 0x1e0) = *(unsigned int *)(base + off + 0x90);
}

unsigned char CControlSurface::GetCurrentKarmaSceneId(int arg1) const
{
	unsigned char *base = (unsigned char *)this;
	unsigned int ptr144 = *(unsigned int *)(base + 0x144);
	if (arg1 == 0)
		return FromU32(ptr144)[0x135] & 7;
	unsigned int ptr148 = *(unsigned int *)(base + 0x148);
	if (ptr148 != 0)
		return FromU32(ptr148)[0x127 + (arg1 - 1) * 0x2e8];
	return FromU32(ptr144)[0x135] & 7;
}

void CControlSurface::InitializeSelectSwitchInAudioInput()
{
	unsigned char *base = (unsigned char *)this;
	base[0x11c] = 0;
	if (*(int *)(base + 0x114) == 0) {
		unsigned int ptr = *(unsigned int *)(base + 0x140);
		unsigned char b = FromU32(ptr)[2] & 0xf;
		if (b < 8)
			base[0x11c] = (unsigned char)(1u << b);
		return;
	}
	int idx = *(int *)(base + 0x128);
	unsigned int ptr7ec = *(unsigned int *)(base + idx * 4 + 0x7ec);
	base[0x11c] = (unsigned char)(*(unsigned short *)(FromU32(ptr7ec) + 0xc));
}

void CControlSurface::EditAudioPan(int index, int value)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int ptr = *(unsigned int *)(base + 0x140);
	unsigned char *p = FromU32(ptr);
	if (*(signed char *)p < 0) {
		if (*(int *)(base + 0x138) == 7 && (unsigned int)index < 8) {
			*(int *)(base + index * 0x10 + 0xc) = value;
		}
	} else {
		if (*(int *)(base + 0x138) == 7 &&
		    (index == -1 || index == (p[2] & 0xf))) {
			*(int *)(base + 0xc) = value;
		}
	}
}

unsigned int CControlSurface::GetCurrentKarmaScene() const
{
	unsigned char *base = (unsigned char *)this;
	unsigned int p144 = *(unsigned int *)(base + 0x144);
	unsigned int p148 = *(unsigned int *)(base + 0x148);
	unsigned char sceneSel = FromU32(p144)[2];
	if (p148 != 0 && (sceneSel & 7) != 0) {
		unsigned int rec = p148 + ((sceneSel & 7) - 1) * 0x2e8;
		return rec + 0x148 + FromU32(rec)[0x127] * 9;
	}
	return p144 + 0x136 + (FromU32(p144)[0x135] & 7) * 9;
}

void CControlSurface::InitializePlayMuteSwitchInModKarma()
{
	unsigned char *base = (unsigned char *)this;
	if (*(int *)(base + 0x128) != 3) {
		unsigned int p144 = *(unsigned int *)(base + 0x144);
		unsigned char b = FromU32(p144)[2];
		unsigned char scene;
		if ((b & 7) == 0 || *(unsigned int *)(base + 0x148) == 0) {
			scene = FromU32(p144)[0x135] & 7;
		} else {
			unsigned int p148 = *(unsigned int *)(base + 0x148);
			scene = FromU32(p148)[0x127 + ((b & 7) - 1) * 0x2e8];
		}
		base[0x11a] = (unsigned char)(1u << (scene & 0x1f));
		return;
	}
	base[0x11a] = 0;
}

void CControlSurface::InitializeSelectSwitchInAudioTrack()
{
	unsigned char *base = (unsigned char *)this;
	if (*(int *)(base + 0x114) == 0) {
		unsigned int ptr = *(unsigned int *)(base + 0x140);
		unsigned int idx = FromU32(ptr)[2] >> 4;
		if (*(int *)(base + 0x138) == 3)
			idx -= 8;
		if (idx < 8)
			base[0x11c] = (unsigned char)(1u << (idx & 0x1f));
		return;
	}
	int slot = *(int *)(base + 0x128);
	unsigned int ptr7ec = *(unsigned int *)(base + slot * 4 + 0x7ec);
	unsigned short v = *(unsigned short *)(FromU32(ptr7ec) + 0x12);
	unsigned int shift = (*(int *)(base + 0x138) != 2) ? 8 : 0;
	base[0x11c] = (unsigned char)(v >> shift);
}

void CControlSurface::EditAudioTrackChannelStripKnob(unsigned int arg1, int value, int knobFader)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int ptr = *(unsigned int *)(base + 0x140);
	unsigned char *p = FromU32(ptr);
	if (*(signed char *)p < 0)
		return;
	if ((unsigned int)(*(int *)(base + 0x138) - 2) >= 2)
		return;
	if (arg1 != 0xffffffffu && arg1 != (unsigned int)(p[2] >> 4))
		return;
	if (knobFader == 0x11)
		return;
	*(int *)(base + knobFader * 0x10 + 0xc) = value;
}

void CControlSurface::EditAudioTrackPan(int index, int value)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int ptr = *(unsigned int *)(base + 0x140);
	unsigned char *p = FromU32(ptr);
	if (*(signed char *)p < 0) {
		int mode = *(int *)(base + 0x138);
		if (mode != 2) {
			if (mode != 3)
				return;
			index -= 8;
		}
		if ((unsigned int)index < 8)
			*(int *)(base + index * 0x10 + 0xc) = value;
	} else if ((unsigned int)(*(int *)(base + 0x138) - 2) < 2 &&
	           (index == -1 || index == (p[2] >> 4))) {
		*(int *)(base + 0xc) = value;
	}
}

void CControlSurface::EditPan(int index, int value)
{
	unsigned char *base = (unsigned char *)this;
	unsigned int ptr = *(unsigned int *)(base + 0x140);
	unsigned char *p = FromU32(ptr);
	if (*(signed char *)p < 0) {
		int mode = *(int *)(base + 0x138);
		if (mode != 0) {
			if (mode != 1)
				return;
			index -= 8;
		}
		if ((unsigned int)index < 8)
			*(int *)(base + index * 0x10 + 0xc) = value;
		return;
	}
	int slot = *(int *)(base + 0x128);
	if (slot == 3 || slot == 1) {
		if (index == -1) {
			if (p[1] > 1)
				return;
		} else if ((unsigned int)p[1] != (unsigned int)index) {
			return;
		}
	} else if ((unsigned int)p[1] != (unsigned int)index) {
		return;
	}
	if (*(unsigned int *)(base + 0x138) < 2 && base[0x12] != s_akbyAreaForIFX[slot]) {
		*(int *)(base + 0xc) = value;
	}
}

void CControlSurface::EditChannelStripKnob(unsigned int arg1, int value, int knobFader)
{
	unsigned char *base = (unsigned char *)this;
	int slot = *(int *)(base + 0x128);
	unsigned char *p;
	if ((slot == 3 || slot == 1) && arg1 == 0xffffffffu) {
		p = FromU32(*(unsigned int *)(base + 0x140));
		if (p[1] < 2)
			arg1 = p[1];
	} else {
		p = FromU32(*(unsigned int *)(base + 0x140));
	}
	if (*(signed char *)p < 0)
		return;
	if (arg1 != (unsigned int)p[1])
		return;
	if (*(unsigned int *)(base + 0x138) >= 2)
		return;
	if (knobFader == 0x11)
		return;
	if (base[knobFader * 0x10 + 0x12] == s_akbyAreaForIFX[slot])
		return;
	*(int *)(base + knobFader * 0x10 + 0xc) = value;
}

void CControlSurface::SetEnabledMIDITrack()
{
	unsigned char *base = (unsigned char *)this;
	int mode = *(int *)(base + 0x128);
	if (mode != 1) {
		if (mode < 2) {
			if (mode == 0)
				goto set_ffff;
		} else {
			if (mode == 2) {
			set_ffff:
				*(unsigned short *)(base + 0x126) = 0xffff;
				return;
			}
			if (mode == 3)
				*(unsigned short *)(base + 0x126) = 1;
		}
		return;
	}

	{
		unsigned int ptr134 = *(unsigned int *)(base + 0x134);
		unsigned char *t = FromU32(ptr134);
		unsigned char b = t[0x9fe] & 7;
		unsigned short result;
		bool done = false;
		if (b < 6) {
			unsigned int bit = 1u << b;
			if ((bit & 0x15) != 0) {
				result = 5;
				done = true;
			} else if ((bit & 0x22) != 0) {
				result = 7;
				done = true;
			} else if ((bit & 8) != 0) {
				*(unsigned short *)(base + 0x126) = 0;
				result = 2;
				if (t[0xb29] < 10 && t[0xb29] != 0) {
					*(unsigned short *)(base + 0x126) = 1;
					result = 3;
				}
				if (t[0xf45] < 10 && t[0xf45] != 0) {
					result |= 4;
					done = true;
				}
			}
		}
		if (!done)
			result = *(unsigned short *)(base + 0x126) | 4;
		*(unsigned short *)(base + 0x126) = result;
	}
}
