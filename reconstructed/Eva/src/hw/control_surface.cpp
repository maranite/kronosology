// SPDX-License-Identifier: GPL-2.0
/*
 * control_surface.cpp  -  CControlSurface, round 49 batch (solo).
 * See include/control_surface.h for the full derivation of every offset
 * used below.
 */
#include "control_surface.h"

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
// beyond being 4-byte-aligned.
extern "C" unsigned int g_oSongForReset = 0;

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
	return &g_oSongForReset;
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
