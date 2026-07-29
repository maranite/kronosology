// SPDX-License-Identifier: GPL-2.0
/*
 * file_ksc_list.h  -  CFileKscList, the on-disk KSC-list (Kronos Sample
 * Card auto-load registry) config-record reader/writer.
 *
 * FOUND 2026-07-29, fresh `nm -C` class-inventory sweep for the next dense,
 * previously-100%-untouched cluster (session running solo -- the standing
 * decompile-everything goal's 200-subagent-dispatch cap was hit; see
 * PROJECT_BRAIN/status.md). Sibling of `CKscSampleManager` (a much larger,
 * separately-tracked 68-method singleton) -- confirmed via a real call from
 * `Load()` into `CKscSampleManager::GetInstance()`/`AddAutoLoadKsc()`, but
 * this class's own per-field accessors are self-contained.
 *
 * REAL SHAPE, confirmed via `objdump -dr -M intel`: every accessor here
 * marshals args and calls through the SAME global `FMApi` god-object's
 * vtable (see `config_manager.cpp`'s own `FMApiGetDriverFactory`/
 * `FMApiRegisterDriver` inline-wrapper convention, reused here for 2 new
 * slots) -- slot `+0x1bc` ("read a positional field") and slot `+0x1c0`
 * ("write a positional field"), both with signature
 * `int(*)(void *fmApiThis, void *handle, void *buf, unsigned int *len)`.
 * `this->mHandle` (offset 0, the class's only field this pass models) is
 * passed as the `handle` arg on EVERY call -- not a per-field key string, a
 * shared identifier for one open KSC-list record. Combined with `ReadDot`/
 * `WriteDot` (below) consuming/emitting a literal "\r\n" and `ReadHeaderId`
 * comparing against a literal "#KSC" 4-byte magic (both confirmed via a
 * direct `.rodata` byte dump at 0x8ef2e20: `0d 0a 00 23 4b 53 43 00` =
 * `"\r\n\0#KSC\0"`), this is a classic CRLF-delimited flat text record
 * format read/written sequentially through an already-open handle -- NOT a
 * keyed random-access profile API. `mHandle`'s own real type/how it's
 * opened is NOT modeled here (that's `Load()`/`Save()`'s job, deferred
 * below); every accessor here just forwards it through, faithfully
 * replicating the marshaling contract without claiming FMApi-internal
 * knowledge, exactly this project's established "god-object opaque
 * forwarding" convention (see `config_manager.cpp`'s own 2 wrappers).
 *
 * TWO REAL FIELD SHAPES, both confirmed via multiple identical instances:
 *   - "string" fields (VendorId 8B, ProductId 16B (0x10), SerialNumber
 *     128B (0x80)): the FMApi call's output buffer IS the caller's own
 *     buffer directly; return is bool(rc==1), nothing else.
 *   - "byte" fields (AutoLoad, BitDepth, LoadMethod): a fixed len=2 local
 *     scratch buffer is used; on Read, byte[1] of that scratch is copied
 *     to the caller's single `char*` output; on Save, byte[1] of a local
 *     len=2 scratch is set from the caller's single input byte before the
 *     call (byte[0] left zero-initialized on Save, an unexplained-but-
 *     faithfully-reproduced real leading pad byte).
 *
 * DEFERRED this pass (documented, not fabricated): `ReadFilePath`/
 * `SaveFilePath` (a length-prefixed-string protocol with a real odd/even
 * padding-byte branch via `CMemoryAccessor::ReadLittle16Bit` -- understood
 * from disassembly but not landed this pass, budget reasons),
 * `RefreshFilePath`/`GetDeviceInfo` (both call further into `CDeviceMgr`,
 * unmodeled this pass), and `Load()`/`Save()` themselves (the 2 large
 * orchestrator methods -- both call into `CKscSampleManager` AND the
 * project-wide out-of-scope growable `CZ` container, the exact same trap
 * `korg_file.h`'s own header comment already documents for `CFileKge`).
 *
 * Non-polymorphic (no vtable/typeinfo -- confirmed via `nm`), ctor/dtor
 * both real, but EMPTY (1-byte `ret`, confirmed via `objdump`) -- `mHandle`
 * is presumably set by a caller after construction, not by the ctor
 * itself.
 */

#ifndef FILE_KSC_LIST_H
#define FILE_KSC_LIST_H

class CFileKscList {
public:
	CFileKscList() {}
	~CFileKscList() {}

	/* "string" fields: direct buffer, bool(rc==1) return. */
	bool ReadVendorId(char *out);
	bool SaveVendorId(const char *in);
	bool ReadProductId(char *out);
	bool SaveProductId(const char *in);
	bool ReadSerialNumber(char *out);
	bool SaveSerialNumber(const char *in);

	/* "byte" fields: len=2 scratch, byte[1] is the real value. */
	bool ReadAutoLoad(char *out);
	bool SaveAutoLoad(const char *in);
	bool ReadBitDepth(char *out);
	bool SaveBitDepth(const char *in);
	bool ReadLoadMethod(char *out);
	bool SaveLoadMethod(const char *in);

	/* Record-format framing. */
	bool ReadHeaderId();
	bool SaveHeaderId();
	bool ReadDot();
	bool WriteDot();

	/* ---- deferred this pass (see header comment) ---- */
	// bool ReadFilePath(char *out, unsigned short *lenOut);
	// bool SaveFilePath(const char *in);
	// void RefreshFilePath(char *, char *, char *, char *);
	// void GetDeviceInfo(EDevice_Id, char *, char *, char *);
	// void Load();
	// void Save();

private:
	void *mHandle; /* +0x0 -- opaque record handle, passed to every FMApi call */
};

#endif // FILE_KSC_LIST_H
