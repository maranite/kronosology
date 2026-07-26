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
 *
 * `Exec@080cc0d0.c` (the class's own `Exec(CMessage&)` override) was likewise
 * promoted Tier B -> Tier A 2026-07-26, also via direct `objdump -dr -M intel`
 * register tracing (Ghidra's own decompile of this one was usable for the
 * control-flow shape but not trusted for the byte-level argument marshalling,
 * same caution as `Load()`). See chunk_server.h's own header comment for the
 * full derivation, including the `TObjArray<SIDEntry>` structural insight.
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

/* Real, out-of-scope non-virtual method `CChunkBase::WriteBinary(void const*,
 * unsigned)` (.text+0x080ae650) -- `CChunkBase` itself is never reconstructed
 * anywhere in this project (same "opaque, pointer-only" treatment as `CChunk`).
 * Modeled as an inert free-function stand-in taking the real call site's own
 * `this` as a raw pointer, same "genuinely undecoded external call, transcribed
 * anyway" convention as `GetResLength()` above.
 */
void CChunkBase_WriteBinary(void *, const void *, unsigned long)
{
}

/* Real, out-of-scope templated container method
 * `TObjArray<CChunkServer::SIDEntry>::Add(CChunkServer::SIDEntry)`
 * (.text+0x081863d0, mangled `_ZN9TObjArrayIN12CChunkServer8SIDEntryEE3AddES1_`)
 * -- not reconstructed as a real C++ template anywhere in this project (same
 * "not a real template, a concretely-named stand-in" convention as
 * `TVector<T,1>::MakeCapacity()`, task.h). Modeled as an inert stand-in, same
 * convention as `CChunkBase_WriteBinary()` above.
 */
void TObjArray_SIDEntry_Add(void *, CChunkServer::SIDEntry)
{
}

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

int CChunkServer::Exec(CMessage &msg)
{
	/* See chunk_server.h's own header comment for the full derivation
	 * (register-traced from `objdump -dr -M intel`, not Ghidra's decompile).
	 */
	unsigned char *raw = reinterpret_cast<unsigned char *>(&msg);
	unsigned short code = *reinterpret_cast<unsigned short *>(raw + 0x8);
	unsigned char *payload = *reinterpret_cast<unsigned char **>(raw + 0x10);

	void **vtbl = *reinterpret_cast<void ***>(this);

	if (code & 0x100) {
		unsigned char opcode = static_cast<unsigned char>(code & 0xff);

		if (opcode == 0xe7) {
			/* this->OnAbort((int)payload) via vtbl[14] (byte 0x38). */
			typedef void (*OnAbortFn)(CChunkServer *, int);
			reinterpret_cast<OnAbortFn>(vtbl[14])(
			    this, static_cast<int>(reinterpret_cast<unsigned long>(payload)));
			return 0;
		}
		if (opcode == 0xe8) {
			/* this->OnStoppedByUser((int)payload) via vtbl[15] (byte 0x3c). */
			typedef void (*OnStoppedFn)(CChunkServer *, int);
			reinterpret_cast<OnStoppedFn>(vtbl[15])(
			    this, static_cast<int>(reinterpret_cast<unsigned long>(payload)));
			return 0;
		}
		if (opcode == 0xe6) {
			/* mSIDArray.Add(SIDEntry(...)) -- `this+0x80` (mUnknown80/
			 * mEntryCount/mUnknown88/mTableBuf) IS the real TObjArray<SIDEntry>'s
			 * own 4-field layout (see header comment); `&mUnknown80` is
			 * byte-identical to ground truth's own `lea ebx,[ebx+0x80]`.
			 */
			unsigned long param = reinterpret_cast<unsigned long>(payload);
			unsigned char keyByte = static_cast<unsigned char>(param & 0xff);
			unsigned char valByte = static_cast<unsigned char>((param >> 8) & 0xff);
			TObjArray_SIDEntry_Add(&mUnknown80, SIDEntry(keyByte, valByte));
			return 0;
		}
		return -1;
	}

	if (!(code & 0x200))
		return -1;

	unsigned char opcode = static_cast<unsigned char>(code & 0xff);
	unsigned char dx = static_cast<unsigned char>(opcode - 0xe0);
	if (dx > 9)
		return -1;

	/* Real: 4-byte big-endian header read from `payload[0..3]`, `body` =
	 * `payload+4` -- shared by every case below.
	 */
	unsigned long header =
	    (static_cast<unsigned long>(payload[0]) << 24) |
	    (static_cast<unsigned long>(payload[1]) << 16) |
	    (static_cast<unsigned long>(payload[2]) << 8) |
	     static_cast<unsigned long>(payload[3]);
	unsigned char *body = payload + 4;

	if (dx <= 3) {
		/* 0xe0 Unlock / 0xe1 Relock / 0xe2 Begin / 0xe3 End -- identical arg
		 * marshalling, only the vtable slot differs (6/7/8/9, byte 0x18/0x1c/
		 * 0x20/0x24). All 4 real base bodies ignore every argument and return 1
		 * unconditionally.
		 */
		static const int kSlot[4] = { 6, 7, 8, 9 };
		unsigned int result = reinterpret_cast<POnCommandFn>(vtbl[kSlot[dx]])(
		    this, body[1], body[0], body[2], body + 3, header);
		return (result == 1) ? 0 : -1;
	}

	if (dx == 4) {
		/* 0xe4 Load. Real: `this->mReserved7c = 0` unconditionally (matches
		 * `Load()`'s own documented side effect, independently re-derived
		 * here), then a mAccessMode-keyed dispatch: `OnLoad(CChunk*,...)` via
		 * vtbl[12] (byte 0x30) when mAccessMode==0, else a soft Api+0x94
		 * assert (line 0x174 -- byte-identical to `Load()`'s own) when
		 * mAccessMode is neither 0 nor 1, then
		 * `OnLoad(ulong,uchar*,uchar,uchar*,ulong)` via vtbl[13] (byte 0x34)
		 * regardless. `loadArea` = `body+4` is a second, independent 4-byte
		 * header + trailing bytes this command's own payload carries.
		 */
		mReserved7c = 0;

		unsigned long loadHeader =
		    (static_cast<unsigned long>(body[0]) << 24) |
		    (static_cast<unsigned long>(body[1]) << 16) |
		    (static_cast<unsigned long>(body[2]) << 8) |
		     static_cast<unsigned long>(body[3]);
		unsigned char *loadArea = body + 4;
		unsigned char *outBuf = loadArea + 9;
		unsigned char c = loadArea[8];

		unsigned int result;
		if (mAccessMode != 0) {
			if (mAccessMode != 1) {
				typedef void (*AssertFn)(void *, const char *, const char *, int);
				AssertFn fn = reinterpret_cast<AssertFn>(
				    (*reinterpret_cast<void ***>(Api))[0x94 / 4]);
				fn(Api, "Assertion failed in module %s, line %i.\n",
				   "ChunkServer.cpp", 0x174);
			}

			unsigned long a = (static_cast<unsigned long>(loadArea[4]) << 24) |
			                   (static_cast<unsigned long>(loadArea[5]) << 16) |
			                   (static_cast<unsigned long>(loadArea[6]) << 8) |
			                    static_cast<unsigned long>(loadArea[7]);
			unsigned long b = (static_cast<unsigned long>(loadArea[0]) << 24) |
			                   (static_cast<unsigned long>(loadArea[1]) << 16) |
			                   (static_cast<unsigned long>(loadArea[2]) << 8) |
			                    static_cast<unsigned long>(loadArea[3]);

			typedef unsigned int (*OnLoad5Fn)(CChunkServer *, unsigned long,
			                                   unsigned char *, unsigned char,
			                                   unsigned char *, unsigned long);
			result = reinterpret_cast<OnLoad5Fn>(vtbl[13])(
			    this, a, reinterpret_cast<unsigned char *>(b), c, outBuf, header);
		} else {
			typedef unsigned int (*OnLoad4Fn)(CChunkServer *, void *, unsigned char,
			                                   unsigned char *, unsigned long);
			result = reinterpret_cast<OnLoad4Fn>(vtbl[12])(
			    this, reinterpret_cast<void *>(loadHeader), c, outBuf, header);
		}

		/* Real: `outBuf[c] = (unsigned char)this->mReserved7c` -- re-read
		 * AFTER the dispatch (a derived override could have written it).
		 */
		outBuf[c] = static_cast<unsigned char>(mReserved7c & 0xff);
		return (result == 1) ? 0 : -1;
	}

	if (dx == 5) {
		/* 0xe5 Save -- mirror of the Load case above (no `mReserved7c = 0`
		 * here; ground truth genuinely omits it on this path). `saveHeader`
		 * doubles as the `CChunk*` handle `OnSave(CChunk*,...)` receives AND
		 * the `this` ground truth passes to `CChunkBase::WriteBinary()` on the
		 * mAccessMode!=0 path.
		 */
		unsigned long saveHeader =
		    (static_cast<unsigned long>(body[0]) << 24) |
		    (static_cast<unsigned long>(body[1]) << 16) |
		    (static_cast<unsigned long>(body[2]) << 8) |
		     static_cast<unsigned long>(body[3]);
		unsigned char *saveArea = body + 4;
		unsigned char *outBuf = saveArea + 9;
		unsigned char c = saveArea[8];

		unsigned int result;
		if (mAccessMode != 0) {
			if (mAccessMode != 1) {
				typedef void (*AssertFn)(void *, const char *, const char *, int);
				AssertFn fn = reinterpret_cast<AssertFn>(
				    (*reinterpret_cast<void ***>(Api))[0x94 / 4]);
				fn(Api, "Assertion failed in module %s, line %i.\n",
				   "ChunkServer.cpp", 0xd8);
			}

			unsigned long lenOut = 0;
			const unsigned char *dataOut = 0;
			typedef unsigned int (*OnSave5Fn)(CChunkServer *, unsigned long *,
			                                   const unsigned char **, unsigned char,
			                                   unsigned char *, unsigned long);
			result = reinterpret_cast<OnSave5Fn>(vtbl[11])(
			    this, &lenOut, &dataOut, c, outBuf, header);
			CChunkBase_WriteBinary(reinterpret_cast<void *>(saveHeader), dataOut, lenOut);
		} else {
			typedef unsigned int (*OnSave4Fn)(CChunkServer *, void *, unsigned char,
			                                   unsigned char *, unsigned long);
			result = reinterpret_cast<OnSave4Fn>(vtbl[10])(
			    this, reinterpret_cast<void *>(saveHeader), c, outBuf, header);
		}

		outBuf[c] = static_cast<unsigned char>(mReserved7c & 0xff);
		return (result == 1) ? 0 : -1;
	}

	if (dx == 9) {
		/* 0xe9 GetSaveBuffSize -- 3 header bytes forwarded straight through
		 * via vtbl[5] (byte 0x14 -- CEditor::CChunkServerTask overrides this
		 * slot, so this must stay a real indirect dispatch, not a direct call
		 * to the base method). 32-bit result written back little-endian into
		 * payload[0..3].
		 */
		typedef int (*GetSaveBuffSizeFn)(const CChunkServer *, unsigned char,
		                                  unsigned char, unsigned char);
		int result = reinterpret_cast<GetSaveBuffSizeFn>(vtbl[5])(
		    this, payload[0], payload[1], payload[2]);

		payload[0] = static_cast<unsigned char>(result & 0xff);
		payload[1] = static_cast<unsigned char>((result >> 8) & 0xff);
		payload[2] = static_cast<unsigned char>((result >> 16) & 0xff);
		payload[3] = static_cast<unsigned char>((result >> 24) & 0xff);
		return 0;
	}

	/* dx 6/7/8 (opcodes 0xe6/0xe7/0xe8) are unreachable via this path in
	 * ground truth -- those 3 codes are only ever handled above, gated on bit
	 * 0x100. The real jumptable's own entries for slots 6/7/8 point at the
	 * same default -1 return every other unmatched code takes.
	 */
	return -1;
}
