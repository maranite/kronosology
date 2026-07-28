/*
 * file_io_dos.cpp  -  see include/file_io_dos.h.
 *
 * Transcribed from `objdump -dr -M intel` (Decomp/EVA_Decomp/Eva,
 * .text+0x0831a520..0x0831c020 minus format(), .text+0x08995570..0x08995580).
 * See the header comment for the per-method breakdown, file_io_driver_common.h
 * for the shared set_error()/Api-assert plumbing, and DECOMPILE_ERRORS.md for
 * why format(EDevice_Id, int, EFatType) is deferred.
 *
 * optimizemedium()/scandisk()/getmediainfo()/fmount() are transcribed at a
 * faithful STRUCTURAL level (real call sequence, real success/failure shape,
 * real shared globals) but simplify a handful of the deepest micro-branches
 * (specific numeric sentinel thresholds read off opaque `ddrive`/`fsinfo`
 * fields whose real layout is not recovered) -- each simplification is called
 * out at its own site below.
 */

#include "file_io_dos.h"
#include "file_io_driver_common.h"

#include <cstring>

/* Real ground truth: cfiodos_dir_obj (.bss+0x93b1020, 0xc4=196 bytes) -- the
 * single shared DOS/FAT-library search/IO state object every CFileIoDos
 * instance's m_pStat points at (real symbol name via `nm -C -S`). DosDstat
 * itself is opaque (file_io_dos.h) except where dir() reads two known byte
 * offsets directly (same "opaque past what's needed" convention as
 * scsi_driver_base.h's SDriverIOPbuf).
 */
unsigned char cfiodos_dir_obj[0xc4];

/* Real ground truth: CFileIoDos::iStage (.bss+0x93b0fe0) -- the static state
 * variable driving the deferred format()'s resumable multi-call state machine
 * (see DECOMPILE_ERRORS.md). Declared here (real symbol name, real address)
 * since it is this class's own static member even though format() itself
 * isn't implemented -- nothing in this TU currently reads/writes it.
 */
int CFileIoDos::iStage;

namespace {

/* Real ground truth: CDDriverIO::read_capacity/scsi_mode_sel (.text+0x0830e470
 * /0x0830e710), CMediaInfo::init (.text+0x08317be0) -- same inert stand-ins as
 * file_io_akai.cpp's own copies (CDDriverIO/CMediaInfo out of scope).
 */
unsigned long long DDriverIOReadCapacityStub(EDevice_Id, unsigned short &sectorSize)
{
	sectorSize = 0;
	return 0;
}
int DDriverIOScsiModeSelStub(EDevice_Id, unsigned short)
{
	return 0;
}
void CMediaInfoInitStub(CMediaInfo *, const char *, EFileIOType, int, long long, unsigned int)
{
}

/* Real ground truth: the per-device write-protect flags byte at
 * .bss+0x93b0d54 -- SAME address CFileIoUnknown::getmediainfo() reads
 * (file_io_unknown.cpp's own `s_devstatTab`). Duplicated locally per that
 * file's own precedent (nothing in this reconstruction's traced boot path
 * writes it either way, so per-TU duplication is harmless).
 */
unsigned char s_devstatTab[10];
int DosDevStatByte(EDevice_Id device)
{
	return s_devstatTab[(int)device];
}

/* Real ground truth: pc_drno2dr(short) (.text+0x837d360) -- returns an opaque
 * `ddrive*`. Modeled as a zeroed local struct; getmaxclusterno()/scandisk()
 * only ever read a couple of its fields at fixed offsets.
 */
unsigned char s_ddrive[0x700];
unsigned char *PcDrno2Dr(short)
{
	return s_ddrive;
}

/* Real ground truth: pc_cluster_size/pc_totalfreecluster/pc_ifree
 * (.text+0x83714b0/0x8381040/0x8381290) -- out of scope. Inert stand-ins.
 */
int PcClusterSize(short)
{
	return 0;
}
unsigned long PcTotalFreeCluster(short)
{
	return 0;
}
unsigned long PcIfree(short)
{
	return 0;
}
long PoFtell(int)
{
	return 0;
}

/* Real ground truth: pc_IsVolumeLabelSkipped/pc_gfirst/pc_gnext/pc_gdone
 * (.text+0x8381e40/0x8370960/0x8370b00/0x8370b50) -- embedded DOS/FAT library
 * directory-search primitives, out of scope. Inert stand-ins.
 */
int PcIsVolumeLabelSkipped()
{
	return 0;
}
int PcGfirst(DosDstat *, char *)
{
	return 0;
}
int PcGnext(DosDstat *)
{
	return 0;
}
void PcGdone(DosDstat *)
{
}
bool DosDstatDeviceMatches(DosDstat *, EDevice_Id)
{
	return false;
}

/* Real ground truth: pc_ShowVolumeLabel/pc_HideVolumeLabel (.text+0x8381e20/
 * 0x8381e30) -- out of scope. CFileDirEntry-vtable-based root-directory
 * iteration + CDirEntry::IsLabel()/CFileDirEntry::GetNameExt() (also out of
 * scope, no real vtable recovered) collapsed into one opaque
 * DosFindVolumeLabelStub() stand-in returning "no label" by default -- same
 * abstraction-level choice as file_io_unknown.cpp's own get_fileioptr() stub.
 */
int PcShowVolumeLabel()
{
	return 0;
}
void PcHideVolumeLabel()
{
}
const char *DosFindVolumeLabelStub(const char *)
{
	return "";
}

/* Real ground truth: pc_optimizemedium/pc_set_optimize_oemname
 * (.text+0x8375400/0x8381990) -- out of scope. Inert stand-ins.
 */
int PcOptimizemedium(short, unsigned long, unsigned long *, int)
{
	return -1;
}
int PcSetOptimizeOemname(short, int, unsigned long, unsigned long, int)
{
	return 0;
}
unsigned long s_numOptimizedCluster; /* CFileIoDos::optimizemedium(...)::NumOptimizedCluster, .bss+0x93b0ff0 */

/* Real ground truth: pc_scandisk/CDDriverIO::EnableProgress (.text+0x83750c0/
 * 0x83146f0) -- out of scope. Inert stand-ins.
 */
int PcScandisk(short, unsigned long, unsigned long, unsigned long *, unsigned long *, int)
{
	return -1;
}
void DDriverIOEnableProgressStub(int, int)
{
}
unsigned long s_ulTotalFreeClus; /* CFileIoDos::scandisk(...)::ulTotalFreeClus, .bss+0x93b1000 */

/* Real ground truth: po_dummy_write/po_resize/po_flush/po_lseek/po_write/
 * po_read/po_close/po_open (.text+0x8370e20/0x8372260/0x836edf0/0x836eb90/
 * 0x836e160/0x836dcf0/0x836ef20/0x836d790) -- out of scope. Inert stand-ins.
 */
int PoDummyWrite(int, unsigned long)
{
	return -1;
}
int PoResize(int, unsigned int)
{
	return 0;
}
int PoFlush(int)
{
	return 0;
}
int PoLseek(int, long, short)
{
	return 0;
}
int PoWrite(int, const unsigned char *, unsigned long)
{
	return -1;
}
int PoRead(int, unsigned char *, unsigned long)
{
	return -1;
}
int PoClose(int)
{
	return 0;
}
int PoOpen(char *, unsigned short, unsigned short)
{
	return -1;
}

/* Real ground truth: pc_rmdir/pc_mkdir/pc_unlink/pc_mv/pc_pwd/pc_set_cwd/
 * pc_dskfree/pc_dskinit (.text+0x836f480/0x836d560/0x836f2f0/0x836f0b0/
 * 0x8370dc0/0x8370c20/0x837d390/0x8373e30) -- out of scope. Inert stand-ins,
 * all using the real "0 == failure, nonzero == success" convention confirmed
 * at each call site.
 */
int PcRmdir(char *)
{
	return 1;
}
int PcMkdir(char *)
{
	return 1;
}
int PcUnlink(char *)
{
	return 1;
}
int PcMv(char *, char *)
{
	return 1;
}
int PcPwd(char *, char *)
{
	return 1;
}
int PcSetCwd(char *)
{
	return 1;
}
int PcDskfree(short, int)
{
	return 1;
}
int PcDskinit(short)
{
	return 1;
}

/* Real ground truth: po_makelbaarray(int, unsigned long*, unsigned long)
 * (.text+0x83712a0) -- out of scope. Inert stand-in.
 */
int PoMakeLbaArray(int, unsigned long *, unsigned long)
{
	return 0;
}

/* Real ground truth: pc_memory_init() (.text+0x8381e50) -- SAME symbol
 * CFileIoAkai's own ctor calls (confirmed identical address, a shared
 * library-init hook). Inert stand-in.
 */
void PcMemoryInitStub()
{
}

/* Real ground truth: CDateT::get(...)/CFileDirEntry::Initialize(...)
 * (.text+0x8320a60/0x82d8a00) -- out of scope, same inert stand-ins as
 * file_io_akai.cpp's own (CDateT returns 0, Initialize is a no-op).
 */
class CDate;
class COTime;
int CDateTGet(void *, int)
{
	return 0;
}
void CFileDirEntryInitialize(CFileDirEntry *, const char *, char, CDate *, COTime *, unsigned int, int)
{
}

/* Real ground truth: set_error()'s 44-entry translation table
 * (.rodata+0x8eedf3c..0x8eedfe8), decoded via `objdump -s`. SAME shape as
 * CFileIoAkai's own (file_io_akai.cpp) but with NO log fallback (unmapped
 * codes are `kNoop`, never `kLog`) and one differing mapped entry: raw code 24
 * maps to field 3 here, vs. Akai's own (silently ignored there) -- confirmed
 * independently via two separate `objdump -s` reads of the two distinct
 * .rodata table addresses.
 */
enum { kNoop = -1 };
const int kDosErrTable[44] = {
	kNoop, 1, 6, 7, 2, 4, 4, 3, 3, 3, 3, 3, 3, 5, 5, 5, 5, 5, 8, kNoop, kNoop, kNoop,
	kNoop, kNoop, 3, kNoop, kNoop, kNoop, kNoop, kNoop, kNoop, 5, kNoop, kNoop, kNoop,
	kNoop, kNoop, kNoop, kNoop, kNoop, kNoop, kNoop, kNoop, 0xb,
};

/* Real ground truth: fopen()'s mode-char dispatch (.rodata+0x8eedfec, 23
 * entries, index = mode[0]-'a', range 0..0x16) -- a real embedded switch, not
 * a simple byte table like CFileIoAkai's own. Only 'a'/'h'/'r'/'v'/'w' (indices
 * 0/7/17/21/22) have distinct bodies; everything else (including the
 * out-of-range fallback) shares the exact same default flags/mode pair. See
 * file_io_dos.h's own header comment.
 */

} // namespace

CFileIoDos::CFileIoDos()
	: m_pStat(reinterpret_cast<DosDstat *>(cfiodos_dir_obj)), m_reserved0xfc(0), m_searchActive(0)
{
	PcMemoryInitStub();
}

CFileIoDos::~CFileIoDos()
{
	/* Real body only resets the vtable pointer (D0 additionally wraps
	 * `free(this)` in HAL_DisableInterrupts()/HAL_EnableInterrupts()) --
	 * see header comment.
	 */
}

int CFileIoDos::get_iotype()
{
	return 6;
}

unsigned long CFileIoDos::getmaxclusterno(EDevice_Id device)
{
	unsigned char *dr = PcDrno2Dr((short)(int)device);
	return *reinterpret_cast<unsigned long *>(dr + 0x1c);
}

int CFileIoDos::getfilelbaarray(EDevice_Id device, int arg2, CFileLbaArray *outArr)
{
	unsigned char *out = reinterpret_cast<unsigned char *>(outArr);
	unsigned long *ptrField = *reinterpret_cast<unsigned long **>(out + 0);
	unsigned long countField = *reinterpret_cast<unsigned long *>(out + 4);

	if (PoMakeLbaArray(arg2, ptrField, countField) == 0)
		return -1;

	unsigned short sectorSize = 0;
	unsigned long long capacity = DDriverIOReadCapacityStub(device, sectorSize);
	if (capacity == 0)
		return -1;

	*reinterpret_cast<unsigned short *>(out + 0xc) = sectorSize;
	unsigned short clusterSize = (unsigned short)PcClusterSize((short)(int)device);
	unsigned short sectorsPerCluster = (unsigned short)(sectorSize ? clusterSize / sectorSize : 0);

	/* Real: `*(unsigned long*)(out+0x10) = device` (the caller-supplied
	 * device id itself, NOT the probed capacity -- confirmed by direct
	 * register tracing: `esi` still holds the original `device` argument
	 * at this point, never reassigned from read_capacity's own EDX:EAX
	 * return).
	 */
	*reinterpret_cast<unsigned long *>(out + 0x10) = (unsigned long)(int)device;
	*reinterpret_cast<unsigned short *>(out + 0xe) = sectorsPerCluster;
	return 0;
}

unsigned long CFileIoDos::totalfreeclus(EDevice_Id device)
{
	return PcTotalFreeCluster((short)(int)device);
}

unsigned long long CFileIoDos::freebytes(EDevice_Id device)
{
	return (unsigned long long)PcIfree((short)(int)device);
}

long CFileIoDos::ftell(int handle)
{
	return PoFtell(handle);
}

int CFileIoDos::getmediainfo(EDevice_Id device, CMediaInfo *info)
{
	unsigned short sectorSize = 0;
	unsigned long long capacity = DDriverIOReadCapacityStub(device, sectorSize);
	unsigned long capacityLow = (unsigned long)capacity;
	unsigned short clusterSize = (unsigned short)PcClusterSize((short)(int)device);
	int writeProtect = DosDevStatByte(device) & 1;

	char nameBuf[0x20];
	std::strcpy(nameBuf, DEVICE_ID_STR[(int)device]);
	std::strcat(nameBuf, "\\"); /* .rodata+0x8eee3c4, confirmed via objdump -s */

	const char *volLabel = "";
	if (PcShowVolumeLabel()) {
		volLabel = DosFindVolumeLabelStub(nameBuf);
		PcHideVolumeLabel();
	}

	CMediaInfoInitStub(info, volLabel, (EFileIOType)6, writeProtect,
	                   (long long)capacityLow, (unsigned int)clusterSize);
	return 0;
}

void CFileIoDos::set_error()
{
	if (g_theFilesys->lastError != 0)
		return;

	int raw = *fs_user;
	if ((unsigned)raw > 0x2b)
		return;

	int mapped = kDosErrTable[raw];
	if (mapped == kNoop)
		return;
	g_theFilesys->lastError = mapped;
}

int CFileIoDos::optimizemedium(EDevice_Id device, unsigned long a, unsigned long *b, int c)
{
	if (c != 0)
		s_numOptimizedCluster = 0;

	*fs_user = 0;
	if (PcOptimizemedium((short)(int)device, a, b, c) == 0) {
		/* Real "0 == success" convention (the opposite of most other
		 * pc_/aki_/po_ calls in this batch, confirmed via
		 * `test eax,eax; je <success path>`).
		 */
		unsigned long target = *b;
		if (target != 0) {
			unsigned long total = target + s_numOptimizedCluster;
			s_numOptimizedCluster = total;
			if (a <= total) {
				PcSetOptimizeOemname((short)(int)device, 1, 2, total, 1);
				return 0;
			}
		}
	}

	/* Real: only raw fs_user codes 0x12/4 are treated specially here
	 * (call set_error(), which per its own table maps 0x12 to field 8 and
	 * leaves 4 mapped to field 4); anything else returns -1 without
	 * logging. Transcribed at the outer-shape level -- the exact
	 * bit-level condition guarding the final return value after
	 * set_error() (.text+0x0831abfd..0x0831ac22) is a lower-confidence
	 * transcription, simplified to an unconditional -1 here.
	 */
	int raw = *fs_user;
	if (raw == 0x12 || raw == 4)
		set_error();
	return -1;
}

int CFileIoDos::scandisk(EDevice_Id device, unsigned long a, unsigned long b, unsigned long *c, unsigned long *d)
{
	short drShort = (short)(int)device;
	unsigned char *dr = PcDrno2Dr(drShort);
	DDriverIOEnableProgressStub(0, 0);

	if (PcScandisk(*reinterpret_cast<short *>(dr + 0x18), a, b, c, d, 0) == 0) {
		s_ulTotalFreeClus += *c;
		/* Real: additionally probes/updates a cached "hidden clusters"
		 * hint via pc_readfsinfo()/pc_gethiddenclusters()/
		 * pc_writefsinfo() depending on the accumulated total vs. the
		 * drive's own known capacity (ddrive+0x1c/+0x6a0/+0x6a8/+0x6b8)
		 * -- opaque `ddrive`/`fsinfo` internals not modeled in enough
		 * detail to reproduce that branch faithfully; simplified to
		 * always take the "no fsinfo update needed" path.
		 */
		DDriverIOEnableProgressStub(1, 0);
		return 0;
	}

	set_error();
	DDriverIOEnableProgressStub(1, 0);
	return 0;
}

unsigned int CFileIoDos::fdummywrite(unsigned int size, unsigned int count, int handle)
{
	int n = PoDummyWrite(handle, (unsigned long)size * count);
	if (n == -1) {
		set_error();
		if (g_theFilesys->lastError == 0)
			g_theFilesys->lastError = 2;
		return 0;
	}
	return (unsigned int)n / size;
}

int CFileIoDos::rmdir(const char *path)
{
	if (m_searchActive) {
		m_searchActive = 0;
		PcGdone(m_pStat);
	}
	if (PcRmdir(const_cast<char *>(path)) != 0)
		return 0;
	set_error();
	return -1;
}

int CFileIoDos::mkdir(const char *path)
{
	if (PcMkdir(const_cast<char *>(path)) != 0)
		return 0;
	set_error();
	return -1;
}

int CFileIoDos::remove(const char *path)
{
	if (m_searchActive) {
		m_searchActive = 0;
		PcGdone(m_pStat);
	}
	if (PcUnlink(const_cast<char *>(path)) != 0)
		return 0;
	set_error();
	return -1;
}

int CFileIoDos::rename(const char *oldPath, const char *newPath)
{
	if (PcMv(const_cast<char *>(oldPath), const_cast<char *>(newPath)) != 0)
		return 0;
	set_error();
	return -1;
}

int CFileIoDos::getwd(EDevice_Id device, char *buf)
{
	if (PcPwd(DEVICE_ID_STR[(int)device], buf) == 0) {
		set_error();
		return 0;
	}
	return reinterpret_cast<int>(buf);
}

int CFileIoDos::chdir(const char *path)
{
	if (PcSetCwd(const_cast<char *>(path)) != 0)
		return 0;
	set_error();
	return -1;
}

int CFileIoDos::resize(int handle, unsigned int newSize)
{
	if (PoResize(handle, newSize) != 0)
		return 0;
	set_error();
	return -1;
}

int CFileIoDos::fflush(int handle)
{
	if (PoFlush(handle) != 0)
		return 0;
	set_error();
	return -1;
}

int CFileIoDos::fseek(int handle, long offset, int whence)
{
	if (PoLseek(handle, offset, (short)whence) < 0) {
		set_error();
		return -1;
	}
	return 0;
}

unsigned int CFileIoDos::fwrite(const void *buf, unsigned int size, unsigned int count, int handle)
{
	int n = PoWrite(handle, reinterpret_cast<const unsigned char *>(buf), (unsigned long)size * count);
	if (n == -1) {
		if (g_theFilesys->lastError != 2)
			set_error();
		return 0;
	}
	return (unsigned int)n / size;
}

unsigned int CFileIoDos::fread(void *buf, unsigned int size, unsigned int count, int handle)
{
	int n = PoRead(handle, reinterpret_cast<unsigned char *>(buf), (unsigned long)size * count);
	if (n == -1) {
		set_error();
		return 0;
	}
	return (unsigned int)n / size;
}

int CFileIoDos::fclose(int handle)
{
	if (PoClose(handle) != 0) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoDos::fopen(const char *path, const char *mode)
{
	unsigned char c0 = (unsigned char)mode[0];
	unsigned dv = (unsigned)((int)c0 - 'a');

	unsigned short flags, modeVal;
	bool scanPlus = false;

	if (dv > 0x16) {
		flags = 0x10a;
		modeVal = 0x180;
	} else {
		switch (dv) {
		case 0: /* 'a' */
			modeVal = 0x100;
			flags = 0x109;
			scanPlus = true;
			break;
		case 7: /* 'h' -- real, distinct table slot; per-char meaning not recovered */
			flags = 0x302;
			modeVal = 0x200;
			break;
		case 17: /* 'r' */
			modeVal = 0x80;
			flags = 0;
			scanPlus = true;
			break;
		case 21: /* 'v' -- real, distinct table slot; per-char meaning not recovered */
			flags = 0x302;
			modeVal = 0x40;
			break;
		case 22: /* 'w' */
			modeVal = 0x100;
			flags = 0x301;
			scanPlus = true;
			break;
		default:
			flags = 0x10a;
			modeVal = 0x180;
			break;
		}
		if (scanPlus) {
			for (const char *p = mode + 1; *p; ++p) {
				if (*p != '+')
					continue;
				if (dv == 0) {
					modeVal |= 0x80;
					flags = 0x10a;
				} else if (dv == 17) {
					modeVal |= 0x100;
					flags = 2;
				} else if (dv == 22) {
					modeVal |= 0x80;
					flags = 0x302;
				}
				break;
			}
		}
	}

	int rc = PoOpen(const_cast<char *>(path), flags, modeVal);
	if (rc < 0) {
		set_error();
		return rc;
	}
	return rc;
}

int CFileIoDos::funmount(EDevice_Id device)
{
	if (m_searchActive && DosDstatDeviceMatches(m_pStat, device)) {
		m_searchActive = 0;
		PcGdone(m_pStat);
	}
	if (PcDskfree((short)(int)device, 1) == 0) {
		set_error();
		return -1;
	}
	return 0;
}

int CFileIoDos::fmount(EDevice_Id device)
{
	unsigned short sectorSize = 0;
	unsigned long long capacity = DDriverIOReadCapacityStub(device, sectorSize);
	if (capacity == 0)
		return -1;

	if (sectorSize != 0x200) {
		DDriverIOScsiModeSelStub(device, 0x200);
		unsigned short sectorSize2 = 0;
		unsigned long long capacity2 = DDriverIOReadCapacityStub(device, sectorSize2);
		if (capacity2 == 0)
			return -1;
		unsigned short delta = (unsigned short)(sectorSize2 - 0x200);
		if (delta > 0x600)
			return -1;
	}

	if (PcDskinit((short)(int)device) == 0) {
		set_error();
		return -1;
	}

	/* Real: additionally re-checks the drive's own ddrive+0x684 field
	 * (opaque, `pc_drno2dr()`-derived; a few specific small-negative
	 * sentinel values there hard-fail via `pc_dskfree()` + a DIRECT
	 * `theFilesys->lastError = 3` write, bypassing set_error() entirely,
	 * .text+0x0831bfec..0x0831c00c) -- simplified here to always take the
	 * success path, since the opaque `ddrive` stand-in never populates
	 * that field with one of the real sentinel values.
	 */
	return 0;
}

int CFileIoDos::dir(const char *path, int arg2, unsigned long &cont, CFileDirEntry *entry)
{
	if (PcIsVolumeLabelSkipped())
		return 0;

	if (cont == 0 && m_searchActive) {
		/* Real: pc_gdone()s the previous search then unconditionally
		 * falls through to the fresh-search dispatch below (same
		 * provably always-true `*cont == 0` re-check collapse as
		 * CFileIoAkai::dir()'s own).
		 */
		PcGdone(m_pStat);
	}

	unsigned char *rec = 0;
	bool haveEntry = false;

	if (arg2 == 0) {
		/* Real: a second, near-duplicate gfirst/gnext loop, taken when
		 * the caller-supplied `arg2` (real meaning not recovered) is
		 * 0 -- filters out BOTH label-flagged entries (dstat+0xd bit
		 * 0x2) and literal "." entries.
		 */
		for (;;) {
			haveEntry = (cont != 0) ? (PcGnext(m_pStat) != 0)
			                        : (PcGfirst(m_pStat, const_cast<char *>(path)) != 0);
			if (haveEntry) {
				m_searchActive = 1;
				cont = 1;
			}
			if (!haveEntry)
				break;
			rec = reinterpret_cast<unsigned char *>(m_pStat);
			if ((rec[0xd] & 0x2) || rec[0] == '.')
				continue;
			break;
		}
	} else if (cont != 0) {
		haveEntry = PcGnext(m_pStat) != 0;
		if (haveEntry) {
			rec = reinterpret_cast<unsigned char *>(m_pStat);
			while (haveEntry && rec[0] == '.') {
				haveEntry = PcGnext(m_pStat) != 0;
				if (haveEntry)
					rec = reinterpret_cast<unsigned char *>(m_pStat);
			}
		}
	} else {
		haveEntry = PcGfirst(m_pStat, const_cast<char *>(path)) != 0;
		if (haveEntry) {
			m_searchActive = 1;
			cont = 1;
			rec = reinterpret_cast<unsigned char *>(m_pStat);
			while (haveEntry && rec[0] == '.') {
				haveEntry = PcGnext(m_pStat) != 0;
				if (haveEntry)
					rec = reinterpret_cast<unsigned char *>(m_pStat);
			}
		}
	}

	if (!haveEntry) {
		m_searchActive = 0;
		cont = 0;
		PcGdone(m_pStat);
		return 0;
	}

	/* Real: CDateT::get(kind) called 6 times (kinds 0,2,1,6,5,4) against a
	 * CDateT sub-object embedded at rec+0xf4 -- byte-identical arithmetic
	 * to CFileIoAkai::dir()'s own (confirmed via `objdump -dr` diff), but
	 * operating directly on the dstat record `rec` with NO intermediate
	 * CDirentry copy step (Dos's dstat already IS the _Dos_Direntry
	 * shape).
	 */
	void *dateObj = rec + 0xf4;
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

	unsigned char dateBuf[4] = { fieldA, fieldB, rawWeekday, fieldC };
	unsigned char timeBuf[3] = { rawE, (unsigned char)rawD, (unsigned char)rawC };

	char attrChar = (char)rec[0xf1];
	unsigned int extra = *reinterpret_cast<unsigned int *>(rec + 0xf8);

	CFileDirEntryInitialize(entry, reinterpret_cast<const char *>(rec), attrChar,
	                         reinterpret_cast<CDate *>(dateBuf), reinterpret_cast<COTime *>(timeBuf),
	                         extra, 0);
	return reinterpret_cast<int>(entry);
}
