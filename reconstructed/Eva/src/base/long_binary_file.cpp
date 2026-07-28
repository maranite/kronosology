/*
 * long_binary_file.cpp  -  see include/long_binary_file.h for full
 * ground-truth provenance, the slot-numbering bug that was caught and
 * corrected mid-reconstruction, and the ReadData/WriteData mByteOrder note.
 */

#include "long_binary_file.h"

#include <cstring>

CLongBinaryFile::CLongBinaryFile()
	: mFile(0)
{
	/* Real: mFileName/mByteOrder deliberately left uninitialized -- see
	 * header comment. Callers rely on Reset() for a clean state. */
}

CLongBinaryFile::~CLongBinaryFile()
{
	/* D0 additionally wraps `free(this)` in HAL_DisableInterrupts()/
	 * HAL_EnableInterrupts() -- not reproduced here, see header comment. */
	if (mFile) {
		CFileOperation::Close(mFile);
		mFile = 0;
	}
}

void CLongBinaryFile::GetFileName(char *dest)
{
	strcpy(dest, mFileName);
}

SFilePointer *CLongBinaryFile::Open(const char *path, int mode, bool /*unused*/)
{
	EOpenModeX om = (mode == 0x1ff) ? eOpenModeWrite : eOpenModeRead;
	mFile = CFileOperation::Open(path, om);
	strcpy(mFileName, path);
	return mFile;
}

void CLongBinaryFile::Close()
{
	CFileOperation::Close(mFile);
	mFile = 0;
}

void CLongBinaryFile::Reset()
{
	memset(mFileName, 0, sizeof(mFileName));
	mByteOrder = 0;
}

unsigned int CLongBinaryFile::Read(void *buf, unsigned int count)
{
	return CFileOperation::Read(buf, 1, count, mFile) ? count : 0;
}

unsigned int CLongBinaryFile::Write(const void *buf, unsigned int count)
{
	return CFileOperation::Write(buf, 1, count, mFile) ? count : 0;
}

long long CLongBinaryFile::Seek(long long offset, int type)
{
	/* Ground truth only forwards the low 32 bits of `offset` -- see
	 * header comment. */
	int ok = CFileOperation::Seek(mFile, (long)offset, (ESeekType)type);
	if (!ok)
		return -1;
	return (long long)Tell();
}

unsigned long CLongBinaryFile::Tell()
{
	unsigned long pos = 0;
	CFileOperation::Tell(mFile, &pos);
	return pos;
}

void CLongBinaryFile::MoveToEnd()
{
	Seek(0, eFileSeekEnd);
}

void CLongBinaryFile::Jump(int offset)
{
	CFileOperation::Seek(mFile, offset, eFileSeekCur);
}

long long CLongBinaryFile::ReadData(int count)
{
	int n = (count > 8) ? 8 : count;
	unsigned char buf[8];
	/* Ground truth calls Read() unconditionally, even for n<=0 (a
	 * negative count would be reinterpreted as a huge unsigned count by
	 * Read() -- reproduced faithfully; every real caller passes
	 * count in [1,8]). */
	Read(buf, (unsigned int)n);

	long long value = 0;
	if (mByteOrder == 0) {
		/* on-disk little-endian: byte[0] is the LSB */
		for (int i = 0; i < n; ++i)
			value |= (long long)(unsigned long long)buf[i] << (8 * i);
	} else {
		/* on-disk big-endian: byte[n-1] is the LSB */
		for (int i = 0; i < n; ++i)
			value |= (long long)(unsigned long long)buf[n - 1 - i] << (8 * i);
	}
	return value;
}

unsigned int CLongBinaryFile::WriteData(long long value, int count)
{
	int n = (count > 8) ? 8 : count;
	unsigned char buf[8];
	if (mByteOrder == 0) {
		for (int i = 0; i < n; ++i)
			buf[i] = (unsigned char)(value >> (8 * i));
	} else {
		for (int i = 0; i < n; ++i)
			buf[n - 1 - i] = (unsigned char)(value >> (8 * i));
	}
	return Write(buf, (unsigned int)n);
}

unsigned int CLongBinaryFile::ReadText(char *dest, int logicalLen, int bufSize)
{
	int n = (logicalLen < bufSize) ? logicalLen : bufSize;
	unsigned int result = Read(dest, (unsigned int)n);
	dest[n] = '\0';
	Jump(logicalLen - n);
	return result;
}

unsigned int CLongBinaryFile::WriteText(const char *str, int minLen)
{
	int len = (int)strlen(str);
	if (minLen <= len)
		return Write(str, (unsigned int)minLen);

	unsigned int result = Write(str, (unsigned int)len);
	int written = len + 1;
	WriteData(0, 1); /* first pad byte, unconditional in ground truth */
	while (written < minLen) {
		WriteData(0, 1);
		++written;
	}
	return result;
}
