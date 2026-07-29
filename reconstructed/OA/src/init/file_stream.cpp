// SPDX-License-Identifier: GPL-2.0
/*
 * file_stream.cpp  -  CFileStream method bodies (round 49, solo). See
 * include/oa_file_stream.h for the full object-layout derivation and
 * the deliberately-deferred SetPositionBeginning() note.
 */
#include "oa_file_stream.h"

extern "C" void        *CSTGFile_Open(const char *path, int mode);
extern "C" int           CSTGFile_Close(void *handle);
extern "C" int           CSTGFile_Seek(void *handle, int offset, int whence);
extern "C" unsigned int  CSTGFile_GetFileSize(void *handle);
extern "C" int           CSTGFile_Read(void *handle, void *buf, unsigned int size);
extern "C" int           CSTGFile_Write(void *handle, const void *buf, unsigned int count);
extern "C" int           CSTGFile_FileExists(const char *path);
extern "C" int           CSTGFile_GetPosition(void *handle);
extern "C" bool          CSTGFile_IsAtEnd(void *handle);
extern "C" void          CSTGFile_Flush(void *handle);

CFileStream::CFileStream(const char *path, int openAttr, int /*endianness*/)
{
	mErrorFlag = 0;
	mVptrPlaceholder = 0; /* real ctor: &PTR__CFileStream_006c0728, see header */
	mFileHandle = 0;
	mSelf = this;
	mFileHandle = CSTGFile_Open(path, openAttr);
	mErrorFlag = (mFileHandle == 0) ? 7 : 0;
}

CFileStream::~CFileStream()
{
	/* real dtor: vptr -> &PTR__CFileStream_006c0728, close, vptr ->
	 * &PTR__CStream_006c07e8 (base-class-vtable reset before the
	 * un-reconstructed CStream subobject destructor runs) -- vptr
	 * writes not modeled, see header comment. */
	if (mFileHandle != 0) {
		CSTGFile_Close(mFileHandle);
		mFileHandle = 0;
	}
}

unsigned int CFileStream::ChangeSize(unsigned int /*newSize*/)
{
	/* Real body ignores its own explicit argument entirely -- ground
	 * truth's `param_1` is actually `this` (EAX) mistyped `uint` by
	 * Ghidra since this method is `__regparm3` not `__thiscall`; the
	 * real explicit `unsigned int` arg (EDX) is never referenced. */
	return (unsigned int)mErrorFlag;
}

void CFileStream::Write(const void *buf, unsigned int len)
{
	if (mErrorFlag == 0) {
		int written = CSTGFile_Write(mFileHandle, buf, len);
		mErrorFlag = (int)(len != (unsigned int)written);
	}
}

void CFileStream::Read(void *buf, unsigned int len)
{
	if (mErrorFlag == 0) {
		unsigned int got = (unsigned int)CSTGFile_Read(mFileHandle, buf, len);
		mErrorFlag = (int)(len != got);
	}
}

int CFileStream::GetSize(unsigned int &outSize) const
{
	int err = mErrorFlag;
	if (err == 0)
		outSize = CSTGFile_GetFileSize(mFileHandle);
	return err;
}

bool CFileStream::IsAtEnd()
{
	if (mErrorFlag != 0)
		return false;
	return CSTGFile_IsAtEnd(mFileHandle);
}

int CFileStream::SetPositionEnd()
{
	if (mErrorFlag != 0)
		return mErrorFlag;
	int r = CSTGFile_Seek(mFileHandle, 0, 2 /* SEEK_END */);
	if (r < 0)
		mErrorFlag = 7;
	return mErrorFlag;
}

int CFileStream::SetPositionRelative(long offset)
{
	if (mErrorFlag != 0)
		return mErrorFlag;
	int r = CSTGFile_Seek(mFileHandle, (int)offset, 1 /* SEEK_CUR */);
	if (r < 0)
		mErrorFlag = 7;
	return mErrorFlag;
}

int CFileStream::SetPosition(unsigned int pos)
{
	if (mErrorFlag != 0)
		return mErrorFlag;
	int r = CSTGFile_Seek(mFileHandle, (int)pos, 0 /* SEEK_SET */);
	if (r < 0)
		mErrorFlag = 7;
	return mErrorFlag;
}

int CFileStream::GetPosition(unsigned int &outPos)
{
	if (mErrorFlag != 0)
		return mErrorFlag;
	int r = CSTGFile_GetPosition(mFileHandle);
	if (r < 0) {
		mErrorFlag = 7;
		return mErrorFlag;
	}
	outPos = (unsigned int)r;
	return mErrorFlag;
}

int CFileStream::Flush()
{
	if (mErrorFlag == 0)
		CSTGFile_Flush(mFileHandle);
	return mErrorFlag;
}

bool CFileStream::Exists(const char *path)
{
	return CSTGFile_FileExists(path) != 0;
}

/*
 * Copy: real ground-truth body (regparm3, no `this`) -- 1KB-chunk
 * copy loop transcribed verbatim, including 2 confirmed real quirks:
 * (1) a zero-byte source file returns success (`err=0`) without
 * entering the copy loop at all; (2) if the source opens but the
 * destination fails to open, `err` stays at its initial `7` AND the
 * source handle is still closed (only the destination's close is
 * gated on the destination having opened) -- both preserved exactly,
 * not "fixed" to a more symmetric close pattern.
 */
unsigned char CFileStream::Copy(const char *dstPath, const char *srcPath)
{
	unsigned char buf[1024];
	unsigned char err = 7;

	void *src = CSTGFile_Open(srcPath, 0 /* O_RDONLY */);
	if (src == 0)
		return err;

	void *dst = CSTGFile_Open(dstPath, 3 /* O_WRONLY|O_CREAT|O_TRUNC */);
	if (dst != 0) {
		unsigned int remaining = CSTGFile_GetFileSize(src);
		if (remaining == 0) {
			err = 0;
		} else {
			for (;;) {
				unsigned int chunk = (remaining < 0x400) ? remaining : 0x400;
				unsigned int got = (unsigned int)CSTGFile_Read(src, buf, chunk);
				if (got != chunk) {
					err = 1;
					break;
				}
				unsigned int wrote = (unsigned int)CSTGFile_Write(dst, buf, chunk);
				err = (unsigned char)(chunk != wrote);
				remaining -= chunk;
				if (err || remaining == 0)
					break;
			}
		}
		CSTGFile_Close(dst);
	}
	CSTGFile_Close(src);
	return err;
}
