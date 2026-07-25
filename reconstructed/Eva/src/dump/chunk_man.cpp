/*
 * chunk_man.cpp  -  see include/chunk_man.h.
 *
 * Real vtable content confirmed by a direct .rodata byte read of
 * PTR__CChunkMan_08e85968 (Eva, VA 0x08e85968): 7 dwords, exactly {~CChunkMan,
 * ~CChunkMan(deleting), CChunkMan::Setup, CChunkMan::Config, CChunkMan::Start,
 * CModule::Destroy, CModule::GetErrorMsg} -- same clean 7-slot CModule-shaped
 * match as CDumpManMod/CEditMan's own vtables.
 *
 * CChkBaseTask/CChkCmd's own real vtables (PTR__CChkBaseTask_08e85648/
 * PTR__CChkCmd_08e85708, also confirmed by direct byte read) are both 8 slots --
 * 1 more than CTask's own base 7, confirmed against the installed-pointer-to-
 * next-symbol boundary (omega_vtables.h). Neither is ever dispatched through by
 * any reconstructed code (nothing destroys a CChkBaseTask/CChkCmd on this pass's
 * own traced boot path).
 */

#include "chunk_man.h"
#include "out_link.h"
#include "omega_vtables.h"
#include "system_api.h"
#include "omega_ptr_array.h"

#include <cstdlib>
#include <new>

extern CSystemApi *Api; /* mains.cpp */

/* ===== CChkBaseTask ===== */

CChkBaseTask::CChkBaseTask(const CModule &owner, const char *name, int level, int scheduleFlag)
	: CTask(owner, name, level, scheduleFlag, 0x8003)
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CChkBaseTask_08e85648;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
		&EvaDataPlaceholder_08e85668;

	new (mRegistrations) COmegaPtrArray(10, 5, 1);
	*reinterpret_cast<void **>(mRegistrations) = (void *)PTR__TPtrArray_08e85698;
}

/* ===== CChkCmd ===== */

CChkCmd::CChkCmd(const CModule &owner)
	: CChkBaseTask(owner, "ChkCmd" /* real: CChkCmd::sm_pkcTaskName, string content
	                                 * not decoded */, 4, 2)
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CChkCmd_08e85708;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
		&EvaDataPlaceholder_08e85728;
	mCommId = 0xff;

	void *raw = malloc(0x38);
	COutLinkMono *link = new (raw) COutLinkMono(
		*this, "ChkCmdInternalOutlink" /* real: CChkCmd::sm_pkcInternalOutlinkName,
		                                 * string content not decoded */,
		0, 0x8003);
	mOutLinkMono = link;

	Add(link);
}

/* ===== CChkCmdBG ===== */

CChkCmdBG::CChkCmdBG(const CModule &owner)
	: CChkBaseTask(owner, "ChkCmdBG" /* real: CChkCmdBG::sm_pkcTaskName, string
	                                   * content not decoded */, 5, 0),
	  mState(2), mHeap1(10, 5), mHeap2(10, 5), mPendingCount(0), mOutLinkMono(0)
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CChkCmdBG_08e85768;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
		&EvaDataPlaceholder_08e85788;

	void *raw = malloc(0x38);
	COutLinkMono *link = new (raw) COutLinkMono(
		*this, "ChkCmdBGInternalForNotify" /* real: CChkCmdBG::sm_pkcInternalForNotify,
		                                     * string content not decoded, but this
		                                     * exact literal is already used by
		                                     * CChunkMan::Config()'s own link-register
		                                     * call below */,
		0, 0x8003);
	mOutLinkMono = link;

	Add(link);
}

CChkCmdBG::~CChkCmdBG()
{
	/* Real dtor asserts mPendingCount == 0 via Api vtable slot +0x94 before
	 * tearing down -- assert call itself not modeled (same "real call, condition
	 * faithfully checked, string transcribed" treatment as tempo.cpp/
	 * edit_server.cpp/config_manager.cpp's own Api+0x94 asserts).
	 */
	if (mPendingCount != 0) {
		typedef void (*AssertFn)(void *, const char *, const char *, int);
		AssertFn fn = (AssertFn)(((void **)*(void **)Api)[0x94 / 4]);
		fn(Api, "Assertion failed in module %s, line %i.\n", "ChkCmdBG.cpp", 0x3f);
	}
	/* mHeap2/mHeap1/base dtor run automatically in reverse declaration order,
	 * matching the real dtor's explicit ~CHeap(+0xa8) then ~CHeap(+0x98) then
	 * ~CChkBaseTask(this) sequence. mOutLinkMono is not freed here, matching
	 * ground truth (the real dtor never touches +0xbc either).
	 */
}

/* ===== CChunkMan ===== */

CChunkMan::CChunkMan()
	: CModule("ChunkMan"), mChkCmdBG(0)
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CChunkMan_08e85968;
}

void CChunkMan::Setup()
{
	void *raw1 = malloc(0x9c);
	CChkCmd *chkCmd = new (raw1) CChkCmd(*this);
	Add(chkCmd);

	void *raw2 = malloc(0xc0);
	CChkCmdBG *chkCmdBG = new (raw2) CChkCmdBG(*this);
	mChkCmdBG = chkCmdBG;
	Add(chkCmdBG);
}

bool CChunkMan::Config()
{
	typedef int (*LinkRegisterFn)(void *, const char *, const char *, const char *,
	                               const char *, const char *, int);
	LinkRegisterFn fn = (LinkRegisterFn)(((void **)*(void **)Api)[17]);

	int result = fn(Api, "ChunkMan", "ChkCmd", "ChkCmdInternalOutlink",
	                 "ChunkMan", "ChkCmdBG", 0);
	if (result <= 0)
		return true;

	result = fn(Api, "ChunkMan", "ChkCmdBG", "ChkCmdBGInternalForNotify",
	            "ChunkMan", "ChkCmd", 0);
	return result <= 0;
}

void CChunkMan::Start()
{
	/* Confirmed genuinely empty (`return 0;`) in the real binary. */
}

extern "C" void CChunkManSetupVSlot(void *obj)
{
	static_cast<CChunkMan *>(obj)->Setup();
}

extern "C" void CChunkManConfigVSlot(void *obj)
{
	static_cast<CChunkMan *>(obj)->Config();
}

extern "C" void CChunkManStartVSlot(void *obj)
{
	static_cast<CChunkMan *>(obj)->Start();
}

/* Real vtable definitions -- moved here from omega_vtables.cpp, same "define
 * locally where the real forwarders live" precedent as dump_man_mod.cpp/
 * edit_man.cpp.
 */
void *PTR__CChunkMan_08e85968[7] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)CChunkManSetupVSlot, (void *)CChunkManConfigVSlot, (void *)CChunkManStartVSlot,
	(void *)EvaVTableStub, (void *)EvaVTableStub,
};

/* CChkBaseTask/CChkCmd's own real vtables + CChkBaseTask's own embedded
 * TPtrArray<CRegistrationEntry> vtable -- all install-only, see file header.
 */
void *PTR__CChkBaseTask_08e85648[8] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__TPtrArray_08e85698[3] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};
void *PTR__CChkCmd_08e85708[8] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};

/* CChkCmdBG's own real vtable -- confirmed by direct .rodata byte read (see file
 * header): 6 real function slots (dtor pair + Exec/ExecMsg/AcceptDuplicate-shaped
 * overrides, all out of scope per file header) followed by the this-adjusted
 * secondary vtable's own [offset_to_top][RTTI] preamble at slots 6/7 -- the 8-dword
 * size is thus CONFIRMED correct, not a heuristic guess.
 */
void *PTR__CChkCmdBG_08e85768[8] = {
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
	(void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub, (void *)EvaVTableStub,
};

/* Opaque data placeholders CChkBaseTask/CChkCmd/CChkCmdBG's own ctors store the
 * ADDRESS of at +0x08 (mIfcThunk) -- never dereferenced by any reconstructed code,
 * same treatment as EvaDataPlaceholder_08e82144 (omega_vtables.cpp). For
 * CChkCmdBG specifically this is confirmed to be the true start of its own
 * this-adjusted secondary vtable's vfunc array (see file header) -- still safe as
 * an opaque, uncalled placeholder since nothing dispatches through it.
 */
int EvaDataPlaceholder_08e85668;
int EvaDataPlaceholder_08e85728;
int EvaDataPlaceholder_08e85788;
