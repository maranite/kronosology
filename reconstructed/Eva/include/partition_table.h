/*
 * partition_table.h  -  CSector / CLittleEndObj / CPartitionData / CMBR / CPBR /
 * CPBRex / CPBRFat12Fat16 / CPBRFat12 / CPBRFat16 / CPBRFat32, Eva's real MBR
 * (Master Boot Record) and PBR (Partition Boot Record / BIOS Parameter Block)
 * on-disk-format value classes.
 *
 * FOUND 2026-07-28, fresh `nm -C` class-inventory sweep for the next dense,
 * mechanical-accessor-shaped cluster following the CSpecialFuncCCMap/CGlobal and
 * CRamSample/CMultiSample/CRamSampleRelative batches (see this project's own
 * `HARDWARE_REVIEW_LOG.md`). Two other promising-looking candidates were traced
 * and REJECTED before settling on this one:
 *   - `CSysExProg`/`CSysExCombi`/... (a ~40-class, ~300-method "SysEx dump digest
 *     object" family, high Get-star/Set-star ratio) -- no real caller found anywhere in
 *     a full disassembly-wide `call` xref sweep of the binary; its only plausible
 *     owner is the SysEx sniffer/tree-builder subsystem, independently already
 *     documented elsewhere in this project (pool.h) as "itself a deep, entirely
 *     unmodeled subsystem". Same god-object-adjacency problem as the excluded
 *     `CESxxxTask` family, just one level removed -- correctly rejected.
 *   - The CD-ROM/Joliet virtual-driver cluster (`CCDEntry`/`CDirCD`/`CVDrvCD`/
 *     `CCDConfigDir`/`CVirtualDriverBase`/`CDriverTaskBase`, ~200 methods) --
 *     `CBigEndObj` (its one small, genuinely mechanical foundational helper) has
 *     real, non-inlined callers, but EVERY one of them is inside this same
 *     already-excluded cluster (`file_io_base.h`'s own documented "OUT OF SCOPE"
 *     `CDDriverIO`/`CScsiDriverBase` optical-media driver family). Correctly
 *     rejected for the same reason as above.
 *
 * REACHABILITY (this cluster): traced via a full `objdump -d` xref sweep (not
 * inlined -- all these functions are real, out-of-line, non-trivial enough that
 * gcc kept them). `CLittleEndObj`'s callers and `CPartitionData`/`CPBR` ctor
 * callers are `CFileMan::ScanPartitionTable`/`CFileMan::FDisk`/
 * `CFileMan::DelLastPartitionsNoCheck` (via `CMBR::CMBR`) and `CConfigVD::
 * ConfigVD2()`/`ConfigVD3()`/`IsDOSFormat()`/`IsPartitionBootSector()` (via
 * `CPBR::CPBR`/`CPBRFat12::CPBRFat12`/etc) -- real, boot-relevant SSD
 * storage-management code. `CFileMan` itself is `file_man.h`'s own documented
 * "genuine god-object, out of scope for this pass" (only its ctor is
 * reconstructed there) -- but exactly the same "out-of-scope CALLER, in-scope
 * DATA class" split already established for `CSpecialFuncCCMap`'s
 * `CESGlobalTask` caller applies here: `CPartitionData`/`CMBR`/`CPBR`-family are
 * small, self-contained, non-`CFileMan`-member value classes, not part of that
 * god object itself. Further independent confirmation: a completely different,
 * separately-reconstructable class `CVirtualDriver` (17 `nm -C` methods, NOT
 * `CVirtualDriverBase` -- a different, real disk-driver class, not touched by
 * this batch) also takes a `CPartitionData const&` in its own ctor and calls
 * `CPartitionData::SetFileSystemType`-shaped code, so this data family is used
 * broadly, not confined to one god object.
 *
 * FORMAT: every field/method here reproduces the real, industry-standard MBR /
 * BIOS Parameter Block (BPB) / FAT12/16/32 Extended BPB byte layout --
 * independently confirmed by decoding this batch's own static offset-table
 * constants (`sm_wXxxOffset` class statics, `.data+0x091ae910..0x091aea82`,
 * read directly via `objdump -s`) and finding EVERY one matches the well-known
 * public FAT/MBR specification exactly (e.g. `CMBR::sm_wPartitionTableOffset`
 * =0x1BE, `CMBR::sm_byPartitionEntrySize`=16, `CPBR::sm_wBytePerSectorOffset`
 * =0x0B, `CSector::sm_wEndSectSignatureOffset`=0x1FE holding the classic
 * 0x55AA boot signature, `CPartitionData::sm_tMaxGeometry`={1024,255,63} the
 * standard INT13-extended CHS ceiling, `CPBRFat16::sm_wDefaultMaxNumRootEntries`
 * =512, `CPBRFat32::sm_wDefaultFatOffset`=32, ...). This is a real, byte-exact
 * transcription, not a guess dressed up to look like one.
 *
 * TWO-LAYER DESIGN, confirmed from every ctor's own body:
 *   (1) RAW layer -- most `Set*`/some `Get*`/`Is*` methods are non-virtual,
 *       `static`-shaped (no `this`, cdecl, first arg is a raw `unsigned char*`
 *       sector buffer) -- used when BUILDING a boot sector on disk (the
 *       `CFileMan::FDisk` path).
 *   (2) DECODED layer -- each class's own ctor(`const unsigned char*`) parses
 *       a raw sector buffer ONCE into normal typed member fields; the `this`-
 *       based `Is*()`/`Get*()` overloads read those decoded fields -- used when
 *       SCANNING an already-loaded boot sector (the `CFileMan::
 *       ScanPartitionTable` path).
 * Both layers reproduce the identical real byte offsets (same static tables).
 *
 * `CPBRex` (found only via a wider address-range sweep between `CPBR` and
 * `CPBRFat12Fat16` -- not part of the original `nm -C` ratio list, real ground
 * truth still puts it in this exact file) is a trivial empty subclass of `CPBR`
 * adding 3 default-value helpers only.
 *
 * `CSector` contributes exactly one real offset constant + 2 tiny static
 * helpers (`GetMinSize()`, `SetEndSectSignature()`) -- modeled minimally here,
 * not as a base class of `CMBR`/`CPBR` (ground truth's own RTTI info suggests
 * it might be, but neither ctor needs virtual dispatch through it and no
 * caller in this reconstruction's own call graph needs that relationship
 * modeled precisely).
 *
 * `Api`'s vtable slot +0x9c (`SetPartitionSerialNum`/`CPBRex::
 * GetNewPartitionSerialNum`) reuses the SAME already-documented dispatch as
 * `timer_engine.cpp`/`kg_msg_processor.cpp`'s own `ApiGetDefault9c()` --
 * apparently a "get a pseudo-random/tick-based default value" call, here used
 * to synthesize a volume serial number. `CPBRex::GetNewPartitionSerialNum()`'s
 * own real return type is `void` -- ground truth calls through the vtable slot
 * and DISCARDS the result; transcribed as-is, not "fixed" to return the value.
 *
 * `CPartitionData::CHStoLBA`/`LBAtoCHS`/`AdjustCHS` real soft-assert calls
 * (`Api`+0x94, "BootSect.cpp", literal line numbers 0x125/0x126/0x127/0x128/
 * 0x13e) are reproduced via the SAME `ApiAssert()` helper already established
 * in `tempo.cpp`/`config_manager.cpp`/etc -- real call, not dropped, since
 * unlike the kernel-side critical-section shims elsewhere in this project
 * these are genuine per-call soft assertions with their own distinct file/line
 * evidence.
 */

#ifndef PARTITION_TABLE_H
#define PARTITION_TABLE_H

/*
 * CSector -- tiny shared boot-sector utility. Real ground truth may make this a
 * base of CMBR/CPBR (RTTI hints this), not modeled as one here -- see file
 * header. Only the one constant CMBR/CPBR ctors actually consume is a class
 * member; the 2 methods are free-standing helpers on a raw buffer.
 */
class CSector {
public:
	/* raw byte offset of the classic 0x55AA end-of-sector boot signature */
	static const unsigned short sm_wEndSectSignatureOffset = 0x1fe;

	static unsigned long GetMinSize();                    /* always 0x200 (512) */
	static void SetEndSectSignature(unsigned char *buf);   /* writes 0xaa55 LE */
};

/*
 * CLittleEndObj -- stateless little-endian byte-buffer field decode/encode
 * helpers (mirrors an OA.ko-style "get/set at a raw offset" utility, but
 * little-endian here, matching real on-disk FAT/MBR byte order). Every method
 * cdecl, no `this`.
 */
class CLittleEndObj {
public:
	static unsigned short GetWord(const unsigned char *p);
	static unsigned long GetDWord(const unsigned char *p);
	static void SetWord(unsigned char *p, unsigned short v);
	static void SetDWord(unsigned char *p, unsigned long v);

	/* Get{Int,Short,Long,UInt,UShort,ULong} are real ground-truth DUPLICATES of
	 * GetDWord/GetWord/GetDWord/GetDWord/GetWord/GetDWord respectively (same
	 * body each, just a different declared return type) -- transcribed as
	 * distinct methods to match the real symbol table, not collapsed.
	 */
	static int GetInt(const unsigned char *p);
	static short GetShort(const unsigned char *p);
	static long GetLong(const unsigned char *p);
	static unsigned int GetUInt(const unsigned char *p);
	static unsigned short GetUShort(const unsigned char *p);
	static unsigned long GetULong(const unsigned char *p);

	static void SetUInt(unsigned char *p, unsigned int v); /* real dup of SetDWord */
	static unsigned long GetU3Byte(const unsigned char *p); /* 3-byte LE read */
};

/*
 * CPartitionData -- one real 16-byte MBR partition-table entry, both raw
 * (buffer, static-shaped) and decoded (this-based, 0x1c/28-byte object) forms.
 */
class CPartitionData {
public:
	/* Real ground-truth values, confirmed by CMBR::IsValid()/GetFirstValid*
	 * literal comparisons (0/0x80) and CPartitionData::IsExtended/IsSupported's
	 * own literal switch sets. Declared as plain ints (matches the raw-byte
	 * usage sites), not `enum class`.
	 */
	enum EStatus { eInactive = 0, eActive = 0x80 };
	enum EType {
		eEmpty = 0,
		eFat12 = 1,
		eFat16Small = 4,
		eExtended = 5,
		eFat16 = 6,
		eFat32 = 0x0b,
		eFat32LBA = 0x0c,
		eFat16LBA = 0x0e,
		eExtendedLBA = 0x0f,
		eExtendedLinux = 0x85,
	};

	/* CHS geometry ceiling, {cylinders, heads, sectorsPerTrack}, all u16 --
	 * order confirmed by CHStoLBA's own field-by-field ApiAssert comparisons.
	 */
	struct SMediaGeometry {
		unsigned short mCylinders;
		unsigned short mHeads;
		unsigned short mSectorsPerTrack;
	};

	/* ---- raw static offsets (real ground-truth statics) ---- */
	static const unsigned short sm_wStatusOffset = 0;
	static const unsigned short sm_wStartHeadOffset = 1;
	static const unsigned short sm_wStartCylAndSectOffset = 2;
	static const unsigned short sm_wTypeOffset = 4;
	static const unsigned short sm_wEndHeadOffset = 5;
	static const unsigned short sm_wEndCylAndSectOffset = 6;
	static const unsigned short sm_wLBAStartLocationOffset = 8;
	static const unsigned short sm_wPartitionSizeOffset = 0x0c;
	static const unsigned short sm_wMaskLowSect = 0x3f;
	static const unsigned short sm_wMaskHighCyl = 0xc0;
	static const unsigned short sm_wMaskLowCyl = 0xff00;

	static const SMediaGeometry sm_tMaxGeometry; /* {1024, 255, 63}, .cpp */

	/* ---- raw (buffer) layer ---- */
	static bool IsEmpty(const unsigned char *buf);
	static bool IsExtended(const unsigned char *buf);
	static void Reset(unsigned char *buf);
	static void SetStatus(unsigned char *buf, EStatus v);
	static void SetType(unsigned char *buf, EType v);
	static unsigned char GetType(const unsigned char *buf);
	static void SetStartCHS(unsigned char *buf, unsigned short cyl, unsigned short head,
		unsigned short sect);
	static void SetEndCHS(unsigned char *buf, unsigned short cyl, unsigned short head,
		unsigned short sect);
	static void SetLBAStartLocation(unsigned char *buf, unsigned long v);
	static unsigned long GetLBAStartLocation(const unsigned char *buf);
	static void SetPartitionSize(unsigned char *buf, unsigned long v);

	/* ---- raw (EType-typed) layer ---- */
	static bool IsEmpty(EType t);
	static bool IsExtended(EType t);
	static bool IsSupported(EType t);

	/* ---- misc ---- */
	static const SMediaGeometry *GetMaxMediaGeometry();
	static int CHStoLBA(unsigned short cyl, unsigned short head, unsigned short sect,
		const SMediaGeometry &geom);
	static void LBAtoCHS(unsigned int lba, unsigned short *cyl, unsigned short *head,
		unsigned short *sect, const SMediaGeometry &geom);
	static unsigned int GetSectCeilLBA(unsigned int lba, unsigned long align,
		const SMediaGeometry &geom);
	static void AdjustCHS(unsigned short *cyl, unsigned short *head, unsigned short *sect,
		const SMediaGeometry &geom);
	static void GetMaxCHS(unsigned short *cyl, unsigned short *head, unsigned short *sect,
		const SMediaGeometry &geom);

	/* ---- decoded (this-based) layer ---- */
	/* Default ctor NOT present in ground truth (every real construction site
	 * uses the raw-buffer or copy ctor) -- added here purely so CMBR's own 4
	 * embedded CPartitionData members are default-constructible; behaves like
	 * Reset(). */
	CPartitionData();
	CPartitionData(const unsigned char *buf);
	CPartitionData(const CPartitionData &other);
	CPartitionData &operator=(const CPartitionData &other);

	void Reset();
	bool IsEmpty() const;
	bool IsExtended() const;
	bool IsSupported() const;

	EStatus mStatus;             /* +0x00 */
	EType mType;                 /* +0x04 */
	unsigned short mStartHead;   /* +0x08 */
	unsigned short mStartCyl;    /* +0x0a */
	unsigned short mStartSect;   /* +0x0c */
	unsigned short mEndHead;     /* +0x0e */
	unsigned short mEndCyl;      /* +0x10 */
	unsigned short mEndSect;     /* +0x12 */
	unsigned long mLBAStart;     /* +0x14 */
	unsigned long mPartitionSize;/* +0x18 */
};

/*
 * CMBR -- real Master Boot Record: 4 embedded CPartitionData entries plus a
 * per-entry "isPrimary/supported" flag, decoded from a raw 512-byte sector.
 * Genuinely virtual (RTTI + a 2-slot D1/D0 vtable, confirmed by direct
 * `.rodata` vtable-content read) -- but ground truth's own vtable holds NO
 * real virtuals beyond the compiler-generated destructor pair, so this class's
 * vtable is modeled the same "all EvaVTableStub" way already established for
 * CDumpTask (omega_vtables.cpp) -- nothing in this reconstruction's own call
 * graph destroys a CMBR polymorphically.
 */
class CMBR {
public:
	static const unsigned short sm_wPartitionTableOffset = 0x1be;
	static const unsigned char sm_byPartitionEntrySize = 0x10;

	/* buf = raw 512-byte sector; extBase = LBA base to add for extended-chain
	 * partitions (real 4th ctor arg, param_4) */
	CMBR(const unsigned char *buf, unsigned short unused1, unsigned short unused2,
		unsigned long extBase);
	~CMBR();

	bool IsValid() const;
	const CPartitionData *GetFirstValidPartitionData() const;
	const CPartitionData *GetFirstValidPartitionData(int mode) const;
	const CPartitionData *GetFirstValidPrimaryPartitionData() const;

private:
	void *mVptr;                      /* +0x00 */
	unsigned short mEndSectSignature; /* +0x04 */
	int mSupported0;                  /* +0x08 */
	CPartitionData mPartition0;       /* +0x0c */
	int mSupported1;                  /* +0x28 */
	CPartitionData mPartition1;       /* +0x2c */
	int mSupported2;                  /* +0x48 */
	CPartitionData mPartition2;       /* +0x4c */
	int mSupported3;                  /* +0x68 */
	CPartitionData mPartition3;       /* +0x6c */
};

/*
 * CPBR -- decoded BIOS Parameter Block (classic FAT12/16 boot sector), the
 * common base every FATxx-specific PBR class below builds on.
 */
class CPBR {
public:
	static const unsigned short sm_wOEMnameOffset = 3;
	static const unsigned short sm_wBytePerSectorOffset = 0x0b;
	static const unsigned short sm_wSectPerClusterOffset = 0x0d;
	static const unsigned short sm_wFatOffsetOffset = 0x0e;
	static const unsigned short sm_wFatNumOffset = 0x10;
	static const unsigned short sm_wMaxNumRootEntriesOffset = 0x11;
	static const unsigned short sm_wNumSectorsOffset = 0x13;
	static const unsigned short sm_wMediaTypeOffset = 0x15;
	static const unsigned short sm_wNumFatSectorsOffset = 0x16;
	static const unsigned short sm_wSectorPerTrackOffset = 0x18;
	static const unsigned short sm_wNumHeadsOffset = 0x1a;
	static const unsigned short sm_wNumHiddenSectorsOffset = 0x1c;
	static const unsigned short sm_wNumSectorsHugeOffset = 0x20;
	static const unsigned char sm_byDefaultFatNum = 2;

	static void SetBytePerSector(unsigned char *buf, unsigned short v);
	static void SetSectPerCluster(unsigned char *buf, unsigned char v);
	static void SetFatOffset(unsigned char *buf, unsigned short v);
	static void SetFatNum(unsigned char *buf, unsigned char v);
	static void SetMaxNumRootEntries(unsigned char *buf, unsigned short v);
	static void SetNumSectors(unsigned char *buf, unsigned short v);
	static void SetMediaType(unsigned char *buf, unsigned char v);
	static void SetNumFatSectors(unsigned char *buf, unsigned short v);
	static void SetSectorPerTrack(unsigned char *buf, unsigned short v);
	static void SetNumHeads(unsigned char *buf, unsigned short v);
	static void SetNumHiddenSectors(unsigned char *buf, unsigned long v);
	static void SetNumSectorsHuge(unsigned char *buf, unsigned long v);
	static unsigned char GetDefaultFatNum();

	CPBR(const unsigned char *buf);

	unsigned short mEndSectSignature;  /* +0x00 */
	char mOEMName[8];                  /* +0x02 */
	unsigned short mBytePerSector;     /* +0x0a */
	unsigned char mSectPerCluster;     /* +0x0c */
	unsigned short mFatOffset;         /* +0x0e */
	unsigned char mFatNum;             /* +0x10 */
	unsigned short mMaxNumRootEntries; /* +0x12 */
	unsigned short mNumSectors;        /* +0x14 */
	unsigned char mMediaType;          /* +0x16 */
	unsigned short mNumFatSectors;     /* +0x18 */
	unsigned short mSectorPerTrack;    /* +0x1a */
	unsigned short mNumHeads;          /* +0x1c */
	unsigned long mNumHiddenSectors;   /* +0x20 */
	unsigned long mNumSectorsHuge;     /* +0x24 */
};

/* Trivial empty subclass of CPBR, 3 default-value helpers only. */
class CPBRex : public CPBR {
public:
	static char *sm_sDefaultVolumeName; /* "NO NAME    ", .cpp */
	static char *sm_sDefaultOEMName;    /* "OMEGA FS", .cpp -- used by
	                                      * CPBRFat12Fat16/CPBRFat32's own
	                                      * SetBeginSectSignature() */

	CPBRex(const unsigned char *buf);

	static char *GetDefaultVolumeName();
	static unsigned long GetVolumeNameSize(); /* always 0x0b */
	static void GetNewPartitionSerialNum();   /* real return value discarded, see header */
};

/* Common FAT12/FAT16 (non-32) Extended BPB fields. */
class CPBRFat12Fat16 : public CPBR {
public:
	static const unsigned short sm_wDriveNumberOffset = 0x24;
	static const unsigned short sm_wExtSignatureOffset = 0x26;
	static const unsigned short sm_wPartitionSerialNumOffset = 0x27;
	static const unsigned short sm_wVolumeNameOffset = 0x2b;
	static const unsigned short sm_wFileSystemNameOffset = 0x36;
	static const unsigned short sm_wDefaultFatOffset = 1;

	static void SetBeginSectSignature(unsigned char *buf); /* JMP+NOP + OEM name */
	static unsigned short GetDefaultFatOffset();
	static unsigned short GetVolumeNameOffset();
	static void SetVolumeName(unsigned char *buf, const char *name); /* NULL -> default */

	CPBRFat12Fat16(const unsigned char *buf);

	unsigned short mDriveNumber;   /* +0x28 */
	unsigned char mExtSignature;   /* +0x2a */
	unsigned long mPartitionSerialNum; /* +0x2c */
	char mVolumeName[11];          /* +0x30 */
	char mFileSystemName[8];       /* +0x3b */
};

class CPBRFat12 : public CPBRFat12Fat16 {
public:
	static char sm_sDefaultFileSystemName[9]; /* "FAT12   ", .cpp */

	static void SetFileSystemName(unsigned char *buf);
	static char *GetDefaultFileSystemName();
	static unsigned long GetDefaultFileSystemNameLen();
	/* param order: totalSectors, reservedSectors, numFats, rootEntries(as
	 * bytes-per-entry-32 factor -- see .cpp), bytesPerSector, sectorsPerCluster */
	static int GetSectorPerFat(unsigned long totalSectors, unsigned short reserved,
		unsigned char numFats, unsigned char rootEntries32, unsigned short bytesPerSector,
		unsigned short sectPerCluster);

	CPBRFat12(const unsigned char *buf);
};

class CPBRFat16 : public CPBRFat12Fat16 {
public:
	static char sm_sDefaultFileSystemName[9];      /* "FAT16   ", .cpp */
	static const unsigned short sm_wDefaultMaxNumRootEntries = 512;

	static void SetFileSystemName(unsigned char *buf);
	static void SetDriveNumber(unsigned char *buf);    /* always writes 0 */
	static void SetExtSignature(unsigned char *buf);   /* always writes 0x29 */
	static void SetPartitionSerialNum(unsigned char *buf); /* Api+0x9c-derived */
	static char *GetDefaultFileSystemName();
	static unsigned long GetDefaultFileSystemNameLen();
	static unsigned short GetDefaultMaxNumRootEntries();
	static int GetSectorPerFat(unsigned long totalSectors, unsigned short reserved,
		unsigned char numFats, unsigned char rootEntries32, unsigned short bytesPerSector,
		unsigned short sectPerCluster);

	CPBRFat16(const unsigned char *buf);
};

/* FAT32 has its own, larger Extended BPB (no FAT12/16-shared fields reused --
 * confirmed by CPBRFat32 re-implementing SetVolumeName/SetDriveNumber/
 * SetExtSignature/SetPartitionSerialNum/SetBeginSectSignature/SetFileSystemName
 * itself rather than inheriting CPBRFat12Fat16's).
 */
class CPBRFat32 : public CPBR {
public:
	static const unsigned short sm_wNumFatSectorsHugeOffset = 0x24;
	static const unsigned short sm_wFatFlagOffset = 0x28;
	static const unsigned short sm_wFat32VersionOffset = 0x2a;
	static const unsigned short sm_wFirstRootClusterOffset = 0x2c;
	static const unsigned short sm_wFSISOffsetOffset = 0x30;
	static const unsigned short sm_wBackupPBROffsetOffset = 0x32;
	static const unsigned short sm_wDriveNumberOffset = 0x40;
	static const unsigned short sm_wExtSignatureOffset = 0x42;
	static const unsigned short sm_wPartitionSerialNumOffset = 0x43;
	static const unsigned short sm_wVolumeNameOffset = 0x47;
	static const unsigned short sm_wFileSystemNameOffset = 0x52;
	static const unsigned short sm_wDefaultFatOffset = 0x20;
	static const unsigned short sm_wBackupPBROffsetDefault = 6;
	static char sm_sDefaultFileSystemName[9]; /* "FAT32   ", .cpp */

	static unsigned short GetFirstRootClusterOffset();
	static unsigned short GetBackupPBROffsetDefault();
	static void SetBeginSectSignature(unsigned char *buf);
	static void SetFileSystemName(unsigned char *buf);
	static void SetNumFatSectorsHuge(unsigned char *buf, unsigned long v);
	static void SetFatFlag(unsigned char *buf, unsigned char activeFat, int mirrored);
	static void SetFat32Version(unsigned char *buf, unsigned char major, unsigned char minor);
	static void SetFirstRootCluster(unsigned char *buf, unsigned long v);
	static void SetFSISOffset(unsigned char *buf, unsigned short v);
	static void SetBackupPBROffset(unsigned char *buf); /* writes the default (6) */
	static void SetDriveNumber(unsigned char *buf);     /* always writes 0x80 */
	static void SetExtSignature(unsigned char *buf);    /* always writes 0x29 */
	static void SetPartitionSerialNum(unsigned char *buf); /* Api+0x9c-derived */
	static void SetVolumeName(unsigned char *buf, const char *name); /* NULL -> default */
	static char *GetDefaultFileSystemName();
	static unsigned long GetDefaultFileSystemNameLen();
	static unsigned short GetDefaultFatOffset();
	static unsigned short GetVolumeNameOffset();
	static unsigned int GetSectorPerFat(unsigned long totalSectors, unsigned short reserved,
		unsigned char numFats, unsigned short bytesPerSector, unsigned short sectPerCluster);

	CPBRFat32(const unsigned char *buf);
	unsigned int GetNumCluster() const;

	/* Real in-memory field ORDER (confirmed by the ctor's own per-offset
	 * writes): the "common trailing EBPB" fields come FIRST, at the exact same
	 * in-memory offsets CPBRFat12Fat16 uses for its own same-named fields --
	 * even though CPBRFat32 does NOT inherit CPBRFat12Fat16 (does not reuse its
	 * ON-DISK sm_wXxxOffset constants, see class header) -- then the 6 FAT32-
	 * specific fields are appended AFTER, not interleaved. Two different offset
	 * spaces (on-disk vs in-memory) coincide here only by the original author's
	 * own field-declaration-order choice, not by inheritance.
	 */
	unsigned short mDriveNumber;      /* +0x28 */
	unsigned char mExtSignature;      /* +0x2a */
	unsigned long mPartitionSerialNum;/* +0x2c */
	char mVolumeName[11];             /* +0x30 */
	char mFileSystemName[8];          /* +0x3b */
	unsigned long mNumFatSectorsHuge; /* +0x44 */
	unsigned short mFatFlag;          /* +0x48 */
	unsigned char mFat32VersionMinor; /* +0x4a */
	unsigned char mFat32VersionMajor; /* +0x4b */
	unsigned long mFirstRootCluster;  /* +0x4c */
	unsigned short mFSISOffset;       /* +0x50 */
	unsigned short mBackupPBROffset;  /* +0x52 */
};

#endif /* PARTITION_TABLE_H */
