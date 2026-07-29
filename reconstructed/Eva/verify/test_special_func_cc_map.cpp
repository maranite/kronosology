/*
 * test_special_func_cc_map.cpp  -  host-side known-answer test for
 * CSpecialFuncCCMap (src/editor/special_func_cc_map.cpp) and its thin CGlobal
 * forwarder (src/editor/cglobal.cpp). See special_func_cc_map.h/cglobal.h for
 * full ground-truth provenance.
 *
 * Checks:
 *   [1] Initialize()/ResetAssignments(): every one of the 39 (chan,cc) slots
 *       defaults to (0x10, 0xff), spot-checked across all 4 offset regions
 *       (fixed group 1 @0x00-0x0f, RTKnobFunc/PadFunc arrays @0x10-0x2f,
 *       fixed group 2 @0x30-0x4d).
 *   [2] Named-slot Get/Set round trips for one slot from each region.
 *   [3] RTKnobFunc/PadFunc array accessors, including the real ground-truth
 *       index-clamp-to-7 behavior for an out-of-range index.
 *   [4] CGlobal's forwarders reach the SAME embedded CSpecialFuncCCMap a
 *       direct CSpecialFuncCCMap::Get* call would see (round-trips through
 *       CGlobal::Set* then reads back via CGlobal::Get*).
 */

#include <cstdio>
#include <cstring>

#include "special_func_cc_map.h"
#include "cglobal.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CSpecialFuncCCMap / CGlobal known-answer test\n");
	printf("==============================================\n");

	printf("[1] Initialize() default state (0x10 channel / 0xff cc)\n");
	{
		CSpecialFuncCCMap m;
		m.Initialize();

		unsigned char chan;
		char cc;

		m.GetProgramUpMIDIChannel(&chan);
		m.GetProgramUpCCAssign(&cc);
		check("ProgramUp defaults (0x10, 0xff)", chan == 0x10 && (unsigned char)cc == 0xff);

		m.GetRibbonLockMIDIChannel(&chan);
		m.GetRibbonLockCCAssign(&cc);
		check("RibbonLock defaults (0x10, 0xff)", chan == 0x10 && (unsigned char)cc == 0xff);

		m.GetRTKnobFuncMIDIChannel(3, &chan);
		m.GetRTKnobFuncCCAssign(3, &cc);
		check("RTKnobFunc[3] defaults (0x10, 0xff)", chan == 0x10 && (unsigned char)cc == 0xff);

		m.GetPadFuncMIDIChannel(7, &chan);
		m.GetPadFuncCCAssign(7, &cc);
		check("PadFunc[7] defaults (0x10, 0xff)", chan == 0x10 && (unsigned char)cc == 0xff);

		m.GetAftertouchLockMIDIChannel(&chan);
		m.GetAftertouchLockCCAssign(&cc);
		check("AftertouchLock defaults (0x10, 0xff)", chan == 0x10 && (unsigned char)cc == 0xff);
	}

	printf("[2] named-slot Get/Set round trips\n");
	{
		CSpecialFuncCCMap m;
		m.Initialize();

		char chanIn = 5;
		char ccIn = 0x40;
		m.SetTapTempoMIDIChannel(&chanIn);
		m.SetTapTempoCCAssign(&ccIn);
		unsigned char chan;
		char cc;
		m.GetTapTempoMIDIChannel(&chan);
		m.GetTapTempoCCAssign(&cc);
		check("TapTempo round trip (5, 0x40)", chan == 5 && cc == 0x40);

		char chan2 = 9;
		char cc2 = 0x7f;
		m.SetChordSwMIDIChannel(&chan2);
		m.SetChordSwCCAssign(&cc2);
		m.GetChordSwMIDIChannel(&chan);
		m.GetChordSwCCAssign(&cc);
		check("ChordSw round trip (9, 0x7f)", chan == 9 && cc == 0x7f);

		/* Unrelated slots must be untouched by the writes above. */
		m.GetProgramUpMIDIChannel(&chan);
		check("ProgramUp unaffected by TapTempo/ChordSw writes", chan == 0x10);
	}

	printf("[3] RTKnobFunc/PadFunc array accessors + index clamp\n");
	{
		CSpecialFuncCCMap m;
		m.Initialize();

		unsigned char chanIn = 2;
		char ccIn = 0x10;
		m.SetRTKnobFuncMIDIChannel(0, &chanIn);
		m.SetRTKnobFuncCCAssign(0, &ccIn);
		unsigned char chan;
		char cc;
		m.GetRTKnobFuncMIDIChannel(0, &chan);
		m.GetRTKnobFuncCCAssign(0, &cc);
		check("RTKnobFunc[0] round trip (2, 0x10)", chan == 2 && cc == 0x10);

		/* ground truth clamps any index >= 8 to 7 */
		unsigned char chanIn2 = 0x0c;
		m.SetRTKnobFuncMIDIChannel(99, &chanIn2);
		m.GetRTKnobFuncMIDIChannel(7, &chan);
		check("index 99 clamps to slot 7", chan == 0x0c);
		m.GetRTKnobFuncMIDIChannel(200, &chan);
		check("reading index 200 also clamps to slot 7", chan == 0x0c);

		unsigned char padChanIn = 3;
		m.SetPadFuncMIDIChannel(4, &padChanIn);
		m.GetPadFuncMIDIChannel(4, &chan);
		check("PadFunc[4] round trip (3)", chan == 3);
		m.GetRTKnobFuncMIDIChannel(4, &chan);
		check("RTKnobFunc[4] independent of PadFunc[4]", chan == 0x10);
	}

	printf("[4] CGlobal forwarders reach the embedded CSpecialFuncCCMap\n");
	{
		CGlobal g;
		char chanIn = 11;
		char ccIn = 0x22;
		g.SetSongStartMIDIChannel(&chanIn);
		g.SetSongStartCCAssign(&ccIn);
		unsigned char chan;
		char cc;
		g.GetSongStartMIDIChannel(&chan);
		g.GetSongStartCCAssign(&cc);
		check("CGlobal SongStart round trip (11, 0x22)", chan == 11 && cc == 0x22);

		unsigned char rtChanIn = 6;
		g.SetRTKnobFuncMIDIChannel(2, &rtChanIn);
		g.GetRTKnobFuncMIDIChannel(2, &chan);
		check("CGlobal RTKnobFunc[2] round trip (6)", chan == 6);
	}

	printf("[5] CGlobal round 57 batch: InitializeSetListParams/InitializeDrumTrackParams/\n"
	       "    InitializeSpecialCCMapping/DumpSpecialFuncCCMapSettings\n");
	{
		CGlobal g;
		unsigned char *raw = reinterpret_cast<unsigned char *>(&g);

		/* real: mOpaqueHead[0x602a]/[0x602b] low 5 bits set, high 3 bits
		 * preserved -- verify both the set-bits and the preserve-high-bits
		 * halves of the real bitwise-OR-with-mask body.
		 */
		raw[0x602a] = 0xA5; /* 1010 0101 -- high 3 bits 101, low 5 bits arbitrary */
		g.InitializeSetListParams();
		check("InitializeSetListParams: low 5 bits set to 6, high 3 preserved",
		      raw[0x602a] == ((0xA5 & 0xe0) | 6));

		raw[0x602b] = 0x40; /* 0100 0000 -- high 3 bits 010 */
		g.InitializeDrumTrackParams();
		check("InitializeDrumTrackParams: low 5 bits set to 9, high 3 preserved",
		      raw[0x602b] == ((0x40 & 0xe0) | 9));

		/* Dirty the embedded CSpecialFuncCCMap, then confirm
		 * InitializeSpecialCCMapping() resets it to the same real
		 * (0x10, 0xff) default already confirmed in [1] above.
		 */
		char dirtyCc = 0x7f;
		g.SetProgramUpCCAssign(&dirtyCc);
		g.InitializeSpecialCCMapping();
		unsigned char chan2;
		char cc2;
		g.GetProgramUpMIDIChannel(&chan2);
		g.GetProgramUpCCAssign(&cc2);
		check("InitializeSpecialCCMapping resets embedded map to defaults",
		      chan2 == 0x10 && (unsigned char)cc2 == 0xff);

		g.DumpSpecialFuncCCMapSettings(); /* real: pushes every slot via
		                                    * USTGAPIGlobal::UpdateGlobalParameter
		                                    * -- confirm it doesn't crash */
		check("DumpSpecialFuncCCMapSettings doesn't crash", true);
	}

	printf("\n%s (%d check%s failed)\n", g_fail ? "FAILED" : "PASSED",
	       g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
