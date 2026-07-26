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
 */

#include "chunk_server.h"
#include "omega_vtables.h"

#include <cstdlib>

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

void CChunkServer::Load(CChunk *, unsigned long, unsigned char *, unsigned char, unsigned char *, unsigned long)
{
	/* Tier B link-stub -- see header comment. */
}

int CChunkServer::Exec(void *)
{
	/* Tier B link-stub -- see header comment. */
	return 0;
}
