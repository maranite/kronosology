/*
 * test_control_surface.cpp  -  host-side known-answer test for
 * CControlSurface's round-49 28-method batch (solo, 2026-07-29). See
 * include/control_surface.h for the full derivation.
 */
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include "control_surface.h"

extern "C" unsigned int CSWTCH_497[9];
extern "C" unsigned int CSWTCH_500[4];

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

// Round 55's own lesson (OA.ko HARDWARE_REVIEW_LOG.md): a packed 32-bit
// pointer field pointed at a plain `static` host array can hold a real
// 64-bit address above 4GB and truncate to garbage. mmap32 keeps every
// buffer a FromU32-style field points at inside the low 4GB.
static void *mmap32(size_t len)
{
	void *p = mmap(0, len, PROT_READ | PROT_WRITE,
	               MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
	return p;
}

int main()
{
	unsigned char *buf = (unsigned char *)mmap32(0x1000);
	memset(buf, 0, 0x1000);
	CControlSurface *cs = reinterpret_cast<CControlSurface *>(buf);

	// [1] real no-op overrides -- reached, no crash
	cs->EditKarmaPadVelocityMode(3);
	cs->UpdateKnobFaderLED();
	cs->UpdateKnobFaderLED(2);
	cs->UpdateKnobFaderLED(2, 5);
	cs->PressPlayMuteSwitchInExternal(1, 2);
	cs->PressSelectSwitchInExternal(1, 2);
	cs->PressPlayMuteSwitchInGraphicEQ(1, 2);
	cs->PressSelectSwitchInGraphicEQ(1, 2);
	cs->MoveKnobInGraphicEQ(1);
	check("no-op overrides: reached here, no crash", true);

	// [2] static GetSongForReset
	check("GetSongForReset() returns a non-null static address",
	      CControlSurface::GetSongForReset() != 0);

	// [3] fixed-offset field get/set
	buf[0x11c] = 0xaa;
	cs->InitializeSelectSwitchInGraphicEQ();
	check("InitializeSelectSwitchInGraphicEQ zeroes +0x11c", buf[0x11c] == 0);

	buf[0x265] = 0x7;
	buf[0x11c] = 0xaa;
	cs->InitializeSelectSwitchInExternal();
	check("InitializeSelectSwitchInExternal copies +0x265 -> +0x11c", buf[0x11c] == 0x7);

	buf[0x11a] = 0xaa;
	buf[0x11b] = 0xaa;
	cs->InitializePlayMuteSwitchInGraphicEQ();
	check("InitializePlayMuteSwitchInGraphicEQ zeroes +0x11a/+0x11b",
	      buf[0x11a] == 0 && buf[0x11b] == 0);

	*(unsigned short *)(buf + 0x264) = 0x1234;
	buf[0x11a] = 0xaa;
	buf[0x11b] = 0xaa;
	cs->InitializePlayMuteSwitchInExternal();
	check("InitializePlayMuteSwitchInExternal: +0x11b=0, +0x11a=low byte of +0x264",
	      buf[0x11b] == 0 && buf[0x11a] == 0x34);

	cs->EditMasterVolume(0x11223344);
	check("EditMasterVolume writes int at +0x10c", *(int *)(buf + 0x10c) == 0x11223344);

	// [4] GetSoloSelected: index at +0x128 selects a pointer at +0x7ec,
	// read +0x24 off that pointed-at object
	unsigned char *trackObj = (unsigned char *)mmap32(0x100);
	memset(trackObj, 0, 0x100);
	*(unsigned int *)(trackObj + 0x24) = 0xcafef00d;
	*(int *)(buf + 0x128) = 3;
	*(unsigned int *)(buf + 3 * 4 + 0x7ec) = (unsigned int)(unsigned long)trackObj;
	check("GetSoloSelected reads +0x24 off the indexed pointer",
	      cs->GetSoloSelected() == 0xcafef00d);

	cs->InitializeModKnob();
	check("InitializeModKnob sets all 8 slots +0x7d0..0x7d7 to 0x40",
	      buf[0x7d0] == 0x40 && buf[0x7d7] == 0x40 && buf[0x7d3] == 0x40);

	*(unsigned short *)(buf + 0x264) = 0x5; // bits 0 and 2 set
	check("GetExternalSwitch bit0 set", cs->GetExternalSwitch(0) == 1);
	check("GetExternalSwitch bit1 clear", cs->GetExternalSwitch(1) == 0);
	check("GetExternalSwitch bit2 set", cs->GetExternalSwitch(2) == 1);

	check("GetExternalKnobFaderMax(-1) == 0", cs->GetExternalKnobFaderMax(-1) == 0);
	check("GetExternalKnobFaderMax(0) == 0x7f", cs->GetExternalKnobFaderMax(0) == 0x7f);

	// [5] per-channel array (stride 0x10)
	*(unsigned int *)(buf + (2 + 0x15) * 0x10 + 0xc) = 0x1111;
	check("GetExternalKnob(2) reads (idx+0x15)*0x10+0xc", cs->GetExternalKnob(2) == 0x1111);
	*(unsigned int *)(buf + (2 + 0x1d) * 0x10 + 0xc) = 0x2222;
	check("GetExternalSlider(2) reads (idx+0x1d)*0x10+0xc", cs->GetExternalSlider(2) == 0x2222);

	*(int *)(buf + 0x138) = 5;
	cs->EditRTKnob(4, 0x99);
	check("EditRTKnob writes when mode==5", *(int *)(buf + 4 * 0x10 + 0xc) == 0x99);
	*(int *)(buf + 0x138) = 0;
	*(int *)(buf + 4 * 0x10 + 0xc) = 0;
	cs->EditRTKnob(4, 0x55);
	check("EditRTKnob no-op when mode!=5", *(int *)(buf + 4 * 0x10 + 0xc) == 0);

	*(int *)(buf + 0x138) = 8;
	cs->EditSetListEQBandLevel(2, 0x77);
	check("EditSetListEQBandLevel writes when mode==8", *(int *)(buf + 2 * 0x10 + 0x8c) == 0x77);

	*(int *)(buf + 0x138) = 7;
	cs->EditAudioVolume(3, 0x42);
	check("EditAudioVolume writes when mode==7, index<8", *(int *)(buf + 3 * 0x10 + 0x8c) == 0x42);
	*(int *)(buf + 3 * 0x10 + 0x8c) = 0;
	cs->EditAudioVolume(200, 0x42);
	check("EditAudioVolume no-op when index>=8", *(int *)(buf + 200 * 0x10 + 0x8c) == 0);

	*(int *)(buf + 0x138) = 2;
	cs->EditAudioTrackVolume(3, 0x63);
	check("EditAudioTrackVolume mode==2: writes at index directly",
	      *(int *)(buf + 3 * 0x10 + 0x8c) == 0x63);
	*(int *)(buf + 0x138) = 3;
	cs->EditAudioTrackVolume(11, 0x64);
	check("EditAudioTrackVolume mode==3: writes at index-8",
	      *(int *)(buf + 3 * 0x10 + 0x8c) == 0x64);

	*(int *)(buf + 0x138) = 0;
	cs->EditVolume(3, 0x71);
	check("EditVolume mode==0: writes at index directly", *(int *)(buf + 3 * 0x10 + 0x8c) == 0x71);
	*(int *)(buf + 0x138) = 1;
	cs->EditVolume(11, 0x72);
	check("EditVolume mode==1: writes at index-8", *(int *)(buf + 3 * 0x10 + 0x8c) == 0x72);

	unsigned char *modObj = (unsigned char *)mmap32(0x10);
	memset(modObj, 0, 0x10);
	modObj[1] = 0x9;
	*(unsigned int *)(buf + 0x140) = (unsigned int)(unsigned long)modObj;
	*(int *)(buf + 0x138) = 6;
	cs->EditKnobFaderForCustomMod(0xffffffff, 5, 0x81);
	check("EditKnobFaderForCustomMod(arg1=-1): writes unconditionally",
	      *(unsigned int *)(buf + 5 * 0x10 + 0xc) == 0x81);
	cs->EditKnobFaderForCustomMod(0x9, 6, 0x82);
	check("EditKnobFaderForCustomMod(arg1==selector): writes",
	      *(unsigned int *)(buf + 6 * 0x10 + 0xc) == 0x82);
	cs->EditKnobFaderForCustomMod(0x3, 7, 0x83);
	check("EditKnobFaderForCustomMod(arg1!=selector): no-op",
	      *(unsigned int *)(buf + 7 * 0x10 + 0xc) == 0);

	// [6] lookup-table LED codes + misc
	CSWTCH_497[3] = 0x21;
	check("GetModeLEDCode(3) in range", cs->GetModeLEDCode(3) == 0x21);
	check("GetModeLEDCode(9) out of range -> 0x4d fallback", cs->GetModeLEDCode(9) == 0x4d);

	CSWTCH_500[1] = 0x55;
	check("GetKarmaModuleSelectLEDCode(1) in range", cs->GetKarmaModuleSelectLEDCode(1) == 0x55);
	check("GetKarmaModuleSelectLEDCode(4) out of range -> 0x37 fallback",
	      cs->GetKarmaModuleSelectLEDCode(4) == 0x37);

	*(int *)(buf + 0x808) = 1;
	check("IsUseGlobalAudio: +0x808 != 0 -> true", cs->IsUseGlobalAudio() == true);
	*(int *)(buf + 0x808) = 0;
	unsigned char *audioObj = (unsigned char *)mmap32(0x10);
	memset(audioObj, 0, 0x10);
	audioObj[3] = 0x1; // bit0 set
	*(unsigned int *)(buf + 0x140) = (unsigned int)(unsigned long)audioObj;
	check("IsUseGlobalAudio: +0x808==0, reads bit0 of pointed-at +3",
	      cs->IsUseGlobalAudio() == true);
	audioObj[3] = 0x0;
	check("IsUseGlobalAudio: bit0 clear -> false", cs->IsUseGlobalAudio() == false);

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
