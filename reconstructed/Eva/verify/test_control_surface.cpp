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
extern "C" unsigned int g_oSongForReset[0xcc5];
extern "C" unsigned char s_akbyAreaForIFX[64];

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

	// [7] round 50 batch
	unsigned char songBuf[sizeof(g_oSongForReset)];
	memset(songBuf, 0xab, sizeof(songBuf));
	CControlSurface::SetSongForReset(songBuf);
	check("SetSongForReset copies the FULL 0xcc5-dword song image",
	      memcmp(g_oSongForReset, songBuf, sizeof(g_oSongForReset)) == 0);
	check("GetSongForReset() address matches the grown array",
	      (void *)CControlSurface::GetSongForReset() == (void *)g_oSongForReset);

	buf[0x118] = 0xaa;
	buf[0x119] = 0xaa;
	cs->SetBackupMode(0);
	check("SetBackupMode(0) writes +0x118", buf[0x118] == 0 && buf[0x119] == 0xaa);
	buf[0x118] = 0xaa;
	buf[0x119] = 0xaa;
	cs->SetBackupMode(2);
	check("SetBackupMode(2) writes +0x119", buf[0x119] == 2 && buf[0x118] == 0xaa);
	buf[0x118] = 0xaa;
	buf[0x119] = 0xaa;
	cs->SetBackupMode(5);
	check("SetBackupMode(5) is a no-op (neither mask matches)",
	      buf[0x118] == 0xaa && buf[0x119] == 0xaa);
	cs->SetBackupMode(9);
	check("SetBackupMode(9) out of [0,8) range is a no-op",
	      buf[0x118] == 0xaa && buf[0x119] == 0xaa);

	check("ShouldSetupFaderAsReverse: in-range knob+algo+value(0x25) -> true",
	      cs->ShouldSetupFaderAsReverse(10, 3, 0x25) == true);
	check("ShouldSetupFaderAsReverse: in-range knob+algo+value(0x10<0x1a) -> true",
	      cs->ShouldSetupFaderAsReverse(10, 3, 0x10) == true);
	check("ShouldSetupFaderAsReverse: wrong algorithm -> false",
	      cs->ShouldSetupFaderAsReverse(10, 1, 0x25) == false);
	check("ShouldSetupFaderAsReverse: knobFader out of [8,16] -> false",
	      cs->ShouldSetupFaderAsReverse(20, 3, 0x25) == false);
	check("ShouldSetupFaderAsReverse: value 0x30 (neither range) -> false",
	      cs->ShouldSetupFaderAsReverse(10, 3, 0x30) == false);

	unsigned char *csObj = (unsigned char *)mmap32(0x10);
	memset(csObj, 0, 0x10);
	*(unsigned int *)(buf + 0x140) = (unsigned int)(unsigned long)csObj;
	*(int *)(buf + 0x138) = 7;
	csObj[2] = 0x5;
	*(int *)(buf + 5 * 0x10 + 0xc) = 0;
	cs->EditAudioChannelStripKnob(0xffffffff, 0x77, 5);
	check("EditAudioChannelStripKnob(arg1=-1) writes unconditionally",
	      *(int *)(buf + 5 * 0x10 + 0xc) == 0x77);
	*(int *)(buf + 5 * 0x10 + 0xc) = 0;
	cs->EditAudioChannelStripKnob(0x11, 0x88, 5);
	check("EditAudioChannelStripKnob(knobFader==0x11) is a no-op",
	      *(int *)(buf + 5 * 0x10 + 0xc) == 0);

	s_akbyAreaForIFX[3] = 0x10;
	*(int *)(buf + 0x128) = 3;
	buf[0x72] = 0x15; // 0x10 + 5
	*(int *)(buf + 0x138) = 7;
	cs->EditIFXSend1(5, 0x99);
	check("EditIFXSend1: area match writes +0x6c", *(int *)(buf + 0x6c) == 0x99);
	buf[0x82] = 0x16; // 0x10 + 6
	cs->EditIFXSend2(6, 0xaa);
	check("EditIFXSend2: area match writes +0x7c", *(int *)(buf + 0x7c) == 0xaa);

	*(int *)(buf + 0x138) = 4;
	*(unsigned int *)(buf + 2 * 0x10 + 4) = 0x1111;
	*(unsigned int *)(buf + 2 * 0x10 + 8) = 0x2222;
	*(unsigned int *)(buf + 2 * 0x10 + 0x10) = 0x4444;
	cs->EditExternalKnob(2, 0x3333);
	check("EditExternalKnob writes value at +0xc", *(int *)(buf + 2 * 0x10 + 0xc) == 0x3333);
	check("EditExternalKnob mirrors 4-dword block (sees NEW +0xc value)",
	      *(unsigned int *)(buf + 2 * 0x10 + 0x154) == 0x1111 &&
	      *(unsigned int *)(buf + 2 * 0x10 + 0x158) == 0x2222 &&
	      *(unsigned int *)(buf + 2 * 0x10 + 0x15c) == 0x3333 &&
	      *(unsigned int *)(buf + 2 * 0x10 + 0x160) == 0x4444);

	*(unsigned int *)(buf + 2 * 0x10 + 0x84) = 0x5555;
	*(unsigned int *)(buf + 2 * 0x10 + 0x88) = 0x6666;
	*(unsigned int *)(buf + 2 * 0x10 + 0x90) = 0x8888;
	cs->EditExternalSlider(2, 0x7777);
	check("EditExternalSlider writes value at +0x8c", *(int *)(buf + 2 * 0x10 + 0x8c) == 0x7777);
	check("EditExternalSlider mirrors 4-dword block (sees NEW +0x8c value)",
	      *(unsigned int *)(buf + 2 * 0x10 + 0x1d4) == 0x5555 &&
	      *(unsigned int *)(buf + 2 * 0x10 + 0x1d8) == 0x6666 &&
	      *(unsigned int *)(buf + 2 * 0x10 + 0x1dc) == 0x7777 &&
	      *(unsigned int *)(buf + 2 * 0x10 + 0x1e0) == 0x8888);

	unsigned char *scene144 = (unsigned char *)mmap32(0x200);
	memset(scene144, 0, 0x200);
	scene144[0x135] = 0x9;
	*(unsigned int *)(buf + 0x144) = (unsigned int)(unsigned long)scene144;
	*(unsigned int *)(buf + 0x148) = 0;
	check("GetCurrentKarmaSceneId(0): reads +0x144's own +0x135 & 7",
	      cs->GetCurrentKarmaSceneId(0) == (0x9 & 7));
	check("GetCurrentKarmaSceneId(1), no +0x148 table: same fallback",
	      cs->GetCurrentKarmaSceneId(1) == (0x9 & 7));

	unsigned char *scene148 = (unsigned char *)mmap32(0x1000);
	memset(scene148, 0, 0x1000);
	scene148[0x127 + 2 * 0x2e8] = 0x44;
	*(unsigned int *)(buf + 0x148) = (unsigned int)(unsigned long)scene148;
	check("GetCurrentKarmaSceneId(3), +0x148 table present: reads its own record",
	      cs->GetCurrentKarmaSceneId(3) == 0x44);

	*(int *)(buf + 0x114) = 0;
	csObj[2] = 0x3; // buf+0x140 still points at csObj here
	cs->InitializeSelectSwitchInAudioInput();
	check("InitializeSelectSwitchInAudioInput: +0x114==0 path sets bit from +0x140's own +2",
	      buf[0x11c] == (unsigned char)(1u << (0x3 & 0xf)));

	*(int *)(buf + 0x114) = 1;
	*(int *)(buf + 0x128) = 4;
	unsigned char *trackObj2 = (unsigned char *)mmap32(0x20);
	memset(trackObj2, 0, 0x20);
	*(unsigned short *)(trackObj2 + 0xc) = 0x1234;
	*(unsigned int *)(buf + 4 * 4 + 0x7ec) = (unsigned int)(unsigned long)trackObj2;
	cs->InitializeSelectSwitchInAudioInput();
	check("InitializeSelectSwitchInAudioInput: +0x114!=0 path reads indexed +0x7ec object's +0xc",
	      buf[0x11c] == (unsigned char)0x1234);

	*(int *)(buf + 0x138) = 7;
	csObj[0] = 0x0; // top bit clear -> "positive" branch
	csObj[2] = 0x5;
	*(int *)(buf + 0xc) = 0;
	cs->EditAudioPan(-1, 0x55);
	check("EditAudioPan: positive-branch arg1==-1 writes +0xc unconditionally",
	      *(int *)(buf + 0xc) == 0x55);
	csObj[0] = 0x80; // top bit set -> "negative" branch
	*(int *)(buf + 3 * 0x10 + 0xc) = 0;
	cs->EditAudioPan(3, 0x66);
	check("EditAudioPan: negative-branch writes per-channel slot",
	      *(int *)(buf + 3 * 0x10 + 0xc) == 0x66);

	scene144[2] = 0x0;
	*(unsigned int *)(buf + 0x148) = 0;
	scene144[0x135] = 0x2;
	unsigned int expectScene = (unsigned int)(unsigned long)scene144 + 0x136 + (0x2 & 7) * 9;
	check("GetCurrentKarmaScene: fallback branch (+0x148==0)",
	      cs->GetCurrentKarmaScene() == expectScene);

	*(int *)(buf + 0x128) = 1;
	scene144[2] = 0x2;
	*(unsigned int *)(buf + 0x148) = (unsigned int)(unsigned long)scene148;
	scene148[0x127] = 0x0; // record for scene index (2&7)-1 == 1: base = scene148 + 1*0x2e8
	unsigned char *rec1 = scene148 + 1 * 0x2e8;
	rec1[0x127] = 0x7;
	unsigned int expectScene2 = (unsigned int)(unsigned long)rec1 + 0x148 + 0x7 * 9;
	check("GetCurrentKarmaScene: real-table branch (+0x148!=0, sceneSel&7!=0)",
	      cs->GetCurrentKarmaScene() == expectScene2);

	*(int *)(buf + 0x128) = 3;
	buf[0x11a] = 0xaa;
	cs->InitializePlayMuteSwitchInModKarma();
	check("InitializePlayMuteSwitchInModKarma: +0x128==3 clears +0x11a", buf[0x11a] == 0);

	*(int *)(buf + 0x128) = 1;
	scene144[2] = 0x0;
	scene144[0x135] = 0x4;
	cs->InitializePlayMuteSwitchInModKarma();
	check("InitializePlayMuteSwitchInModKarma: fallback scene sets bit",
	      buf[0x11a] == (unsigned char)(1u << (0x4 & 0x1f)));

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
