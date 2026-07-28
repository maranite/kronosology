/*
 * file_io_akai.cpp  -  see include/file_io_akai.h.
 *
 * Transcribed from `objdump -dr -M intel` (Decomp/EVA_Decomp/Eva,
 * .text+0x08317c70..0x083183c0 / 0x08995430..0x08995440). See the header comment
 * for the per-method breakdown and file_io_driver_common.h for the shared
 * set_error()/Api-assert plumbing.
 *
 * Storage for the shared globals declared in file_io_driver_common.h (except
 * `Api`, whose real storage already lives in mains.cpp -- see that global's own
 * comment there and file_io_base.cpp's own precedent of never redefining it) is
 * defined here, arbitrarily -- this is the first/smallest of the three driver
 * TUs.
 */

#include "file_io_akai.h"
#include "file_io_driver_common.h"

#include <cstring>

/* --- shared-global storage (file_io_driver_common.h) --------------------- */

CFilesysErrShim s_theFilesysStorage;
CFilesysErrShim *g_theFilesys = &s_theFilesysStorage;

int g_rawFsErrorStorage;
int *fs_user = &g_rawFsErrorStorage;

/* Real ground truth: DEVICE_ID_STR (.data+0x91b9900, 10 `char*` entries) --
 * shared identically by CFileIoAkai::getwd() and CFileIoDos::getwd()/
 * getmediainfo() (confirmed: all read the exact same address, indexed by
 * `device`). Real per-device path strings not transcribed (not needed --
 * DDriverIOReadCapacityStub/AkiutilPwd etc. are themselves inert stand-ins that
 * never dereference their string arguments); modeled as a fixed-size array of
 * empty strings.
 */
char *DEVICE_ID_STR[10] = {
	(char *)"", (char *)"", (char *)"", (char *)"", (char *)"",
	(char *)"", (char *)"", (char *)"", (char *)"", (char *)"",
};

/* Real ground truth: cfioakai_dir_obj (.bss+0x93b0ee0, 0xc4=196 bytes) -- the
 * single shared Akai-format-library search/IO state object every CFileIoAkai
 * instance's m_pStat points at (real symbol name via `nm -C -S`). AkiAstat
 * itself is opaque (file_io_akai.h) -- storage modeled as a plain byte buffer of
 * the real size, never dereferenced by any of the reconstructed methods below.
 */
unsigned char cfioakai_dir_obj[0xc4];

/* --- opaque forward decls used only by dir()'s date-decode tail ---------- */

class CDate;
class COTime;

namespace {

/* Real ground truth: CDDriverIO::read_capacity(EDevice_Id, unsigned short&)
 * (.text+0x0830e470), CDDriverIO::getdevinfo(EDevice_Id) (.text+0x0830db00),
 * CDDriverIO::scsi_mode_sel(EDevice_Id, unsigned short) (.text+0x0830e710) --
 * CDDriverIO (85 methods) is out of scope (file_io_base.h). Modeled as inert
 * stand-ins, same convention as file_io_unknown.cpp's own copy.
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

/* Real ground truth: CMediaInfo::init(...) (.text+0x08317be0) -- out of scope,
 * same inert stand-in as file_io_unknown.cpp.
 */
void CMediaInfoInitStub(CMediaInfo *, const char *, EFileIOType, int, long long, unsigned int)
{
}

/* Real ground truth: CFilesys::get_fileioptr(EFileIOType) (.text+0x083217e0) --
 * out of scope, same inert stand-in as file_io_unknown.cpp.
 */
CFileIoBase *FilesysGetFileIoPtrStub(EFileIOType)
{
	static CFileIoBase s_stubDriver;
	return &s_stubDriver;
}

/* Real ground truth: akiutil_getvolumename(short) (.text+0x8364770) -- the
 * embedded Akai-format library (out of scope). Inert stand-in, fixed name.
 */
const char *AkiutilGetVolumeNameStub(short)
{
	return "";
}

/* Real ground truth: aki_gfirst(astat*, char*) / aki_gnext(astat*) /
 * aki_gdone(astat*) (.text+0x8363670/0x83637e0/0x83638c0) -- embedded Akai
 * library directory-search primitives, out of scope. Inert stand-ins: no
 * entries found by default (deterministic, matches CFileIoUnknown's own
 * "no dependency populates real state" convention).
 */
int AkiGfirst(AkiAstat *, char *)
{
	return 0;
}
int AkiGnext(AkiAstat *)
{
	return 0;
}
void AkiGdone(AkiAstat *)
{
}

/* Real ground truth: whether the shared search state's own drive index
 * matches `device` (funmount()'s own 2-levels-of-indirection read off
 * m_pStat+0xbc, opaque -- astat's real layout is not recovered). Modeled as
 * always false.
 */
bool AkiAstatDeviceMatches(AkiAstat *, EDevice_Id)
{
	return false;
}

/* Real ground truth: aki_open/aki_read/aki_close/aki_lseek/akiext_ftell
 * (.text+0x8362da0/0x8362f30/0x8363610/0x8363510/0x83638f0) -- out of scope.
 * Inert stand-ins.
 */
int AkiOpen(char *, unsigned char)
{
	return -1;
}
int AkiRead(int, unsigned char *, unsigned long)
{
	return -1;
}
int AkiClose(int)
{
	return 0;
}
int AkiLseek(int, long, short)
{
	return 0;
}
long AkiextFtell(int)
{
	return 0;
}

/* Real ground truth: akiutil_pwd/akiutil_set_cwd/akiutil_dskinit/
 * alowl_dskfree (.text+0x8364da0/0x8364c00/0x8364040/0x8365da0) -- out of
 * scope. Inert stand-ins. All four use the "0 == failure, nonzero ==
 * success" convention (matches ground truth's own `test eax,eax; je fail`
 * at each call site) -- default return values below are chosen per-function
 * to exercise a mix of the success/failure paths across this file's tests.
 */
int AkiutilPwd(char *, char *)
{
	return 1;
}
int AkiutilSetCwd(char *)
{
	return 0;
}
int AkiutilDskinit(short, int /*DENSITY_AKAI*/, unsigned short, unsigned long)
{
	return 1;
}
int AlowlDskfree(short, int)
{
	return 0;
}

/* Real ground truth: pc_memory_init()/ak_memory_init() (.text+0x8381e50/
 * 0x8364e80) -- out of scope library-init hooks called from the ctor. Inert
 * stand-ins.
 */
void PcMemoryInitStub()
{
}
void AkMemoryInitStub()
{
}

/* Real ground truth: CDirentry::operator=(_Dos_Direntry const&)
 * (.text+0x8320db0), CDateT::get(CDateT::DateVal) const (.text+0x8320a60),
 * CFileDirEntry::Initialize(...) (.text+0x82d8a00) -- all out of scope
 * (CDirentry/CDateT/CFileDirEntry not modeled beyond file_io_base.h's own
 * opaque CFileDirEntry forward decl). Inert stand-ins: CDirentryAssign()
 * leaves the destination untouched (matches CMediaInfo::init's own "does
 * nothing to the passed-in struct" convention), CDateTGet() returns 0.
 */
void CDirentryAssign(unsigned char *, AkiAstat *)
{
}
int CDateTGet(void *, int)
{
	return 0;
}
void CFileDirEntryInitialize(CFileDirEntry *, const char *, char, CDate *, COTime *, unsigned int, int)
{
}

/* Real ground truth: fopen()'s mode-char translation table
 * (.rodata+0x8eede60, 33 bytes, index = mode[0]-'P', range 0..0x20). Real
 * per-char semantics beyond 'd'->'d'/'o'->'c'/'p'->'p' (identity/near-identity)
 * are not recovered -- transcribed exactly via `objdump -s`.
 */
const unsigned char kAkaiFopenModeTable[0x21] = {
	0xf0, 0x73, 0x73, 0xf3, 0x73, 0x73, 0x73, 0x73, /* 'P'..'W' */
	0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, /* 'X'..'_' */
	0x73, 0x73, 0x73, 0x73, 0x64, 0x73, 0x73, 0x73, /* '`'..'g' */
	0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x73, 0x63, /* 'h'..'o' */
	0x70,                                           /* 'p' */
};

/* Real ground truth: set_error()'s 44-entry translation table
 * (.rodata+0x8eedda0..0x8eede4c), decoded via `objdump -s` against the 9
 * distinct case bodies at .text+0x8318090..0x8318110 (each a
 * `theFilesys->lastError = N; return;`) plus the shared "log and return"
 * fallthrough at .text+0x831804b. kNoop = raw code silently ignored (no
 * field write, no log); kLog = raw code falls through to the Api-assert log.
 */
enum { kNoop = -1, kLog = -2 };
const int kAkaiErrTable[44] = {
	kNoop, 1, 6, 7, 2, 4, 4, 3, 3, 3, 3, 3, 3, 5, 5, 5, 5, 5, 8, kNoop, kNoop, kNoop,
	kLog, kLog, kLog, kLog, kLog, kLog, kLog, kLog, kLog, 5, kLog, kLog, kLog, kLog,
	kLog, kLog, kLog, kLog, kLog, kLog, kLog, 0xb,
};

} // namespace

CFileIoAkai::CFileIoAkai()
	: m_pStat(reinterpret_cast<AkiAstat *>(cfioakai_dir_obj)), m_reserved0xfc(0), m_searchActive(0)
{
	std::memset(m_path, 0, sizeof(m_path));
	PcMemoryInitStub();
	AkMemoryInitStub();
}

CFileIoAkai::~CFileIoAkai()
{
	/* Real body only resets the vtable pointer (D0 additionally wraps
	 * `free(this)` in HAL_DisableInterrupts()/HAL_EnableInterrupts()) --
	 * see header comment.
	 */
}

int CFileIoAkai::get_iotype()
{
	return 5;
}

unsigned long long CFileIoAkai::freebytes(EDevice_Id)
{
	return 0;
}

int CFileIoAkai::getmediainfo(EDevice_Id device, CMediaInfo *info)
{
	unsigned short sectorSize;
	unsigned long long sectorCount = DDriverIOReadCapacityStub(device, sectorSize);
	const char *volName = AkiutilGetVolumeNameStub((short)(int)device);

	CMediaInfoInitStub(info, volName, (EFileIOType)5, 1, (long long)sectorCount, 0x400);
	return 0;
}

int CFileIoAkai::format(EDevice_Id device, int arg2)
{
	return FilesysGetFileIoPtrStub((EFileIOType)6)->format(device, arg2, (EFatType)0);
}

long CFileIoAkai::ftell(int handle)
{
	return AkiextFtell(handle);
}

void CFileIoAkai::ConvertPath(const char *path)
{
	std::strncpy(m_path, path, 0xf0);
	m_path[0xf0] = 0;
}

void CFileIoAkai::set_error()
{
	if (g_theFilesys->lastError != 0)
		return;

	int raw = *fs_user;
	int mapped = kLog;
	if ((unsigned)raw <= 0x2b)
		mapped = kAkaiErrTable[raw];

	if (mapped == kNoop)
		return;
	if (mapped != kLog) {
		g_theFilesys->lastError = mapped;
		return;
	}
	FileIoDriverApiAssert("DiskUtil/CustomFs/FileIoAkai.cpp", 0x2b1);
}

int CFileIoAkai::getwd(EDevice_Id device, char *buf)
{
	if (AkiutilPwd(DEVICE_ID_STR[(int)device], buf) == 0) {
		set_error();
		return 0;
	}
	return reinterpret_cast<int>(buf);
}

int CFileIoAkai::chdir(const char *path)
{
	std::strncpy(m_path, path, 0xf0);
	m_path[0xf0] = 0;
	if (AkiutilSetCwd(m_path) == 0) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoAkai::fseek(int handle, long offset, int whence)
{
	if (AkiLseek(handle, offset, (short)whence) < 0) {
		set_error();
		return -1;
	}
	return 0;
}

unsigned int CFileIoAkai::fread(void *buf, unsigned int size, unsigned int count, int handle)
{
	int n = AkiRead(handle, reinterpret_cast<unsigned char *>(buf), size * count);
	if (n == -1) {
		set_error();
		return 0;
	}
	return (unsigned int)n / size;
}

int CFileIoAkai::fclose(int handle)
{
	if (AkiClose(handle) != 0) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoAkai::fopen(const char *path, const char *mode)
{
	std::strncpy(m_path, path, 0xf0);
	if (m_searchActive) {
		m_searchActive = 0;
		AkiGdone(m_pStat);
	}
	m_path[0xf0] = 0;

	unsigned char c = (unsigned char)mode[0];
	unsigned char modeByte = 0x73; /* default 's' */
	unsigned dv = (unsigned)((int)c - 'P');
	if (dv <= 0x20)
		modeByte = kAkaiFopenModeTable[dv];

	int rc = AkiOpen(m_path, modeByte);
	if (rc < 0) {
		set_error();
		return rc;
	}
	return rc;
}

int CFileIoAkai::funmount(EDevice_Id device)
{
	if (m_searchActive && AkiAstatDeviceMatches(m_pStat, device)) {
		m_searchActive = 0;
		AkiGdone(m_pStat);
	}
	if (AlowlDskfree((short)(int)device, 1) == 0) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoAkai::fmount(EDevice_Id device)
{
	DevInfoShim *info = DDriverIOGetDevInfoStub(device);
	if (info && info->pad[1] == 7) {
		DDriverIOScsiModeSelStub(device, 0x800);
		/* falls through to the read_capacity probe below (real:
		 * `jmp 0x83183e0`).
		 */
	}

	unsigned short sectorSize = 0;
	unsigned long long capacity = DDriverIOReadCapacityStub(device, sectorSize);
	long high = (long)(capacity >> 32);
	unsigned long low = (unsigned long)capacity;

	int result;
	if (high < 0 || (high == 0 && (long)low <= 0)) {
		result = -1;
	} else {
		result = 0;
		if (sectorSize != 0x800)
			sectorSize = 0x800;
	}

	if (result != 0)
		return result;

	if (AkiutilDskinit((short)(int)device, 2 /* DENSITY_AKAI */, sectorSize, low) != 0)
		return result; /* 0 */

	set_error();
	return -1;
}

int CFileIoAkai::dir(const char *path, int /*arg2*/, unsigned long &cont, CFileDirEntry *entry)
{
	bool haveEntry = false;

	if (cont != 0) {
		haveEntry = AkiGnext(m_pStat) != 0;
	} else {
		if (m_searchActive) {
			/* Real: aki_gdone()s the previous search, then re-checks
			 * *arg3ptr -- provably still 0 here (nothing between the
			 * two reads can change it), so the real "continue instead
			 * of restart" branch this guards is unreachable; collapsed
			 * to the always-taken restart path below.
			 */
			AkiGdone(m_pStat);
		}
		ConvertPath(path);
		if (AkiGfirst(m_pStat, m_path)) {
			m_searchActive = 1;
			cont = 1;
			haveEntry = true;
		}
	}

	if (!haveEntry) {
		m_searchActive = 0;
		cont = 0;
		AkiGdone(m_pStat);
		return 0;
	}

	/* Real: reinterprets the shared search-state record as a
	 * `_Dos_Direntry` and copies it via `CDirentry::operator=()` (opaque,
	 * out of scope). Zero-initialized here so the downstream date/attr
	 * reads below are deterministic.
	 */
	unsigned char rawDirentry[0x150];
	std::memset(rawDirentry, 0, sizeof(rawDirentry));
	CDirentryAssign(rawDirentry, m_pStat);

	/* Real: CDateT::get(kind) called 6 times (kinds 0,2,1,6,5,4) against a
	 * CDateT sub-object embedded at rawDirentry+0xf4 (opaque), then packs
	 * the results into a 4-byte CDate-shaped buffer {year,month,weekday,
	 * day} and a 3-byte COTime-shaped buffer {h,m,s} via a real inline
	 * divide-by-100-via-multiply idiom (0x147b >> 0x11 ~= /100). Byte
	 * offsets/arithmetic transcribed exactly from
	 * .text+0x8317e45..0x8317f52.
	 */
	void *dateObj = rawDirentry + 0xf4;
	int rawA = CDateTGet(dateObj, 0);
	int rawB = CDateTGet(dateObj, 2);
	unsigned char rawWeekday = (unsigned char)CDateTGet(dateObj, 1);
	unsigned short shiftedA = (unsigned short)(rawA >> 2);
	unsigned char fieldA = (unsigned char)(((unsigned)shiftedA * 0x147bu) >> 0x11);
	unsigned char fieldB = (unsigned char)((unsigned short)rawA - (unsigned short)(fieldA * 100));
	unsigned char fieldC = (unsigned char)rawB;

	int rawC = CDateTGet(dateObj, 6);
	int rawD = CDateTGet(dateObj, 5);
	unsigned char rawE = (unsigned char)CDateTGet(dateObj, 4);

	unsigned char dateBuf[4];
	dateBuf[0] = fieldA;
	dateBuf[1] = fieldB;
	dateBuf[2] = rawWeekday;
	dateBuf[3] = fieldC;

	unsigned char timeBuf[3];
	timeBuf[0] = rawE;
	timeBuf[1] = (unsigned char)rawD;
	timeBuf[2] = (unsigned char)rawC;

	unsigned char attrRaw = rawDirentry[0xf1];
	char attrChar = (char)(attrRaw | 0x40);
	unsigned int extra = *reinterpret_cast<unsigned int *>(rawDirentry + 0xf8);

	CFileDirEntryInitialize(entry, reinterpret_cast<const char *>(rawDirentry),
	                         attrChar, reinterpret_cast<CDate *>(dateBuf),
	                         reinterpret_cast<COTime *>(timeBuf), extra, 0);
	return reinterpret_cast<int>(entry);
}
