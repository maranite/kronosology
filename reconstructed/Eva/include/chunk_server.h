/*
 * chunk_server.h  -  CChunkServer, the base class `CEditor::Setup()`'s LAST
 * previously-deferred fan-out target (`CEditor::CChunkServerTask`, editor.h)
 * derives from. Was flagged "NOT tractable this pass" by the original
 * dedicated `CEditor` batch (editor.h's own header comment, now updated) --
 * revisited 2026-07-26 following the `CreateUserModules()` unlock (see
 * eva_createusermodules_editor_unlock_2026-07-26) that made `CEditor::Setup()`
 * itself confirmed live-boot-reachable, prompting a re-survey of its
 * remaining unwired fan-out. Unlike the `CAlphaKeybIfcTask` sibling (gated
 * behind "ALPHAKEYBOARD=Yes"), `CChunkServerTask` construction is
 * UNCONDITIONAL in `CEditor::Setup()` -- always on the real boot path.
 *
 * GROUND TRUTH SHAPE: `.text+0x080cba90..0x080cc0d0`, `CChunkServer` (21
 * `nm -C`-confirmed methods total, including 2 dtor variants and 3
 * `SIDEntry` nested-struct ctors not modeled here -- see below). Real base is
 * `CTask` (task.h) -- `CChunkServer::CChunkServer(CModule const&,
 * CChunkServer::EAccessMode)` (.text+0x080cbcf0, `CChunkServer@080cbcf0.c`)
 * base-constructs via `CTask::CTask(owner, "ChunkServer", 5, 0, 0x8003)`
 * (name confirmed via a direct `.data`/`.rodata` string read at the real
 * `CChunkServer::sm_pkcTaskName` symbol, 0x091ae8ec -> "ChunkServer").
 *
 * `ECommand`/`EAccessMode` ARE real nested enums in ground truth (every
 * method's own real prototype names them) but neither their full enumerator
 * sets nor canonical names are confirmed beyond the literal values actually
 * exercised by a reconstructed caller (`EAccessMode` 0 [`CChunkServerTask`'s
 * own ctor argument]/1 [assert-guarded in `Load()`, Tier B]; `ECommand` value
 * 2 [`Unlock()`'s own hardcoded argument]) -- kept as opaque `int`, same
 * convention as `CEditor`'s own `ELedCode`/`ELedState` (editor.h).
 *
 * REAL LAYOUT (0x94 bytes total, matching every real caller's own
 * `malloc(0x94)`; base `CTask` ends at 0x7c, task.h):
 *   +0x7c  mReserved7c  never touched by the real ctor, BUT `Load()` (below,
 *                       now Tier A) unconditionally writes 0 here as its own
 *                       first real side effect -- corrected 2026-07-26 from
 *                       an earlier "never touched" claim that was only ever
 *                       true of the ctor. Still never READ back by any
 *                       reconstructed method.
 *   +0x80  mUnknown80   ctor sets 1; never read back by any reconstructed
 *                       method
 *   +0x84  mEntryCount  ctor zeroes; the real "how many key/value rows exist"
 *                       counter `GetServerID()`/`GetServerHandle()` both
 *                       gate on (`if (count < 1) return -1;`) before scanning
 *                       mTableBuf
 *   +0x88  mUnknown88   ctor sets 1; never read back
 *   +0x8c  mTableBuf    unsigned char* -- ctor allocates a 2-byte seed
 *                       buffer (`{0xff, 0}`, i.e. one sentinel "no entry"
 *                       row); real ground truth treats this as a
 *                       2-byte-stride `{key, value}` table, `GetServerID()`
 *                       indexing it directly by `param*2`,
 *                       `GetServerHandle()` linear-scanning it for a
 *                       matching key byte (real body is Duff's-device
 *                       unrolled, collapsed to a plain loop here -- same
 *                       license as `CParameterString`'s own "ALPHAKEYBOARD"
 *                       compare, parameter_string.cpp). No reconstructed
 *                       method ever GROWS this table past its 1-entry ctor
 *                       seed (that would be `Load()`'s own job, Tier B) --
 *                       so `GetServerHandle()`/`GetServerID()` only ever see
 *                       the sentinel row in this reconstruction, matching
 *                       `mEntryCount`'s own ctor-zeroed value gating both
 *                       scans off entirely in practice.
 *   +0x90  mAccessMode  the ctor's own `EAccessMode` argument, stored
 *                       verbatim -- read by `GetSaveBuffSize()` (`== 0` gate)
 *                       and `Load()` (Tier B)
 *
 * VTABLE: real 16-slot primary (5-slot CTask-family header + 11 own new
 * virtuals) + 3-slot secondary ("mIfcThunk", this+8) -- confirmed byte-exact
 * via a direct `.rodata` dword read at 0x08e859a0, NOT inferred. See
 * omega_vtables.h's own header comment for the full slot-by-slot derivation.
 * Install-only in this reconstruction (EvaVTableStub-backed) -- nothing in
 * this project's own call graph dispatches through it yet.
 *
 * TIER SPLIT: every method EXCEPT `Exec(CMessage&)` is now Tier A (`Load()`
 * promoted 2026-07-26 -- see below). `Unlock()` (.text+0x080cbdf0, 63 bytes)
 * is real but forwards through this object's OWN vtable at byte offset +0x18
 * from the installed pointer (i.e. primary-array index 6, `GetSaveBuffSize`'s
 * own following-11-virtuals group's 2nd entry -- confirmed by direct address
 * match against the `.rodata` dump, NOT by name) -- since `CEditor::
 * CChunkServerTask` (the only real derived class in this project) does not
 * override that slot, a direct call to the base `OnUnlock()` its own 6-arg
 * call site's 3rd literal argument (`2`) would otherwise select gives
 * IDENTICAL behavior to a true vtable dispatch here; modeled as a genuine
 * indirect call through the installed vtable array (matching ground truth's
 * own mechanism exactly, not simplified to a direct call) since the array is
 * available and correctly sized regardless.
 *
 * `Load()` (.text+0x080cbfd0, 250 bytes) was originally left Tier B because
 * Ghidra's own decompiler could not recover its indirect-call argument list
 * ("WARNING: Could not recover jumptable... Treating indirect jump as
 * call"). Recovered 2026-07-26 by transcribing directly from `objdump -dr
 * -M intel` instead (same technique already used for `CTask::RegisterIfc()`/
 * `TVector<T,1>::MakeCapacity()`): it is `mReserved7c = 0;` followed by a
 * `mAccessMode`-keyed TAIL CALL through this object's own installed vtable --
 * slot 12 (byte 0x30, `OnLoad(CChunk*,uchar,uchar*,ulong)`) when
 * `mAccessMode == 0`; otherwise slot 13 (byte 0x34, `OnLoad(ulong,uchar*,
 * uchar,uchar*,ulong)`), with a real (but non-aborting) `Api`+0x94 soft-
 * assert first when `mAccessMode` is neither 0 nor 1 ("Assertion failed in
 * module %s, line %i.\n" / "ChunkServer.cpp" / 0x174). Both vtable targets
 * are the trivial `return 0;` overloads for a plain `CChunkServer`, but for
 * the ONE real derived class in this project (`CEditor::CChunkServerTask`)
 * slot 12 is instead that class's own real, non-trivial `OnLoad(CChunk*,...)`
 * override (editor.cpp, itself still Tier B -- genuine `PegResourceHandler::
 * Load()` depth, out of scope) -- Load()'s own dispatch is modeled as a
 * genuine indirect vtable call, not a hardcoded direct call to either
 * overload, so this composes correctly regardless of which concrete class is
 * actually installed.
 *
 * `Exec(CMessage&)` (.text+0x080cc0d0, 1336 bytes) stays Tier B -- genuine
 * chunk-protocol depth, same bar as `CEditor::CMainTask::Exec()`/
 * `CAlphaKeybIfcTask::ProcessCode()`.
 */

#ifndef CHUNK_SERVER_H
#define CHUNK_SERVER_H

#include "task.h"

/* Opaque forward declaration -- real class not reconstructed anywhere in
 * this project. Only ever used as an incomplete pointer type (matching
 * `IAlphaKeybCode`'s own treatment, alpha_keyb_ifc_task.h).
 */
class CChunk;

class CChunkServer : public CTask {
public:
	/* .text+0x080cbcf0, 167 bytes. */
	CChunkServer(const CModule &owner, int accessMode);

	/* .text+0x080cbb90 (D1, complete-object destructor). The D0 "deleting
	 * destructor" variant (.text+0x080cbc10, this same body + `free(this)`)
	 * is not modeled separately, same convention as every other
	 * reconstructed class's D0/D1 split in this project.
	 */
	~CChunkServer();

	/* .text+0x080cba90/0x080cbaa0/0x080cbab0/0x080cbac0, 6 bytes each. All
	 * REAL: every real body ignores all its own arguments and returns 1
	 * unconditionally. `this` is included in the signature to match the real
	 * vtable-slot call site (`Unlock()`, below) but is likewise unused.
	 */
	unsigned int OnUnlock(unsigned char a, int cmd, unsigned char b, unsigned char *c, unsigned long d);
	unsigned int OnRelock(unsigned char a, int cmd, unsigned char b, unsigned char *c, unsigned long d);
	unsigned int OnBegin(unsigned char a, int cmd, unsigned char b, unsigned char *c, unsigned long d);
	unsigned int OnEnd(unsigned char a, int cmd, unsigned char b, unsigned char *c, unsigned long d);

	/* .text+0x080cbad0/0x080cbae0, 3 bytes each. REAL: both unconditionally
	 * `return 0;`.
	 */
	unsigned int OnSave(CChunk *chunk, unsigned char a, unsigned char *b, unsigned long c);
	unsigned int OnSave(unsigned long &a, const unsigned char *&b, unsigned char c, unsigned char *d, unsigned long e);

	/* .text+0x080cbaf0/0x080cbb00, 3 bytes each. REAL: both unconditionally
	 * `return 0;`.
	 */
	unsigned int OnLoad(CChunk *chunk, unsigned char a, unsigned char *b, unsigned long c);
	unsigned int OnLoad(unsigned long a, unsigned char *b, unsigned char c, unsigned char *d, unsigned long e);

	/* .text+0x080cbb10/0x080cbb20, 1 byte each. REAL: both a bare `ret`. */
	void OnAbort(int cmd);
	void OnStoppedByUser(int cmd);

	/* .text+0x080cbb30, 89 bytes. REAL: `mAccessMode == 0` gate around
	 * `GetResLength(a,b,c) + 0x800` -- `GetResLength(unsigned int,
	 * unsigned int, unsigned int)` is a genuinely undecoded external
	 * dependency (.text+0x080688d0, no other reference anywhere in this
	 * project), modeled as an inert stand-in (chunk_server.cpp), same
	 * "transcribed anyway" convention as panel_ifc_task.cpp's own
	 * PegMessageQueuePush/ControlSurface stand-ins.
	 */
	int GetSaveBuffSize(unsigned char a, unsigned char b, unsigned char c) const;

	/* .text+0x080cbdc0, 34 bytes. REAL: `mEntryCount == 0` guard, else
	 * `mTableBuf[index*2]` (the real ground truth's own direct-index read --
	 * no bounds check on `index` beyond the count guard, transcribed as
	 * found).
	 */
	unsigned int GetServerID(int index) const;

	/* .text+0x080cbdf0, 63 bytes. REAL -- see header comment above for the
	 * real vtable-slot-6 dispatch this forwards through (`ECommand` argument
	 * hardcoded to `2` at this call site, ground truth's own literal).
	 */
	void Unlock(unsigned char a, unsigned char b, unsigned char *c);

	/* .text+0x080cbe30, 403 bytes. REAL -- ground truth's own body is a
	 * Duff's-device-unrolled (unroll factor 8) linear scan of `mTableBuf`'s
	 * 2-byte-stride rows for a matching key byte, returning the row's value
	 * byte, or 0xffffffff if `mEntryCount < 1` or no match is found.
	 * Collapsed to a plain loop here (same license as
	 * `CParameterString`'s own Duff's-device compares).
	 */
	unsigned int GetServerHandle(unsigned char key) const;

	/* .text+0x080cbfd0, 250 bytes. REAL -- see header comment above for the
	 * full mAccessMode-keyed tail-call-through-own-vtable derivation
	 * (recovered via direct objdump register tracing after Ghidra's own
	 * decompiler failed on it).
	 */
	void Load(CChunk *chunk, unsigned long a, unsigned char *b, unsigned char c, unsigned char *d, unsigned long e);

	/* .text+0x080cc0d0, 1336 bytes (the ONE real override CChunkServer
	 * itself provides over CTask's own generic `Exec(CMessage&)`). Tier B
	 * link-stub -- genuine chunk-protocol depth, same bar as
	 * `CEditor::CMainTask::Exec()`.
	 */
	int Exec(void *msg);

protected:
	int            mReserved7c; /* +0x7c, ctor never touches it, but Load() does (see above) */
	int            mUnknown80;  /* +0x80 */
	int            mEntryCount; /* +0x84 */
	int            mUnknown88;  /* +0x88 */
	unsigned char *mTableBuf;   /* +0x8c */
	int            mAccessMode; /* +0x90 */

	friend struct ChunkServerTestHooks;

private:
	/* Not implemented -- same "never copied in ground truth" convention as
	 * CParameterString/CEditor.
	 */
	CChunkServer(const CChunkServer &);
	CChunkServer &operator=(const CChunkServer &);
};

#endif /* CHUNK_SERVER_H */
