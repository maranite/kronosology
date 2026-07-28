/*
 * file_operation_stub.cpp  -  REAL host-functional CFileOperation backed by
 * stdio, for verify/test_long_binary_file.cpp and verify/test_audio_file.cpp.
 *
 * CFileOperation is Eva's own out-of-scope global file manager (see
 * long_binary_file.h's header comment) -- not reconstructed here. Unlike
 * the inert libxml2 host stubs (src/convert/libxml2_host_stubs.cpp), this
 * one does real work: `SFilePointer` is a thin wrapper around a real
 * `FILE*`, so the CLongBinaryFile/CAudioFile family's own KAT tests can do
 * genuine round-trip file I/O (write a WAV, read it back, compare) against
 * this project's build host filesystem instead of only exercising
 * pure/leaf logic.
 */

#include "long_binary_file.h"

#include <cstdio>

struct SFilePointer {
	FILE *fp;
};

SFilePointer *CFileOperation::Open(const char *path, EOpenModeX mode)
{
	FILE *fp = fopen(path, (mode == eOpenModeWrite) ? "wb" : "rb");
	if (!fp)
		return 0;
	SFilePointer *out = new SFilePointer();
	out->fp = fp;
	return out;
}

void CFileOperation::Close(SFilePointer *fp)
{
	if (!fp)
		return;
	fclose(fp->fp);
	delete fp;
}

unsigned int CFileOperation::Read(void *buf, unsigned int elemSize, unsigned int count, SFilePointer *fp)
{
	if (!fp)
		return 0;
	return (unsigned int)fread(buf, elemSize, count, fp->fp);
}

unsigned int CFileOperation::Write(const void *buf, unsigned int elemSize, unsigned int count, SFilePointer *fp)
{
	if (!fp)
		return 0;
	return (unsigned int)fwrite(buf, elemSize, count, fp->fp);
}

int CFileOperation::Seek(SFilePointer *fp, long offset, ESeekType type)
{
	if (!fp)
		return 0;
	int whence = (type == eFileSeekEnd) ? SEEK_END : (type == eFileSeekCur) ? SEEK_CUR : SEEK_SET;
	return fseek(fp->fp, offset, whence) == 0;
}

unsigned int CFileOperation::Tell(SFilePointer *fp, unsigned long *outPos)
{
	if (!fp) {
		*outPos = 0;
		return 0;
	}
	long pos = ftell(fp->fp);
	*outPos = (unsigned long)pos;
	return 1;
}
