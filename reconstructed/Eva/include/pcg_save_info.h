/*
 * pcg_save_info.h  -  CPcgSaveInfo, a real class (confirmed via nm -C, 30
 * methods total) that tracks which PCG (Program/Combi/Global) banks are
 * selected for save/export. Fresh cluster, reconstructed 2026-07-29 --
 * CDDriverIO (round 52) survey turned up CFilePcg's own calcsizesaveXxx
 * family reading this class byte-for-byte, which led here.
 *
 * REAL SHAPE (confirmed by cross-referencing the ctor against Clear(),
 * HasNothingToSave(), Compare(), and every setsaveXxxbank* method -- all
 * agree on the same offsets):
 *   +0x00 ushort progLo     +0x04 ushort progHi     (prog bank bitmask)
 *   +0x08 ushort combiLo    +0x0c ushort combiHi    (combi bank bitmask)
 *   +0x10 ushort dkitLo     +0x14 ushort dkitHi     (drum kit bank bitmask)
 *   +0x18 ushort wseqLo     +0x1c ushort wseqHi     (wave-seq bank bitmask)
 *   +0x20 byte flagA, +0x21 byte flagB, +0x23 byte flagC (real, all 3 set
 *         to 1 by the ctor, cleared together by Clear()/checked together
 *         by HasNothingToSave())
 *   +0x24 dword (ctor sets 1; ProcessEndian() treats only its low 16 bits
 *         as a byte-swappable value and zero-extends the result back into
 *         the full 4 bytes -- transcribed as ground truth has it, not
 *         "corrected")
 *
 * Real but NOT independently placed by anything in this batch: +0x02,
 * +0x0a, +0x12, +0x1a (each set once by the ctor to a small constant --
 * 0x15/0xe/0xf/0xf -- but never read/written by any of the 13 methods
 * landed here) and +0x22 (real: CPcgSaveInfo::Compare() deliberately masks
 * it OUT of its 4-byte +0x20 equality check via `& 0xff00ffff`, proving the
 * byte exists and is intentionally excluded -- not part of this ctor's own
 * writes). Modeled as opaque padding bytes at their real offsets, same
 * "declared uncertain fields clearly" convention used throughout this
 * project. Likely used by CPcgSaveInfo::getsaveitem() (1974B, deferred,
 * see below).
 *
 * DEFERRED, 2 reasons (17 of 30 methods, not implemented this pass):
 *   - HasSaveProgBankInfo/CombiBankInfo/DkitBankInfo/WseqBankInfo (4),
 *     SetSaveProgBankInfo/CombiBankInfo/DkitBankInfo/WseqBankInfo (4),
 *     ClearExceptAppointSaveXxxBankInfo (4), ClearSaveXxxBankInfo (4):
 *     all 16 index into a real compiler-generated switch/jump table
 *     (`CSWTCH_128`/`131`/`134`/`137`, one dword per enumerator packing
 *     two 16-bit masks) whose actual .rodata contents aren't cheaply
 *     recoverable from the available export data -- deferred rather than
 *     guessing the per-enumerator mask values.
 *   - getsaveitem() (1974B): far larger than anything else in this class,
 *     genuinely deep logic, not attempted this pass.
 */
#ifndef PCG_SAVE_INFO_H
#define PCG_SAVE_INFO_H

class CPcgSaveInfo {
public:
	CPcgSaveInfo();

	void Clear(CPcgSaveInfo &other);
	bool HasNothingToSave() const;
	bool Compare(CPcgSaveInfo &other) const;
	void ProcessEndian();

	void setsaveprogbank(int b0, int b1, int b2, int b3, int b4, int b5);
	void setsavecombibank(int b0, int b1, int b2, int b3, int b4, int b5, int b6);
	void setsavedkitbank(int enable);
	void setsavewseqbank(int enable);

	void setsaveprogbank_exb(int b0, int b1, int b2, int b3, int b4, int b5, int b6,
	                          int b7, int b8, int b9, int b10, int b11, int b12, int b13);
	void setsavecombibank_exb(int b0, int b1, int b2, int b3, int b4, int b5, int b6);
	void setsavedkitbank_exb(int b0, int b1, int b2, int b3, int b4, int b5, int b6,
	                          int b7, int b8, int b9, int b10, int b11, int b12, int b13);
	void setsavewseqbank_exb(int b0, int b1, int b2, int b3, int b4, int b5, int b6,
	                          int b7, int b8, int b9, int b10, int b11, int b12, int b13);

private:
	unsigned char _unknown_0_1f[0x20];
	unsigned char flagA;
	unsigned char flagB;
	unsigned char _unknown_22;
	unsigned char flagC;
	unsigned int _unknown_24;
};

#endif /* PCG_SAVE_INFO_H */
