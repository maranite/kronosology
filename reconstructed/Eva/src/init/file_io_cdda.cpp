/*
 * file_io_cdda.cpp  -  see include/file_io_cdda.h.
 *
 * Transcribed from `objdump -dr -M intel` (Decomp/EVA_Decomp/Eva,
 * .text+0x08318ea0..0x0831a520 / 0x08995520..0x08995530). getcurpos() is
 * deferred (see DECOMPILE_ERRORS.md) -- its slot is declared but not
 * defined, matching this file's own header comment.
 */

#include "file_io_cdda.h"
#include "file_io_driver_common.h"

#include <cstring>

/* Real ground truth: cdda_errno (.bss+0x9600554, plain `int`) -- this
 * class's OWN raw error code, distinct from the shared `fs_user`/`cd_errno`
 * globals the other 4 sibling classes use (confirmed via `nm -C -S`).
 */
int cdda_errno;

namespace {

/* Real ground truth: set_error()'s 23-entry translation table
 * (.rodata+0x8eede84..0x8eedee0), decoded via `objdump -s` against the 5
 * distinct case bodies at .text+0x8319b10..0x8319b50 plus the shared
 * "no-op" epilogue (index 0) and the out-of-range Api-assert log. Same
 * kNoop/kLog convention as file_io_akai.cpp's own kAkaiErrTable.
 */
enum { kNoop = -1, kLog = -2 };
const int kCddaErrTable[23] = {
	kNoop, 3, 1, 3, 3, 3, 1, 6, 2, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 3, 4, 4, 3,
};

/* Real ground truth: the shared search/IO state object CFileIoCdda::m_pStat
 * points at (.bss+0x93b0fc4) -- same "single shared global instance"
 * convention as CFileIoAkai/CFileIoIso9660's own cfioakai_dir_obj/
 * cfioiso_dir_obj. Modeled as a real local buffer here, NOT a hardcoded
 * ground-truth address (that address doesn't exist in this host process).
 */
unsigned char cfiocdda_dir_obj[0x40];

/* Real ground truth: CDDriverIO::cdcapstat_tab (.bss+0x93b0d5e, SAME shared
 * per-device flags byte table used by CFileIoUnknown::format()/
 * CFileIoUdf::getmediainfo()'s own). Out of scope; modeled as a zeroed
 * local array indexed by device, same convention as file_io_unknown.cpp's
 * own devstat_tab/cdcapstat_tab stand-ins.
 */
unsigned char s_cdcapstatTab[10];

/* Real ground truth: cdda_drvno2drv/cdda_totalbytes/cdda_freebytes/
 * CDDriverIO::scsi_read_diskinfo/CDDriverIO::read_capacity/CDDriverIO::
 * scsi_mode_sel/CDDriverIO::getdevinfo/CDDriverIO::scsi_get_event/
 * CDDriverIO::scsi_mode_sense10/CDDriverIO::scsi_close_trk/
 * USTGAPICDAudio::GetCurrentPosition -- out of scope. Inert stand-ins, same
 * convention as file_io_akai.cpp/file_io_iso9660.cpp's own.
 */
unsigned char s_cddaDrive[0x700];
unsigned char *CddaDrvno2Drv(short) { return s_cddaDrive; }
unsigned long CddaTotalBytes(short) { return 0; }
long CddaFreeBytes(short) { return 0; }
int CDDriverIOScsiReadDiskinfo(EDevice_Id, unsigned char *buf) { std::memset(buf, 0, 4); return 0; }
unsigned long long DDriverIOReadCapacityStub(EDevice_Id, unsigned short &sectorSize) { sectorSize = 0; return 0; }
int DDriverIOScsiModeSelStub(EDevice_Id, unsigned short) { return 0; }
struct DevInfoShim { unsigned char pad[2]; };
DevInfoShim s_devInfo;
DevInfoShim *DDriverIOGetDevInfoStub(EDevice_Id) { return &s_devInfo; }
int CDDriverIOScsiModeSense10(EDevice_Id, unsigned char *, unsigned long, unsigned long) { return 0; }
int CDDriverIOScsiModeSel10(EDevice_Id, unsigned char *, unsigned long) { return 0; }
int CDDriverIOScsiCloseTrk(EDevice_Id, int, short, unsigned short, unsigned char *) { return 0; }

/* Real ground truth: cdda_drv2drvno(char const*) (out of scope), CDeviceMgr::
 * is_writable(EDevice_Id) (.text+0x8315860, out of scope) -- fopen()'s
 * write-permission gate for modes that require it.
 */
short CddaDrv2DrvnoStub(const char *) { return 0; }
int CDeviceMgrIsWritableStub(EDevice_Id) { return 1; }

/* Real ground truth: CMediaInfo::init/CFilesys::get_fileioptr -- out of
 * scope, same inert stand-ins as the other sibling classes' own.
 */
void CMediaInfoInitStub(CMediaInfo *, const char *, EFileIOType, int, long long, unsigned int) {}
CFileIoBase *FilesysGetFileIoPtrStub(EFileIOType)
{
	static CFileIoBase s_stubDriver;
	return &s_stubDriver;
}

/* Real ground truth: CFilePath::operator+=(char const*) -- same stand-in
 * convention as file_io_iso9660.cpp's own CFilePathAppendStub.
 */
void CFilePathAppendStub(char *path, const char *suffix) { std::strcat(path, suffix); }

/* Real ground truth: cdda_memory_init/cdda_gdone/cdda_dskclose/cdda_open/
 * cdda_close/cdda_read/cdda_write/cdda_lseek/cdda_dskopen/cdda_isemphasized/
 * cdda_getidxlen/cdda_gettrklen/cdda_getmaxidxno/cdda_getmaxtrkno/
 * cdda_writesetup/cdda_stopscan/cdda_rewscan/cdda_ffscan/cdda_resume/
 * cdda_pause/cdda_stop/cdda_play/cdda_fileno2file -- the embedded CD-DA
 * library, out of scope. Inert stand-ins.
 */
void CddaMemoryInitStub() {}
void CddaGdone(CddaDstat *) {}
void CddaDskclose(char *) {}
int CddaOpen(char *, unsigned short, unsigned short) { return -1; }
int CddaClose(short) { return 0; }
int CddaRead(short, unsigned char *, unsigned long) { return -1; }
int CddaWrite(short, const unsigned char *, unsigned long) { return -1; }
int CddaLseek(short, long, short) { return 0; }
int CddaDskopen(char *, EMountIoType, int *) { return 0; }
int CddaIsEmphasized(short, int *) { return 0; }
int CddaGetIdxLen(short, unsigned char, unsigned char, unsigned char, unsigned long *) { return 0; }
int CddaGetTrkLen(short, unsigned char, unsigned long *, int) { return 0; }
int CddaGetMaxIdxNo(short, unsigned char, unsigned char *) { return 0; }
int CddaGetMaxTrkNo(short, unsigned char *) { return 0; }
int CddaWriteSetup(short, int) { return 0; }
int CddaStopscan(short) { return 0; }
int CddaRewscan(short, short, short, unsigned long, unsigned long) { return 0; }
int CddaFfscan(short, short, short, unsigned long, unsigned long) { return 0; }
int CddaResume(short) { return 0; }
int CddaPause(short) { return 0; }
int CddaStop(short) { return 0; }
int CddaPlay(short, short, short, unsigned long, unsigned long) { return 0; }
unsigned char s_cddaFile[0x20];
unsigned char *CddaFileno2File(short) { return s_cddaFile; }

/* Real "last known play position" cache globals (.bss+0x93b0fb0/0x91b94a0/
 * 0x91b94b0/0x91b94c0/0x93b0fc0) -- written by play(), read by the
 * deferred getcurpos(). Semantic names beyond "position cache" not
 * recovered; kept here only so play()'s own writes are faithfully modeled.
 */
unsigned long g_cddaPosCache;
unsigned char g_cddaLastA;
unsigned char g_cddaLastA2;
unsigned char g_cddaLastB;
unsigned long g_cddaPlayEnd;

} // namespace

CFileIoCdda::CFileIoCdda()
	: m_pStat(reinterpret_cast<CddaDstat *>(cfiocdda_dir_obj)), m_reserved0xfc(0), m_searchActive(0)
{
	std::memset(m_path, 0, sizeof(m_path));
	/* Real: cdda_memory_init()'s return value is discarded, no failure
	 * check at all -- see header comment.
	 */
	CddaMemoryInitStub();
}

CFileIoCdda::~CFileIoCdda()
{
}

int CFileIoCdda::get_iotype() { return 2; }
int CFileIoCdda::fflush(int) { return -1; }
int CFileIoCdda::resize(int, unsigned int) { return -1; }
int CFileIoCdda::chdir(const char *) { return 0; }
int CFileIoCdda::dir(const char *, int, unsigned long &, CFileDirEntry *) { return 0; }
int CFileIoCdda::rename(const char *, const char *) { return -1; }
int CFileIoCdda::remove(const char *) { return -1; }
int CFileIoCdda::mkdir(const char *) { return -1; }
int CFileIoCdda::rmdir(const char *) { return -1; }

unsigned long CFileIoCdda::totalfreeclus(EDevice_Id device)
{
	return (unsigned long)(CddaFreeBytes((short)(int)device) / 2352);
}

unsigned long long CFileIoCdda::freebytes(EDevice_Id device)
{
	return (unsigned long long)(unsigned long)CddaFreeBytes((short)(int)device);
}

int CFileIoCdda::getmediainfo(EDevice_Id device, CMediaInfo *info)
{
	short dev = (short)(int)device;
	unsigned long totalBytes = CddaTotalBytes(dev);
	unsigned char *drv = CddaDrvno2Drv(dev);

	int flag;
	bool typeKnown = false;
	if (drv) {
		unsigned char type = drv[4];
		typeKnown = (type == 1 || type == 4 || type == 6);
	}

	if (typeKnown) {
		flag = 1;
	} else {
		unsigned char capstat = s_cdcapstatTab[(int)device % 10];
		bool bit4Clear = (capstat & 0x10) == 0;
		bool bit5Clear = (capstat & 0x20) == 0;

		if (bit4Clear && bit5Clear) {
			flag = 1;
		} else {
			/* Real: probes CDDriverIO::scsi_read_diskinfo(); on failure sets
			 * theFilesys->lastError=3 and returns -1 immediately. On success,
			 * flag = (probe[2] & 0x10) ? presetFlag : 0, where presetFlag is 1
			 * if bit4 was clear (only reachable here when bit5 was set), else 0.
			 */
			unsigned char probe[8];
			if (!CDDriverIOScsiReadDiskinfo(device, probe)) {
				g_theFilesys->lastError = 3;
				return -1;
			}
			int presetFlag = bit4Clear ? 1 : 0;
			flag = (probe[2] & 0x10) ? presetFlag : 0;
		}
	}

	drv = CddaDrvno2Drv(dev);
	const char *name = (drv && drv[0x664]) ? reinterpret_cast<const char *>(drv + 0x664) : "No Label"; /* .rodata+0x8eedd00 */

	CMediaInfoInitStub(info, name, (EFileIOType)2, flag, (long long)totalBytes, 2352);
	return 0;
}

int CFileIoCdda::getwd(EDevice_Id, char *buf)
{
	char path[16];
	path[0] = 0;
	std::strcat(path, "\\"); /* .rodata+0x8eee3c4 */
	std::strncpy(buf, path, 0xf0);
	buf[0xf0] = 0;
	return reinterpret_cast<int>(buf);
}

int CFileIoCdda::format(EDevice_Id device, int arg2)
{
	return FilesysGetFileIoPtrStub((EFileIOType)3)->format(device, arg2);
}

long CFileIoCdda::ftell(int handle)
{
	unsigned char *file = CddaFileno2File((short)handle);
	return *reinterpret_cast<long *>(file + 0x10);
}

int CFileIoCdda::funmount(EDevice_Id device)
{
	if (m_searchActive) {
		m_searchActive = 0;
		CddaGdone(m_pStat);
	}
	char path[16];
	path[0] = 0;
	CFilePathAppendStub(path, DEVICE_ID_STR[(int)device]); /* .rodata+0x8eef280, same content as the shared table */
	CddaDskclose(path);
	return 0;
}

int CFileIoCdda::getemphasized(int device, int *out)
{
	if (!CddaIsEmphasized((short)device, out)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoCdda::getidxlen(EDevice_Id device, unsigned char track, unsigned char idxA, unsigned char idxB, unsigned long *out)
{
	if (!CddaGetIdxLen((short)(int)device, track, idxA, idxB, out)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoCdda::gettrklen(EDevice_Id device, unsigned char track, unsigned long *out)
{
	if (!CddaGetTrkLen((short)(int)device, track, out, 0)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoCdda::getmaxidx(EDevice_Id device, unsigned char track, unsigned char *out)
{
	if (!CddaGetMaxIdxNo((short)(int)device, track, out)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoCdda::getmaxtrk(EDevice_Id device, unsigned char *out)
{
	if (!CddaGetMaxTrkNo((short)(int)device, out)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoCdda::writesetup(EDevice_Id device, int arg2)
{
	if (!CddaWriteSetup((short)(int)device, arg2)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoCdda::stopscan(EDevice_Id device)
{
	if (!CddaStopscan((short)(int)device)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoCdda::rewscan(EDevice_Id device, unsigned char a, unsigned char b, unsigned long c, unsigned long d)
{
	if (!CddaRewscan((short)(int)device, a, b, c, d)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoCdda::ffscan(EDevice_Id device, unsigned char a, unsigned char b, unsigned long c, unsigned long d)
{
	if (!CddaFfscan((short)(int)device, a, b, c, d)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoCdda::resume(EDevice_Id device)
{
	if (!CddaResume((short)(int)device)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoCdda::pause(EDevice_Id device)
{
	if (!CddaPause((short)(int)device)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoCdda::stop(EDevice_Id device)
{
	if (!CddaStop((short)(int)device)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoCdda::play(EDevice_Id device, unsigned char a, unsigned char b, unsigned long c, unsigned long d)
{
	g_cddaPosCache = 0;
	g_cddaLastB = b;
	g_cddaLastA = a;
	g_cddaLastA2 = a;
	g_cddaPlayEnd = c + d - 1;

	if (!CddaPlay((short)(int)device, a, b, c, d)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoCdda::fseek(int handle, long offset, int whence)
{
	if (CddaLseek((short)handle, offset, (short)whence) < 0) {
		set_error();
		return -1;
	}
	return 0;
}

unsigned int CFileIoCdda::fwrite(const void *buf, unsigned int size, unsigned int count, int handle)
{
	unsigned int n = size * count;
	int written = CddaWrite((short)handle, reinterpret_cast<const unsigned char *>(buf), n);
	if (written == -1) {
		if (g_theFilesys->lastError != 2)
			set_error();
		return 0;
	}
	return (unsigned int)written / size;
}

unsigned int CFileIoCdda::fread(void *buf, unsigned int size, unsigned int count, int handle)
{
	unsigned int n = size * count;
	int got = CddaRead((short)handle, reinterpret_cast<unsigned char *>(buf), n);
	if (got == -1) {
		set_error();
		return 0;
	}
	return (unsigned int)got / size;
}

int CFileIoCdda::fclose(int handle)
{
	if (CddaClose((short)handle) != 0) {
		set_error();
		return -1;
	}
	return 0;
}

void CFileIoCdda::ConvertPathRtfsToCdda(const char *path)
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

	/* Real: same medium-confidence tail-strcat() disclosure as
	 * CFileIoIso9660::ConvertPathRtfsToCdfs()'s own -- see that method's
	 * comment.
	 */
	std::strcat(m_path, ".");
	std::strcat(m_path, dot + 1);
}

void CFileIoCdda::set_error()
{
	if (g_theFilesys->lastError != 0)
		return;

	int raw = cdda_errno;
	int mapped = kLog;
	if ((unsigned)raw <= 22)
		mapped = kCddaErrTable[raw];

	if (mapped == kNoop)
		return;
	if (mapped != kLog) {
		g_theFilesys->lastError = mapped;
		return;
	}
	FileIoDriverApiAssert("DiskUtil/CustomFs/FileIoCdda.cpp", 0x585);
}

int CFileIoCdda::fopen(const char *path, const char *mode)
{
	ConvertPathRtfsToCdda(path);
	if (m_searchActive) {
		m_searchActive = 0;
		CddaGdone(m_pStat);
	}

	/* Real: 23-entry mode-char jump table (mode[0]-'a', 0..0x16), 5 real
	 * letter-specific slots + a shared default -- see header comment.
	 */
	unsigned short openMode;
	unsigned short openFlags;
	bool checkWritable;

	unsigned char idx = (unsigned char)(mode[0] - 'a');
	switch (idx > 0x16 ? 0xff : idx) {
	case 0: { /* 'a' -- append */
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
	case 7: /* 'h' */
		openFlags = 0x200;
		openMode = 0x302;
		checkWritable = false;
		break;
	case 17: { /* 'r' -- read */
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
	case 21: /* 'v' */
		openFlags = 0x40;
		openMode = 0x302;
		checkWritable = false;
		break;
	case 22: { /* 'w' -- write */
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
		short drvno = CddaDrv2DrvnoStub(m_path);
		if (!CDeviceMgrIsWritableStub((EDevice_Id)drvno)) {
			g_theFilesys->lastError = 9;
			return -1;
		}
	}

	int rc = CddaOpen(m_path, openMode, openFlags);
	if (rc < 0) {
		set_error();
		return rc;
	}
	return rc;
}

int CFileIoCdda::fmount(EDevice_Id device, EMountIoType ioType, int *arg3)
{
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
		/* Real: does NOT bail on retry failure -- always continues to the
		 * dskopen step regardless of the second probe's outcome (a genuine
		 * difference from CFileIoIso9660::fmount()'s own retry shape).
		 */
	}

	char path[16];
	path[0] = 0;
	CFilePathAppendStub(path, DEVICE_ID_STR[(int)device]);
	if (!CddaDskopen(path, ioType, arg3)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoCdda::settestmode(EDevice_Id device, int /*mode*/)
{
	if (!CddaWriteSetup((short)(int)device, 0)) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoCdda::finalize(EDevice_Id device)
{
	short dev = (short)(int)device;

	/* Real: cdda_writesetup() FAILURE (0) is the early-out here -- the
	 * opposite sense from settestmode()'s/writesetup()'s own callers; a
	 * SUCCESSFUL writesetup() means real finalize work still needs to
	 * happen below.
	 */
	if (!CddaWriteSetup(dev, 0)) {
		set_error();
		return -1;
	}

	unsigned char diskinfo[8];
	if (!CDDriverIOScsiReadDiskinfo(device, diskinfo)) {
		g_theFilesys->lastError = 3;
		return -1;
	}

	/* Real: low nibble of diskinfo[2] == 1 -> bail with -1, no error field
	 * write, no further calls (a genuine, silent early-out).
	 */
	if ((diskinfo[2] & 0xf) == 1)
		return -1;

	if (!CDDriverIOScsiModeSense10(device, diskinfo, 5, 0x40)) {
		g_theFilesys->lastError = 3;
		return -1;
	}

	unsigned long selVal = (unsigned long)diskinfo[1] + 2;
	if (!CDDriverIOScsiModeSel10(device, diskinfo, selVal)) {
		g_theFilesys->lastError = 4;
		return -1;
	}

	if (!CDDriverIOScsiCloseTrk(device, 1, 0, 0, diskinfo)) {
		g_theFilesys->lastError = 4;
		return -1;
	}
	return 0;
}
