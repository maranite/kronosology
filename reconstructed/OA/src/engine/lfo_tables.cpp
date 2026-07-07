// SPDX-License-Identifier: GPL-2.0
/*
 * lfo_tables.cpp  -  CSTGLFOTables::CSTGLFOTables() (batch 28,
 * `.text+0x12e260`, 2433 bytes).
 *
 * Kept in its own dedicated translation unit (not managers.cpp, not
 * bar2_stubs.cpp) since `verify/test_engine_init.cpp` (the only file
 * that links `engine_init.cpp`, the real caller) carries its own
 * `MOCK_CTOR_ONLY(CSTGLFOTables)` trivial mock and does NOT link
 * `bar2_stubs.cpp` either -- matching the by-now-standard "give a big
 * new ctor its own file" precedent (sec 10.145/10.158/10.164 etc), so
 * that test is left completely untouched.
 *
 * Confirmed via full disassembly + relocation resolution: ZERO `call`
 * instructions and ZERO indirect/vtable dispatch anywhere in the
 * function (confirmed no vtable for CSTGLFOTables at all, `nm -C`) --
 * the same "safe by instruction class, not by byte size" category as
 * `CSTGSamplingInterface::CSTGSamplingInterface()` (sec 10.160) and
 * `CSTGCCInfo::sCCInfoTable`'s populating function (sec 10.161), just
 * with SEVERAL distinct loop shapes chained together rather than one
 * flat byte table.
 *
 * Given the size and structural intricacy (at least 9 distinct
 * loop/table blocks, several with non-obvious index arithmetic), this
 * was derived by writing a small from-scratch x87-stack + x86-register
 * interpreter (a mechanical replay of the exact `objdump -dr` output,
 * not a generic disassembler -- every mnemonic actually used by this
 * one function: mov/movl/lea/xor/sub/subl/cmp/je/jne/jl/jmp/push/pop
 * plus fld/fld1/fldz/flds/fst/fsts/fstp/fstps/fadd/fadds/fsub/fsubs/
 * fchs/fxch) executing over a real 0x1830-byte virtual buffer, exactly
 * matching this project's own established "replay-script" technique
 * for large branch-free functions (sec 10.161/10.171/10.172). The
 * resulting byte-exact buffer was then cross-checked against
 * independent hand-derivation for the first several blocks (below)
 * before being trusted for the rest -- every value matched on the
 * first run.
 *
 * Confirmed real field layout (all offsets relative to `this`; no
 * vtable, so `+0x0` is the very first data field):
 *
 *   +0x000..+0x07c  (32 floats) rampUp[i]      = i * (1/32),        i=0..31
 *   +0x080..+0x0fc  (32 floats) rampDown[i]    = 1.0 - i*(1/32)
 *   +0x100..+0x17c  (32 floats) rampNeg[i]     = -(i*(1/32))
 *   +0x180..+0x1fc  (32 floats) rampMinus1[i]  = i*(1/32) - 1.0
 *   +0x200          (1 dword)   dup of +0x000 (rampUp[0] = 0.0) -- a
 *                    self-referential read of an EARLIER field this
 *                    same ctor already wrote a few instructions before,
 *                    not a read of uninitialized memory (confirmed:
 *                    every "dup" field in this object is `this[X] =
 *                    this[Y]` for a Y already written earlier in this
 *                    SAME constructor -- verified true for every one of
 *                    the 9 dup slots below via the interpreter replay).
 *   +0x204..+0x400  (128 floats) descRamp[i]   = 1.0 - i*(1/64),    i=0..127
 *   +0x404          (1 float)   literal -1.0
 *   +0x408..+0x504  (64 floats) constant 1.0   -- the field
 *                    `CSTGLFOBase::InitializeQuad()` (lfo_stepseq_quad.cpp)
 *                    already calls "lfoTables" is the ADDRESS of this
 *                    array's first entry, not the sine/S-curve tables
 *                    below -- a real, faithfully-preserved quirk.
 *   +0x508..+0x604  (64 floats) constant -1.0
 *   +0x608          (1 dword)   dup of +0x604 (last entry of the -1.0 array)
 *   +0x60c..+0x68c  (33 floats) kQuarterSine[k] = sin(k*pi/64), k=0..32 --
 *                    LITERAL immediates baked directly into the real
 *                    .text (movl, not a .rodata load) -- almost
 *                    certainly a compile-time-constant-folded C array
 *                    initializer in the original source.
 *   +0x690..+0x708  (32 floats) mirror[k]      = kQuarterSine[31-k], k=0..31
 *   +0x70c..+0x788  (32 floats) negQuarter[k]  = -kQuarterSine[k],   k=0..31
 *   +0x78c..+0x808  (32 floats) negMirror[k]   = -kQuarterSine[31-k],k=0..31
 *                    (together, +0x60c..+0x808 is a full 128-entry sine
 *                    table built via the classic "quarter wave table +
 *                    mirror + negate" technique -- confirmed by direct
 *                    comparison against sinf(i*pi/64) for i=0..127,
 *                    exact match at every one of the 4 quadrant
 *                    boundaries checked (i=0,32,64,96))
 *   +0x80c          (1 dword)   dup of +0x60c (kQuarterSine[0] = 0.0)
 *   +0x810..+0x844  (14 dwords) zero
 *   +0x848..+0x9fc  (110 floats) kEnvelope[110] -- an asymmetric
 *                    envelope/window curve (peak ~0.988 at index 71,
 *                    not centered); no closed-form (sin^n, raised-
 *                    cosine, gaussian) matched within the time budget
 *                    of this pass -- reproduced verbatim as literal
 *                    data from the real binary's own compile-time-
 *                    constant literal store sequence, same treatment
 *                    as CSTGDrumKitData's UUID table (sec 10.171).
 *   +0xa00..+0xa10  (5 dwords)  zero
 *   +0xa14..+0xb10  (64 floats) evenFine[k]    = kFineTable[2*k],       k=0..63
 *   +0xb14..+0xc10  (64 floats) oddFineRev[k]  = kFineTable[127-2*k],   k=0..63
 *   +0xc14          (1 dword)   dup of +0xa14 (evenFine[0] = kFineTable[0])
 *   +0xc18..+0xe14  (128 floats) fineRev[i]    = kFineTable[127-i],     i=0..127
 *   +0xe18          (1 dword)   dup of +0xe14 (fineRev[127] = kFineTable[0])
 *   +0xe1c..+0x1018 (128 floats) fineFwd[i]    = kFineTable[i],         i=0..127
 *   +0x101c         (1 dword)   dup of +0x1018 (fineFwd[127] = kFineTable[127])
 *   +0x1020..+0x121c (128 floats) 4x32-level staircase: -1, 0, +1, 0
 *   +0x1220         (1 dword)   dup of +0x121c (last segment = 0.0)
 *   +0x1224..+0x1420 (128 floats) 6-level staircase (uneven 22/21/22/21/22/20
 *                    segments): -1, -1/3, +1/3, +1, +1/3, -1/3 -- NOTE the
 *                    two "1/3" segments and two "-1/3" segments each use a
 *                    DIFFERENT literal bit pattern one ULP apart
 *                    (0x3eaaaaac vs 0x3eaaaaaa, 0xbeaaaaaa vs 0xbeaaaaac)
 *                    -- confirmed real (two independently-derived
 *                    reciprocal-of-3 constants in the original source),
 *                    faithfully preserved exactly, not "cleaned up" to
 *                    a single shared value.
 *   +0x1424         (1 dword)   dup of +0x1420 (last segment)
 *   +0x1428..+0x1624 (128 floats) 4x32-level staircase: +1, +1/3, -1/3, -1
 *   +0x1628         (1 dword)   dup of +0x1624 (last segment = -1.0)
 *   +0x162c..+0x1828 (128 floats) 6-level staircase (uneven 22/21/22/21/22/20
 *                    segments): +1, +0.6, +0.2, -0.2, -0.6, -1
 *   +0x182c         (1 dword)   dup of +0x1828 (last segment = -1.0)
 *
 * Total: 0x1830 bytes exactly, matching `CSTGBankMemory::
 * AllocAligned(0x1830, 0x10)` at the one real call site
 * (engine_init.cpp). No vtable (confirmed: `nm -C` has no `vtable for
 * CSTGLFOTables` symbol at all), so this is a plain data-only class.
 *
 * The four staircase tables (3/4/4/6 discrete levels respectively)
 * strongly suggest a stepped/quantized/"random" LFO waveform mode
 * offering a choice of step counts -- not independently confirmed
 * against any caller in this pass (no consumer of these specific
 * offsets has been reconstructed yet).
 */

#include "oa_engine_init.h"

/*
 * kFineTable[128]: a symmetric S-curve/tanh-like ramp from -1.0 to 1.0,
 * confirmed real `.rodata` data (NOT `.rodata.cst4`/cst8/cst16 -- a
 * plain flat float array, `readelf`-confirmed 128 consecutive entries,
 * `.rodata` file offset 0x4bf20..0x4c120). Extracted directly via
 * `struct.unpack('<f', ...)` (this project's own established technique,
 * sec 10.174/10.151), not hand-computed.
 */
static const float kFineTable[128] = {
	-1.0f, -0.999877930f, -0.999511719f, -0.998901308f, -0.998046815f, -0.996948123f,
	-0.995544314f, -0.993957341f, -0.992065191f, -0.989989936f, -0.987609506f, -0.985045910f,
	-0.982177198f, -0.979064286f, -0.975707293f, -0.972106099f, -0.968260765f, -0.964171290f,
	-0.959837615f, -0.955259860f, -0.950437963f, -0.945371866f, -0.940000594f, -0.934446216f,
	-0.928586662f, -0.922544003f, -0.916196167f, -0.909604192f, -0.902829051f, -0.895748794f,
	-0.888424337f, -0.880855739f, -0.873043001f, -0.864986122f, -0.856685102f, -0.848139882f,
	-0.839350581f, -0.830256045f, -0.820978403f, -0.811456621f, -0.801629663f, -0.791558564f,
	-0.781304359f, -0.770744979f, -0.759941399f, -0.748954713f, -0.737662911f, -0.726126909f,
	-0.714346766f, -0.702322483f, -0.690053999f, -0.677480400f, -0.664723635f, -0.651722789f,
	-0.638416708f, -0.624927521f, -0.611194193f, -0.597155690f, -0.582872987f, -0.568407238f,
	-0.553636253f, -0.538621187f, -0.523361921f, -0.507858515f, -0.492110968f, -0.476119280f,
	-0.459883422f, -0.443403423f, -0.426679283f, -0.409649938f, -0.392437518f, -0.374919891f,
	-0.357219160f, -0.339213222f, -0.321024209f, -0.302529991f, -0.283791631f, -0.264809102f,
	-0.245643482f, -0.226172671f, -0.206457719f, -0.186437577f, -0.166234314f, -0.145786926f,
	-0.125095367f, -0.104098633f, -0.082918793f, -0.061494797f, -0.039765619f, -0.017853329f,
	0.004364147f, 0.026825771f, 0.049531542f, 0.072420426f, 0.095614493f, 0.119052708f,
	0.142735064f, 0.166661575f, 0.190893278f, 0.215308085f, 0.239967033f, 0.264870137f,
	0.290078431f, 0.315469831f, 0.341166407f, 0.367046118f, 0.393231004f, 0.419660032f,
	0.446333200f, 0.473189503f, 0.500350952f, 0.527756572f, 0.555406332f, 0.583300292f,
	0.611499369f, 0.639881611f, 0.668507934f, 0.697378457f, 0.726554155f, 0.755912960f,
	0.785576940f, 0.815485120f, 0.845576346f, 0.875972748f, 0.906613350f, 0.937498093f,
	0.968626976f, 1.0f,
};

/*
 * kQuarterSine[33]: sin(k*pi/64), k=0..32 -- LITERAL immediate stores
 * in the real .text (movl, not a rodata load).
 */
static const float kQuarterSine[33] = {
	0.0f, 0.049067676f, 0.098017141f, 0.146730483f, 0.195090324f, 0.242980182f,
	0.290284693f, 0.336889863f, 0.382683456f, 0.427555114f, 0.471396744f, 0.514102757f,
	0.555570245f, 0.595699310f, 0.634393275f, 0.671558976f, 0.707106769f, 0.740951121f,
	0.773010492f, 0.803207576f, 0.831469655f, 0.857728601f, 0.881921291f, 0.903989315f,
	0.923879564f, 0.941544056f, 0.956940353f, 0.970031261f, 0.980785310f, 0.989176512f,
	0.995184720f, 0.998795450f, 1.0f,
};

/*
 * kEnvelope[110]: an asymmetric envelope/window curve (peak ~0.988 at
 * index 71). No closed form identified within this pass's time budget
 * -- reproduced verbatim, same treatment as CSTGDrumKitData's UUID
 * table (sec 10.171).
 */
static const float kEnvelope[110] = {
	0.000207439996f, 0.00109507004f, 0.00459876982f, 0.00804662984f, 0.0109863998f, 0.0170350298f,
	0.0239290409f, 0.0322397798f, 0.0431219898f, 0.0546394214f, 0.0669116303f, 0.0808909014f,
	0.0957723185f, 0.110085942f, 0.125862807f, 0.142982349f, 0.158318385f, 0.172364146f,
	0.188392937f, 0.20543173f, 0.222339183f, 0.240492344f, 0.256815434f, 0.270832092f,
	0.285289556f, 0.299246848f, 0.311133862f, 0.323927045f, 0.34035787f, 0.357499599f,
	0.375985593f, 0.397338837f, 0.416410983f, 0.4324072f, 0.449630827f, 0.467451125f,
	0.486710966f, 0.506615639f, 0.523395717f, 0.540754139f, 0.55919379f, 0.579089403f,
	0.601829767f, 0.623533547f, 0.64472115f, 0.666255414f, 0.687098205f, 0.708563745f,
	0.730321169f, 0.753056407f, 0.777127504f, 0.799911737f, 0.822373688f, 0.843722463f,
	0.862361431f, 0.881733119f, 0.900523245f, 0.917004049f, 0.933197975f, 0.946512401f,
	0.956703722f, 0.966551244f, 0.975070059f, 0.980903327f, 0.984229982f, 0.986057401f,
	0.987134278f, 0.987730086f, 0.988158405f, 0.988354206f, 0.988348901f, 0.988447368f,
	0.988237202f, 0.987231255f, 0.98415935f, 0.976528108f, 0.961556315f, 0.938812435f,
	0.914482117f, 0.890740216f, 0.863327324f, 0.82809931f, 0.785698235f, 0.746680498f,
	0.705392003f, 0.652563393f, 0.597639978f, 0.542566538f, 0.489585072f, 0.441434741f,
	0.39594233f, 0.350490421f, 0.306859046f, 0.271213502f, 0.236614034f, 0.201939553f,
	0.168354273f, 0.135542631f, 0.109606609f, 0.0878758579f, 0.0687249303f, 0.0532795191f,
	0.0401804596f, 0.0285889804f, 0.0197344702f, 0.0139323901f, 0.00808541011f, 0.0041734199f,
	0.00231972989f, 0.000236940003f,
};

/*
 * Fill(dst, count, value): the mechanical per-segment "constant fill"
 * operation used repeatedly for the constant-1.0/-1.0 blocks and the
 * four staircase tables below -- kept as an explicit named helper
 * (rather than one giant literal array) so each call site's own
 * (count, value) pair stays directly reviewable against the disassembly
 * derivation above.
 */
static void Fill(float *dst, int count, float value)
{
	for (int i = 0; i < count; i++)
		dst[i] = value;
}

CSTGLFOTables::CSTGLFOTables()
{
	unsigned char *base = (unsigned char *)this;
	float *rampUp = (float *)(base + 0x000);
	float *rampDown = (float *)(base + 0x080);
	float *rampNeg = (float *)(base + 0x100);
	float *rampMinus1 = (float *)(base + 0x180);

	/* Loop 1 (`.text+0x12e278`, 32 iterations): four parallel phase ramps. */
	for (int i = 0; i < 32; i++) {
		float phase = (float)i * (1.0f / 32.0f);
		rampUp[i] = phase;
		rampDown[i] = 1.0f - phase;
		rampNeg[i] = -phase;
		rampMinus1[i] = phase - 1.0f;
	}

	/* +0x200: dup of rampUp[0] (already written above). */
	*(unsigned int *)(base + 0x200) = *(unsigned int *)(base + 0x000);

	/* Loop 2 (`.text+0x12e2c0`, 128 iterations): descending ramp. */
	float *descRamp = (float *)(base + 0x204);
	for (int i = 0; i < 128; i++)
		descRamp[i] = 1.0f - (float)i * (1.0f / 64.0f);

	*(float *)(base + 0x404) = -1.0f;

	/* +0x408..+0x604: two 64-entry constant fills. */
	Fill((float *)(base + 0x408), 64, 1.0f);
	Fill((float *)(base + 0x508), 64, -1.0f);

	/* +0x608: dup of the second fill's own last entry. */
	*(unsigned int *)(base + 0x608) = *(unsigned int *)(base + 0x604);

	/* +0x60c..+0x808: full 128-entry sine table, built exactly the way
	 * the real binary builds it (quarter table + mirror + negate),
	 * not via a runtime sinf() call (no libm in a kernel module). */
	float *quarter = (float *)(base + 0x60c);
	for (int i = 0; i < 33; i++)
		quarter[i] = kQuarterSine[i];

	float *mirror = (float *)(base + 0x690);
	float *negQuarter = (float *)(base + 0x70c);
	float *negMirror = (float *)(base + 0x78c);
	/* mirror has only 31 real entries (+0x690..+0x708, k=0..30,
	 * kQuarterSine[31] down to kQuarterSine[1]) -- +0x70c is the START
	 * of the SEPARATE negQuarter array, confirmed real via a byte-exact
	 * interpreter replay: an earlier draft looped mirror over k=0..31,
	 * whose k=31 write (kQuarterSine[0] = +0.0) silently aliased and
	 * clobbered negQuarter[0]'s own correct -0.0 at the same address --
	 * caught by the golden-buffer KAT, not by inspection. */
	for (int k = 0; k < 31; k++)
		mirror[k] = kQuarterSine[31 - k];
	for (int k = 0; k < 32; k++) {
		negQuarter[k] = -kQuarterSine[k];
		/* negMirror[k] = -kQuarterSine[32-k], NOT -kQuarterSine[31-k]
		 * (negMirror[0] == -1.0 == -kQuarterSine[32], confirmed via the
		 * same interpreter replay -- an off-by-one relative to the
		 * plain `mirror` array's own 31-k formula). */
		negMirror[k] = -kQuarterSine[32 - k];
	}

	/* +0x80c: dup of quarter[0] (already written above). */
	*(unsigned int *)(base + 0x80c) = *(unsigned int *)(base + 0x60c);

	/* +0x810..+0x844: 14 dwords of zero. */
	for (int i = 0; i < 14; i++)
		*(unsigned int *)(base + 0x810 + i * 4) = 0;

	/* +0x848..+0x9fc: the 110-entry envelope table. */
	float *envelope = (float *)(base + 0x848);
	for (int i = 0; i < 110; i++)
		envelope[i] = kEnvelope[i];

	/* +0xa00..+0xa10: 5 dwords of zero. */
	for (int i = 0; i < 5; i++)
		*(unsigned int *)(base + 0xa00 + i * 4) = 0;

	/* +0xa14/+0xb14: interleaved even/odd half-resolution extraction
	 * from kFineTable, forward and reversed respectively. */
	float *evenFine = (float *)(base + 0xa14);
	float *oddFineRev = (float *)(base + 0xb14);
	for (int k = 0; k < 64; k++) {
		evenFine[k] = kFineTable[2 * k];
		oddFineRev[k] = kFineTable[127 - 2 * k];
	}

	/* +0xc14: dup of evenFine[0] (= kFineTable[0]). */
	*(unsigned int *)(base + 0xc14) = *(unsigned int *)(base + 0xa14);

	/* +0xc18..+0xe14: kFineTable, full resolution, REVERSED. */
	float *fineRev = (float *)(base + 0xc18);
	for (int i = 0; i < 128; i++)
		fineRev[i] = kFineTable[127 - i];

	/* +0xe18: dup of fineRev[127] (= kFineTable[0]). */
	*(unsigned int *)(base + 0xe18) = *(unsigned int *)(base + 0xe14);

	/* +0xe1c..+0x1018: kFineTable, full resolution, FORWARD. */
	float *fineFwd = (float *)(base + 0xe1c);
	for (int i = 0; i < 128; i++)
		fineFwd[i] = kFineTable[i];

	/* +0x101c: dup of fineFwd[127] (= kFineTable[127]). */
	*(unsigned int *)(base + 0x101c) = *(unsigned int *)(base + 0x1018);

	/* +0x1020..+0x121c: 4x32-level staircase -1,0,+1,0. */
	{
		float *p = (float *)(base + 0x1020);
		Fill(p + 0 * 32, 32, -1.0f);
		Fill(p + 1 * 32, 32, 0.0f);
		Fill(p + 2 * 32, 32, 1.0f);
		Fill(p + 3 * 32, 32, 0.0f);
	}
	*(unsigned int *)(base + 0x1220) = *(unsigned int *)(base + 0x121c);

	/* +0x1224..+0x1420: 6-level staircase, uneven segment lengths.
	 * NOTE the two distinct one-ULP-apart "1/3" constants -- confirmed
	 * real, deliberately NOT unified (see header comment). */
	{
		float *p = (float *)(base + 0x1224);
		int off = 0;
		Fill(p + off, 22, -1.0f);            off += 22;
		Fill(p + off, 21, -0.333333313f);    off += 21;
		Fill(p + off, 22, 0.333333373f);     off += 22;
		Fill(p + off, 21, 1.0f);             off += 21;
		Fill(p + off, 22, 0.333333313f);     off += 22;
		Fill(p + off, 20, -0.333333373f);    off += 20;
	}
	*(unsigned int *)(base + 0x1424) = *(unsigned int *)(base + 0x1420);

	/* +0x1428..+0x1624: 4x32-level staircase +1,+1/3,-1/3,-1. */
	{
		float *p = (float *)(base + 0x1428);
		Fill(p + 0 * 32, 32, 1.0f);
		Fill(p + 1 * 32, 32, 0.333333313f);
		Fill(p + 2 * 32, 32, -0.333333373f);
		Fill(p + 3 * 32, 32, -1.0f);
	}
	*(unsigned int *)(base + 0x1628) = *(unsigned int *)(base + 0x1624);

	/* +0x162c..+0x1828: 6-level staircase, uneven segment lengths. */
	{
		float *p = (float *)(base + 0x162c);
		int off = 0;
		Fill(p + off, 22, 1.0f);             off += 22;
		Fill(p + off, 21, 0.600000024f);     off += 21;
		Fill(p + off, 22, 0.199999988f);     off += 22;
		Fill(p + off, 21, -0.200000018f);    off += 21;
		Fill(p + off, 22, -0.600000024f);    off += 22;
		Fill(p + off, 20, -1.0f);            off += 20;
	}
	*(unsigned int *)(base + 0x182c) = *(unsigned int *)(base + 0x1828);
}
