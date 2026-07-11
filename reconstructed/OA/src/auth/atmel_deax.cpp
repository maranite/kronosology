// SPDX-License-Identifier: GPL-2.0
/*
 * atmel_deax.cpp  -  cm_AuthenEncryptMAC(): the GPA key-schedule step
 * SetupAtmelForAuthorizations() (atmel_setup.cpp) calls twice per
 * handshake round. See oa_atmel.h for the full symbol-name mapping
 * (this project's descriptive `cm_*` alias for OA_real.ko's own
 * obfuscated real name).
 *
 * cm_AuthenEncryptMAC @ .text+0x4f4210 (real name `fFfFfFfFfFfF11`, 1575
 * bytes) drives a 20-byte internal cipher state -- confirmed to live in
 * OA_real.ko's own .bss at 0x5c90c1..0x5c90d4 -- through a fixed sequence
 * of per-byte "steps" of a proprietary stream cipher this project's own
 * earlier AT88 hardware-extraction work already reverse engineered and
 * named the "DEAX" cipher: see
 * `AT88VirtualChip/chip_state.cpp`'s own `deax_step()`/
 * `deax_compute_challenges()`, hardware-validated against real captured
 * $B8 challenge/response traffic (two independent from-scratch Python
 * ports plus a from-scratch KAT all agree, per MASTER_REFERENCE.md's own
 * DEAX cipher notes) -- NOT re-derived from the raw disassembly a second
 * time from scratch here, ported instead.
 *
 * This function's own driving loop (`fFfFfFfFfFfF11`), freshly
 * disassembled and traced instruction-by-instruction this batch, is
 * STRUCTURALLY IDENTICAL to that already-validated
 * `deax_compute_challenges()`: same 20-byte zero-init (20 individual
 * byte stores, matching `deax_init()`'s 20 field zeroes exactly), same
 * "3 steps per iv-byte / 3 steps per kin-byte / 1 step per c1-byte"
 * input-expansion phase (8 groups of 7 steps: 4 driven by `iv`
 * interleaved with the front half of `c1`, then 4 more driven by `kin`
 * interleaved with the back half of `c1`), same "6 steps then 8 output
 * bytes at 6/7/7/7/7/7/7/7-step spacing" first output phase (into
 * `c2out`), same "iv[0] forced to 0xff, then iv[1..7] read back at
 * 2-step spacing" second phase (confirms `iv` is genuinely mutated in
 * place, not merely read), same "8 more output bytes at 2-step spacing
 * plus 1 extra trailing step" third phase (into `c3out`) --
 * just with this function's own 5 parameters standing in for
 * `deax_compute_challenges()`'s `nc`/`p2`/`p3`/`q_out`/`p5_out`:
 * `c1`==nc, `kin`==p2, `iv`==p3, `c2out`==q_out, `c3out`==p5_out.
 * `SetupAtmelForAuthorizations()`'s own already-real usage independently
 * confirms the `iv`==p3 mapping: `iv` is read ONCE from chip zone 0x50
 * then handed to this function across BOTH handshake rounds, exactly
 * matching `deax_compute_challenges()`'s own documented p3 contract
 * ("input: cfg[0x50..0x57]-derived seed; output: updated in place").
 *
 * NOT a cross-module call into AT88VirtualChip.ko (that module is this
 * project's own emulated CHIP side of the wire -- a completely separate
 * kernel module OA.ko has no dependency on in real life, and OA_real.ko
 * certainly doesn't). The cipher step logic is REPRODUCED here as OA.ko's
 * own private, freestanding implementation. The 20-byte state is modeled
 * as a plain struct with this project's own field layout/naming -- not
 * claiming to reproduce OA_real.ko's exact 0x5c90c1..0x5c90d4 .bss byte
 * order (functionally irrelevant to THIS function's own correctness,
 * since it always fully re-zeroes all 20 bytes via its own confirmed
 * 20-instruction inline init at entry, exactly like deax_init(), so no
 * incoming state ever matters).
 *
 * IMPORTANT, found in batch 43 and deliberately handled, not overlooked:
 * a whole-binary relocation scan found TWO OTHER real functions --
 * `fFfFfFfFfFfF13` (.text+0x4f4840) and `fFfFfFfFfFfF1C`
 * (.text+0x4f4a80, plus a `.clone.0`) -- that also READ (never write,
 * never touch the other 19 bytes) this exact same `gpa` byte
 * (0x5c90c1), each XORing a single external buffer byte with it as a
 * one-time-pad/keystream byte. Batch 43 left these NOT reconstructed
 * (out of scope at the time -- that batch's target was only
 * `cm_AuthenEncryptMAC`) and flagged them as almost certainly the same
 * "f13"-style Zone0 decrypt-continuation functions already referenced
 * qualitatively in this project's own AT88 chip-extraction documentation
 * (`bbbbbbbba12` calls `f11` twice to reset the cipher to a known
 * position, and a subsequent `f13` call continues decrypting FROM that
 * exact position using the cipher state `f11` left behind). Because of
 * this, `gpa`'s FINAL value at the end of a real `cm_AuthenEncryptMAC`
 * call is a genuine, persistent, observable side effect other real code
 * may depend on -- NOT safe to model as a function-local/stack-only
 * variable that vanishes on return (that would be a real fidelity
 * regression the moment a future batch reconstructs either sibling).
 * Modeled instead as file-static state, matching ground truth's own
 * persistent `.bss` semantics; `cm_AuthenEncryptMAC` itself still fully
 * re-initializes it every call (matching its own confirmed 20-byte
 * zero-init preamble), so this call's own behavior is unaffected -- only
 * the state's postcondition (final `gpa`) now persists correctly for
 * whatever future code may come to read it.
 *
 * UPDATE (batch 46): both siblings are now real too -- see
 * src/auth/atmel_zone_io.cpp. That file needs read/step access to this
 * exact persistent `DeaxState` (ground truth's own ` bzzzzzzzzzzzt12`,
 * .text+0x4f3d00, is a real, separately-callable single-step primitive --
 * confirmed via its own non-obfuscated `.bss` field names, `nm -C`:
 * `gpa_byte`@0x5c90c1 plus `RA`..`RG`/`SA`..`SG`/`TA`..`TE` at
 * 0x5c90c2-0x5c90d4, immediately adjacent to the real, non-obfuscated
 * `mode`@0x5c90c0 -- this OA.ko_Decomp image is not fully stripped, an
 * unusually direct corroboration this project doesn't normally get).
 * `bzzzzzzzzzzzt12`/the `gpa` read are exposed below as two small bridge
 * functions operating on the SAME `g_atmelDeaxState` this file already
 * defines -- they can see it because C++ anonymous-namespace members are
 * visible to the REST OF THIS TU, just not externally linkable; only the
 * bridge functions themselves need real (extern "C") linkage to be
 * callable from atmel_zone_io.cpp.
 *
 * Real signature-fidelity fix, found while re-deriving this: this
 * project's own pre-existing oa_atmel.h declared `iv` as
 * `const unsigned char *` -- wrong, both ground truth and
 * SetupAtmelForAuthorizations()'s own already-real call sites confirm
 * `iv` is genuinely mutated by this call (round 2 reuses round 1's
 * mutated `iv` value, not the original zone-0x50 read). Fixed in
 * oa_atmel.h alongside this promotion; the caller already treats its own
 * local `iv` array as non-const, so this is a pure signature correction,
 * no caller-side behavior change.
 */

#include "oa_atmel.h"

namespace {

struct DeaxState {
	unsigned char gpa;
	unsigned char RA, RB, RC, RD, RE, RF, RG;	/* 5-bit-modulus bank */
	unsigned char SA, SB, SC, SD, SE, SF, SG;	/* 7-bit-modulus bank */
	unsigned char TA, TB, TC, TD, TE;		/* 5-bit-modulus bank */
};

void DeaxInit(DeaxState &d)
{
	d.gpa = 0;
	d.RA = d.RB = d.RC = d.RD = d.RE = d.RF = d.RG = 0;
	d.SA = d.SB = d.SC = d.SD = d.SE = d.SF = d.SG = 0;
	d.TA = d.TB = d.TC = d.TD = d.TE = 0;
}

/* Ported from AT88VirtualChip/chip_state.cpp's own deax_step() -- see
 * that file's header comment for the hardware-validated derivation;
 * not re-derived from the raw disassembly here. */
void DeaxStep(DeaxState &d, unsigned char in)
{
	unsigned char v = (unsigned char)(in ^ d.gpa);
	unsigned char bl_in = (unsigned char)((v >> 5) | ((v & 0xf) << 3));
	unsigned char dl_in = (unsigned char)(v & 0x1f);

	/* R-bank: new_RA = rot5L1(RG) + RD, mod-5bit */
	unsigned char rg_rot = (unsigned char)(((d.RG & 0xf) << 1) | (d.RG >> 4));
	unsigned char sum_r = (unsigned char)(rg_rot + d.RD);
	unsigned char new_ra = (sum_r >= 0x20) ? (unsigned char)(sum_r - 0x1f) : sum_r;
	unsigned char old_rd = d.RD;
	unsigned char old_ra = d.RA, old_rb = d.RB, old_rc = d.RC;
	unsigned char old_re = d.RE, old_rf = d.RF;
	d.RA = new_ra;
	d.RB = old_ra;
	d.RC = old_rb;
	d.RD = (unsigned char)(dl_in ^ old_rc);
	d.RE = old_rd;
	d.RF = old_re;
	d.RG = old_rf;
	unsigned char cl_t = (unsigned char)(new_ra ^ old_rd);

	/* S-bank: new_SA = rot7L1(SG) + SF, mod-7bit */
	unsigned char sg_rot = (unsigned char)(((d.SG & 0x3f) << 1) | (d.SG >> 6));
	unsigned char sum_s = (unsigned char)(sg_rot + d.SF);
	unsigned char new_sa = (sum_s >= 0x80) ? (unsigned char)(sum_s - 0x7f) : sum_s;
	unsigned char old_sa = d.SA, old_sb = d.SB, old_sc = d.SC;
	unsigned char old_sd = d.SD, old_se = d.SE, old_sf = d.SF;
	d.SA = new_sa;
	d.SB = old_sa;
	d.SC = old_sb;
	d.SD = old_sc;
	d.SE = old_sd;
	d.SF = (unsigned char)(bl_in ^ old_se);
	d.SG = old_sf;

	/* T-bank: new_TA = TC + TE, mod-5bit */
	unsigned char sum_t = (unsigned char)(d.TC + d.TE);
	unsigned char new_ta = (sum_t >= 0x20) ? (unsigned char)(sum_t - 0x1f) : sum_t;
	unsigned char old_tc = d.TC;
	unsigned char old_ta = d.TA, old_tb = d.TB, old_td = d.TD;
	d.TA = new_ta;
	d.TB = old_ta;
	d.TC = (unsigned char)((v >> 3) ^ old_tb);
	d.TD = old_tc;
	d.TE = old_td;

	/* MUX and GPA update */
	unsigned char bl_t = (unsigned char)(new_ta ^ old_tc);
	unsigned char mux = (unsigned char)((bl_t & new_sa) | ((unsigned char)~new_sa & cl_t));
	d.gpa = (unsigned char)(((d.gpa & 0xf) << 4) | (mux & 0xf));
}

/* File-scoped persistent state, not a stack-local -- see this file's own
 * header comment ("IMPORTANT, found this batch...") for why: ground
 * truth's own equivalent state lives in persistent .bss, and its final
 * `gpa` value is a real, observable postcondition two other (not yet
 * reconstructed) sibling functions read as a continuing keystream.
 * Deliberately left inside THIS file's own anonymous namespace for now
 * (matching this project's established "give an isolated concern its
 * own file-local storage until something else needs it" convention,
 * e.g. CSTGStreamingEventManager::sInstance's own homing history,
 * sec 10.158) -- a future batch reconstructing either sibling should
 * move `DeaxState`/this instance into a shared header at that point,
 * not before. */
DeaxState g_atmelDeaxState;

} /* anonymous namespace */

void cm_AuthenEncryptMAC(const unsigned char *c1, const unsigned char *kin,
			 unsigned char *iv,
			 unsigned char *c2out, unsigned char *c3out)
{
	DeaxState &d = g_atmelDeaxState;
	DeaxInit(d);

	/* Phase 1: 8 groups of "3 steps + 3 steps + 1 step" -- 4 groups
	 * driven by iv (p3-equivalent) interleaved with the front half of
	 * c1 (nc-equivalent), then 4 more driven by kin (p2-equivalent)
	 * interleaved with the back half of c1. */
	for (int i = 0; i < 4; i++) {
		DeaxStep(d, iv[2 * i]);     DeaxStep(d, iv[2 * i]);     DeaxStep(d, iv[2 * i]);
		DeaxStep(d, iv[2 * i + 1]); DeaxStep(d, iv[2 * i + 1]); DeaxStep(d, iv[2 * i + 1]);
		DeaxStep(d, c1[i]);
	}
	for (int i = 0; i < 4; i++) {
		DeaxStep(d, kin[2 * i]);     DeaxStep(d, kin[2 * i]);     DeaxStep(d, kin[2 * i]);
		DeaxStep(d, kin[2 * i + 1]); DeaxStep(d, kin[2 * i + 1]); DeaxStep(d, kin[2 * i + 1]);
		DeaxStep(d, c1[i + 4]);
	}

	/* Phase 2: 8 output bytes into c2out -- 6 steps before the first,
	 * 7 steps before each subsequent one. */
	for (int i = 0; i < 6; i++)
		DeaxStep(d, 0);
	c2out[0] = d.gpa;
	for (int i = 1; i < 8; i++) {
		for (int j = 0; j < 7; j++)
			DeaxStep(d, 0);
		c2out[i] = d.gpa;
	}

	/* Phase 3: iv is mutated IN PLACE here -- iv[0] is forced to 0xff,
	 * iv[1..7] are read back from the cipher state 2 steps apart. */
	iv[0] = 0xff;
	DeaxStep(d, 0); DeaxStep(d, 0);
	for (int i = 1; i < 8; i++) {
		iv[i] = d.gpa;
		DeaxStep(d, 0); DeaxStep(d, 0);
	}

	/* Phase 4: 8 more output bytes into c3out, 2 steps apart, plus one
	 * extra trailing step after the last byte (confirmed real in the
	 * disassembly -- matches deax_compute_challenges()'s own documented
	 * identical quirk: "loop already adds 2 steps after p5[7]; real
	 * total is 3, so 1 more here"). */
	for (int i = 0; i < 8; i++) {
		c3out[i] = d.gpa;
		DeaxStep(d, 0); DeaxStep(d, 0);
	}
	DeaxStep(d, 0);
}

/*
 * bzzzzzzzzzzzt11() (batch 46, real ground-truth name, .text+0x4f4180,
 * 144 bytes) -- the DEAX cipher's own real init primitive (a plain
 * 20-byte zero-init, byte-for-byte equivalent to DeaxInit() above, just
 * in a different field order -- functionally identical since both fully
 * zero every field). NOT called by fFfFfFfFfFfF13/fFfFfFfFfFfF1C
 * (neither resets the cipher -- both continue from whatever state is
 * already there, matching ground truth). Exposed anyway: it's a real,
 * already-internally-validated (via cm_AuthenEncryptMAC's own confirmed
 * 20-byte zero-init preamble) ground-truth symbol, and having a real
 * reset entry point is useful for tests that need to reach a known,
 * reproducible cipher state (see test_atmel_zone_io.cpp).
 */
extern "C" void bzzzzzzzzzzzt11(void)
{
	DeaxInit(g_atmelDeaxState);
}

/*
 * bzzzzzzzzzzzt12(in) (batch 46, real ground-truth name, .text+0x4f3d00) --
 * the DEAX cipher's real single-step primitive, called directly by
 * fFfFfFfFfFfF13/fFfFfFfFfFfF1C (src/auth/atmel_zone_io.cpp) as well as
 * inlined into cm_AuthenEncryptMAC's own body above. Declared in
 * oa_atmel.h. Operates on the exact same persistent `g_atmelDeaxState`
 * cm_AuthenEncryptMAC fully re-initializes on every one of its own calls
 * -- callers that need a "clean slate" must still go through
 * cm_AuthenEncryptMAC (or a future `bzzzzzzzzzzzt11`/DeaxInit bridge, not
 * needed by either currently-reconstructed sibling), not this function.
 */
extern "C" void bzzzzzzzzzzzt12(unsigned char in)
{
	DeaxStep(g_atmelDeaxState, in);
}

/*
 * DeaxCurrentGpa() -- NOT a ground-truth symbol of its own (the real
 * disassembly just reads `byte ds:0x5c90c1` directly inline at each of
 * fFfFfFfFfFfF13/fFfFfFfFfFfF1C's own decode sites); exposed here purely
 * as a same-behavior accessor so atmel_zone_io.cpp doesn't need its own
 * copy of (or direct access to) `DeaxState`.
 */
extern "C" unsigned char DeaxCurrentGpa(void)
{
	return g_atmelDeaxState.gpa;
}
