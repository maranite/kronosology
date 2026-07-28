/*
 * file_io_iso9660.cpp  -  see include/file_io_iso9660.h.
 *
 * Transcribed from `objdump -dr -M intel` (Decomp/EVA_Decomp/Eva,
 * .text+0x0831c020..0x0831ca70+0xfd / 0x089955c0..0x089955d0). See the header
 * comment for the per-method breakdown, file_io_driver_common.h for the shared
 * set_error()/Api-assert plumbing, and fmount()'s own header comment for a
 * genuine "always returns -1" finding.
 *
 * dir()'s attribute-byte filter bitmask and ConvertPathRtfsToCdfs()'s final
 * `strcat()` argument wiring are transcribed at a faithful structural level
 * with a couple of medium-confidence simplifications, each called out at its
 * own site below (same disclosure convention as file_io_dos.cpp's own).
 */

#include "file_io_iso9660.h"
#include "file_io_driver_common.h"

#include <cstring>

/* Real ground truth: cfioiso_dir_obj (.bss+0x93b1100, 0x12a=298 bytes) -- the
 * single shared ISO9660-library search/IO state object every CFileIoIso9660
 * instance's m_pStat points at (real symbol name via `nm -C -S`).
 */
unsigned char cfioiso_dir_obj[0x12a];

/* Real ground truth: cd_errno (.bss+0x9647ff4, plain `int`) -- set_error()'s
 * own raw error code (file_io_driver_common.h). Storage defined here (only
 * CFileIoIso9660 reads it).
 */
int cd_errno;

namespace {

/* Real ground truth: CDDriverIO::read_capacity/getdevinfo/scsi_mode_sel
 * (.text+0x0830e470/0x0830db00/0x0830e710) -- out of scope, same inert
 * stand-ins as file_io_akai.cpp's own copies.
 */
unsigned long long DDriverIOReadCapacityStub(EDevice_Id, unsigned short &sectorSize)
{
	sectorSize = 0;
	return 0;
}
struct DevInfoShim {
	unsigned char pad[2];
};
DevInfoShim s_devInfo;
DevInfoShim *DDriverIOGetDevInfoStub(EDevice_Id)
{
	return &s_devInfo;
}
int DDriverIOScsiModeSelStub(EDevice_Id, unsigned short)
{
	return 0;
}

/* Real ground truth: CMediaInfo::init (.text+0x08317be0), CFilesys::
 * get_fileioptr (.text+0x083217e0) -- out of scope, same inert stand-ins as
 * file_io_akai.cpp's own.
 */
void CMediaInfoInitStub(CMediaInfo *, const char *, EFileIOType, int, long long, unsigned int)
{
}
CFileIoBase *FilesysGetFileIoPtrStub(EFileIOType)
{
	static CFileIoBase s_stubDriver;
	return &s_stubDriver;
}

/* Real ground truth: cd_drvno2drv(short) (.text+0x83857b0) -- returns an
 * opaque per-drive struct; getmediainfo() only reads its own +0x5fa flag
 * byte. Modeled as a zeroed local buffer.
 */
unsigned char s_isoDrive[0x600];
unsigned char *CdDrvno2Drv(short)
{
	return s_isoDrive;
}

/* Real ground truth: cd_fileno2file(short) (.text+0x83857e0) -- returns an
 * opaque per-handle record; ftell() only reads its own +0x14 field.
 */
unsigned char s_isoFile[0x20];
unsigned char *CdFileno2File(short)
{
	return s_isoFile;
}

/* Real ground truth: cd_close/cd_gdone/cd_gfirst/cd_gnext/cd_read/cd_lseek/
 * cd_scwd/cd_gcwd/cd_open/cd_dskclose/cd_dskopen (.text+0x8383840/0x8383f80/
 * 0x8384200/0x8384020/0x8384ee0/0x8384320/0x83852d0/0x8383d20/0x8384d60/
 * 0x8385420/0x8385570) -- embedded ISO9660 library, out of scope. Inert
 * stand-ins.
 */
void CdClose(short)
{
}
void CdGdone(IsoCddstat *)
{
}
int CdGfirst(IsoCddstat *, char *)
{
	return 0;
}
int CdGnext(IsoCddstat *, char *)
{
	return 0;
}
int CdRead(short, unsigned char *, unsigned long)
{
	return -1;
}
int CdLseek(short, long, short)
{
	return 0;
}
int CdScwd(char *)
{
	return 0;
}
int CdGcwd(char *)
{
	return 0;
}
int CdOpen(char *)
{
	return -1;
}
void CdDskclose(char *)
{
}
int CdDskopen(char *)
{
	return -1;
}

/* Real ground truth: cd_memory_init() (.text+0x838bd90) -- out of scope
 * library-init hook called from the ctor. Inert stand-in, "0 == failure"
 * (matches ground truth's own `test eax,eax; je <log-and-continue>`).
 */
int CdMemoryInitStub()
{
	return 1;
}

/* Real ground truth: CFilePath::operator+=(char const*) (.text+0x8321150),
 * CFilePath::get_last() const (.text+0x8321480) -- CFilePath is out of scope
 * (not otherwise reconstructed in this project). Modeled as plain `strcat`-
 * shaped helpers over a raw `char*` buffer, matching the observed call shape
 * closely enough for funmount()/getwd() (both only ever append one string and
 * never otherwise touch the CFilePath object itself).
 */
void CFilePathAppendStub(char *path, const char *suffix)
{
	std::strcat(path, suffix);
}
const char *CFilePathGetLastStub(char *path)
{
	return path;
}

/* Real ground truth: char2long(char const*) (.text+0x8381cc0) -- decodes an
 * ISO9660 dual-endian 32-bit numeric field (the on-disk format is genuinely
 * "both-endian": little-endian half followed by big-endian half). Out of
 * scope; inert stand-in reading the little-endian half only (a reasonable,
 * if not byte-exact, approximation -- CFileDirEntryInitialize() below is
 * itself an inert no-op, so this value is never actually observed by any
 * test).
 */
long Char2Long(const char *p)
{
	return *reinterpret_cast<const long *>(p);
}

/* Real ground truth: CFileDirEntry::Initialize(...) (.text+0x82d8a00) -- out
 * of scope, same inert stand-in as file_io_akai.cpp's own.
 */
class CDate;
class COTime;
void CFileDirEntryInitialize(CFileDirEntry *, const char *, char, CDate *, COTime *, unsigned int, int)
{
}

} // namespace

CFileIoIso9660::CFileIoIso9660()
	: m_pStat(reinterpret_cast<IsoCddstat *>(cfioiso_dir_obj)), m_reserved0xfc(0), m_searchActive(0)
{
	std::memset(m_path, 0, sizeof(m_path));
	if (!CdMemoryInitStub())
		FileIoDriverApiAssert("DiskUtil/CustomFs/FileIoIso9660.cpp", 0x2b);
}

CFileIoIso9660::~CFileIoIso9660()
{
	/* Real body only resets the vtable pointer (D0 additionally wraps
	 * `free(this)` in HAL_DisableInterrupts()/HAL_EnableInterrupts()) --
	 * see header comment.
	 */
}

int CFileIoIso9660::get_iotype()
{
	return 4;
}

unsigned long long CFileIoIso9660::freebytes(EDevice_Id)
{
	return 0;
}

int CFileIoIso9660::getmediainfo(EDevice_Id device, CMediaInfo *info)
{
	unsigned short sectorSize;
	unsigned long long capacity = DDriverIOReadCapacityStub(device, sectorSize);
	unsigned char *drv = CdDrvno2Drv((short)(int)device);
	const char *name = drv[0x5fa] ? reinterpret_cast<const char *>(drv + 0x5fa) : "No Label"; /* .rodata+0x8eedd00 */

	CMediaInfoInitStub(info, name, (EFileIOType)4, 1, (long long)capacity, 0x800);
	return 0;
}

int CFileIoIso9660::format(EDevice_Id device, int arg2)
{
	return FilesysGetFileIoPtrStub((EFileIOType)3)->format(device, arg2, (EFatType)0);
}

long CFileIoIso9660::ftell(int handle)
{
	unsigned char *file = CdFileno2File((short)handle);
	return *reinterpret_cast<long *>(file + 0x14);
}

int CFileIoIso9660::fclose(int handle)
{
	CdClose((short)handle);
	return 0;
}

int CFileIoIso9660::funmount(EDevice_Id device)
{
	if (m_searchActive) {
		m_searchActive = 0;
		CdGdone(m_pStat);
	}

	char path[16];
	path[0] = 0;
	CFilePathAppendStub(path, DEVICE_ID_STR[(int)device]); /* SAME table content as .rodata+0x91b9900 -- confirmed byte-identical via objdump -s at .rodata+0x8eef2c0 */
	CdDskclose(path);
	return 0;
}

unsigned int CFileIoIso9660::fread(void *buf, unsigned int size, unsigned int count, int handle)
{
	int n = CdRead((short)handle, reinterpret_cast<unsigned char *>(buf), (unsigned long)size * count);
	if (n == -1) {
		set_error();
		return 0;
	}
	return (unsigned int)n / size;
}

int CFileIoIso9660::getwd(EDevice_Id device, char *buf)
{
	char path[64];
	path[0] = 0;
	CFilePathAppendStub(path, DEVICE_ID_STR[(int)device]);

	if (CdGcwd(path) == 0) {
		set_error();
		return 0;
	}
	if (CFilePathGetLastStub(path)[0] != 0)
		std::strcat(path, "\\"); /* .rodata+0x8eee3c4, confirmed via objdump -s */

	std::strncpy(buf, path, 0xf0);
	buf[0xf0] = 0;
	return reinterpret_cast<int>(buf);
}

int CFileIoIso9660::fseek(int handle, long offset, int whence)
{
	if (CdLseek((short)handle, offset, (short)whence) < 0) {
		set_error();
		return -1;
	}
	return 0;
}

void CFileIoIso9660::ConvertPathRtfsToCdfs(const char *path)
{
	std::strncpy(m_path, path, 0xf0);
	m_path[0xf0] = 0;

	bool hasDriveColon = (m_path[0] != 0) && (m_path[1] == ':');

	char *component;
	char *lastSlash = std::strrchr(m_path, '\\');
	if (lastSlash)
		component = lastSlash + 1;
	else
		component = hasDriveColon ? (m_path + 2) : m_path;

	char *space = std::strchr(component, ' ');
	if (!space)
		return;

	char *dot = std::strchr(component, '.');
	*space = 0;
	if (!dot || dot <= space)
		return;

	char *secondSpace = std::strchr(dot, ' ');
	if (secondSpace)
		*secondSpace = 0;

	/* Real: tail-calls `strcat()` once more with an argument pair derived
	 * from `ebx`/`edi` whose exact register provenance is ambiguous past
	 * this point (register reuse obscures whether the final append is
	 * "." + extension or something else) -- modeled as the most
	 * semantically sensible reading given the "." literal
	 * (.rodata+0x8eee3c6, confirmed via objdump -s) and an 8.3-style
	 * "NAME.EXT" target shape: append "." then the extension text found
	 * after the dot.
	 */
	std::strcat(m_path, ".");
	std::strcat(m_path, dot + 1);
}

void CFileIoIso9660::set_error()
{
	if (g_theFilesys->lastError != 0)
		return;

	int raw = cd_errno;
	if ((unsigned)raw <= 0xa) {
		unsigned bit = 1u << raw;
		if (bit & 0x428) {
			g_theFilesys->lastError = 1;
			return;
		}
		if (bit & 0x3c3) {
			g_theFilesys->lastError = 3;
			return;
		}
	}
	FileIoDriverApiAssert("DiskUtil/CustomFs/FileIoIso9660.cpp", 0x2c2);
}

int CFileIoIso9660::chdir(const char *path)
{
	ConvertPathRtfsToCdfs(path);
	if (CdScwd(m_path) == 0) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoIso9660::fopen(const char *path, const char * /*mode*/)
{
	ConvertPathRtfsToCdfs(path);
	if (m_searchActive) {
		m_searchActive = 0;
		CdGdone(m_pStat);
	}

	int rc = CdOpen(m_path);
	if (rc < 0) {
		set_error();
		return rc;
	}
	return rc;
}

int CFileIoIso9660::fmount(EDevice_Id device)
{
	DevInfoShim *info = DDriverIOGetDevInfoStub(device);
	if (!info || info->pad[1] != 7)
		return -1;

	unsigned short sectorSize = 0;
	unsigned long long capacity = DDriverIOReadCapacityStub(device, sectorSize);
	if (capacity == 0)
		return -1;

	if (sectorSize != 0x800) {
		DDriverIOScsiModeSelStub(device, 0x800);
		unsigned short sectorSize2 = 0;
		unsigned long long capacity2 = DDriverIOReadCapacityStub(device, sectorSize2);
		if (capacity2 == 0 || sectorSize2 != 0x800)
			return -1;
	}

	char path[16];
	path[0] = 0;
	CFilePathAppendStub(path, DEVICE_ID_STR[(int)device]);
	if (CdDskopen(path) == 0)
		set_error();

	/* Real: every path through this method returns -1, including this
	 * one -- see header comment for the full finding.
	 */
	return -1;
}

int CFileIoIso9660::dir(const char *path, int arg2, unsigned long &cont, CFileDirEntry *entry)
{
	if (cont == 0 && m_searchActive)
		CdGdone(m_pStat);

	bool haveEntry = false;
	unsigned char *rec = 0;

	for (;;) {
		if (cont != 0) {
			haveEntry = CdGnext(m_pStat, m_path) != 0;
		} else {
			if (arg2 != 0)
				ConvertPathRtfsToCdfs(path);
			haveEntry = CdGfirst(m_pStat, m_path) != 0;
			if (haveEntry) {
				m_searchActive = 1;
				cont = 1;
			}
		}
		if (!haveEntry)
			break;

		rec = reinterpret_cast<unsigned char *>(m_pStat);
		unsigned char attr = rec[0x12];
		/* Real: filters entries whose attribute byte (+0x12) has bit
		 * 0x4 or bit 0x1 set (skip unconditionally), or bit 0x2 set
		 * AND `rec[0x18] <= 1` (skip); everything else is a real,
		 * wanted entry. Transcribed at the bitmask level -- the exact
		 * `rec[0x18]`-vs-1 comparison's real semantic meaning (a
		 * hidden-file generation/version count?) is not recovered.
		 */
		if ((attr & 0x4) || (attr & 0x1))
			continue;
		if ((attr & 0x2) && rec[0x18] <= 1)
			continue;
		break;
	}

	if (!haveEntry) {
		m_searchActive = 0;
		cont = 0;
		CdGdone(m_pStat);
		return 0;
	}

	/* Real: ISO9660 directory records store their date/time as raw bytes
	 * directly (no CDateT::get() indirection needed here, unlike
	 * CFileIoAkai/CFileIoDos's own dir()) -- year (record+0xb, biased by
	 * +0x76c before the SAME real inline divide-by-100-via-multiply idiom
	 * as the other two drivers' own dir() methods), month (+0xc), day
	 * (+0xd), hour (+0xe), minute (+0xf), second (+0x10), plus a real
	 * `char2long()` call for the dual-endian LBA field at record+7.
	 */
	unsigned char yearRaw = rec[0xb];
	unsigned short biased = (unsigned short)(yearRaw + 0x76c);
	unsigned short shifted = (unsigned short)(biased >> 2);
	unsigned char fieldA = (unsigned char)(((unsigned)shifted * 0x147bu) >> 0x11);
	unsigned char fieldB = (unsigned char)(biased - (unsigned short)(fieldA * 100));
	unsigned char monthRaw = rec[0xc];
	unsigned char dayRaw = rec[0xd];
	unsigned char hourRaw = rec[0xe];
	unsigned char minRaw = rec[0xf];
	unsigned char secRaw = rec[0x10];

	unsigned char dateBuf[4] = { fieldA, fieldB, monthRaw, dayRaw };
	unsigned char timeBuf[3] = { hourRaw, minRaw, secRaw };

	unsigned int lba = (unsigned int)Char2Long(reinterpret_cast<const char *>(rec + 7));
	char attrChar = (char)rec[0x18];

	/* Real: `name` argument to Initialize() is derived from the record's
	 * own variable-length name field (offset depends on the record's own
	 * length byte, not a fixed offset) -- approximated here as a fixed
	 * offset since CFileDirEntryInitialize() is itself an inert no-op
	 * stand-in (this value is never actually observed by any test).
	 */
	CFileDirEntryInitialize(entry, reinterpret_cast<const char *>(rec) + 0x21, attrChar,
	                         reinterpret_cast<CDate *>(dateBuf), reinterpret_cast<COTime *>(timeBuf),
	                         lba, 0);
	return reinterpret_cast<int>(entry);
}
