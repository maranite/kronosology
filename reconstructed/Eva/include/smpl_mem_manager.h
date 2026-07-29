/*
 * smpl_mem_manager.h  -  CSmplMemManager, the RAM sample-memory slot
 * allocator: multisample slots (0..3999), sample/drumsample slots
 * (0..15999), and "relative" (loop/attack point) slots, plus RAM byte-range
 * management (cut/insert/clear/copy) and HD-free-size bookkeeping.
 *
 * Found via a fresh `nm -C` class-inventory sweep (2026-07-28) for the next
 * CStorageConverterBase-shaped opportunity, following the same 2026-07-28
 * session's CRamSample/CMultiSample/CRamSampleRelative find. 55 `nm -C`
 * entries, `.text+0x08d624b0..0x08d679c0` (dechdfreesize, the last one),
 * all real (thiscall-with-this-on-
 * stack, matching this project's established Eva convention -- NOT
 * regparm(3) like OA.ko's STG family). Non-virtual (no vtable/typeinfo
 * symbol for CSmplMemManager) -- a real singleton (`theSmplMemManager`)
 * manipulated only through free functions, matching this project's
 * CTask-style "manual dispatch, no C++ virtuals" convention (irrelevant
 * here since it never even has a vtable-shaped dispatch array).
 *
 * REAL LAYOUT (0x18 = 24 bytes, confirmed by `malloc(0x18)` at the
 * singleton's own allocation site in csmplmemmanagerstartup(), matching
 * exactly the sum of the fields below -- no hidden/unknown fields):
 *   +0x00 (s16) mMsTop       "multisample top" bump index (msno counters)
 *   +0x02 (s16) mMsCount     multisample in-use count
 *   +0x04 (s16) mSmplTop     "sample top" bump index (smplno counters)
 *   +0x06 (s16) mSmplCount   sample/drumsample in-use count
 *   +0x08 (s16) mRltvCount   "relative" (loop/attack point) in-use count
 *   +0x0c (void*) mScratchBuf  copied from `*CSmplModeMgr::theSmplModeMgr`
 *                              at construction -- a shared scratch buffer
 *                              (ground truth: `samplingmodebuff`, a real
 *                              0x8000-byte bss buffer owned by the
 *                              out-of-scope singleton CSmplModeMgr) used as
 *                              the RAMBankPCM_read/write staging area for
 *                              cutdata/insertdata/dataclear/copydata's
 *                              chunked RAM<->RAM byte moves.
 *   +0x10 (u32) mHdFreeLo )  64-bit HD-free-size cache, written only by
 *   +0x14 (s32) mHdFreeHi )  refreshhdfreesize()/dechdfreesize() -- NOT
 *                            initialized by the ctor (ground truth leaves it
 *                            as whatever malloc() returned until the first
 *                            refresh -- a real, preserved quirk).
 *
 * The class also owns 3 module-scope globals (`ramsize`/`ramtop`/
 * `ramfreetop`, real names from `nm -C`) tracking the RAM sample heap's
 * overall size/high-water-mark/free-pointer, and a singleton pointer
 * `theSmplMemManager`. See smpl_mem_manager.cpp's own top-of-file comment
 * for the documented `(&ramfreetop)[bank]`-style raw pointer-arithmetic
 * quirk those three globals are read/written through.
 *
 * DEFERRED (3 of 55 methods fully stubbed, 1 more partial, all documented
 * in smpl_mem_manager.cpp at their
 * definition site -- same "well-documented partial deferral" shape already
 * used for CChunkClient (64/67) and CRamSample (68/69)):
 *   - csmplmemmanagerstartup()  -- one-time boot bulk-init touching the raw
 *     internal layout of 4000 CUsrMultisample + 16000 CUsrSample/
 *     CUsrDrumsample records directly (zeroing their name fields via manual
 *     offset writes, not through any accessor) -- not part of the class's
 *     own steady-state allocator API, and needs full CUsrMultisample/
 *     CUsrSample/CUsrDrumsample layout knowledge this project doesn't have.
 *   - multisamplecompare()  -- PARTIAL: the real fast path (name strncmp +
 *     attack-count-equality check, including the common "both slots
 *     unused" trivial-match case) IS implemented; only the deep per-attack-
 *     point CUsrRel/CSmplPair stereo-pair renegotiation loop (ground truth
 *     `.text+0x08d63800-0x08d63a40`) is stubbed (conservatively returns
 *     "not compatible" rather than risk a false match) -- that loop reaches
 *     into CSmplPair's raw internal layout (offsets +0x8/+0xc), a separate
 *     unmodeled class, purely to auto-negotiate stereo linking of two
 *     already-populated drum slots.
 *   - adjuststereomsno(short,short,short) / adjuststereosampleno(short,short,short)
 *     -- the two 3-argument "core" overloads. Real ground truth builds a
 *     "-L"/"-R" partner name and searches for it via searchstereonomore/
 *     searchstereonoless with an intricate cross-argument thread between
 *     the outer call's own 3 arguments and the inner search calls' 5 (one
 *     argument, confirmed via raw disassembly, is silently unused by each
 *     search variant -- floor for nomore, ceiling for noless -- a real
 *     Ghidra calling-convention misdetection was also hit and independently
 *     corrected via raw disassembly for the CUsrSample noless overload, see
 *     searchstereonoless()'s own comment). Getting the 3-arg core bit-exact
 *     would need the same raw-disassembly-level tracing for both the "-L"
 *     and "-R" branches and their ms/sample siblings -- out of proportion
 *     for this batch. The 2-argument "sub" and 0-argument "adjust everything"
 *     overloads (both ms and sample) ARE real -- they're simple unrolled
 *     loops calling the deferred 3-arg core, so they compile and run,
 *     they'll just no-op the actual pairing until the core is filled in.
 *
 * 52 of 55 methods are fully real (including multisamplecompare's fast
 * path); the 3 fully-stubbed methods above still compile and are called
 * faithfully by name.
 */

#ifndef SMPL_MEM_MANAGER_H
#define SMPL_MEM_MANAGER_H

#include "file_io_base.h" /* EDevice_Id */

/* ---- Minimal extern surface for foreign "CUsrXxx" handle classes ----
 *
 * CUsrMultisample/CUsrSample/CUsrDrumsample/CUsrRel are Eva's own unmodeled
 * "user-facing sample record" handle classes (ram_sample.h already forward-
 * declares CUsrSample as fully opaque for the same reason). Ground truth
 * treats them as thin proxy/handle objects: Bless(index) resolves the
 * handle to point at a real backing record; the resolved raw pointer lives
 * at a DIFFERENT byte offset per class (confirmed via `nm -C`'s `._N_4_`
 * pseudo-field naming on the global `USTGAPIPCMBanks::sMult/sSamp/sDrum/
 * sRel` handle instances CSmplMemManager reuses):
 *   CUsrMultisample  resolved ptr @ +0x00  (ground-truth local handle size:
 *                     `int local_24[5]` = 20 bytes, matches mPad[16])
 *   CUsrDrumsample   resolved ptr @ +0x04  (only ever used via the global
 *                     singleton handle, no local-stack-size constraint)
 *   CUsrRel          resolved ptr @ +0x04  (ditto)
 *   CUsrSample       resolved ptr @ +0x08  (ground-truth local handle size:
 *                     `ushort local_28[4]; int local_20;` = 12 bytes,
 *                     matches mPad0/mPad1/mResolved)
 * Only the specific methods CSmplMemManager actually calls are declared --
 * NOT a full reconstruction of these classes (their own real internal
 * record layout -- name field, attack points, etc -- is intentionally
 * NOT modeled here beyond the handful of byte offsets CSmplMemManager's own
 * ground truth dereferences directly, exactly mirroring the raw
 * `*(char*)(ptr+0x1a)`-style access the decompiler itself shows).
 */
class CUsrMultisample {
public:
	void Init();
	void Bless(unsigned int index);
	void Curse();
	void *mResolved; /* +0x00 */
	char mPad[16];
};

class CUsrDrumsample {
public:
	void Bless(unsigned int index);
	void *mPad0; /* +0x00 */
	void *mResolved; /* +0x04 */
};

class CUsrRel {
public:
	void Bless(unsigned int index);
	void *mPad0; /* +0x00 */
	void *mResolved; /* +0x04 */
};

class CUsrSample {
public:
	void Init();
	void Bless(unsigned int index);
	void Curse();
	unsigned long GetSampleRate() const;
	void *mPad0; /* +0x00 */
	void *mPad1; /* +0x04 */
	void *mResolved; /* +0x08 */
};

/* CSmplModeMgr -- Eva's out-of-scope singleton owning the shared
 * SAMPLINGMODEBUFFSIZE-byte scratch buffer CSmplMemManager's ctor/
 * cutdata/insertdata/dataclear/copydata borrow. Only the 2 symbols
 * CSmplMemManager's own ground truth reads are declared. */
class CSmplModeMgr {
public:
	static CSmplModeMgr *theSmplModeMgr;
	static unsigned long SAMPLINGMODEBUFFSIZE;
	void *mScratchBuf; /* +0x00 -- ground truth: `*theSmplModeMgr` */
};

/* CDeviceDesc -- Eva's out-of-scope device-descriptor/media-info class.
 * Only the one static helper refreshhdfreesize() calls. */
class CDeviceDesc {
public:
	static unsigned long long get_media_free_size(EDevice_Id device);
};

/* USTGAPIPCMBanks -- Eva's shared PCM-bank STGMessage API, already flagged
 * "genuinely deep, not reconstructed" by ustg_api_sampling.h's own header
 * comment. Only the primitives + global handle instances CSmplMemManager's
 * ground truth actually touches are declared here. */
class USTGAPIPCMBanks {
public:
	static void InitUserSampling();
	static unsigned long GetUsrPCMHeapSize();
	static void SetUsrPCMUsedSize(unsigned long size);
	static void RAMBankPCM_read(void *scratchBuf, unsigned long offset, unsigned long size);
	static void RAMBankPCM_write(void *scratchBuf, unsigned long offset, unsigned long size);
	static unsigned short *GetUsrDSStereoMapping(unsigned int drumIndex);

	static CUsrMultisample sMult;
	static CUsrSample sSamp;
	static CUsrDrumsample sDrum;
	static CUsrRel sRel;
};

class CSmplMemManager {
public:
	CSmplMemManager();
	~CSmplMemManager();

	static void csmplmemmanagerstartup(); /* DEFERRED, see header comment */

	void setramsize();
	void updateramsize(unsigned long newSize);

	/* Multisample (0..3999) slot allocation. */
	void getnewmsno(int wantSecond, short *outFirst, short *outSecond);
	void getnewmsnodec(int wantSecond, short *outFirst, short *outSecond);
	void incms(short msno);
	void decms(short msno);
	void addms(short newTop);
	void clearms();
	int getfreemsnum(unsigned char *outPercent);
	unsigned int getslidermsno(short sliderPos);

	/* Sample/drumsample (0..15999) slot allocation. */
	void getnewsmplno(int wantSecond, short *outFirst, short *outSecond, short startFrom);
	void getnewsmplnodec(int wantSecond, short *outFirst, short *outSecond, short startFrom);
	void incsmpl(short smplno);
	void decsmpl(short smplno);
	void addsmpl(short newTop);
	void clearsmpl();
	int getfreesmplnum(unsigned char *outPercent);
	unsigned int getslidersampleno(short sliderPos);

	/* "Relative" (loop/attack point) slot bookkeeping (no per-slot search --
	 * just a counter, same shape as ms/smpl count fields). */
	void addrltv(short n);
	void decrltv(short n);
	void clearrltv();
	unsigned short getuserltvnum();
	int getfreerltvnum(unsigned char *outPercent);

	/* Name+rate stereo-pair matching, used by the search family. */
	bool multisamplecompare(CUsrMultisample *candidate, char *name, CUsrMultisample *target); /* PARTIAL, see header comment */
	bool samplecompare(CUsrSample *candidate, char *name, CUsrSample *target);

	/* Linear stereo-partner search (real callers pass the class's own
	 * multisamplecompare/samplecompare). The CUsrMultisample overloads do a
	 * real two-phase scan using BOTH floor and ceil (phase 1 near `start`,
	 * phase 2 the rest of [floor,ceil)); the CUsrSample overloads are
	 * single-phase and confirmed (via raw disassembly, see .cpp) to leave
	 * one of the two bound arguments completely unused. */
	int searchstereonoless(char *name, short start, CUsrMultisample *target, short floor, short ceil);
	int searchstereonomore(char *name, short start, CUsrMultisample *target, short floor, short ceil);
	int searchstereonoless(char *name, short start, CUsrSample *target, short floor, short unusedCeil);
	unsigned int searchstereonomore(char *name, short start, CUsrSample *target, short unusedFloor, short ceil);

	/* Stereo auto-pairing on record. 3-arg cores DEFERRED, see header
	 * comment; sub/0-arg wrappers are real. */
	void adjuststereomsno(short msno, short floor, short ceil);
	void adjuststereomsnosub(short floor, short ceil);
	void adjuststereomsno();
	void adjuststereosampleno(short smplno, short floor, short ceil);
	void adjuststereosamplenosub(short floor, short ceil);
	void adjuststereosampleno();

	/* Per-bank bookkeeping. `bank` indexes a reinterpreted small array --
	 * see smpl_mem_manager.cpp's top-of-file comment for why. */
	bool isexistbank(unsigned char bank);
	unsigned long getfreetop(unsigned char bank);
	void setfreetop(unsigned char bank, unsigned long value);
	int getremainsize(unsigned char bank);
	void getremainsize(unsigned long *out);
	int getremainsmpltimems(unsigned char bank, int highRate);
	unsigned int getremainsmpltimeandsize(unsigned char bank, unsigned long *outKb, unsigned char *outPercent);

	/* Raw RAM byte-range operations, chunked through mScratchBuf in
	 * SAMPLINGMODEBUFFSIZE-byte pieces. */
	void writedata(unsigned char bank, char *dst, char *src, unsigned long size);
	void readdata(unsigned char bank, char *dst, char *src, unsigned long size);
	void cutdata(unsigned char bank, unsigned long dstOffset, unsigned long srcOffset);
	void insertdata(unsigned char bank, unsigned long fromOffset, unsigned long toOffset);
	void dataclear(unsigned char bank, unsigned long offset, unsigned long size);
	void copydata(unsigned char dstBank, unsigned long dstOffset, unsigned char srcBank, unsigned long srcOffset, unsigned long size);

	/* Iterates the "relative" slots belonging to multisample msno; call
	 * with *ioReset!=0 to (re)start, ==0 to continue. Real static-local
	 * iterator state (magic-statics idiom), single-instance/non-reentrant,
	 * exactly matching ground truth. */
	unsigned int getusedsampleno(int msno, int *outIndex, int *ioReset);

	void refreshhdfreesize(EDevice_Id device);
	unsigned int gethdfreesize();
	void dechdfreesize(unsigned long amount);

	static CSmplMemManager *theSmplMemManager;

private:
	short mMsTop;
	short mMsCount;
	short mSmplTop;
	short mSmplCount;
	short mRltvCount;
	void *mScratchBuf;
	unsigned int mHdFreeLo;
	int mHdFreeHi;
};

#endif /* SMPL_MEM_MANAGER_H */
