/*
 * es_disk_command_task.h  -  CESDiskCommandTask's 84-method "command trampoline"
 * family (round 44, solo, 2026-07-29).
 *
 * FOUND via a fresh eva_functions.csv survey filtered to pending methods with
 * NO in_stack_ffffffXX/unaff_/"Could not recover jumptable" warnings, grouped
 * by class, sorted by average method size -- CESDiskCommandTask (95 pending
 * methods, avg 89 bytes) surfaced as an unclaimed cluster. Of its 95 pending
 * methods, 84 share one exact 44-byte shape (LoadMultiFile@08ddddc0.c through
 * Load1MossProg@08de01d0.c): each writes a literal opcode constant to
 * `this+0xa8`, then calls the already-real `CTask::SetMask(0)`
 * (task.h/task.cpp, round-49-adjacent), then returns 1. Every one of the 84
 * ground-truth decompiles was read individually (not pattern-guessed) to
 * confirm both its exact opcode literal and that no method deviates from the
 * shape -- see stg_disk_command_task.cpp's own per-method literal table.
 *
 * The other 11 CESDiskCommandTask methods (2 ctors -- one is a duplicate/
 * thunk entry, 3 dtor variants, ExecuteLoadMultiFile/
 * ExecuteMakeAudioCommand/ExecuteUtilityCommand/ExecuteSaveCommand/
 * ExecuteLoadCommand/Exec) are DEFERRED, not attempted this round: the ctor
 * chains through `CESDiskCommandTaskBase` (its own 8-method base class, not
 * yet reconstructed -- CESDiskCommandTaskBase::CESDiskCommandTaskBase@08dddc60,
 * 178 bytes) and `CEditable::AddDescriptorsMap` against a real
 * `descCESDiskCommandTask` SDescriptor table (contents unrecovered) --
 * out of scope for a single round. Left for a future dedicated pass, same
 * "flagged, not guessed at" convention as every other project deferral.
 *
 * REAL LAYOUT for JUST what these 84 methods touch (everything else, `this`
 * offset 0x00 through 0xa8, is CTask's own base plus an unmodeled
 * CEditable+CESDiskCommandTaskBase gap -- see below):
 *   +0x00..0x7c  CTask base (task.h) -- confirmed: every trampoline method's
 *                own `CTask::SetMask(_param_1, 0)` call operates directly on
 *                the `this` pointer with zero adjustment, meaning CTask is
 *                the FIRST base (matches every other CTask-derived class in
 *                this project, e.g. edit_task.h/batch_disk_main_task.h).
 *   +0x7c..0xa8  UNMODELED gap (0x2c = 44 bytes) -- real ground truth
 *                (CESDiskCommandTask::CESDiskCommandTask@08ddedb0.c, not
 *                reconstructed this round, see above) shows a `CEditable`
 *                base at +0x7c (`CEditable::AddDescriptorsMap((CEditable*)
 *                (this+0x7c), ...)`, same convention as edit_task.h) followed
 *                by whatever `CESDiskCommandTaskBase`'s own fields are.
 *                Kept as opaque padding here -- same "preserve the real
 *                offset without fabricating an unconfirmed sub-layout"
 *                convention as oa_stg_key_track.h's own mUnknownRamps (OA.ko)
 *                and this project's own CPanelIfcTask mReserved84.
 *   +0xa8  mCommandOpcode (unsigned int) -- each trampoline's own literal
 *          write target; ExecuteLoadCommand/ExecuteSaveCommand/etc (deferred)
 *          presumably switch on this value, unconfirmed since those methods
 *          are out of scope this round.
 *
 * No vtable install/read is performed by any of these 84 methods (the ctor's
 * own `&PTR__CESDiskCommandTask_08fcc448` install is part of the deferred
 * ctor) -- so a bare CTask-derived object, default-constructed via CTask's
 * own protected Tier-B test ctor (task.h), is sufficient to exercise every
 * one of these bodies safely; no vtable dispatch is ever involved.
 */

#ifndef ES_DISK_COMMAND_TASK_H
#define ES_DISK_COMMAND_TASK_H

#include "task.h"

class CESDiskCommandTask : public CTask {
public:
	/* --- 84 command trampolines, round 44. Each writes its own real,
	 * individually-confirmed opcode literal (see stg_disk_command_task.cpp)
	 * to mCommandOpcode then calls the inherited CTask::SetMask(0).
	 * Grouped in ground-truth address order (Save* before most Load*,
	 * matching the real .text layout), not alphabetically. */
	unsigned int LoadMultiFile(unsigned char);
	unsigned int Blank(unsigned char);
	unsigned int Finalize(unsigned char);
	unsigned int BurnAudio(unsigned char);
	unsigned int StartMIDIReceiver(unsigned char);
	unsigned int StartSetDate(unsigned char);
	unsigned int FileUnprotect(unsigned char);
	unsigned int FileProtect(unsigned char);
	unsigned int OptimizeMedium(unsigned char);
	unsigned int CheckMedium(unsigned char);
	unsigned int RateConvert(unsigned char);
	unsigned int ConvertToIso(unsigned char);
	unsigned int Format(unsigned char);
	unsigned int CreateDir(unsigned char);
	unsigned int DeleteUnusedWav(unsigned char);
	unsigned int Delete(unsigned char);
	unsigned int Copy(unsigned char);
	unsigned int Rename(unsigned char);
	unsigned int SaveKfx(unsigned char);
	unsigned int Save1Song(unsigned char);
	unsigned int SaveKcd(unsigned char);
	unsigned int SaveAifWav(unsigned char);
	unsigned int SaveExclusive(unsigned char);
	unsigned int SaveSMF(unsigned char);
	unsigned int SaveKge(unsigned char);
	unsigned int SaveSample(unsigned char);
	unsigned int SaveSeq(unsigned char);
	unsigned int SavePcg(unsigned char);
	unsigned int SavePcgSeq(unsigned char);
	unsigned int SaveAll(unsigned char);
	unsigned int LoadKscItem(unsigned char);
	unsigned int LoadKontaktSample(unsigned char);
	unsigned int LoadKontaktInstrument(unsigned char);
	unsigned int LoadKontaktMulti(unsigned char);
	unsigned int LoadKontaktBank(unsigned char);
	unsigned int LoadSF2(unsigned char);
	unsigned int Load1Fx(unsigned char);
	unsigned int LoadFxBank(unsigned char);
	unsigned int LoadFxs(unsigned char);
	unsigned int LoadKfx(unsigned char);
	unsigned int LoadKcd(unsigned char);
	unsigned int LoadAkaiVolume(unsigned char);
	unsigned int LoadAkaiProg(unsigned char);
	unsigned int LoadAkaiSample(unsigned char);
	unsigned int LoadWav(unsigned char);
	unsigned int LoadAif(unsigned char);
	unsigned int LoadKsf(unsigned char);
	unsigned int LoadKmp(unsigned char);
	unsigned int LoadExclusive(unsigned char);
	unsigned int LoadSMF(unsigned char);
	unsigned int Load1Pattern(unsigned char);
	unsigned int LoadTracks(unsigned char);
	unsigned int Load1Song(unsigned char);
	unsigned int Load1Region(unsigned char);
	unsigned int LoadRegionBank(unsigned char);
	unsigned int LoadCueLists(unsigned char);
	unsigned int LoadTemplateBank(unsigned char);
	unsigned int LoadTemplates(unsigned char);
	unsigned int Load1GE(unsigned char);
	unsigned int LoadGEBank(unsigned char);
	unsigned int LoadGEs(unsigned char);
	unsigned int Load1SetListSlot(unsigned char);
	unsigned int Load1SetList(unsigned char);
	unsigned int LoadSetLists(unsigned char);
	unsigned int LoadGlobal(unsigned char);
	unsigned int Load1DrumTrackPattern(unsigned char);
	unsigned int LoadDrumTrackPatterns(unsigned char);
	unsigned int Load1Wseq(unsigned char);
	unsigned int LoadWseqBank(unsigned char);
	unsigned int LoadWaveSeqs(unsigned char);
	unsigned int Load1Dkit(unsigned char);
	unsigned int LoadDkitBank(unsigned char);
	unsigned int LoadDkits(unsigned char);
	unsigned int Load1Combi(unsigned char);
	unsigned int LoadCombiBank(unsigned char);
	unsigned int LoadCombis(unsigned char);
	unsigned int Load1Prog(unsigned char);
	unsigned int LoadSyx(unsigned char);
	unsigned int LoadProgBank(unsigned char);
	unsigned int LoadPrograms(unsigned char);
	unsigned int LoadPcgRamSmpl(unsigned char);
	unsigned int LoadAll(unsigned char);
	unsigned int LoadMossBank(unsigned char);
	unsigned int Load1MossProg(unsigned char);

protected:
	/* Test-only, default-constructible via CTask's own protected Tier-B
	 * ctor (task.h) -- see header comment: none of these 84 methods touch
	 * the unmodeled +0x7c..0xa8 gap or any vtable, so this is sufficient
	 * to exercise every one of them safely. Never invoked by any
	 * reconstructed real code path (the real ctor is deferred, see above).
	 */
	CESDiskCommandTask() : CTask() {}

private:
	void SetOpcode(unsigned int opcode)
	{
		*reinterpret_cast<unsigned int *>(reinterpret_cast<unsigned char *>(this) + 0xa8) = opcode;
		SetMask(0);
	}

	unsigned char mUnknownGap[0x2c]; /* +0x7c..+0xa8, see header comment */
	unsigned int mCommandOpcode;     /* +0xa8 */

	friend struct DiskCommandTaskTestHooks;
};

#endif /* ES_DISK_COMMAND_TASK_H */
