/*
 * scsi_driver_base.h  -  CScsiDriverBase, the SCSI/ATAPI command-descriptor-block
 * (CDB) assembly layer beneath CDDriverIO (file_io_base.h's own "OUT OF SCOPE" list
 * -- CDDriverIO/CScsiDriverBase, 85+50 methods, the deeper optical-media driver
 * cluster). Reconstructed 2026-07-28 storage-cluster follow-up, picked over
 * CFileIoCdda/CFileIoUdf (both far larger: CFileIoUdf::format() alone is 0x12b7 =
 * 4791 bytes of genuine VFAT-format logic, not remotely tractable in one batch) and
 * over the "CFileIoKge" name in file_io_base.h's original out-of-scope list, which
 * does not actually exist in the binary -- `nm -C` turns up an unrelated `CFileKge`
 * (GE/bank sample-storage file format, hundreds of methods, no CFileIoBase
 * relationship at all) instead. That name appears to have been a mistaken guess in
 * an earlier pass's header comment, not a real class; treat file_io_base.h's
 * mention of it as stale.
 *
 * REAL SHAPE: 50 `nm -C` symbols total (`.text+0x083086a0`..`0x0830ab30`).
 * Ctor/dtor/GetResultOfScsiCommandAsync (3) + 35 standalone `SetParamXxx(
 * SDriverIOPbuf*)` CDB builders are ALL reconstructed here. The remaining 11 --
 * `SetCommandParameter` (`.text+0x08308760`, 0xb77=2935 bytes), `Execute`
 * (0x100=256), `AfterProcess` (0x47e=1150) + 6 `AfterProcessXxx` helpers -- are
 * DEFERRED, documented below, not implemented this pass.
 *
 * GENUINELY INTERESTING FINDING, independently verified (grepped every `call` in
 * the whole `Eva` binary for the 35 `SetParamXxx` addresses): none of them has ANY
 * caller anywhere. `CScsiDriverBase::Execute()` (0x0830a6c0) calls
 * `SetCommandParameter()` and `AfterProcess()` only (confirmed by direct
 * disassembly of Execute's body) -- `SetCommandParameter` is a SEPARATE giant
 * `jmp [ecx*4+0x8eedb28]` jump-table switch that reimplements, inline, the exact
 * same per-command CDB-building logic as each corresponding standalone `SetParamXxx`
 * (verified by comparing several cases byte-for-byte -- e.g. its ReadCapacity case
 * at `.text+0x08309a30`-shaped code is byte-identical in structure to the standalone
 * `SetParamReadCapacity`). So the 35 methods below are real, fully decompilable, and
 * exactly match ground truth -- but on the real binary's own reachability, they are
 * dead code, superseded by SetCommandParameter's inlined duplicate. Reconstructed
 * anyway per the "decompile everything" standing goal; this is exactly the kind of
 * finding that goal is meant to surface. `CScsiDriverBase::vtable` is only 4 slots
 * (0x18 bytes = offset-to-top+RTTI+D1+D0) -- confirmed non-virtual for everything
 * else, so this reachability gap isn't a virtual-dispatch artifact.
 *
 * CScsiDriverBase member layout (`this`, from ctor `.text+0x083086e0` +
 * cross-checked against every SetParamXxx's own field writes):
 *   +0x00        vptr (compiler)
 *   +0x04..0x0f  m_cdb[12]     -- outgoing CDB bytes (opcode at [0], operands [1..11])
 *   +0x10..0x13  unknown, never touched by any of the 35 methods below
 *   +0x14        m_cdbLen      -- valid CDB length (6/10/12; NOT always == bytes
 *                                 actually written, see SetParamReadFmtCapacity's own
 *                                 comment for a real 10-vs-12 mismatch, transcribed
 *                                 as-is, not "corrected")
 *   +0x15..0x17  unknown, never touched
 *   +0x18        m_xferDir     -- 0=DATA_IN(read), 1=DATA_OUT(write), 2=NO_DATA
 *                                 (inferred from usage, not a recovered symbol name)
 *   +0x1c        m_dataPtr     -- data-transfer buffer pointer
 *   +0x20        m_xferLen     -- data-transfer byte length / SCSI allocation length
 *   +0x24..0x33  unknown, never touched
 *   +0x34        unknown dword, zeroed by ctor, checked (not written) by the
 *                out-of-scope Execute() -- looks like a cached "retry same command"
 *                flag; left opaque since Execute() itself is deferred
 *   +0x38..0x3b  unknown bytes, zeroed by ctor (also re-zeroed by
 *                SetCommandParameter's own entry prologue at +0x3b/+0x3d -- a
 *                real cross-check that these are per-command transient flags)
 *   +0x3c        m_deviceId    -- ctor's own `unsigned char` argument
 *   +0x3d        unknown byte, zeroed by ctor
 *   +0x3e..0x63  unknown, never touched
 *   +0x64,+0x68  unknown dwords, zeroed by ctor
 *   +0x6c        unknown dword, ctor sets 0xffffffff (sentinel, meaning not
 *                recovered)
 * Reserved ranges above are modeled as plain byte-array padding, sized to keep
 * every named field at its real offset, per this project's "declared uncertain
 * fields clearly" convention (see file_io_base.h's own EDevice_Id-family comment).
 *
 * SDriverIOPbuf: real layout is NOT recovered as a single coherent struct -- it is a
 * command-multiplexed parameter blob genuinely reinterpreted differently by every
 * SetParamXxx (e.g. offset 0x0c is a 32-bit LBA in SetParamReadTrackInfo but a
 * 32-bit byte-count in SetParamReadWrite). Modeled here as an opaque fixed-size
 * blob, same "opaque past what's needed" precedent as file_io_base.h's own
 * CMediaInfo/CFileDirEntry stand-ins -- see scsi_driver_base.cpp's U8/U16/U32
 * helpers and each method's own per-offset comment for the real field meaning in
 * that specific command's context.
 *
 * EMsgTypScsi: real mangled enum name (confirmed via `nm -C`,
 * `SetCommandParameter(EMsgTypScsi, SDriverIOPbuf*)`), only needed by the 11
 * deferred dispatcher methods -- declared here as an opaque placeholder for
 * forward-declaration continuity (same convention as file_io_base.h's own
 * EDevice_Id/EMountIoType family), unused by anything actually compiled this pass.
 *
 * DEFERRED (documented, not implemented this pass -- future follow-up):
 *   void SetCommandParameter(EMsgTypScsi msgType, SDriverIOPbuf *pbuf);  // 0x08308760, 2935B, 39-case jump table, ecx range 0..0x26
 *   int  Execute(EMsgTypScsi msgType, SDriverIOPbuf *pbuf);              // 0x0830a6c0, 256B, calls SetCommandParameter + AfterProcess
 *   void AfterProcess(EMsgTypScsi msgType, SDriverIOPbuf *pbuf);         // 0x0830a240, 1150B, response-side dispatcher (mirror of SetCommandParameter)
 *   void AfterProcessInquiry(SDriverIOPbuf *pbuf);                      // 0x0830a0b0, 399B
 *   void AfterProcessReadCapacity(SDriverIOPbuf *pbuf);                 // 0x0830a7c0, 103B
 *   void AfterProcessCheckWriteProtect(SDriverIOPbuf *pbuf);            // 0x0830a830, 18B
 *   void AfterProcessRequestSense(SDriverIOPbuf *pbuf);                 // 0x0830a850, 66B
 *   void AfterProcessModeSense10(SDriverIOPbuf *pbuf);                  // 0x0830a8a0, 486B
 *   void AfterProcessCheckDeviceChange(SDriverIOPbuf *pbuf);            // 0x0830aa90, 149B
 *   void AfterProcessGetProgress(SDriverIOPbuf *pbuf);                  // 0x0830ab30, 146B
 */

#ifndef SCSI_DRIVER_BASE_H
#define SCSI_DRIVER_BASE_H

/* Opaque placeholder -- see header comment. Not used by anything implemented
 * this pass (only the deferred dispatcher family takes it).
 */
enum EMsgTypScsi { kMsgTypScsi_Placeholder = 0 };

/* Opaque command-multiplexed parameter blob -- see header comment. Sized to
 * cover the highest byte offset any SetParamXxx below reads (0x19), rounded up.
 */
struct SDriverIOPbuf {
	unsigned char opaque[0x20];
};

class CScsiDriverBase {
public:
	/* Static shared "scratch" data-transfer buffer -- real symbol
	 * `CScsiDriverBase::sm_oDataBuf` (.bss+0x093b0c40, 0x64 bytes, confirmed
	 * via `nm -C -S`). Used as the response-data target for commands that
	 * don't take a caller-supplied buffer (TestUnitReady, Inquiry,
	 * RequestSense, ModeSense10, ModeSelect6, ReadCapacity,
	 * CheckWriteProtect).
	 */
	static unsigned char sm_oDataBuf[0x64];

	explicit CScsiDriverBase(unsigned char deviceId);
	virtual ~CScsiDriverBase();

	/* .text+0x083086b0, 3 bytes. Real: `return 0;` unconditionally -- no
	 * member read at all in ground truth.
	 */
	int GetResultOfScsiCommandAsync();

	/* The 35 standalone CDB-builder methods, in `nm -C` address order.
	 * Each fills m_cdb[]/m_cdbLen/m_xferDir and (where the command needs
	 * one) m_dataPtr/m_xferLen from `pbuf`'s command-specific fields --
	 * see scsi_driver_base.cpp for the exact per-command field mapping.
	 */
	void SetParamGetEventStatusNtf(SDriverIOPbuf *pbuf);
	void SetParamSendEvent(SDriverIOPbuf *pbuf);
	void SetParamGetPerformance(SDriverIOPbuf *pbuf);
	void SetParamReadDVDStructure(SDriverIOPbuf *pbuf);
	void SetParamSendDVDStructure(SDriverIOPbuf *pbuf);
	void SetParamReportKey(SDriverIOPbuf *pbuf);
	void SetParamSendKey(SDriverIOPbuf *pbuf);
	void SetParamWriteAndVerify(SDriverIOPbuf *pbuf);
	void SetParamFormatUnit(SDriverIOPbuf *pbuf);
	void SetParamReadFmtCapacity(SDriverIOPbuf *pbuf);
	void SetParamGetConfiguration(SDriverIOPbuf *pbuf);
	void SetParamTestUnitReady(SDriverIOPbuf *pbuf);
	void SetParamInquiry(SDriverIOPbuf *pbuf);
	void SetParamRmvLock(SDriverIOPbuf *pbuf);
	void SetParamRequestSense(SDriverIOPbuf *pbuf);
	void SetParamModeSense10(SDriverIOPbuf *pbuf);
	void SetParamModeSelect6(SDriverIOPbuf *pbuf);
	void SetParamModeSelect10(SDriverIOPbuf *pbuf);
	void SetParamReadCapacity(SDriverIOPbuf *pbuf);
	void SetParamReadDiskInfo(SDriverIOPbuf *pbuf);
	void SetParamReadTrackInfo(SDriverIOPbuf *pbuf);
	void SetParamReserveTrack(SDriverIOPbuf *pbuf);
	void SetParamSeek(SDriverIOPbuf *pbuf);
	/* isRead: real 3rd argument is a plain `int` (mangled `...Pbufi`), non-zero
	 * selects READ(10)/0x28/DATA_IN, zero selects WRITE(10)/0x2a/DATA_OUT.
	 */
	void SetParamReadWrite(SDriverIOPbuf *pbuf, int isRead);
	void SetParamSyncCache(SDriverIOPbuf *pbuf);
	void SetParamCloseSession(SDriverIOPbuf *pbuf);
	void SetParamBlank(SDriverIOPbuf *pbuf);
	void SetParamReadTocAtip(SDriverIOPbuf *pbuf);
	void SetParamPlayAudio(SDriverIOPbuf *pbuf);
	void SetParamReadSub(SDriverIOPbuf *pbuf);
	void SetParamReadCD(SDriverIOPbuf *pbuf);
	void SetParamSetSpeed(SDriverIOPbuf *pbuf);
	void SetParamReadBufferCapacity(SDriverIOPbuf *pbuf);
	void SetParamStartStop(SDriverIOPbuf *pbuf);
	void SetParamCheckWriteProtect(SDriverIOPbuf *pbuf);
	void SetParamSleep(SDriverIOPbuf *pbuf);

protected:
	unsigned char m_cdb[12];         /* +0x04..+0x0f */
	unsigned char m_unknown_10[4];   /* +0x10..+0x13 */
	unsigned char m_cdbLen;          /* +0x14 */
	unsigned char m_unknown_15[3];   /* +0x15..+0x17 */
	unsigned long m_xferDir;         /* +0x18: 0=DATA_IN, 1=DATA_OUT, 2=NO_DATA */
	void         *m_dataPtr;         /* +0x1c */
	unsigned long m_xferLen;         /* +0x20 */
	unsigned char m_unknown_24[0x10]; /* +0x24..+0x33 */
	unsigned long m_unknown_34;      /* +0x34 */
	unsigned char m_unknown_38;      /* +0x38 */
	unsigned char m_unknown_39;      /* +0x39 */
	unsigned char m_unknown_3a;      /* +0x3a */
	unsigned char m_unknown_3b;      /* +0x3b */
	unsigned char m_deviceId;        /* +0x3c */
	unsigned char m_unknown_3d;      /* +0x3d */
	unsigned char m_unknown_3e[0x26]; /* +0x3e..+0x63 */
	unsigned long m_unknown_64;      /* +0x64 */
	unsigned long m_unknown_68;      /* +0x68 */
	long          m_unknown_6c;      /* +0x6c, ctor sets -1 */
};

#endif /* SCSI_DRIVER_BASE_H */
