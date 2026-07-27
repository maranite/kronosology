/*
 * file_io_base.cpp  -  see include/file_io_base.h.
 *
 * All 49 non-ctor/dtor methods transcribed from `objdump -dr -M intel`
 * (Decomp/EVA_Decomp/Eva, .text+0x08318480..0x08318cf0). 30 "heavy" methods share
 * one real shape: load the global `Api` object (`ds:0x930a1f4`), call its own
 * vtable slot +0x94 with `(Api, "Assertion failed in module %s, line %i.\n",
 * "DiskUtil/CustomFs/FileIoBase.cpp", <line>)`, then return a fixed sentinel --
 * same ApiAssert idiom already established project-wide (tempo.cpp, mains.cpp,
 * config_manager.cpp, stg_unsol_msg_handler.cpp). The other 19 "light" methods
 * (incl. get_iotype()) skip the assert call entirely and just return the
 * sentinel. See file_io_base.h's own header comment for the full breakdown and
 * per-method real addresses/line numbers.
 */

#include "file_io_base.h"
#include "system_api.h"

extern CSystemApi *Api;

namespace {

/* Real Api+0x94 assert-report call -- see header comment. cdecl,
 * EvaVTableStub-backed wherever not independently confirmed; never fatal (same
 * "forwards to the standing EvaVTableStub" note as mains.cpp's own ApiAssert()).
 */
inline void ApiAssert(int line)
{
	typedef void (*Fn)(void *, const char *, const char *, int);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x94);
	fn(Api, "Assertion failed in module %s, line %i.\n", "DiskUtil/CustomFs/FileIoBase.cpp", line);
}

} // namespace

CFileIoBase::CFileIoBase()
{
	/* Real body only sets the vtable pointer -- compiler bookkeeping, empty
	 * ctor is faithful (see header comment).
	 */
}

CFileIoBase::~CFileIoBase()
{
	/* Real body only resets the vtable pointer (D0 additionally free()s
	 * `this`) -- compiler bookkeeping, empty dtor is faithful (see header
	 * comment).
	 */
}

int CFileIoBase::get_iotype()
{
	return 0;
}

int CFileIoBase::fmount(EDevice_Id)
{
	ApiAssert(70);
	return -1;
}

int CFileIoBase::fmount(EDevice_Id, EMountIoType, int *)
{
	ApiAssert(79);
	return -1;
}

int CFileIoBase::funmount(EDevice_Id)
{
	ApiAssert(88);
	return -1;
}

int CFileIoBase::fopen(const char *, const char *)
{
	ApiAssert(97);
	return -1;
}

int CFileIoBase::fclose(int)
{
	ApiAssert(106);
	return -1;
}

unsigned int CFileIoBase::fread(void *, unsigned int, unsigned int, int)
{
	ApiAssert(115);
	return 0;
}

unsigned int CFileIoBase::fwrite(const void *, unsigned int, unsigned int, int)
{
	ApiAssert(124);
	return 0;
}

int CFileIoBase::fseek(int, long, int)
{
	ApiAssert(133);
	return -1;
}

long CFileIoBase::ftell(int)
{
	ApiAssert(142);
	return -1;
}

int CFileIoBase::fflush(int)
{
	ApiAssert(151);
	return -1;
}

int CFileIoBase::resize(int, unsigned int)
{
	ApiAssert(160);
	return -1;
}

int CFileIoBase::format(EDevice_Id, int)
{
	ApiAssert(169);
	return -1;
}

int CFileIoBase::format(EDevice_Id, int, EFatType)
{
	/* Light -- the one 3-arg format() overload that skips the assert call,
	 * faithfully preserved (see header comment).
	 */
	return -1;
}

unsigned long long CFileIoBase::freebytes(EDevice_Id)
{
	return 0;
}

unsigned long CFileIoBase::totalfreeclus(EDevice_Id)
{
	ApiAssert(198);
	return 0;
}

int CFileIoBase::chdir(const char *)
{
	return -1;
}

int CFileIoBase::getwd(EDevice_Id, char *)
{
	return 0;
}

int CFileIoBase::dir(const char *, int, unsigned long &, CFileDirEntry *)
{
	return 0;
}

int CFileIoBase::rename(const char *, const char *)
{
	ApiAssert(235);
	return -1;
}

int CFileIoBase::remove(const char *)
{
	ApiAssert(244);
	return -1;
}

int CFileIoBase::mkdir(const char *)
{
	ApiAssert(253);
	return -1;
}

int CFileIoBase::rmdir(const char *)
{
	ApiAssert(262);
	return -1;
}

int CFileIoBase::getmediainfo(EDevice_Id, CMediaInfo *)
{
	return -1;
}

int CFileIoBase::play(EDevice_Id, unsigned char, unsigned char, unsigned long, unsigned long)
{
	return -1;
}

int CFileIoBase::stop(EDevice_Id)
{
	return -1;
}

int CFileIoBase::pause(EDevice_Id)
{
	return -1;
}

int CFileIoBase::resume(EDevice_Id)
{
	return -1;
}

int CFileIoBase::ffscan(EDevice_Id, unsigned char, unsigned char, unsigned long, unsigned long)
{
	return -1;
}

int CFileIoBase::rewscan(EDevice_Id, unsigned char, unsigned char, unsigned long, unsigned long)
{
	return -1;
}

int CFileIoBase::stopscan(EDevice_Id)
{
	return -1;
}

int CFileIoBase::getcurpos(EDevice_Id, EAudioStatusMMC *, unsigned char *, unsigned char *, int, int)
{
	return 0;
}

int CFileIoBase::getmaxtrk(EDevice_Id, unsigned char *)
{
	return -1;
}

int CFileIoBase::getmaxidx(EDevice_Id, unsigned char, unsigned char *)
{
	return -1;
}

int CFileIoBase::gettrklen(EDevice_Id, unsigned char, unsigned long *)
{
	return -1;
}

int CFileIoBase::getidxlen(EDevice_Id, unsigned char, unsigned char, unsigned char, unsigned long *)
{
	return -1;
}

int CFileIoBase::finalize(EDevice_Id)
{
	ApiAssert(386);
	return -1;
}

int CFileIoBase::settestmode(EDevice_Id, int)
{
	ApiAssert(395);
	return -1;
}

unsigned int CFileIoBase::fdummywrite(unsigned int, unsigned int, int)
{
	ApiAssert(401);
	return 0;
}

int CFileIoBase::getfilelbaarray(EDevice_Id, int, CFileLbaArray *)
{
	ApiAssert(407);
	return -1;
}

int CFileIoBase::opennextpath(EDevice_Id)
{
	ApiAssert(416);
	return -1;
}

int CFileIoBase::closepath(EDevice_Id, int)
{
	ApiAssert(424);
	return -1;
}

int CFileIoBase::sortdir(EDevice_Id)
{
	ApiAssert(432);
	return -1;
}

int CFileIoBase::isodir(EDevice_Id, udf_iso_rec *, udf_iso_rec *)
{
	ApiAssert(440);
	return -1;
}

int CFileIoBase::getemphasized(int, int *)
{
	ApiAssert(449);
	return -1;
}

unsigned long CFileIoBase::getmaxclusterno(EDevice_Id)
{
	ApiAssert(458);
	return 0;
}

int CFileIoBase::scandisk(EDevice_Id, unsigned long, unsigned long, unsigned long *, unsigned long *)
{
	ApiAssert(466);
	return -1;
}

int CFileIoBase::optimizemedium(EDevice_Id, unsigned long, unsigned long *, int)
{
	ApiAssert(474);
	return -1;
}

int CFileIoBase::chmod(const char *, unsigned char)
{
	ApiAssert(482);
	return -1;
}
