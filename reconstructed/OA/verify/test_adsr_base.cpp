// SPDX-License-Identifier: GPL-2.0
/*
 * test_adsr_base.cpp  -  host-side known-answer test for CSTGADSRBase
 * (src/engine/adsr_base.cpp). Covers: constructor field defaults,
 * HandlesCC's real 6-entry lookup table, the canonical
 * ApplyIntensityBlend formula (spot-checked against the SAME sample
 * triples the native-execution harness used to derive it -- see
 * oa_adsr_base.h's file header), PrecomputeData's I=0 special case,
 * HandleCC's cc->parameter dispatch, the Update main-4 family's
 * (intensity, x) argument swap, the ToneAdjust "absolute"/"Relative"
 * pair, and a representative Get* accessor's shared-scratch behavior.
 *
 * InitVoice is deliberately not exercised here -- see its own
 * declaration comment in oa_adsr_base.h (unconditional call through
 * an out-of-scope sibling virtual, InitVoiceMonoLegato).
 */

#include <cstdio>
#include <cmath>
#include <cstring>
#include "oa_adsr_base.h"

/*
 * Link-time-only mocks for symbols this TU's real, reconstructed code
 * genuinely calls (InitAMSSourceAddresses/PropagateAMSSourceAddress/
 * InitializeQuad) but that this test never actually exercises at
 * runtime (activeVoiceListTable is always null below, and
 * InitializeQuad isn't called at all) -- same "provide just enough
 * storage/stub to satisfy the linker" precedent as e.g.
 * test_param_convertor.cpp's own CSTGPerformanceVarsManager::sInstance
 * mock. CSTGVoice::GetAMSSourceAddress itself is a deliberately
 * deferred real method (see oa_engine.h); this stub body is
 * verify-only, not a claim about its real implementation.
 */
void *CSTGVoice::GetAMSSourceAddress(int) { return nullptr; }
CSTGVoiceModelManager *CSTGVoiceModelManager::sInstance;
static unsigned char g_globalBuf[0x2a00000];
CSTGGlobal *CSTGGlobal::sInstance = reinterpret_cast<CSTGGlobal *>(g_globalBuf);

static int g_fail;

static void check_near(const char *label, float got, float want, float eps = 1e-4f)
{
	bool ok = fabsf(got - want) <= eps;
	if (!ok)
		g_fail++;
	printf("  %s  %-55s %.6g\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted %.6g, diff %.6g)\n", want, got - want);
}

static void check_eq(const char *label, int got, int want)
{
	bool ok = got == want;
	if (!ok)
		g_fail++;
	printf("  %s  %-55s %d\n", ok ? "ok  " : "FAIL", label, got);
	if (!ok)
		printf("        (wanted %d)\n", want);
}

static void check_true(const char *label, bool got)
{
	if (!got)
		g_fail++;
	printf("  %s  %s\n", got ? "ok  " : "FAIL", label);
}

/* Real closed-form formula, reproduced independently here (not just a
 * call into the implementation) so this test can't pass by
 * tautology -- mirrors oa_adsr_base.h's own documented derivation. */
static float ReferenceBlend(float B, float I, float x)
{
	float a = (I >= 0.0f) ? (B + I * (1.0f - B)) : (B * (1.0f + I));
	return (x >= 0.0f) ? (a + x * (1.0f - a)) : (a * (1.0f + x));
}

/* Minimal fake CSTGComponentSlotInfo/CSTGPatchMessageContext wiring
 * for PrecomputeData/HandleCC/ToneAdjust*, none of which touch the
 * active-voice-list path this test doesn't exercise. */
static CSTGComponentSlotInfo g_slotInfo;
static unsigned char g_paramConstants[0x400];
static unsigned char g_precomputedArena[0x100];

static void SetupCtx(CSTGPatchMessageContext &ctx)
{
	memset(&ctx, 0, sizeof(ctx));
	ctx.activeVoiceListTable = nullptr; /* propagation families not exercised here */
	ctx.paramConstantsTable = g_paramConstants;
	ctx.precomputedBaseOffset = reinterpret_cast<char *>(g_precomputedArena);
	g_slotInfo.precomputedSlotIndex = 0;
	g_slotInfo.subRateBaseIndex = 0;
}

int main(void)
{
	printf("CSTGADSRBase known-answer test\n");
	printf("===============================\n");

	printf("[1] Constructor field defaults\n");
	{
		CSTGADSRBase adsr;
		check_near("attackTime default", adsr.attackTime, 0.05f);
		check_near("decayTime default", adsr.decayTime, 0.3f);
		check_near("sustainLevel default", adsr.sustainLevel, 1.0f);
		check_near("releaseTime default", adsr.releaseTime, 0.3f);
		check_eq("attackTimeAMSIntensity default", adsr.attackTimeAMSIntensity, 0);
		check_eq("releaseTimeAMSIntensityAMSSource default", adsr.releaseTimeAMSIntensityAMSSource, 0);
		check_true("_slotInfo defaults to null", adsr._slotInfo == nullptr);
		check_true("vtable ptr installed", *reinterpret_cast<void **>(&adsr) == _ZTV12CSTGADSRBase + 8);
	}

	printf("[2] HandlesCC real 6-entry table (.rodata+0x7df68)\n");
	{
		CSTGADSRBase adsr;
		check_true("0x45 (below range) -> false", !adsr.HandlesCC(0x45));
		check_true("0x46 (Sustain) -> true", adsr.HandlesCC(0x46));
		check_true("0x47 (unused) -> false", !adsr.HandlesCC(0x47));
		check_true("0x48 (Release) -> true", adsr.HandlesCC(0x48));
		check_true("0x49 (Attack) -> true", adsr.HandlesCC(0x49));
		check_true("0x4a (unused) -> false", !adsr.HandlesCC(0x4a));
		check_true("0x4b (Decay) -> true", adsr.HandlesCC(0x4b));
		check_true("0x4c (above range) -> false", !adsr.HandlesCC(0x4c));
	}

	printf("[3] ApplyIntensityBlend cross-check (via PrecomputeAttackTimePlusCC)\n");
	{
		float samples[][3] = {
			{ 0.05f, -0.8f, -0.5f }, { 0.05f, -0.8f, 0.5f },
			{ 0.3f, 0.3f, -0.1f },   { 0.3f, 0.3f, 0.1f },
			{ 1.0f, -1.0f, 0.0f },   { 1.0f, -1.0f, 0.5f },
			{ 0.7f, 1.5f, -0.5f },   { 0.7f, 1.5f, 0.5f },
		};
		for (auto &s : samples) {
			STGADSRBasePrecomputed p;
			memset(&p, 0, sizeof(p));
			p.raw[0] = s[0];
			p.intensitySlot[0] = s[1];
			CSTGADSRBase::PrecomputeAttackTimePlusCC(&p, s[2]);
			char label[64];
			snprintf(label, sizeof(label), "B=%.2g I=%.2g x=%.2g", s[0], s[1], s[2]);
			check_near(label, p.plusCC[0], ReferenceBlend(s[0], s[1], s[2]));
		}
	}

	printf("[4] PrecomputeData (I implicitly 0)\n");
	{
		CSTGADSRBase adsr;
		CSTGPatchMessageContext ctx;
		SetupCtx(ctx);
		adsr._slotInfo = &g_slotInfo;
		*reinterpret_cast<float *>(g_paramConstants + 0x370) = 0.2f;  /* Attack G */
		*reinterpret_cast<float *>(g_paramConstants + 0x388) = -0.3f; /* Decay G */
		*reinterpret_cast<float *>(g_paramConstants + 0x34c) = 0.0f;  /* Sustain G */
		*reinterpret_cast<float *>(g_paramConstants + 0x364) = 0.5f;  /* Release G */

		adsr.PrecomputeData(ctx);
		auto *p = reinterpret_cast<STGADSRBasePrecomputed *>(g_precomputedArena);
		check_near("raw[Attack] == attackTime", p->raw[0], adsr.attackTime);
		check_eq("amsIntensity[Attack] == 0", p->amsIntensity[0], 0);
		check_near("intensitySlot[Attack] == 0", p->intensitySlot[0], 0.0f);
		check_near("plusCC[Attack] == blend(0.05,0,0.2)", p->plusCC[0], ReferenceBlend(0.05f, 0.0f, 0.2f));
		check_near("plusCC[Decay] == blend(0.3,0,-0.3)", p->plusCC[1], ReferenceBlend(0.3f, 0.0f, -0.3f));
		check_near("plusCC[Release] == blend(0.3,0,0.5)", p->plusCC[3], ReferenceBlend(0.3f, 0.0f, 0.5f));
	}

	printf("[5] HandleCC cc->parameter dispatch\n");
	{
		CSTGADSRBase adsr;
		CSTGPatchMessageContext ctx;
		SetupCtx(ctx);
		adsr._slotInfo = &g_slotInfo;
		auto *p = reinterpret_cast<STGADSRBasePrecomputed *>(g_precomputedArena);
		memset(p, 0, sizeof(*p));
		p->raw[0] = 0.05f; p->raw[1] = 0.3f; p->raw[2] = 1.0f; p->raw[3] = 0.3f;

		CSTGControllerValue cc;
		memset(&cc, 0, sizeof(cc));
		cc.field4 = 0.4f;

		adsr.HandleCC(ctx, 0x49, cc); /* Attack */
		check_near("cc 0x49 updates plusCC[Attack]", p->plusCC[0], ReferenceBlend(0.05f, 0.0f, 0.4f));
		check_near("cc 0x49 leaves plusCC[Decay] alone", p->plusCC[1], 0.0f);

		adsr.HandleCC(ctx, 0x47, cc); /* unhandled */
		check_near("cc 0x47 is a no-op", p->plusCC[0], ReferenceBlend(0.05f, 0.0f, 0.4f));
	}

	printf("[6] UpdateAttackTime (note: G/intensitySlot argument order is swapped\n"
	       "    vs Precompute*PlusCC -- see adsr_base.cpp's own comment)\n");
	{
		CSTGADSRBase adsr;
		CSTGPatchMessageContext ctx;
		SetupCtx(ctx);
		adsr._slotInfo = &g_slotInfo;
		*reinterpret_cast<float *>(g_paramConstants + 0x370) = 0.25f; /* Attack G */
		auto *p = reinterpret_cast<STGADSRBasePrecomputed *>(g_precomputedArena);
		memset(p, 0, sizeof(*p));
		p->intensitySlot[0] = 0.6f;

		/* ctx._vtablePtr display predicate: always-true stub. */
		static bool alwaysTrue = true;
		typedef bool (*PredFn)(CSTGPatchMessageContext *);
		static PredFn predTable[1];
		predTable[0] = [](CSTGPatchMessageContext *) -> bool { return true; };
		ctx._vtablePtr = predTable;
		(void)alwaysTrue;

		STGConvertedParam newVal;
		memset(&newVal, 0, sizeof(newVal));
		int bits;
		float v = 0.12f;
		memcpy(&bits, &v, 4);
		newVal.value = bits;

		adsr.UpdateAttackTime(ctx, newVal);
		check_near("attackTime persisted", adsr.attackTime, 0.12f);
		check_near("plusCC[Attack] == blend(0.12, G=0.25, I=0.6)", p->plusCC[0],
		           ReferenceBlend(0.12f, 0.25f, 0.6f));
	}

	printf("[7] ToneAdjustAttackTime / ToneAdjustAttackTimeRelative\n");
	{
		CSTGADSRBase adsr;
		CSTGPatchMessageContext ctx;
		SetupCtx(ctx);
		adsr._slotInfo = &g_slotInfo;
		*reinterpret_cast<float *>(g_paramConstants + 0x370) = 0.1f;
		auto *p = reinterpret_cast<STGADSRBasePrecomputed *>(g_precomputedArena);
		memset(p, 0, sizeof(*p));

		STGConvertedParam scale;
		memset(&scale, 0, sizeof(scale));
		int bits;
		float v = 0.08f;
		memcpy(&bits, &v, 4);
		scale.value = bits;
		/* bit0 of +0x14 clear -> use scale.value, not the current field */
		reinterpret_cast<unsigned char *>(&scale)[0x14] = 0;

		adsr.ToneAdjustAttackTime(ctx, scale);
		check_near("raw[Attack] == scale.value", p->raw[0], 0.08f);
		check_near("plusCC[Attack] == blend(0.08, I=0, G=0.1)", p->plusCC[0],
		           ReferenceBlend(0.08f, 0.0f, 0.1f));

		float relBits = 0.4f;
		memcpy(&bits, &relBits, 4);
		scale.value = bits;
		adsr.ToneAdjustAttackTimeRelative(ctx, scale);
		check_near("intensitySlot[Attack] == scale.value", p->intensitySlot[0], 0.4f);
		check_near("plusCC[Attack] == blend(0.08, I=0.4, G=0.1)", p->plusCC[0],
		           ReferenceBlend(0.08f, 0.4f, 0.1f));
	}

	printf("[8] Get* shared-scratch accessors\n");
	{
		CSTGADSRBase adsr;
		CSTGPatchMessageContext ctx;
		SetupCtx(ctx);
		adsr.attackTime = 0.42f;
		STGConvertedParam &r = adsr.GetAttackTime(ctx);
		check_true("returns the shared static", &r == &CSTGParamsOwner::sValueGetterTemp);
		float rValueAsFloat;
		memcpy(&rValueAsFloat, &r.value, 4);
		check_near("value bit-matches attackTime", rValueAsFloat, 0.42f);
		check_eq("displayValue mirrors value (float family)", r.displayValue, r.value);

		adsr.attackTimeAMSSource = -5;
		STGConvertedParam &r2 = adsr.GetAttackTimeAMSSource(ctx);
		check_eq("AMSSource getter sign-extends", r2.value, -5);
	}

	printf("\n%s (%d failure%s)\n", g_fail ? "FAILED" : "PASSED", g_fail, g_fail == 1 ? "" : "s");
	return g_fail ? 1 : 0;
}
