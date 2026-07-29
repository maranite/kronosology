/*
 * es_disk_task.h  -  CESDiskTask, round 45 (solo, 2026-07-29).
 *
 * FOUND via a fresh manifest survey filtered to pending methods with no
 * in_stack_ffffffXX/unaff_/"Could not recover" warnings, sorted by class
 * average size -- CESDiskTask (143 pending methods, avg 145.8 bytes)
 * surfaced as an unclaimed cluster, ALL 143 of which are warning-free.
 * 39/143 landed this round (see below for the 3 shapes); the remaining
 * 104 are deferred for 2 distinct reasons (see the .cpp's own header
 * comment) -- a large, genuinely tractable backlog for a future round,
 * not a dead end.
 *
 * REAL BASE CLASSES (from CESDiskTask::CESDiskTask@08ddd100.c, the ctor
 * itself -- 1767 bytes, deferred this round, but its own base-construction
 * calls are unambiguous): `CTask::CTask(this,owner,"DiskTask",4,1,0x804b)`
 * then `CEditable::CEditable(this+0x7c, ...)` -- same "CTask(0x7c) +
 * CEditable(4)" layout already established for CESDiskCommandTask
 * (es_disk_command_task.h, round 44) and CEditTask (edit_task.h).
 *
 * Fields confirmed by the 3 landed "this-offset" accessors below (all
 * GET-side only; the SET-side counterparts are larger/deferred, see
 * .cpp):
 *   +0x7c..0x84  unmodeled CEditable base + gap (0x8 bytes)
 *   +0x84  mBankToWrite       GetBankProgToWrite's own 1st byte
 *   +0x85  mProgToWrite       GetBankProgToWrite's own 2nd byte, ALSO
 *                              reused by GetBankProgToWriteFullRange's
 *                              own 2nd byte (confirmed shared field,
 *                              not a coincidence -- same real formula
 *                              shape `bank*0x80 + prog` in both)
 *   +0x86  mCombiBankToWrite  GetBankCombiToWrite's own 1st byte
 *   +0x87  mCombiToWrite      GetBankCombiToWrite's own 2nd byte
 *   +0x88  mFullRangeBankToWrite  GetBankProgToWriteFullRange's own
 *                              1st byte (a DIFFERENT bank field than
 *                              +0x84, real quirk preserved verbatim)
 *   +0x89..0x8b  unmodeled gap (3 bytes, not touched by any landed
 *                              method)
 *   +0x8c  mOscTypeToWrite    a real 4-byte field, but SetOscTypeToWrite's
 *                              own real body only ever reads *param_2 (ONE
 *                              byte, zero-extended) despite the field's
 *                              own 4-byte width; GetOscTypeToWrite reads
 *                              back only the low 16 bits as a short --
 *                              both real, preserved verbatim, not "fixed"
 *                              to the field's own full 32-bit width
 *
 * All 3 GET*ToWrite formulas (`bank*0x80 + secondary`) match this
 * project's own already-established "packed bank+index" idiom seen
 * elsewhere (e.g. CKGParamEdit's own SendGEValue family, OA.ko).
 *
 * `descCESDiskTask`/`descCESDiskTaskLd1CombiDialog`/etc (the ctor's own
 * ~30 AddDescriptorsMap calls) are SDescriptor tables whose contents are
 * unrecovered -- same "table contents unknown, real caller shape IS
 * reconstructable" distinction already established project-wide, not
 * needed for any of this round's own landed methods anyway.
 */

#ifndef ES_DISK_TASK_H
#define ES_DISK_TASK_H

#include "task.h"

/* CDiskUtil -- real class, 59 pending methods of its own (separate
 * cluster, out of scope this round). Only this ONE method is needed
 * here (SetWriteExcl's own real callee); given a real body (identity
 * pass-through stand-in, see es_disk_task.cpp) rather than left as a
 * pure declaration, since Eva's own `make verify` links every test
 * target against the FULL reconstructed object tree -- any called-but-
 * undefined extern would break every other test binary, not just this
 * one (established project convention: minimal no-op stand-in, not a
 * bare extern). CDiskUtil's own remaining 58 methods stay untouched. */
struct CDiskUtil {
	static unsigned char WriteByteToSharedBuffer(unsigned char value);
};

/* CLoadSampleDlogMgr -- real class, only this ONE static member
 * confirmed (a fixed 0xec-byte filename buffer GetLoadSampleDialog/
 * SetLoadSampleDialog round-trip through CopyBytes) -- own class layout
 * otherwise entirely out of scope, same convention as CDiskUtil above. */
struct CLoadSampleDlogMgr {
	static unsigned char sm_caLoadFileName[0xec];
};

class CESDiskTask : public CTask {
public:
	/* Test-only, default-constructible via CTask's own protected Tier-B
	 * ctor (task.h) -- same established convention as
	 * es_disk_command_task.h's own CESDiskCommandTask (round 44). None
	 * of this round's 39 methods touch CTask's own state or any
	 * vtable. The real ctor (1767 bytes, needs the unrecovered
	 * descCESDiskTask* SDescriptor tables) is deferred, see .cpp. */
	CESDiskTask() : CTask() {}

	/* --- plain static-global accessors, round 45 --- */
	unsigned int GetFilerMsg(unsigned char, unsigned char *value);
	unsigned int SetFilerMsg(unsigned char, const unsigned char *value);
	unsigned int GetResultWriteExcl(unsigned char, unsigned char *value);
	unsigned int GetProgress(unsigned char, unsigned char *value);
	unsigned int SetProgress(unsigned char, const unsigned char *value);
	unsigned int GetMultipleSelect(unsigned char, unsigned char *value);
	unsigned int SetMultipleSelect(unsigned char, const unsigned char *value);
	unsigned int GetNotifyFileSelected(unsigned char, unsigned char *value);
	unsigned int SetNotifyFileSelected(unsigned char, const unsigned char *value);
	/* Real callee CDiskUtil::WriteByteToSharedBuffer's own return value
	 * IS consumed here (unlike many project "eax discarded" cases). */
	unsigned int SetWriteExcl(unsigned char, const unsigned char *value);
	/* cc=__cdecl, no `this` at all in ground truth -- genuinely static,
	 * matches CSTGKeyTrack's own InitializeQuad precedent (OA.ko). */
	static char *GetDefaultFileName();

	/* --- this-offset accessors, round 45 (GET side only, see header
	 * comment -- SET side is larger/deferred) --- */
	unsigned int GetBankProgToWrite(unsigned char, char *value) const;
	unsigned int GetBankProgToWriteFullRange(unsigned char, unsigned char *value) const;
	unsigned int GetBankCombiToWrite(unsigned char, char *value) const;
	unsigned int GetOscTypeToWrite(unsigned char, unsigned char *value) const;
	unsigned int SetOscTypeToWrite(unsigned char, const unsigned char *value);

	/* --- single-branch "index-gated dialog" static-global accessors,
	 * round 45. Every one of these ONLY acts when the real ground-truth
	 * param_1 == 0 (the common "primary" dialog field slot) -- other
	 * param_1 values are real no-ops in ground truth (preserved
	 * verbatim, not "completed"). --- */
	unsigned int GetLdCombiBankDialog(unsigned char, unsigned char *value);
	unsigned int SetLdCombiBankDialog(unsigned char, const unsigned char *value);
	unsigned int GetLdDkitBankDialog(unsigned char, unsigned char *value);
	unsigned int SetLdDkitBankDialog(unsigned char, const unsigned char *value);
	unsigned int GetLdKarmaGEBankDialog(unsigned char, unsigned char *value);
	unsigned int SetLdKarmaGEBankDialog(unsigned char, const unsigned char *value);
	unsigned int GetLdTemplateBankDialog(unsigned char, unsigned char *value);
	unsigned int SetLdTemplateBankDialog(unsigned char, const unsigned char *value);
	unsigned int GetLdProgBankDialog(unsigned char, unsigned char *value);
	unsigned int SetLdProgBankDialog(unsigned char, const unsigned char *value);
	unsigned int GetLdWseqBankDialog(unsigned char, unsigned char *value);
	unsigned int SetLdWseqBankDialog(unsigned char, const unsigned char *value);
	unsigned int GetLoadSampleDialog(unsigned char, unsigned char *value);
	unsigned int SetLoadSampleDialog(unsigned char, const unsigned char *value);
	unsigned int GetLoadRegionsDialog(unsigned char, unsigned char *value);
	unsigned int SetLoadRegionsDialog(unsigned char, const unsigned char *value);
	unsigned int GetEraseCDRWDialog(unsigned char, unsigned char *value);
	unsigned int SetEraseCDRWDialog(unsigned char, const unsigned char *value);
	unsigned int GetDeleteDialog(unsigned char, unsigned char *value);
	unsigned int SetDeleteDialog(unsigned char, const unsigned char *value);
	unsigned int GetNewDirDialog(unsigned char, unsigned char *value);
	unsigned int SetNewDirDialog(unsigned char, const unsigned char *value);

	/* CopyBytes -- ground truth is a 634-byte compiler-unrolled byte-copy
	 * loop (classic GCC memcpy inlining, Duff's-device-shaped) that ALSO
	 * null-terminates dst[len] whenever len<=0xf0 -- transcribed as its
	 * real SEMANTIC operation (memcpy + conditional null-term), not the
	 * literal unrolled instruction sequence, matching this project's
	 * established "recover the real operation, not the compiler's
	 * unrolling artifacts" convention. Confirmed equivalent by re-tracing
	 * every one of the real function's own basic blocks: len<1 skips the
	 * copy entirely (still reaches the null-term tail); len>0xf0 skips
	 * the null-term (early return); every other len does both. */
	void CopyBytes(unsigned char *dst, const unsigned char *src, int len);

private:
	unsigned char mUnknownGap[8];       /* +0x7c..+0x84, see header comment */
	unsigned char mBankToWrite;         /* +0x84 */
	unsigned char mProgToWrite;         /* +0x85 */
	unsigned char mCombiBankToWrite;    /* +0x86 */
	unsigned char mCombiToWrite;        /* +0x87 */
	unsigned char mFullRangeBankToWrite; /* +0x88 */
	unsigned char mUnknown89[3];        /* +0x89..+0x8b, not touched by any landed method */
	unsigned int mOscTypeToWrite;       /* +0x8c */
};

#endif /* ES_DISK_TASK_H */
