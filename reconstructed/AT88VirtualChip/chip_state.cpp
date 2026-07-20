// SPDX-License-Identifier: GPL-2.0
/*
 * chip_state.cpp  -  see at88_chip.h.
 *
 * The DEAX cipher (deax_init/deax_step/deax_compute_challenges) is ported
 * directly from `KronosExtract/source/kronos_extract.c`'s own
 * deax_init()/deax_step()/deax_compute_challenges() -- already reverse-
 * engineered and hardware-validated by an earlier phase of this project
 * (those functions are themselves labeled "software replica of
 * bzzzzzzzzzzzt12"/"fFfFfFfFfFfF11", the real OA.ko symbols). Not
 * re-derived here; ported, with the bare file-scope globals kronos_extract.c
 * uses turned into an explicit DeaxState struct so more than one session can
 * exist at once, and comments trimmed to what's specific to this file.
 */

#include "at88_chip.h"

void deax_init(struct DeaxState *d)
{
	d->gpa = 0;
	d->RA = d->RB = d->RC = d->RD = d->RE = d->RF = d->RG = 0;
	d->SA = d->SB = d->SC = d->SD = d->SE = d->SF = d->SG = 0;
	d->TA = d->TB = d->TC = d->TD = d->TE = 0;
}

void deax_step(struct DeaxState *d, unsigned char in)
{
	unsigned char v, bl_in, dl_in;
	unsigned char rg_rot, sum_r, new_ra, old_rd;
	unsigned char old_ra, old_rb, old_rc, old_re, old_rf;
	unsigned char cl_t;
	unsigned char sg_rot, sum_s, new_sa;
	unsigned char old_sa, old_sb, old_sc, old_sd, old_se, old_sf;
	unsigned char sum_t, new_ta, old_tc;
	unsigned char old_ta, old_tb, old_td;
	unsigned char bl_t, mux;

	v     = in ^ d->gpa;
	bl_in = (unsigned char)((v >> 5) | ((v & 0xf) << 3));
	dl_in = (unsigned char)(v & 0x1f);

	/* R-bank: new_RA = rot5L1(RG) + RD, mod-5bit */
	rg_rot = (unsigned char)(((d->RG & 0xf) << 1) | (d->RG >> 4));
	sum_r  = (unsigned char)(rg_rot + d->RD);
	new_ra = (sum_r >= 0x20) ? (unsigned char)(sum_r - 0x1f) : sum_r;
	old_rd = d->RD;
	old_ra = d->RA; old_rb = d->RB; old_rc = d->RC;
	old_re = d->RE; old_rf = d->RF;
	d->RA = new_ra;
	d->RB = old_ra;
	d->RC = old_rb;
	d->RD = (unsigned char)(dl_in ^ old_rc);
	d->RE = old_rd;
	d->RF = old_re;
	d->RG = old_rf;
	cl_t = (unsigned char)(new_ra ^ old_rd);

	/* S-bank: new_SA = rot7L1(SG) + SF, mod-7bit */
	sg_rot = (unsigned char)(((d->SG & 0x3f) << 1) | (d->SG >> 6));
	sum_s  = (unsigned char)(sg_rot + d->SF);
	new_sa = (sum_s >= 0x80) ? (unsigned char)(sum_s - 0x7f) : sum_s;
	old_sa = d->SA; old_sb = d->SB; old_sc = d->SC;
	old_sd = d->SD; old_se = d->SE; old_sf = d->SF;
	d->SA = new_sa;
	d->SB = old_sa;
	d->SC = old_sb;
	d->SD = old_sc;
	d->SE = old_sd;
	d->SF = (unsigned char)(bl_in ^ old_se);
	d->SG = old_sf;

	/* T-bank: new_TA = TC + TE, mod-5bit */
	sum_t  = (unsigned char)(d->TC + d->TE);
	new_ta = (sum_t >= 0x20) ? (unsigned char)(sum_t - 0x1f) : sum_t;
	old_tc = d->TC;
	old_ta = d->TA; old_tb = d->TB; old_td = d->TD;
	d->TA = new_ta;
	d->TB = old_ta;
	d->TC = (unsigned char)((v >> 3) ^ old_tb);
	d->TD = old_tc;
	d->TE = old_td;

	/* MUX and GPA update */
	bl_t = (unsigned char)(new_ta ^ old_tc);
	mux  = (unsigned char)((bl_t & new_sa) | ((unsigned char)~new_sa & cl_t));
	d->gpa = (unsigned char)(((d->gpa & 0xf) << 4) | (mux & 0xf));
}

void deax_compute_challenges(struct DeaxState *d, const unsigned char *nc,
			     const unsigned char *p2, unsigned char *p3,
			     unsigned char *q_out, unsigned char *p5_out)
{
	int i;
	deax_init(d);

	/* Phase 1: 8 groups x 7 steps (4 groups from p3, 4 from p2, interleaved with nc) */
	for (i = 0; i < 4; i++) {
		deax_step(d, p3[2*i]);   deax_step(d, p3[2*i]);   deax_step(d, p3[2*i]);
		deax_step(d, p3[2*i+1]); deax_step(d, p3[2*i+1]); deax_step(d, p3[2*i+1]);
		deax_step(d, nc[i]);
	}
	for (i = 0; i < 4; i++) {
		deax_step(d, p2[2*i]);   deax_step(d, p2[2*i]);   deax_step(d, p2[2*i]);
		deax_step(d, p2[2*i+1]); deax_step(d, p2[2*i+1]); deax_step(d, p2[2*i+1]);
		deax_step(d, nc[i+4]);
	}

	/* Phase 2: output Q (8 bytes). 6 zero steps before Q[0], 7 before Q[1..7] */
	deax_step(d, 0); deax_step(d, 0); deax_step(d, 0);
	deax_step(d, 0); deax_step(d, 0); deax_step(d, 0);
	q_out[0] = d->gpa;
	for (i = 1; i < 8; i++) {
		deax_step(d, 0); deax_step(d, 0); deax_step(d, 0);
		deax_step(d, 0); deax_step(d, 0); deax_step(d, 0); deax_step(d, 0);
		q_out[i] = d->gpa;
	}

	/* Phase 3: update p3 and p5 in-place. */
	p3[0] = 0xff;
	deax_step(d, 0); deax_step(d, 0);
	for (i = 1; i < 8; i++) {
		p3[i] = d->gpa;
		deax_step(d, 0); deax_step(d, 0);
	}
	for (i = 0; i < 8; i++) {
		p5_out[i] = d->gpa;
		deax_step(d, 0); deax_step(d, 0);
	}
	/* loop already adds 2 steps after p5[7]; real total is 3, so 1 more here */
	deax_step(d, 0);
}

/* Standard CRC-32 (IEEE 802.3, init=~0, final XOR ~0 -- matches zlib.crc32()
 * and Linux's crc32_le(), confirmed against the real captured
 * KronosExtract.bin's own trailing crc32 field in verify/test_chip_state.cpp). */
static unsigned int crc32_ieee(const unsigned char *buf, unsigned int len)
{
	unsigned int crc = 0xFFFFFFFFu;
	for (unsigned int i = 0; i < len; i++) {
		crc ^= buf[i];
		for (int b = 0; b < 8; b++)
			crc = (crc >> 1) ^ (0xEDB88320u & (unsigned int)(-(int)(crc & 1)));
	}
	return crc ^ 0xFFFFFFFFu;
}

int at88_chip_load_from_extract(struct AT88ChipState *chip,
				 const unsigned char *blob, unsigned int blobLen)
{
	/* Confirmed layout (KronosExtract.bin, 188 bytes total):
	 *   off 0    "KREX" magic
	 *   off 4    version (1)
	 *   off 5    flags (bit0=pf3_ok, bit1=exs_ok)
	 *   off 8    cfg[128]
	 *   off 136  pf3_zone[24]  = Zone0[0x00..0x17]
	 *   off 160  exs_zone[24]  = Zone0[0x10..0x27]
	 *   off 184  crc32(4), over bytes 0..183
	 */
	if (blobLen != 188)
		return -1;
	if (blob[0] != 'K' || blob[1] != 'R' || blob[2] != 'E' || blob[3] != 'X')
		return -2;

	unsigned int storedCrc = (unsigned int)blob[184]
				| ((unsigned int)blob[185] << 8)
				| ((unsigned int)blob[186] << 16)
				| ((unsigned int)blob[187] << 24);
	if (crc32_ieee(blob, 184) != storedCrc)
		return -3;

	unsigned char flags = blob[5];
	if (!(flags & 0x1) || !(flags & 0x2))
		return -4;	/* incomplete capture -- pf3 and/or exs not captured */

	const unsigned char *cfg      = blob + 8;
	const unsigned char *pf3_zone = blob + 136;	/* Zone0[0x00..0x17] */
	const unsigned char *exs_zone = blob + 160;	/* Zone0[0x10..0x27] */

	/* pf3_zone and exs_zone overlap at Zone0[0x10..0x17] (8 bytes) -- assert
	 * they actually agree rather than silently trusting one source. */
	for (int i = 0; i < 8; i++)
		if (pf3_zone[16 + i] != exs_zone[i])
			return -5;

	for (int i = 0; i < 128; i++)
		chip->configZone[i] = cfg[i];

	for (int i = 0; i < 16; i++)
		chip->zone0[i] = pf3_zone[i];		/* Zone0[0x00..0x0f] */
	for (int i = 0; i < 24; i++)
		chip->zone0[16 + i] = exs_zone[i];	/* Zone0[0x10..0x27] */

	deax_init(&chip->session);
	for (int i = 0; i < 8; i++) {
		chip->p2[i] = 0;
		chip->p3[i] = 0;
	}
	chip->b8RoundsAccepted = 0;

	chip->dataLoaded = 1;
	return 0;
}

void at88_chip_load_synthetic(struct AT88ChipState *chip)
{
	for (int i = 0; i < 128; i++)
		chip->configZone[i] = 0;

	/* AAC byte -- see at88_chip.h's own header comment for this function
	 * for the full derivation of why this MUST start saturated, not zero.
	 * kNv2acStatusZone (reconstructed/OA/src/auth/nv2ac_handshake.cpp) is
	 * {0x50,0x60,0x70,0x80} indexed by `sel` (0..3), but every real call
	 * site in this project's own reconstruction hardcodes sel=0 (both
	 * nv2ac_enable_cipher(0,...) and nv2ac_enable_encrypt(0,...)), so 0x50
	 * is the only one of the four ever actually read here. 0x60/0x70 are
	 * left at their zero default (never exercised, so harmless either
	 * way); 0x80 (=128) is NOT touched deliberately -- it is one past the
	 * end of this chip's own modeled 128-byte configZone (valid range
	 * 0x00..0x7f) and would silently alias into zone0[0] if written
	 * (caught by this file's own KAT, verify/test_chip_state.cpp). */
	chip->configZone[0x50] = 0xff;

	for (int i = 0; i < 40; i++)
		chip->zone0[i] = 0;

	chip->selectedZone = 0;
	deax_init(&chip->session);
	for (int i = 0; i < 8; i++) {
		chip->p2[i] = 0;
		chip->p3[i] = 0;
	}
	chip->b8RoundsAccepted = 0;

	chip->dataLoaded = 0;	/* honest: not real per-device data -- see header comment */
}

int at88_chip_read_config(const struct AT88ChipState *chip,
			   unsigned char addr, unsigned char len,
			   unsigned char *out)
{
	if ((unsigned int)addr + (unsigned int)len > sizeof(chip->configZone))
		return -1;
	for (unsigned char i = 0; i < len; i++)
		out[i] = chip->configZone[addr + i];
	return 0;
}

int at88_chip_read_zone0(struct AT88ChipState *chip, struct DeaxState *d,
			  unsigned char addr, unsigned char len,
			  unsigned char *out)
{
	if ((unsigned int)addr + (unsigned int)len > sizeof(chip->zone0))
		return -1;

	/* Pre-authentication: raw passthrough, no DEAX involved at all. See
	 * this function's doc comment in at88_chip.h for the ground truth
	 * (KRONOS_V06R06.VSB's CryptoAt88.cpp self-test) behind this branch. */
	if (chip->b8RoundsAccepted < 2) {
		for (unsigned char i = 0; i < len; i++)
			out[i] = chip->zone0[addr + i];
		return 0;
	}

	/* 12 pre-steps, matching kronos_extract.c's synth_zone0_read() exactly:
	 * step(0)x5, step(addr), step(0)x5, step(len). */
	deax_step(d, 0); deax_step(d, 0); deax_step(d, 0); deax_step(d, 0); deax_step(d, 0);
	deax_step(d, addr);
	deax_step(d, 0); deax_step(d, 0); deax_step(d, 0); deax_step(d, 0); deax_step(d, 0);
	deax_step(d, len);

	/* mode=2: XOR each real plaintext byte with the current keystream byte
	 * to produce the on-wire ciphertext, then step on the PLAINTEXT byte
	 * (matching synth_zone0_read()'s decrypt-then-step-on-plaintext order,
	 * since encrypt and decrypt are the same XOR here -- the chip and the
	 * host must step on the same value either way for the streams to stay
	 * synchronized). */
	for (unsigned char i = 0; i < len; i++) {
		unsigned char plain = chip->zone0[addr + i];
		out[i] = (unsigned char)(plain ^ d->gpa);
		deax_step(d, plain);
		deax_step(d, 0); deax_step(d, 0); deax_step(d, 0); deax_step(d, 0); deax_step(d, 0);
	}
	return 0;
}

int at88_chip_read_zone(struct AT88ChipState *chip, struct DeaxState *d,
			 unsigned char zone, unsigned char addr,
			 unsigned char len, unsigned char *out)
{
	/* Zone 0: the one real, ground-truthed, per-device secret -- unchanged
	 * behavior, see at88_chip_read_zone0()'s own doc comment. */
	if (zone == 0)
		return at88_chip_read_zone0(chip, d, addr, len, out);

	/* Any other zone: no captured ground truth exists (see this function's
	 * at88_chip.h doc comment) -- synthetic all-zero placeholder, always a
	 * raw passthrough. Bound check uses zone0[]'s own size purely for
	 * consistency; not a claim about any real zone's actual size. */
	if ((unsigned int)addr + (unsigned int)len > sizeof(chip->zone0))
		return -1;
	for (unsigned char i = 0; i < len; i++)
		out[i] = 0;
	return 0;
}

int at88_chip_write_zone0(struct AT88ChipState *chip, unsigned char addr,
			   unsigned char len, const unsigned char *in)
{
	if ((unsigned int)addr + (unsigned int)len > sizeof(chip->zone0))
		return -1;

	for (unsigned char i = 0; i < len; i++)
		chip->zone0[addr + i] = in[i];
	return 0;
}
