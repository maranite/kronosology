// SPDX-License-Identifier: GPL-2.0
/*
 * oa_file_stream.h  -  CFileStream: OA.ko's own concrete disk-backed
 * `STGStream::CStream` implementation, used throughout the file-chunk
 * import cluster (CKorgFileKMP/CKorgFileKSF/CMultisampleChunk/
 * CSampleChunk/etc, all already referencing `CFileStream&` in their own
 * mangled signatures per symbols.csv).
 *
 * FOUND 2026-07-29 (round 49, solo -- session-wide 200-subagent dispatch
 * cap hit, see PROJECT_BRAIN/status.md). Confirmed via
 * /home/share/Decomp/oa_export's own per-function decompiles + symbols.csv
 * mangled-name cross-check for every method below.
 *
 * Confirmed real object layout (this project's regparm(3) ABI: `this` is
 * EAX, ground truth's own decompile shows it as an unused declared
 * parameter with the real body reading an `in_EAX` pseudo-variable --
 * same gotcha already documented in oa_ckg_midi_msg_handler.h):
 *   +0x00  vptr (real virtual class -- ctor sets `&PTR__CFileStream_
 *          006c0728`, dtor resets it there then to `&PTR__CStream_
 *          006c07e8` before the base-class subobject destructor runs,
 *          confirmed via `_ZTV11CFileStream`@006c0720/`_ZTVN9STGStream7
 *          CStreamE`@006c07e0 in symbols.csv -- both real ELF vtable
 *          symbols, standard g++ derived-then-base destructor codegen)
 *   +0x04  mErrorFlag (int) -- 0 = ok, 7 = generic I/O error; every
 *          method below checks/sets this
 *   +0x08  mSelf -- set to `this` in the ctor, never read by any of the
 *          16 methods this batch reconstructs (real semantics unknown,
 *          plausibly a callback-context pointer for the un-reconstructed
 *          `CStream` base's own methods)
 *   +0x0c  mFileHandle (raw `CSTGFile_Open()` handle, opaque `void*`
 *          from this project's own already-established CSTGFile_* ABI)
 *
 * `CStream` (`STGStream::CStream`, `_ZTVN9STGStream7CStreamE`@006c07e0)
 * is CFileStream's real base -- a 22-slot pure-virtual stream interface
 * (vtable spans 006c07e0..006c0840, vs. CFileStream's own smaller
 * 16-slot vtable at 006c0720..006c0768) NOT reconstructed this round;
 * out of scope for this batch's file-I/O-cluster slice.
 *
 * `SetPositionBeginning()` (`_ZN11CFileStream20SetPositionBeginningEv`)
 * is the ONE method in this cluster deliberately NOT reconstructed: its
 * real body is `(**(code**)(*in_EAX + 0xc))()` -- a genuine virtual
 * dispatch through CFileStream's OWN vtable at slot index 3 (byte
 * offset 0xc from the vptr, which per `_ZTV11CFileStream`@006c0720 +
 * 8-byte ABI header = `PTR__CFileStream_006c0728`, matching the ctor's
 * own vptr-store value) -- almost certainly a `this->SetPosition(0)`
 * self-call given the other 3 SetPosition* siblings' own semantics, but
 * resolving WHICH named method occupies vtable slot 3 needs the full
 * 22-slot `CStream` base interface reconstructed first (to establish
 * slot-index-to-method-name ordering), out of scope here. Left
 * undeclared/uncredited rather than guessed at, matching this project's
 * "genuinely-unresolvable decompile recognition" convention.
 *
 * `Exists()`/`Copy()` are `static` (`__cdecl`/`__regparm3`, no `this` --
 * confirmed via their own decompiled signatures never referencing
 * `in_EAX`), thin wrappers over already-reconstructed `CSTGFile_*`
 * free functions (`file_io.cpp`). `GetPosition`/`IsAtEnd`/`Flush`
 * needed 3 new small `CSTGFile_*` primitives (`file_io.cpp`, same
 * batch) -- see that file's own header comment for their derivation.
 */

#ifndef OA_FILE_STREAM_H
#define OA_FILE_STREAM_H

class CFileStream {
public:
	CFileStream(const char *path, int openAttr, int endianness);
	~CFileStream();

	unsigned int ChangeSize(unsigned int newSize);
	void Write(const void *buf, unsigned int len);
	void Read(void *buf, unsigned int len);
	int GetSize(unsigned int &outSize) const;
	bool IsAtEnd();
	int SetPositionEnd();
	int SetPositionRelative(long offset);
	int SetPosition(unsigned int pos);
	int GetPosition(unsigned int &outPos);
	int Flush();

	static bool Exists(const char *path);
	/* Real param order confirmed from ground truth: param_1 (first) is
	 * opened WRITE/CREATE/TRUNC, param_2 (second) is opened READ-ONLY
	 * and drives the copy -- i.e. (dstPath, srcPath), NOT (src, dst). */
	static unsigned char Copy(const char *dstPath, const char *srcPath);

private:
	void *mVptrPlaceholder;	/* +0x00, see header comment */
	int mErrorFlag;		/* +0x04 */
	void *mSelf;		/* +0x08 */
	void *mFileHandle;	/* +0x0c */
};

#endif /* OA_FILE_STREAM_H */
