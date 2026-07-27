/*
 * file_io_unknown.cpp  -  see include/file_io_unknown.h.
 *
 * 6 overridden methods + dtor pair, transcribed from `objdump -dr -M intel`
 * (Decomp/EVA_Decomp/Eva, .text+0x08318d30..0x08318e80 / 0x08995490..0x089954a0).
 * See header comment for the full per-method breakdown and the EvaVTableStub-style
 * stand-in convention used for the 3 out-of-scope external dependencies
 * (CDDriverIO::read_capacity, CFilesys::get_fileioptr, CMediaInfo::init -- same
 * convention as src/ui/panel_ifc_task.cpp's PegMessageQueuePush/
 * ControlSurfaceGetInstance stand-ins).
 */

#include "file_io_unknown.h"

namespace {

/* Real ground-truth globals: CDDriverIO::devstat_tab / CDDriverIO::cdcapstat_tab
 * (.bss+0x093b0d54 / +0x093b0d5e, 10 bytes each, confirmed via `nm -C -S`) -- real
 * per-device byte tables CFileIoUnknown::getmediainfo()/format() index directly
 * with `device` as a plain array index. CDDriverIO itself (85 methods) is out of
 * scope (file_io_base.h's "OUT OF SCOPE" list); these two tables are modeled
 * locally, zero-initialized, since nothing in this reconstruction's own traced
 * boot path populates them (real behavior with all bits clear: getmediainfo()'s
 * write-protect flag reads 0, format()'s selector branch takes the "no bits set"
 * -> 6 path).
 */
unsigned char s_devstatTab[10];
unsigned char s_cdCapStatTab[10];

/* Real ground-truth external dependency: CDDriverIO::read_capacity(EDevice_Id,
 * unsigned short&) (.text+0x0830e470) -- returns a 64-bit sector count (EDX:EAX)
 * and writes a sector-size hint through the reference out-param. Modeled as an
 * inert stand-in returning 0/0, same convention as panel_ifc_task.cpp.
 */
unsigned long long DDriverIOReadCapacityStub(EDevice_Id /*device*/, unsigned short &sectorSize)
{
	sectorSize = 0;
	return 0;
}

/* Real ground-truth external dependency: CMediaInfo::init(char const*,
 * EFileIOType, int, long long, unsigned int) (.text+0x08317be0) -- CMediaInfo is
 * entirely out of scope (file_io_base.h), no known members to write faithfully.
 * Modeled as an inert stand-in, same convention as above.
 */
void CMediaInfoInitStub(CMediaInfo * /*info*/, const char * /*name*/, EFileIOType /*type*/,
                         int /*flag*/, long long /*size*/, unsigned int /*extra*/)
{
}

/* Real ground-truth external dependency: CFilesys::get_fileioptr(EFileIOType)
 * (.text+0x083217e0) -- CFilesys (87 methods, VFAT-backed concrete driver) looks
 * up the CFileIoBase-derived driver instance responsible for a given
 * EFileIOType. Modeled as an inert stand-in returning a pointer to a local
 * static plain CFileIoBase (itself fully real, file_io_base.h) -- gives
 * CFileIoUnknown::format()'s real tail-call-through-vtable shape a genuinely
 * dispatchable, safe target (CFileIoBase's own format() bodies already return
 * -1 with an assert-log, exactly the right "unknown/unhandled driver" behavior)
 * without fabricating CFilesys's real driver-selection logic.
 */
CFileIoBase *FilesysGetFileIoPtrStub(EFileIOType /*selector*/)
{
	static CFileIoBase s_stubDriver;
	return &s_stubDriver;
}

} // namespace

int CFileIoUnknown::get_iotype()
{
	return 1;
}

int CFileIoUnknown::fmount(EDevice_Id)
{
	return -1;
}

int CFileIoUnknown::funmount(EDevice_Id)
{
	return 0;
}

int CFileIoUnknown::getmediainfo(EDevice_Id device, CMediaInfo *info)
{
	unsigned short sectorSize;
	unsigned long long sectorCount = DDriverIOReadCapacityStub(device, sectorSize);
	int writeProtect = s_devstatTab[device] & 1;

	/* .rodata+0x8eedd09 == "Unformatted" (confirmed via objdump -s -j .rodata). */
	CMediaInfoInitStub(info, "Unformatted", kFileIOType_Unknown,
	                   writeProtect, (long long)sectorCount, 0);
	return 0;
}

int CFileIoUnknown::format(EDevice_Id device, int arg2)
{
	EFileIOType selector = (s_cdCapStatTab[device] & 0x30) ? (EFileIOType)3 : (EFileIOType)6;
	return FilesysGetFileIoPtrStub(selector)->format(device, arg2);
}

int CFileIoUnknown::format(EDevice_Id device, int arg2, EFatType fatType)
{
	return FilesysGetFileIoPtrStub((EFileIOType)6)->format(device, arg2, fatType);
}

CFileIoUnknown::~CFileIoUnknown()
{
	/* Real body only resets the vtable pointer (D0 additionally free()s
	 * `this`) -- same compiler-bookkeeping shape as CFileIoBase's own dtor
	 * pair (see header comment).
	 */
}
