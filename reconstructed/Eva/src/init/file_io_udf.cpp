/*
 * file_io_udf.cpp  -  see include/file_io_udf.h.
 *
 * Transcribed from `objdump -dr -M intel` (Decomp/EVA_Decomp/Eva,
 * .text+0x0831cb80..0x0831f4c0 / 0x08995610..0x08995620). format() is
 * deferred (see DECOMPILE_ERRORS.md) -- its slot is declared but not
 * defined, matching this file's own header comment.
 */

#include "file_io_udf.h"
#include "file_io_driver_common.h"

#include <cstring>

/* Real ground truth: CFileIoUdf::iStage (.bss+0x93b1240) -- the deferred
 * format()'s own resumable-stage state, declared for completeness.
 */
int CFileIoUdf::iStage;

/* Real ground truth: udf_errno (.bss+0x93b1794, plain `int`) -- this
 * class's own raw error code (confirmed via `nm -C -S`).
 */
int udf_errno;

namespace {

/* Real ground truth: set_error()'s 82-entry translation table
 * (.rodata+0x8eee054..0x8eee19c), decoded via `objdump -s` against the 9
 * distinct case bodies at .text+0x831d350..0x831d3d0 plus the shared
 * "no-op" epilogue (indices 0/23/41/42/43/44/81) and the out-of-range
 * Api-assert log. Same kNoop/kLog convention as file_io_cdda.cpp's own.
 */
enum { kNoop = -1, kLog = -2 };
const int kUdfErrTable[82] = {
	kNoop, 3, 3, 1, 3, 1, 3, 3, 3, 3, 3, 4, 9, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, kNoop,
	5, 7, 2, 6, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, kNoop, kNoop, kNoop, kNoop,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 3, 4, 3, 3, 4, 0xe, 4, kNoop,
};

/* Real ground truth: the shared search/IO state object CFileIoUdf::m_pStat
 * points at (.bss+0x93b1260) -- same "single shared global instance"
 * convention as CFileIoAkai/CFileIoCdda/CFileIoIso9660's own. Modeled as a
 * real local buffer here, NOT a hardcoded ground-truth address.
 */
unsigned char cfioudf_dir_obj[0x400];

/* Real ground truth: CDDriverIO::cdcapstat_tab (.bss+0x93b0d5e, SAME shared
 * per-device flags byte table CFileIoCdda::getmediainfo() also reads). Out
 * of scope; modeled as a zeroed local array indexed by device.
 */
unsigned char s_cdcapstatTab[10];

/* Real ground truth: udf_drvno2drv/udf_totalblks/udf_mediatype/udf_is_wps/
 * udf_freeblks/udf_fileno2file/udf_drv2drvno -- the embedded UDF library,
 * out of scope. Inert stand-ins.
 */
unsigned char s_udfDrive[0x1a10];
unsigned char *UdfDrvno2Drv(short) { return s_udfDrive; }
unsigned long UdfTotalBlks(short) { return 0; }
unsigned char UdfMediaType(short) { return 0; }
unsigned char UdfIsWps(short) { return 0; }
unsigned long UdfFreeBlks(short) { return 0; }
unsigned char s_udfFile[0x20];
unsigned char *UdfFileno2File(short) { return s_udfFile; }
short UdfDrv2DrvnoStub(const char *) { return 0; }

/* Real ground truth: CMediaInfo::init -- out of scope, same inert stand-in
 * as the other sibling classes' own.
 */
void CMediaInfoInitStub(CMediaInfo *, const char *, EFileIOType, int, long long, unsigned int) {}

/* Real ground truth: CFilePath::operator+=(char const*)/get_last() -- out
 * of scope, same stand-in convention as file_io_iso9660.cpp's own.
 */
void CFilePathAppendStub(char *path, const char *suffix) { std::strcat(path, suffix); }
const char *CFilePathGetLastStub(char *path) { return path; }

/* Real ground truth: udf_memory_init/udf_gdone/udf_dskclose/udf_gfirst/
 * udf_gnext/udf_next_isodir/udf_sortdir/udf_closepath/udf_open_nextpath/
 * udf_gcwd/udf_scwd/udf_flush/udf_lseek/udf_write/udf_read/udf_close/
 * udf_dskopen/udf_chmod/udf_rmdir/udf_mkdir/udf_unlink/udf_mv/udf_open --
 * the embedded UDF library, out of scope. Inert stand-ins.
 */
int UdfMemoryInitStub() { return 1; }
void UdfGdone(UdfDstat *) {}
void UdfDskclose(char *) {}
int UdfGfirst(UdfDstat *, char *) { return 0; }
int UdfGnext(UdfDstat *, char *) { return 0; }
int UdfNextIsodir(short, udf_iso_rec *, udf_iso_rec *) { return 0; }
int UdfSortdir(short) { return 0; }
int UdfClosepath(short, int) { return 0; }
int UdfOpenNextpath(short) { return 0; }
int UdfGcwd(char *) { return 0; }
int UdfScwd(char *) { return 0; }
int UdfFlush(short, unsigned char) { return 0; }
int UdfLseek(short, long, short) { return 0; }
int UdfWrite(short, const unsigned char *, unsigned long) { return -1; }
int UdfRead(short, unsigned char *, unsigned long) { return -1; }
short UdfClose(short) { return 0; }
int UdfDskopen(char *) { return 0; }
int UdfChmod(char *, unsigned long) { return 0; }
int UdfRmdir(char *) { return 0; }
int UdfMkdir(char *) { return 0; }
int UdfUnlink(char *) { return 0; }
int UdfMv(char *, char *) { return 0; }
int UdfOpen(char *, unsigned short, unsigned short) { return -1; }

/* Real ground truth: CDDriverIO::getdevinfo/read_capacity/scsi_mode_sel/
 * scsi_read_diskinfo/scsi_read_trkinfo/scsi_mode_sense10/scsi_mode_sel10/
 * scsi_getwritespeed/scsi_getmedia_recspeed/scsi_set_speed -- out of scope.
 */
struct DevInfoShim { unsigned char pad[2]; };
DevInfoShim s_devInfo;
DevInfoShim *DDriverIOGetDevInfoStub(EDevice_Id) { return &s_devInfo; }
unsigned long long DDriverIOReadCapacityStub(EDevice_Id, unsigned short &sectorSize) { sectorSize = 0; return 0; }
int DDriverIOScsiModeSelStub(EDevice_Id, unsigned short) { return 0; }
int DDriverIOScsiReadDiskinfo(EDevice_Id, unsigned char *buf) { std::memset(buf, 0, 4); return 0; }
int DDriverIOScsiReadTrkinfo(EDevice_Id, unsigned char *buf, unsigned long) { std::memset(buf, 0, 0x30); return 0; }
int DDriverIOScsiModeSense10(EDevice_Id, unsigned char *buf, unsigned long, unsigned long) { std::memset(buf, 0, 0x14); return 0; }
int DDriverIOScsiModeSel10(EDevice_Id, unsigned char *, unsigned long) { return 0; }
int DDriverIOScsiGetWriteSpeed(EDevice_Id, unsigned char *, unsigned char *) { return 0; }
int DDriverIOScsiGetMediaRecSpeed(EDevice_Id, unsigned char *, unsigned char *) { return 0; }
int DDriverIOScsiSetSpeed(EDevice_Id, unsigned char, unsigned char) { return 0; }

int CDeviceMgrIsWritableStub(EDevice_Id) { return 1; }

/* Real ground truth: convert_cmpress_dstring/lchar2short/cmpbuf/
 * CFileDirEntry::Initialize -- out of scope helpers used by dir(). Inert
 * stand-ins that always report "no match"/"empty name", matching the same
 * "dir()'s own consumer is itself a no-op stand-in" reasoning as
 * file_io_iso9660.cpp's own CFileDirEntryInitialize().
 */
short LChar2ShortStub(unsigned char *) { return 0; }
void ConvertCmpressDstringStub(char *, unsigned char *, unsigned short) {}
int CmpBufStub(unsigned char *, unsigned char *, short) { return 1; /* != 0 => "not equal" */ }
class CDate;
class COTime;
void CFileDirEntryInitializeStub(CFileDirEntry *, const char *, char, CDate *, COTime *, unsigned int, int) {}

} // namespace

CFileIoUdf::CFileIoUdf()
	: m_pStat(reinterpret_cast<UdfDstat *>(cfioudf_dir_obj)), m_reserved0xfc(0), m_searchActive(0), m_diskFlag(0)
{
	std::memset(m_path, 0, sizeof(m_path));
	if (!UdfMemoryInitStub())
		FileIoDriverApiAssert("DiskUtil/CustomFs/FileIoUdf.cpp", 0x66);
}

CFileIoUdf::~CFileIoUdf()
{
}

void CFileIoUdf::ConvertPathRtfsToUdffs(const char *path)
{
	std::strncpy(m_path, path, 0xf0);
	m_path[0xf0] = 0;
}

int CFileIoUdf::get_iotype() { return 3; }

unsigned long CFileIoUdf::totalfreeclus(EDevice_Id device)
{
	return UdfFreeBlks((short)(int)device);
}

unsigned long long CFileIoUdf::freebytes(EDevice_Id device)
{
	return (unsigned long long)UdfFreeBlks((short)(int)device) << 11;
}

int CFileIoUdf::getmediainfo(EDevice_Id device, CMediaInfo *info)
{
	unsigned char capstat = s_cdcapstatTab[(int)device % 10];
	bool bit4Clear = (capstat & 0x10) == 0;
	short dev = (short)(int)device;

	unsigned long blocks = UdfTotalBlks(dev);
	unsigned long long totalBytes = (unsigned long long)blocks << 11;

	int flag;
	if (bit4Clear && (capstat & 0x20) == 0) {
		flag = 1;
	} else {
		unsigned char mediatype = UdfMediaType(dev);
		if (mediatype == 1 && bit4Clear) {
			flag = 1;
		} else {
			flag = UdfIsWps(dev) ? 1 : 0;
		}
	}

	unsigned char *drv = UdfDrvno2Drv(dev);
	const char *name = (drv && drv[0x1a00]) ? reinterpret_cast<const char *>(drv + 0x1a00) : "No Label"; /* .rodata+0x8eedd00 */

	CMediaInfoInitStub(info, name, (EFileIOType)3, flag, (long long)totalBytes, 0x800);
	return 0;
}

long CFileIoUdf::ftell(int handle)
{
	unsigned char *file = UdfFileno2File((short)handle);
	return *reinterpret_cast<long *>(file + 0x18);
}

int CFileIoUdf::funmount(EDevice_Id device)
{
	if (m_searchActive) {
		m_searchActive = 0;
		UdfGdone(m_pStat);
	}
	char path[16];
	path[0] = 0;
	CFilePathAppendStub(path, DEVICE_ID_STR[(int)device]); /* .rodata+0x8eef2c0 */
	UdfDskclose(path);
	return 0;
}

int CFileIoUdf::dir(const char *path, int arg2, unsigned long &cont, CFileDirEntry *entry)
{
	static const char kNonAllocatable[] = "NON-ALLOCATABLE";
	static const char kNonAllocatableSpace[] = "NON-ALLOCATABLE SPACE";

	if (cont == 0 && m_searchActive)
		UdfGdone(m_pStat);

	bool haveEntry = false;
	char nameBuf[64];
	nameBuf[0] = 0;
	for (;;) {
		if (cont != 0) {
			haveEntry = UdfGnext(m_pStat, m_path) != 0;
		} else {
			if (arg2 != 0) {
				std::strncpy(m_path, path, 0xf0);
				m_path[0xf0] = 0;
			}
			haveEntry = UdfGfirst(m_pStat, m_path) != 0;
			if (haveEntry) {
				m_searchActive = 1;
				cont = 1;
			}
		}
		if (!haveEntry)
			break;

		/* Real: decompresses the found dstring name (`convert_cmpress_dstring`)
		 * into a scratch buffer, then filters out the two real UDF
		 * pseudo-entry names before applying an attribute-byte mask.
		 */
		unsigned char *rec = reinterpret_cast<unsigned char *>(m_pStat);
		unsigned char flags = rec[0x14];
		unsigned short nameLen = (unsigned short)LChar2ShortStub(rec + 0x15);
		nameBuf[0] = 0;
		ConvertCmpressDstringStub(nameBuf, rec + 0x26 + nameLen, 0);

		if (arg2 == 0) {
			/* cont-continue path re-tests flags 0x1/0x4 only. */
			if (flags & 0x4) {
				if (cont == 0)
					continue;
				break;
			}
		}

		if ((flags & 1) || (flags & 4))
			continue;

		if (CmpBufStub(reinterpret_cast<unsigned char *>(nameBuf), reinterpret_cast<unsigned char *>(const_cast<char *>(kNonAllocatable)), 0x14) == 0)
			continue;
		if (CmpBufStub(reinterpret_cast<unsigned char *>(nameBuf), reinterpret_cast<unsigned char *>(const_cast<char *>(kNonAllocatableSpace)), 0x15) == 0)
			continue;

		if (flags & 0xa) {
			/* Real: an extra hidden-name-length gate
			 * (`lchar2short(rec+0x26) != 0` then `!= 1`) -- transcribed
			 * structurally; exact semantic meaning of the two checks is
			 * not recovered beyond "skip empty/singleton hidden names".
			 */
			continue;
		}
		break;
	}

	if (!haveEntry) {
		m_searchActive = 0;
		cont = 0;
		UdfGdone(m_pStat);
		return 0;
	}

	unsigned char *rec = reinterpret_cast<unsigned char *>(m_pStat);

	/* Real: a real per-record attribute/hidden-flag combine (record+0x236..
	 * +0x237) feeding CFileDirEntry::Initialize()'s own attribute byte --
	 * transcribed structurally (exact bit semantics beyond "hidden"/
	 * "system"/"read-only"-shaped not recovered).
	 */
	unsigned char attrLo = rec[0x236];
	unsigned char attrHi = rec[0x237];
	unsigned short combined = (unsigned short)(attrLo + (attrHi << 8));

	unsigned short check = (unsigned short)(combined & 0x842);
	unsigned short flagsWord = (check != 0x842) ? 0x12 : 0x10;
	char attrChar = (char)flagsWord;

	/* Real inline divide-by-100-via-multiply idiom, SAME magic constant
	 * (0x147b) as CFileIoAkai/Dos/Iso9660's own dir() date decode.
	 */
	unsigned short packedDate = (unsigned short)(combined >> 2);
	unsigned char yearField = (unsigned char)(((unsigned)packedDate * 0x147bu) >> 0x11);
	unsigned char monthDayField = (unsigned char)(packedDate - (unsigned short)(yearField * 100));

	unsigned char dateBuf[4] = { yearField, monthDayField, rec[0x238], rec[0x239] };
	unsigned char timeBuf[3] = { rec[0x23a], rec[0x23b], rec[0x23c] };
	unsigned int lba = *reinterpret_cast<unsigned int *>(rec + 0x230);

	CFileDirEntryInitializeStub(entry, nameBuf, attrChar,
	                             reinterpret_cast<CDate *>(dateBuf), reinterpret_cast<COTime *>(timeBuf),
	                             lba, 1);
	return reinterpret_cast<int>(entry);
}

int CFileIoUdf::isodir(EDevice_Id device, udf_iso_rec *a, udf_iso_rec *b)
{
	if (!UdfNextIsodir((short)(int)device, a, b)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoUdf::sortdir(EDevice_Id device)
{
	if (!UdfSortdir((short)(int)device)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoUdf::closepath(EDevice_Id device, int arg2)
{
	if (!UdfClosepath((short)(int)device, arg2)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoUdf::opennextpath(EDevice_Id device)
{
	if (!UdfOpenNextpath((short)(int)device)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoUdf::getwd(EDevice_Id device, char *buf)
{
	char path[16];
	path[0] = 0;
	CFilePathAppendStub(path, DEVICE_ID_STR[(int)device]); /* .rodata+0x8eee440 */

	if (!UdfGcwd(path)) {
		set_error();
		return 0;
	}
	if (CFilePathGetLastStub(path)[0] != 0)
		std::strcat(path, "\\"); /* .rodata+0x8eee3c4 */

	std::strncpy(buf, path, 0xf0);
	buf[0xf0] = 0;
	return reinterpret_cast<int>(buf);
}

int CFileIoUdf::chdir(const char *path)
{
	std::strncpy(m_path, path, 0xf0);
	m_path[0xf0] = 0;

	if (!UdfScwd(m_path)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoUdf::fflush(int handle)
{
	if (!UdfFlush((short)handle, m_diskFlag)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoUdf::fseek(int handle, long offset, int whence)
{
	if (UdfLseek((short)handle, offset, (short)whence) < 0) {
		set_error();
		return -1;
	}
	return 0;
}

unsigned int CFileIoUdf::fwrite(const void *buf, unsigned int size, unsigned int count, int handle)
{
	unsigned int n = size * count;
	int written = UdfWrite((short)handle, reinterpret_cast<const unsigned char *>(buf), n);
	if (written == -1) {
		if (g_theFilesys->lastError != 2)
			set_error();
		return 0;
	}
	return (unsigned int)written / size;
}

unsigned int CFileIoUdf::fread(void *buf, unsigned int size, unsigned int count, int handle)
{
	unsigned int n = size * count;
	int got = UdfRead((short)handle, reinterpret_cast<unsigned char *>(buf), n);
	if (got == -1) {
		set_error();
		return 0;
	}
	return (unsigned int)got / size;
}

int CFileIoUdf::fclose(int handle)
{
	if (UdfClose((short)handle) != 0) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoUdf::fmount(EDevice_Id device)
{
	g_theFilesys->lastError = 0;

	DevInfoShim *info = DDriverIOGetDevInfoStub(device);
	if (!info || info->pad[1] != 7)
		return -1;

	unsigned short sectorSize;
	unsigned long long capacity = DDriverIOReadCapacityStub(device, sectorSize);
	if (capacity == 0)
		return -1;

	if (sectorSize != 0x800) {
		DDriverIOScsiModeSelStub(device, 0x800);
		capacity = DDriverIOReadCapacityStub(device, sectorSize);
		if (capacity == 0)
			return -1;
		if (sectorSize != 0x800)
			return -1;
	}

	char path[16];
	path[0] = 0;
	CFilePathAppendStub(path, DEVICE_ID_STR[(int)device]); /* .rodata+0x8eef2c0 */
	if (!UdfDskopen(path)) {
		g_theFilesys->lastError = 5;
		return -1;
	}

	unsigned char probe[8];
	if (!DDriverIOScsiReadDiskinfo(device, probe)) {
		set_error();
		return -1;
	}
	m_diskFlag = (probe[2] & 0x10) ? 1 : 0;
	return 0;
}

int CFileIoUdf::writesetup(EDevice_Id device, int /*arg2*/)
{
	short dev = (short)(int)device;
	unsigned char localOut[4] = { 0, 0, 0, 0 };
	(void)localOut;

	unsigned char *drv = UdfDrvno2Drv(dev);
	if (!drv) {
		g_theFilesys->lastError = 3;
		return -1;
	}
	if (*reinterpret_cast<unsigned long *>(drv + 4) != 0)
		return 0;

	bool isCdrw = (drv[2] == 1);
	unsigned char trkCount = isCdrw ? 1 : (unsigned char)(drv[0x25bd] + 1);

	unsigned char trkbuf[0x30];
	if (!DDriverIOScsiReadTrkinfo(device, trkbuf, trkCount)) {
		g_theFilesys->lastError = 4;
		return -1;
	}

	unsigned char byte5 = trkbuf[5];

	if (!DDriverIOScsiModeSense10(device, trkbuf, 5, 0x40)) {
		g_theFilesys->lastError = 4;
		return -1;
	}

	unsigned long selLen = (unsigned long)trkbuf[1] + 2;

	/* Real MMC "WRITE PARAMETERS" mode-select page, real fixed-byte
	 * template with 2 real variants selected by isCdrw -- see header
	 * comment.
	 */
	unsigned char page[0x31];
	std::memset(page, 0, sizeof(page));
	page[1] = 0x0a;
	page[2] = 0x00;
	if (isCdrw)
		page[3] = (unsigned char)((byte5 & 0xf) | 0x20);
	else
		page[3] = (unsigned char)((byte5 & 0xf) | 0xc0);
	page[6] = 0x20;
	page[9] = 0x20;
	page[0xd] = 0x20;
	page[0xe] = 0x20;
	page[0xf] = 0x20;
	page[0x10] = 0x20;
	page[0x11] = 0x20;
	page[0x12] = 0x20;
	page[0x13] = 0x20;
	page[0x14] = 0x20;
	page[0x15] = 0x20;
	page[0x1d] = 0x20;
	page[0x1e] = 0x30;
	page[0x1f] = 0x30;
	page[0x20] = 0x30;
	page[0x21] = 0x30;
	page[0x22] = 0x30;
	page[0x23] = 0x30;
	page[0x24] = 0x30;
	page[0x2e] = 0x08;

	if (!DDriverIOScsiModeSel10(device, page, selLen)) {
		g_theFilesys->lastError = 4;
		return -1;
	}

	unsigned char writeSpeedA = 0, writeSpeedB = 0;
	DDriverIOScsiGetWriteSpeed(device, &writeSpeedA, &writeSpeedB);
	unsigned char recSpeedA = 0, recSpeedB = 0;
	DDriverIOScsiGetMediaRecSpeed(device, &recSpeedA, &recSpeedB);

	if (!DDriverIOScsiSetSpeed(device, 0xff, 0xff)) {
		g_theFilesys->lastError = 4;
		return -1;
	}

	*reinterpret_cast<unsigned long *>(drv + 4) = 1;
	return 0;
}

int CFileIoUdf::chmod(const char *path, unsigned char mode)
{
	std::strncpy(m_path, path, 0xf0);
	m_path[0xf0] = 0;

	short drvno = UdfDrv2DrvnoStub(m_path);
	if (writesetup((EDevice_Id)drvno, 0) < 0)
		return -1;

	unsigned long mask = (mode & 1) ? 0x35ad : 0x7fff;
	if (!UdfChmod(m_path, mask)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoUdf::rmdir(const char *path)
{
	std::strncpy(m_path, path, 0xf0);
	m_path[0xf0] = 0;

	short drvno = UdfDrv2DrvnoStub(m_path);
	if (writesetup((EDevice_Id)drvno, 0) < 0)
		return -1;

	if (m_searchActive) {
		m_searchActive = 0;
		UdfGdone(m_pStat);
	}
	if (!UdfRmdir(m_path)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoUdf::mkdir(const char *path)
{
	std::strncpy(m_path, path, 0xf0);
	m_path[0xf0] = 0;

	short drvno = UdfDrv2DrvnoStub(m_path);
	if (writesetup((EDevice_Id)drvno, 0) < 0)
		return -1;

	if (!UdfMkdir(m_path)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoUdf::remove(const char *path)
{
	std::strncpy(m_path, path, 0xf0);
	m_path[0xf0] = 0;

	short drvno = UdfDrv2DrvnoStub(m_path);
	if (writesetup((EDevice_Id)drvno, 0) < 0)
		return -1;

	if (m_searchActive) {
		m_searchActive = 0;
		UdfGdone(m_pStat);
	}
	if (!UdfUnlink(m_path)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoUdf::rename(const char *oldPath, const char *newPath)
{
	std::strncpy(m_path, oldPath, 0xf0);
	m_path[0xf0] = 0;

	short drvno = UdfDrv2DrvnoStub(m_path);
	if (writesetup((EDevice_Id)drvno, 0) < 0)
		return -1;

	if (!UdfMv(m_path, const_cast<char *>(newPath))) {
		set_error();
		return -1;
	}
	return 0;
}

void CFileIoUdf::set_error()
{
	if (g_theFilesys->lastError != 0)
		return;

	int raw = udf_errno;
	int mapped = kLog;
	if ((unsigned)raw <= 0x51)
		mapped = kUdfErrTable[raw];

	if (mapped == kNoop)
		return;
	if (mapped != kLog) {
		g_theFilesys->lastError = mapped;
		return;
	}
	FileIoDriverApiAssert("DiskUtil/CustomFs/FileIoUdf.cpp", 0x757);
}

int CFileIoUdf::fopen(const char *path, const char *mode)
{
	std::strncpy(m_path, path, 0xf0);
	m_path[0xf0] = 0;

	if (m_searchActive) {
		m_searchActive = 0;
		UdfGdone(m_pStat);
	}

	/* Real: 23-entry mode-char jump table (mode[0]-'a', 0..0x16) -- richer
	 * than the other siblings' own, see header comment for the 'c'/'p'
	 * multi-char-literal compares and the 'v' unconditional-failure quirk.
	 */
	unsigned short openMode;
	unsigned short openFlags;
	bool checkWritable;

	unsigned char idx = (unsigned char)(mode[0] - 'a');
	switch (idx > 0x16 ? 0xff : idx) {
	case 0: { /* 'a' */
		openFlags = 0x100;
		openMode = 0x109;
		const char *p = mode;
		while (*++p) {
			if (*p == '+') {
				openFlags |= 0x80;
				openMode = 0x10a;
			}
		}
		checkWritable = true;
		break;
	}
	case 2: { /* 'c' -- compares mode against literal "cp" (medium confidence). */
		int cmp = std::strncmp(mode, "cp", 3);
		if (cmp <= 0) {
			openMode = 0x22;
		} else {
			openMode = 0x302;
		}
		openFlags = 0x180;
		checkWritable = true;
		break;
	}
	case 7: /* 'h' */
		openFlags = 0x200;
		openMode = 0x302;
		checkWritable = false;
		break;
	case 15: { /* 'p' -- compares mode against literal "pcp" (medium confidence). */
		int cmp = std::strncmp(mode, "pcp", 4);
		if (cmp <= 0) {
			openMode = 0x110;
		} else {
			openMode = 0x302;
		}
		openFlags = 0x180;
		checkWritable = true;
		break;
	}
	case 17: { /* 'r' */
		openFlags = 0x80;
		openMode = 0;
		const char *p = mode;
		while (*++p) {
			if (*p == '+') {
				openFlags |= 0x100;
				openMode = 2;
			}
		}
		checkWritable = (openFlags & 0x100) != 0;
		break;
	}
	case 21: { /* 'v' -- GENUINE FINDING: returns -1 unconditionally, no other work. */
		return -1;
	}
	case 22: { /* 'w' */
		openFlags = 0x100;
		openMode = 0x301;
		const char *p = mode;
		while (*++p) {
			if (*p == '+') {
				openFlags |= 0x80;
				openMode = 0x302;
			}
		}
		checkWritable = true;
		break;
	}
	default:
		openFlags = 0x180;
		openMode = 0x10a;
		checkWritable = true;
		break;
	}

	if (checkWritable) {
		short drvno = UdfDrv2DrvnoStub(m_path);
		if (!CDeviceMgrIsWritableStub((EDevice_Id)drvno)) {
			g_theFilesys->lastError = 9;
			return -1;
		}
	}

	int rc = UdfOpen(m_path, openMode, openFlags);
	if (rc < 0) {
		set_error();
		return rc;
	}
	return rc;
}

int CFileIoUdf::SetRecoveryParam(EDevice_Id device, int /*arg2*/)
{
	unsigned char buf[0x14];
	if (!DDriverIOScsiModeSense10(device, buf, 1, 0x14))
		return -1;

	unsigned int len = (unsigned int)buf[1] + 2;
	unsigned int selLen = (len <= 0x14) ? len : 0x14;

	int rc = DDriverIOScsiModeSel10(device, buf, selLen);
	return (rc == 1) ? 0 : -1;
}

int CFileIoUdf::formatsub(EDevice_Id device, int arg2, unsigned char *out3, long *outLong)
{
	unsigned char buf[0x30];
	if (!DDriverIOScsiReadTrkinfo(device, buf, 1))
		return -1;

	unsigned int idx = (arg2 == 0) ? 0xc : 4;
	out3[0xc] = buf[0x28 + idx];
	out3[0xd] = buf[0x29 + idx];
	out3[0xe] = buf[0x2a + idx];
	out3[0xf] = buf[0x2b + idx];

	unsigned long value = ((unsigned long)out3[0xc] << 24) | ((unsigned long)out3[0xd] << 16) |
	                       ((unsigned long)out3[0xe] << 8) | (unsigned long)out3[0xf];

	if (value <= 0x3ffff)
		return -1;

	if (value > 0x50000) {
		/* Real: round to the nearest multiple of 0x40 via signed division
		 * (magic-constant reciprocal, transcribed as a plain division --
		 * behaviorally identical for this value range).
		 */
		long signedVal = (long)value;
		value = (unsigned long)((signedVal / 0x40) * 0x40);
	}
	value &= 0xffffffe0u;

	out3[0xc] = (unsigned char)(value >> 24);
	out3[0xd] = (unsigned char)(value >> 16);
	out3[0xe] = (unsigned char)(value >> 8);
	out3[0xf] = (unsigned char)value;

	*outLong = (long)value;
	return 0;
}

void CFileIoUdf::setfmtparam(unsigned char *params)
{
	std::memset(params, 0, 0xc);
	params[3] = 8;
}
