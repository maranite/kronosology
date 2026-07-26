/*
 * chunk_server.cpp  -  see include/chunk_server.h.
 *
 * Transcribed from Decomp/EVA_Decomp/eva_export/functions/
 * CChunkServer@080cbcf0.c (ctor), _CChunkServer@080cbb90.c (D1 dtor),
 * OnUnlock@080cba90.c / OnRelock@080cbaa0.c / OnBegin@080cbab0.c /
 * OnEnd@080cbac0.c / OnSave@080cbad0.c / OnSave@080cbae0.c /
 * OnLoad@080cbaf0.c / OnLoad@080cbb00.c / OnAbort@080cbb10.c /
 * OnStoppedByUser@080cbb20.c / GetSaveBuffSize@080cbb30.c /
 * GetServerID@080cbdc0.c / Unlock@080cbdf0.c / GetServerHandle@080cbe30.c.
 *
 * `Load()` (.text+0x080cbfd0) was promoted from Tier B to Tier A 2026-07-26 --
 * NOT transcribed from Ghidra's own decompile (it failed to recover the
 * indirect-call jumptable there), but directly from `objdump -dr -M intel`
 * register tracing. See chunk_server.h's own header comment for the full
 * derivation.
 */

#include "chunk_server.h"
#include "omega_vtables.h"
#include "system_api.h"

#include <cstdlib>

extern CSystemApi *Api;

namespace {

/* Real ground-truth external dependency: `GetResLength(unsigned int,
 * unsigned int, unsigned int)` (.text+0x080688d0) -- not referenced anywhere
 * else in this project. Modeled as an inert stand-in purely so
 * GetSaveBuffSize()'s own real `mAccessMode == 0` gate and `+ 0x800` add can
 * be transcribed faithfully, same "genuinely undecoded external call,
 * transcribed anyway" convention as panel_ifc_task.cpp's own
 * PegMessageQueuePush/ControlSurface stand-ins.
 */
int GetResLength(unsigned int, unsigned int, unsigned int)
{
	return 0;
}

typedef unsigned int (*POnCommandFn)(CChunkServer *, unsigned char, int, unsigned char, unsigned char *, unsigned long);

} /* anonymous namespace */

CChunkServer::CChunkServer(const CModule &owner, int accessMode)
	: CTask(owner, "ChunkServer", 5, 0, 0x8003)
	  /* mReserved7c deliberately left uninitialized -- ground truth's own
	   * ctor never touches this slot either (see header comment).
	   */
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CChunkServer_08e859a8;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
	    (void *)PTR__CChunkServer_08e859f0;

	mUnknown80 = 1;
	mEntryCount = 0;
	mUnknown88 = 1;

	unsigned char *buf = static_cast<unsigned char *>(malloc(2));
	buf[0] = 0xff;
	buf[1] = 0;
	mTableBuf = buf;

	mAccessMode = accessMode;
}

CChunkServer::~CChunkServer()
{
	unsigned char *ptr = mTableBuf;

	*reinterpret_cast<void **>(this) = (void *)PTR__CChunkServer_08e859a8;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
	    (void *)PTR__CChunkServer_08e859f0;

	if (ptr != 0)
		free(ptr);

	/* CTask::~CTask() runs automatically after this body returns (real
	 * single C++ inheritance), same as every other CTask-derived class in
	 * this project.
	 */
}

unsigned int CChunkServer::OnUnlock(unsigned char, int, unsigned char, unsigned char *, unsigned long)
{
	return 1;
}

unsigned int CChunkServer::OnRelock(unsigned char, int, unsigned char, unsigned char *, unsigned long)
{
	return 1;
}

unsigned int CChunkServer::OnBegin(unsigned char, int, unsigned char, unsigned char *, unsigned long)
{
	return 1;
}

unsigned int CChunkServer::OnEnd(unsigned char, int, unsigned char, unsigned char *, unsigned long)
{
	return 1;
}

unsigned int CChunkServer::OnSave(CChunk *, unsigned char, unsigned char *, unsigned long)
{
	return 0;
}

unsigned int CChunkServer::OnSave(unsigned long &, const unsigned char *&, unsigned char, unsigned char *, unsigned long)
{
	return 0;
}

unsigned int CChunkServer::OnLoad(CChunk *, unsigned char, unsigned char *, unsigned long)
{
	return 0;
}

unsigned int CChunkServer::OnLoad(unsigned long, unsigned char *, unsigned char, unsigned char *, unsigned long)
{
	return 0;
}

void CChunkServer::OnAbort(int)
{
}

void CChunkServer::OnStoppedByUser(int)
{
}

int CChunkServer::GetSaveBuffSize(unsigned char a, unsigned char b, unsigned char c) const
{
	int result = 0;
	if (mAccessMode == 0)
		result = GetResLength(a, b, c) + 0x800;
	return result;
}

unsigned int CChunkServer::GetServerID(int index) const
{
	if (mEntryCount == 0)
		return 0xffffffff;
	return mTableBuf[index * 2];
}

void CChunkServer::Unlock(unsigned char a, unsigned char b, unsigned char *c)
{
	/* Real: `(**(code**)(*(int*)this + 0x18))(this, a, 2, b, c, 0);` -- a
	 * genuine indirect call through this object's own installed vtable at
	 * primary-array index 6 (byte offset 0x18 from the installed pointer),
	 * see header comment for why index 6 is correct and why the real ECommand
	 * argument at this one real call site is the literal `2`. Discards the
	 * slot's own return value, matching ground truth's own void return.
	 */
	void **vtbl = *reinterpret_cast<void ***>(this);
	POnCommandFn fn = reinterpret_cast<POnCommandFn>(vtbl[6]);
	fn(this, a, 2, b, c, 0);
}

unsigned int CChunkServer::GetServerHandle(unsigned char key) const
{
	if (mEntryCount < 1)
		return 0xffffffff;

	for (int i = 0; i < mEntryCount; ++i) {
		if (mTableBuf[i * 2] == key)
			return mTableBuf[i * 2 + 1];
	}
	return 0xffffffff;
}

void CChunkServer::Load(CChunk *chunk, unsigned long a, unsigned char *b, unsigned char c, unsigned char *d, unsigned long e)
{
	/* Real body recovered 2026-07-26 via `objdump -dr -M intel` register
	 * tracing -- Ghidra's own decompiler failed here ("WARNING: Could not
	 * recover jumptable ... Treating indirect jump as call"). Two real
	 * TAIL-CALL dispatches through this object's own installed vtable, keyed
	 * on mAccessMode -- same "genuine indirect call through the installed
	 * vtable array" treatment already established for Unlock() above. Byte
	 * offsets/slot indices confirmed by a direct .rodata dword read of
	 * PTR__CChunkServer_08e859a8 (see header comment).
	 *
	 * mode == 0: vtbl[12] (byte 0x30) = OnLoad(CChunk*, uchar, uchar*, ulong),
	 *            forwarded (chunk, c, d, e).
	 * mode == 1: skip straight to the mode-!=-0/1 path's own tail call.
	 * else:      real ground truth soft-asserts here (Api+0x94, "ChunkServer.cpp"
	 *            line 0x174) but does NOT return/abort -- falls through to the
	 *            mode==1 path regardless, matching this project's established
	 *            soft-assert idiom (tempo.cpp/chunk_man.cpp/config_manager.cpp's
	 *            own Api+0x94 calls: real condition, real string, call made,
	 *            execution continues).
	 *            vtbl[13] (byte 0x34) = OnLoad(ulong, uchar*, uchar, uchar*,
	 *            ulong), forwarded (a, b, c, d, e).
	 *
	 * `this->mReserved7c = 0` is a REAL, previously-undocumented side effect
	 * (unconditional, before either branch) -- corrects chunk_server.h's own
	 * older "ctor never touches +0x7c" note, which was only ever a claim
	 * about the ctor, not this method; header updated to flag it.
	 */
	mReserved7c = 0;

	void **vtbl = *reinterpret_cast<void ***>(this);

	if (mAccessMode == 0) {
		typedef unsigned int (*OnLoad4Fn)(CChunkServer *, CChunk *, unsigned char, unsigned char *, unsigned long);
		OnLoad4Fn fn = reinterpret_cast<OnLoad4Fn>(vtbl[12]);
		fn(this, chunk, c, d, e);
		return;
	}

	if (mAccessMode != 1) {
		typedef void (*AssertFn)(void *, const char *, const char *, int);
		AssertFn fn = (AssertFn)(((void **)*(void **)Api)[0x94 / 4]);
		fn(Api, "Assertion failed in module %s, line %i.\n", "ChunkServer.cpp", 0x174);
	}

	typedef unsigned int (*OnLoad5Fn)(CChunkServer *, unsigned long, unsigned char *, unsigned char, unsigned char *, unsigned long);
	OnLoad5Fn fn = reinterpret_cast<OnLoad5Fn>(vtbl[13]);
	fn(this, a, b, c, d, e);
}

int CChunkServer::Exec(void *)
{
	/* Tier B link-stub -- see header comment. */
	return 0;
}
