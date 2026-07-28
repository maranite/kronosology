/*
 * test_kontakt_parameter_family.cpp  -  host-side known-answer test for the
 * 10 concrete CKontaktXxxParameter classes in kontakt_parameter_family.cpp,
 * 2026-07-28.
 *
 * Each check constructs the owning raw-offset struct, wraps it in its
 * Parameter class, and drives AddIndexedParameter()/AddParameter()/
 * AddDynamicParameter() directly (bypassing AddAttribute's name/value
 * plumbing, already covered by test_kontakt_parameter_base.cpp) with the
 * real per-class field index confirmed from the real jump table, checking
 * the owning struct's field lands at the right value.
 */

#include <cstdio>
#include <cstring>

#include "kontakt_parameter_family.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

#define U (const unsigned char *)

/* Every CKontaktXxxParameter class's own family base (CKontaktParameter/
 * IndexedParameter/DynamicParameter) now supplies a real, non-pure
 * Identifier() override (returns "V" -- see kontakt_parameter_base.h's
 * 2026-07-28 "Parameters" factory-family resolution), so none of these
 * concrete classes are abstract any more; no test-only override needed. */
#define TESTABLE(ClassName, OwnerType) \
	class T##ClassName : public ClassName { \
	public: \
		T##ClassName(OwnerType *owner) : ClassName(owner) {} \
	}

TESTABLE(CKontaktGroupParameter, CKontaktGroup);
TESTABLE(CKontaktZoneParameter, CKontaktZone);
TESTABLE(CKontaktEffectParameter, CKontaktEffect);
TESTABLE(CKontaktFilterParameter, CKontaktFilter);
TESTABLE(CKontaktOutputParameter, CKontaktOutput);
TESTABLE(CKontaktLfoParameter, CKontaktLfo);
TESTABLE(CKontaktLoopParameter, CKontaktLoop);
TESTABLE(CKontaktEnvelopeParameter, CKontaktEnvelope);
TESTABLE(CKontaktPlaybackModeParameter, CKontaktPlaybackMode);
TESTABLE(CKontaktStartCriteriaParameter, CKontaktStartCriteria);
TESTABLE(CKontaktScriptParameter, CKontaktScript);
TESTABLE(CKontaktOutputsParameter, CKontaktOutputs);
TESTABLE(CKontaktContainerParameter, CKontaktContainer);
TESTABLE(CKontaktSampleParameter, CKontaktSample);
TESTABLE(CKontaktProgramParameter, CKontaktProgram);
TESTABLE(CKontaktBankParameter, CKontaktBank);
TESTABLE(CKontaktSendLevelsParameter, CKontaktSendLevels);
TESTABLE(CKontaktIntModulatorParameter, CKontaktIntModulator);
TESTABLE(CKontaktExtModulatorParameter, CKontaktExtModulator);
TESTABLE(CKontaktVoiceGroupParameter, CKontaktVoiceGroup);
TESTABLE(CKontaktTargetParameter, CKontaktTarget);

int main(void)
{
	printf("CKontaktXxxParameter family known-answer test\n");
	printf("================================================\n");

	printf("[1] CKontaktGroupParameter (indexed family, 32 fields + 1 special case)\n");
	{
		CKontaktGroup g;
		memset(&g, 0, sizeof(g));
		TCKontaktGroupParameter p(&g);
		p.AddIndexedParameter(0, 0, U"0.75");
		check("case 0 (volume) -> float 0.75", g.volume > 0.749f && g.volume < 0.751f);
		p.AddIndexedParameter(3, 0, U"yes");
		check("case 3 (keyTracking) -> bool true", g.keyTracking == true);
		p.AddIndexedParameter(19, 0, U"normal");
		check("case 19 (interpQuality) -> enum index 0 (\"normal\")", g.interpQuality == 0);
		p.AddIndexedParameter(19, 0, U"unrecognized");
		check("case 19 unrecognized string -> left unchanged", g.interpQuality == 0);
		/* real special case: "outRouting_3" resolves to (index=11, suffix=3);
		 * dispatch subtracts 1 -> SetOutputRouting(2, value) */
		p.AddIndexedParameter(11, 3, U"9");
		check("case 11 (outRouting_N) -> outRouting[suffix-1] = value", g.outRouting[2] == 9);
		p.AddIndexedParameter(31, 0, U"64");
		check("case 31 (fadeHighKey, last field) -> unsigned 64", g.fadeHighKey == 64);
		p.AddIndexedParameter(99, 0, U"ignored"); /* out of range -> falls to base no-op, must not crash */
		check("out-of-range index falls through to base no-op without crashing", true);
	}

	printf("[2] CKontaktZoneParameter (33-entry list, only 0-15 dispatched)\n");
	{
		CKontaktZone z;
		memset(&z, 0, sizeof(z));
		TCKontaktZoneParameter p(&z);
		p.AddParameter(0, U"1000");
		check("case 0 (sampleStart) -> unsigned 1000", z.sampleStart == 1000);
		p.AddParameter(11, U"60");
		check("case 11 (rootKey) -> unsigned 60", z.rootKey == 60);
		p.AddParameter(12, U"0.9");
		check("case 12 (zoneVolume) -> float 0.9", z.zoneVolume > 0.899f && z.zoneVolume < 0.901f);
		p.AddParameter(15, U"grid_mode_none");
		check("case 15 (gridMode) -> enum index 0", z.gridMode == 0);
		/* confirmed real no-op cases (jump table points straight at the
		 * shared fallback, not any field-writing block) -- just proving
		 * this doesn't crash; there's no observable field to check. */
		p.AddParameter(7, U"123");
		check("cases 7-10 (fade*) are confirmed real no-ops", true);
	}

	printf("[3] CKontaktEffectParameter\n");
	{
		CKontaktEffect e;
		memset(&e, 0, sizeof(e));
		TCKontaktEffectParameter p(&e);
		p.AddParameter(1, U"5");
		check("case 1 (classID) -> unsigned 5", e.classID == 5);
		p.AddParameter(5, U"-3");
		check("case 5 (sendFXOutPartition) -> signed -3", e.sendFXOutPartition == -3);
	}

	printf("[4] CKontaktFilterParameter (all 19 fields are float)\n");
	{
		CKontaktFilter f;
		memset(&f, 0, sizeof(f));
		TCKontaktFilterParameter p(&f);
		p.AddParameter(0, U"1000.0");
		check("case 0 (cutoff) -> float 1000.0", f.cutoff > 999.9f && f.cutoff < 1000.1f);
		p.AddParameter(6, U"1.0");
		check("case 6 (typeA, genuinely a float not a bool) -> float 1.0", f.typeA > 0.999f && f.typeA < 1.001f);
		p.AddParameter(18, U"0.5");
		check("case 18 (gain_2, last field) -> float 0.5", f.gain_2 > 0.499f && f.gain_2 < 0.501f);
	}

	printf("[5] CKontaktOutputParameter (no jump table, plain compare chain)\n");
	{
		CKontaktOutput o;
		memset(&o, 0, sizeof(o));
		TCKontaktOutputParameter p(&o);
		p.AddParameter(0, U"2");
		check("case 0 (numChannels) -> unsigned 2", o.numChannels == 2);
		p.AddParameter(2, U"0.8");
		check("case 2 (volume) -> float 0.8", o.volume > 0.799f && o.volume < 0.801f);
	}

	printf("[6] CKontaktLfoParameter\n");
	{
		CKontaktLfo l;
		memset(&l, 0, sizeof(l));
		TCKontaktLfoParameter p(&l);
		p.AddParameter(0, U"4.5");
		check("case 0 (frequency) -> float 4.5", l.frequency > 4.499f && l.frequency < 4.501f);
		p.AddParameter(4, U"yes");
		check("case 4 (normalizeMultiLFO) -> bool true", l.normalizeMultiLFO == true);
	}

	printf("[7] CKontaktLoopParameter\n");
	{
		CKontaktLoop lp;
		memset(&lp, 0, sizeof(lp));
		TCKontaktLoopParameter p(&lp);
		p.AddParameter(0, U"100");
		check("case 0 (loopStart) -> unsigned 100", lp.loopStart == 100);
		p.AddParameter(3, U"until_release");
		check("case 3 (mode) -> enum index 1 (\"until_release\")", lp.mode == 1);
		p.AddParameter(6, U"0.25");
		check("case 6 (xfadeLength) -> float 0.25", lp.xfadeLength > 0.249f && lp.xfadeLength < 0.251f);
	}

	printf("[8] CKontaktEnvelopeParameter (case 1 int-parsed-then-float-converted)\n");
	{
		CKontaktEnvelope env;
		memset(&env, 0, sizeof(env));
		TCKontaktEnvelopeParameter p(&env);
		p.AddParameter(0, U"0.1");
		check("case 0 (atkCurving) -> float 0.1", env.atkCurving > 0.099f && env.atkCurving < 0.101f);
		p.AddParameter(1, U"500");
		check("case 1 (attack) -> parsed as int 500, stored as float 500.0", env.attack > 499.9f && env.attack < 500.1f);
		p.AddParameter(8, U"0.3");
		check("case 8 (breakLevel, real XML name \"break\") -> float 0.3", env.breakLevel > 0.299f && env.breakLevel < 0.301f);
	}

	printf("[9] CKontaktPlaybackModeParameter\n");
	{
		CKontaktPlaybackMode pm;
		memset(&pm, 0, sizeof(pm));
		TCKontaktPlaybackModeParameter p(&pm);
		p.AddParameter(0, U"time_machine");
		check("case 0 (type) -> enum index 1 (\"time_machine\")", pm.type == 1);
		p.AddParameter(3, U"yes");
		check("case 3 (zoneLockedSpeed) -> bool true", pm.zoneLockedSpeed == true);
	}

	printf("[10] CKontaktStartCriteriaParameter (2 independent enums)\n");
	{
		CKontaktStartCriteria sc;
		memset(&sc, 0, sizeof(sc));
		TCKontaktStartCriteriaParameter p(&sc);
		p.AddParameter(0, U"on_controller");
		check("case 0 (mode) -> enum index 1", sc.mode == 1);
		p.AddParameter(2, U"and_not");
		check("case 2 (nextCriteria) -> enum index 1 (separate list)", sc.nextCriteria == 1);
		p.AddParameter(4, U"10");
		check("case 4 (cc_min) -> unsigned 10", sc.cc_min == 10);
	}

	printf("[11] CKontaktScriptParameter (dynamic/text-suffix family)\n");
	{
		CKontaktScript s;
		memset(&s, 0, sizeof(s));
		TCKontaktScriptParameter p(&s);
		p.AddDynamicParameter(4, "", U"a test description");
		check("case 4 (description) -> strncpy'd into fixed buffer", strcmp(s.description, "a test description") == 0);
		p.AddDynamicParameter(0, "", U"print(1)");
		check("case 0 (sourceText) -> heap-allocated via SetSourceText", s.sourceText != 0 && strncmp(s.sourceText, "print(1)", 8) == 0);
		p.AddDynamicParameter(6, "0", U"ignored"); /* confirmed real no-op */
		check("case 6 (persistent_var_) is a confirmed real no-op", true);
		delete[] s.sourceText; /* test-only cleanup (real allocator mismatch documented in the header, not reproduced here) */
	}

	printf("[12] CKontaktOutputsParameter (indexed family, 1 field, UnpackPath-unblocked owner setter)\n");
	{
		CKontaktOutputs outs;
		memset(&outs, 0, sizeof(outs));
		TCKontaktOutputsParameter p(&outs);
		p.AddIndexedParameter(0, 5, U"3");
		check("case 0 (physOutMapping_N) -> physicalOutputMapping[suffix] = value",
		      outs.physicalOutputMapping[5] == 3);
		p.AddIndexedParameter(99, 0, U"ignored");
		check("out-of-range index falls through to base no-op without crashing", true);
	}

	printf("[13] CKontaktContainerParameter (15 fields, 1 UnpackPath case -- 2026-07-28 UnpackPath follow-up)\n");
	{
		CKontaktContainer c;
		memset(&c, 0, sizeof(c));
		TCKontaktContainerParameter p(&c);
		p.AddParameter(0, U"yes");
		check("case 0 (loadPurged) -> bool true", c.loadPurged == true);
		p.AddParameter(4, U"12345");
		check("case 4 (libraryID) -> unsigned 12345", c.libraryID == 12345);
		p.AddParameter(5, U"-2");
		check("case 5 (loadingFlags) -> signed -2", c.loadingFlags == -2);
		p.AddParameter(8, U"0.6");
		check("case 8 (volume) -> float 0.6", c.volume > 0.599f && c.volume < 0.601f);
		/* case 13 (origSubDir): UnpackPath()'d into a stack buffer, then
		 * CKontaktContainer::SetOriginalSubDirectory() -- exercise the
		 * real 'd' packed-path token end to end. */
		p.AddParameter(13, U"@d007SubDir");
		check("case 13 (origSubDir) -> UnpackPath \"@d007SubDir\" -> \"SubDir/\"",
		      strcmp(c.origSubDir, "SubDir/") == 0);
		p.AddParameter(14, U"yes");
		check("case 14 (hasBeenSaved, last field, contiguity-confirmed offset) -> bool true", c.hasBeenSaved == true);
	}

	printf("[14] CKontaktSampleParameter (17 fields, 2 UnpackPath cases -- 2026-07-28 UnpackPath follow-up)\n");
	{
		CKontaktSample smp;
		memset(&smp, 0, sizeof(smp));
		TCKontaktSampleParameter p(&smp);
		/* case 0 (file_ex2): UnpackPath()'d then CKontaktSample::SetFile(). */
		p.AddParameter(0, U"@FXXXXX008YYYSample1");
		check("case 0 (file_ex2) -> UnpackPath 'F' name -> SetFile(\"Sample1\")",
		      strcmp(smp.file_ex2, "Sample1") == 0);
		/* case 1 (file_pbn): same idea, SetFilePbn(). */
		p.AddParameter(1, U"@FXXXXX008YYYSample2");
		check("case 1 (file_pbn) -> UnpackPath 'F' name -> SetFilePbn(\"Sample2\")",
		      strcmp(smp.file_pbn, "Sample2") == 0);
		p.AddParameter(8, U"44100");
		check("case 8 (sampleRate) -> unsigned 44100", smp.sampleRate == 44100);
		p.AddParameter(14, U"1.0");
		check("case 14 (tuning) -> float 1.0", smp.tuning > 0.999f && smp.tuning < 1.001f);
		p.AddParameter(16, U"999999");
		check("case 16 (expectedDataSize, last field, contiguity-confirmed offset) -> unsigned 999999",
		      smp.expectedDataSize == 999999);
	}

	printf("[15] concrete plural \"Parameters\" wrappers -- MakeXxx() factory bodies (5 classes)\n");
	{
		CKontaktGroup g;
		memset(&g, 0, sizeof(g));
		CKontaktGroupParameters gp(&g);
		CKontaktIndexedParameter *child1 = gp.MakeIndexedParameter();
		check("CKontaktGroupParameters::MakeIndexedParameter() returns non-NULL", child1 != 0);
		child1->AddIndexedParameter(0, 0, U"0.25");
		check("...and it's a real, working CKontaktGroupParameter(owner=&g)", g.volume > 0.249f && g.volume < 0.251f);
		delete child1;

		CKontaktOutput o;
		memset(&o, 0, sizeof(o));
		CKontaktOutputParameters op(&o);
		CKontaktParameter *child2 = op.MakeParameter();
		child2->AddParameter(0, U"7");
		check("CKontaktOutputParameters::MakeParameter() -> real CKontaktOutputParameter(owner=&o)", o.numChannels == 7);
		delete child2;

		CKontaktZone z;
		memset(&z, 0, sizeof(z));
		CKontaktZoneParameters zp(&z);
		CKontaktParameter *child3 = zp.MakeParameter();
		child3->AddParameter(0, U"2000");
		check("CKontaktZoneParameters::MakeParameter() -> real CKontaktZoneParameter(owner=&z)", z.sampleStart == 2000);
		delete child3;

		CKontaktContainer c;
		memset(&c, 0, sizeof(c));
		CKontaktContainerParameters cp(&c);
		CKontaktParameter *child4 = cp.MakeParameter();
		child4->AddParameter(4, U"42");
		check("CKontaktContainerParameters::MakeParameter() -> real CKontaktContainerParameter(owner=&c)", c.libraryID == 42);
		delete child4;

		CKontaktOutputs outs;
		memset(&outs, 0, sizeof(outs));
		CKontaktOutputsParameters ovp(&outs);
		CKontaktIndexedParameter *child5 = ovp.MakeIndexedParameter();
		child5->AddIndexedParameter(0, 2, U"9");
		check("CKontaktOutputsParameters::MakeIndexedParameter() -> real CKontaktOutputsParameter(owner=&outs)",
		      outs.physicalOutputMapping[2] == 9);
		delete child5;

		CKontaktBank b;
		memset(&b, 0, sizeof(b));
		CKontaktBankParameters bp(&b);
		CKontaktIndexedParameter *child6 = bp.MakeIndexedParameter();
		child6->AddIndexedParameter(1, 0, U"77");
		check("CKontaktBankParameters::MakeIndexedParameter() -> real CKontaktBankParameter(owner=&b)", b.libraryID == 77);
		delete child6;

		CKontaktProgram pr;
		memset(&pr, 0, sizeof(pr));
		CKontaktProgramParameters prp(&pr);
		CKontaktParameter *child7 = prp.MakeParameter();
		child7->AddParameter(0, U"555");
		check("CKontaktProgramParameters::MakeParameter() -> real CKontaktProgramParameter(owner=&pr)", pr.numBytesSamplesTotal == 555);
		delete child7;
	}

	printf("[16] CKontaktProgramParameter (55-entry list, only 10 dispatched -- 2026-07-28 size-deferred follow-up)\n");
	{
		CKontaktProgram p;
		memset(&p, 0, sizeof(p));
		TCKontaktProgramParameter param(&p);
		param.AddParameter(0, U"1000000");
		check("case 0 (numBytesSamplesTotal) -> unsigned 1000000", p.numBytesSamplesTotal == 1000000);
		param.AddParameter(2, U"0.5");
		check("case 2 (volume) -> float 0.5", p.volume > 0.499f && p.volume < 0.501f);
		param.AddParameter(9, U"ignored");
		check("case 9 (defaultKeyKeySwitch) confirmed real no-op", true);
		param.AddParameter(14, U"cc64_mode_sustain_plus_controller");
		check("case 14 (cc64Mode) -> enum index 0 on exact match", p.cc64Mode == 0);
		param.AddParameter(30, U"ignored");
		check("case 30 (editorOpenMap, mid-range no-op run) confirmed real no-op", true);
		param.AddParameter(51, U"@d007SubDir");
		check("case 51 (wallpaperFile, last in-table case) -> UnpackPath -> SetWallpaperFile", p.wallpaperFile != 0 && strcmp(p.wallpaperFile, "SubDir/") == 0);
		param.AddParameter(53, U"ignored");
		check("case 53 (muted, past guard) falls straight to base no-op, no crash", true);
		delete[] p.wallpaperFile; /* test-only cleanup, real allocator mismatch not reproduced here */
	}

	printf("[17] CKontaktBankParameter (13-entry indexed list, 5 real owner setters)\n");
	{
		CKontaktBank b;
		memset(&b, 0, sizeof(b));
		TCKontaktBankParameter p(&b);
		p.AddIndexedParameter(0, 0, U"yes");
		check("case 0 (loadPurged) -> bool true", b.loadPurged == true);
		p.AddIndexedParameter(3, 2, U"5");
		check("case 3 (midiChannel_slot) -> SetSlotMidiChannel(2, 5)", *(unsigned int *)((unsigned char *)&b + 2 * 0x20 + 0x3c) == 5);
		p.AddIndexedParameter(5, 2, U"yes");
		check("case 5 (mute_slot) -> SetSlotMute(2, true)", *((unsigned char *)&b + 2 * 0x20 + 0x41) == 1);
		p.AddIndexedParameter(7, 2, U"3");
		check("case 7 (auxSendLevel1_slot) -> SetSlotAuxSendLevel(2, 1, (float)3) shared jump-table target",
		      *(float *)((unsigned char *)&b + 2 * 0x20 + 1 * 4 + 0x44) > 2.99f &&
		      *(float *)((unsigned char *)&b + 2 * 0x20 + 1 * 4 + 0x44) < 3.01f);
		p.AddIndexedParameter(10, 2, U"9");
		check("case 10 (reorderIdx_) -> SetSlotReorderIndex(2, 9)", *(unsigned int *)((unsigned char *)&b + 2 * 0x20 + 0x54) == 9);
		p.AddIndexedParameter(12, 0, U"yes");
		check("case 12 (origAbsolutePaths, last field) -> bool true", b.origAbsolutePaths == true);
	}

	printf("[18] CKontaktSendLevelsParameter (1-entry indexed list, bounds-checked owner setter)\n");
	{
		CKontaktSendLevels sl;
		memset(&sl, 0, sizeof(sl));
		TCKontaktSendLevelsParameter p(&sl);
		p.AddIndexedParameter(0, 3, U"0.25");
		check("case 0 (level_N) -> SetLevel(3, 0.25)", sl.level[3] > 0.249f && sl.level[3] < 0.251f);
		p.AddIndexedParameter(0, 99, U"1.0");
		check("case 0 out-of-range suffix (>7) real bounds check, no crash, no write", sl.level[7] == 0.0f);
	}

	printf("[19] CKontaktIntModulatorParameter (10-entry list, name field contiguity-confirmed)\n");
	{
		CKontaktIntModulator im;
		memset(&im, 0, sizeof(im));
		TCKontaktIntModulatorParameter p(&im);
		p.AddParameter(0, U"yes");
		check("case 0 (routersOpen) -> bool true", im.routersOpen == true);
		p.AddParameter(5, U"3");
		check("case 5 (classID) -> unsigned 3", im.classID == 3);
		p.AddParameter(6, U"LFO 1");
		check("case 6 (name) -> SetName strncpy", strcmp(im.name, "LFO 1") == 0);
		p.AddParameter(9, U"440.0");
		check("case 9 (noteValue_Frequency, last field) -> float 440.0", im.noteValue_Frequency > 439.9f && im.noteValue_Frequency < 440.1f);
	}

	printf("[20] CKontaktExtModulatorParameter (11-entry list, 2 StringIndex enums + FPU round-trip case)\n");
	{
		CKontaktExtModulator em;
		memset(&em, 0, sizeof(em));
		TCKontaktExtModulatorParameter p(&em);
		p.AddParameter(0, U"extMod");
		check("case 0 (type) -> enum index 0", em.type == 0);
		p.AddParameter(1, U"velocity");
		check("case 1 (source) -> enum index 2", em.source == 2);
		p.AddParameter(2, U"-7");
		check("case 2 (delay, real FPU round-trip) -> signed -7", em.delay == -7);
		p.AddParameter(8, U"Vel Mod");
		check("case 8 (name) -> SetName strncpy", strcmp(em.name, "Vel Mod") == 0);
		p.AddParameter(10, U"64");
		check("case 10 (ccNumber, last field) -> unsigned 64", em.ccNumber == 64);
	}

	printf("[21] CKontaktVoiceGroupParameter (6-entry list, name at index 0)\n");
	{
		CKontaktVoiceGroup vg;
		memset(&vg, 0, sizeof(vg));
		TCKontaktVoiceGroupParameter p(&vg);
		p.AddParameter(0, U"Group A");
		check("case 0 (name) -> SetName strncpy", strcmp(vg.name, "Group A") == 0);
		p.AddParameter(1, U"kill_oldest");
		check("case 1 (mode) -> enum index 0", vg.mode == 0);
		p.AddParameter(5, U"-1");
		check("case 5 (exclusionGroup, last field) -> signed -1", vg.exclusionGroup == -1);
	}

	printf("[22] CKontaktTargetParameter (8-entry list, own separate 8-entry target enum)\n");
	{
		CKontaktTarget t;
		memset(&t, 0, sizeof(t));
		TCKontaktTargetParameter p(&t);
		p.AddParameter(0, U"filterCutoff");
		check("case 0 (target) -> enum index 3 (own separate list)", t.target == 3);
		p.AddParameter(1, U"0.8");
		check("case 1 (intensity) -> float 0.8", t.intensity > 0.799f && t.intensity < 0.801f);
		p.AddParameter(5, U"Target 1");
		check("case 5 (name) -> SetName strncpy", strcmp(t.name, "Target 1") == 0);
		p.AddParameter(7, U"12");
		check("case 7 (targetObjIdx, last field) -> unsigned 12", t.targetObjIdx == 12);
	}

	printf("\n%s\n", g_fail == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
	return g_fail == 0 ? 0 : 1;
}
