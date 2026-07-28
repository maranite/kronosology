/*
 * scsi_driver_base.cpp  -  see include/scsi_driver_base.h.
 *
 * Ctor/dtor/GetResultOfScsiCommandAsync + all 35 standalone SetParamXxx CDB
 * builders, transcribed byte-for-byte from `objdump -dr -M intel`
 * (Decomp/EVA_Decomp/Eva, .text+0x083086a0..0x0830a0b0). See the header comment
 * for the class layout, the SDriverIOPbuf opaque-blob rationale, and the
 * dead-code finding (none of these 35 methods has any caller in the real binary
 * -- CScsiDriverBase::Execute() calls the separate, out-of-scope
 * SetCommandParameter() jump-table switch instead, which duplicates this exact
 * logic inline).
 *
 * Every SetParamXxx below follows the same real shape: fill m_cdb[]/m_cdbLen
 * (always), m_xferDir (always), and m_dataPtr/m_xferLen (only for commands that
 * move data). CDB multi-byte fields are consistently big-endian (matches real
 * SCSI/MMC wire format); the per-byte shift chains below mirror the ground
 * truth's own shr/mov sequences exactly, including two confirmed real quirks
 * that are transcribed as-is rather than "corrected":
 *   - SetParamReadFmtCapacity sets m_cdbLen=12 but only ever writes CDB bytes
 *     0..9 (10 bytes) -- bytes 10/11 are left whatever a prior call left them.
 *   - SetParamSetSpeed only writes CDB bytes 2/3 (resp. 4/5) when the
 *     corresponding pbuf word is non-zero -- real conditional skip, not a bug.
 */

#include "scsi_driver_base.h"

#include <cstring>

namespace {

/* SDriverIOPbuf field readers -- see header comment, the real struct is a
 * command-multiplexed blob, not a coherent set of named fields. memcpy avoids
 * strict-aliasing / alignment concerns for the handful of dword/word reads at
 * odd offsets ground truth performs directly via x86 unaligned loads.
 */
inline unsigned char U8(const SDriverIOPbuf *p, unsigned off)
{
	return reinterpret_cast<const unsigned char *>(p)[off];
}

inline unsigned short U16(const SDriverIOPbuf *p, unsigned off)
{
	unsigned short v;
	std::memcpy(&v, reinterpret_cast<const unsigned char *>(p) + off, sizeof v);
	return v;
}

inline unsigned long U32(const SDriverIOPbuf *p, unsigned off)
{
	unsigned long v;
	std::memcpy(&v, reinterpret_cast<const unsigned char *>(p) + off, sizeof v);
	return v;
}

enum { kScsiDataIn = 0, kScsiDataOut = 1, kScsiNoData = 2 };

} // namespace

unsigned char CScsiDriverBase::sm_oDataBuf[0x64];

/* .text+0x083086e0. Real: zeroes the CDB/xfer fields, sets m_xferDir=NO_DATA,
 * m_cdbLen=0, m_xferLen=0, stores the device-id argument at +0x3c, zeroes
 * +0x38/0x39/0x3a/0x3b/0x3d/0x34/0x64/0x68, sets +0x6c=-1. See header comment
 * for the full offset table.
 */
CScsiDriverBase::CScsiDriverBase(unsigned char deviceId)
{
	std::memset(m_cdb, 0, sizeof m_cdb);
	std::memset(m_unknown_10, 0, sizeof m_unknown_10);
	m_cdbLen = 0;
	std::memset(m_unknown_15, 0, sizeof m_unknown_15);
	m_xferDir = kScsiNoData;
	m_dataPtr = 0;
	m_xferLen = 0;
	std::memset(m_unknown_24, 0, sizeof m_unknown_24);
	m_unknown_34 = 0;
	m_unknown_38 = 0;
	m_unknown_39 = 0;
	m_unknown_3a = 0;
	m_unknown_3b = 0;
	m_deviceId = deviceId;
	m_unknown_3d = 0;
	std::memset(m_unknown_3e, 0, sizeof m_unknown_3e);
	m_unknown_64 = 0;
	m_unknown_68 = 0;
	m_unknown_6c = -1;
}

/* .text+0x083086a0 (D1) / 0x083086c0 (D0). Real body only resets the vtable
 * pointer (D0 additionally free()s `this`) -- compiler bookkeeping, empty
 * virtual dtor is faithful, same convention as file_io_base.h's CFileIoBase.
 */
CScsiDriverBase::~CScsiDriverBase()
{
}

/* .text+0x083086b0, 3 bytes. Real: `return 0;` unconditionally. */
int CScsiDriverBase::GetResultOfScsiCommandAsync()
{
	return 0;
}

/* GET EVENT STATUS NOTIFICATION (0x4a), CDB len 10, DATA_IN.
 * pbuf: dw08=buffer ptr, dw10=alloc length (also cdb[7..8] BE16), b0c=polled
 * flag (cdb[1]), b0d=notification class mask (cdb[4]).
 */
void CScsiDriverBase::SetParamGetEventStatusNtf(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataIn;
	unsigned long alloc = U32(pbuf, 0x10);
	m_xferLen = alloc;
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	m_cdbLen = 10;
	m_cdb[0] = 0x4a;
	m_cdb[1] = U8(pbuf, 0x0c);
	m_cdb[2] = 0;
	m_cdb[3] = 0;
	m_cdb[4] = U8(pbuf, 0x0d);
	m_cdb[5] = 0;
	m_cdb[6] = 0;
	m_cdb[7] = static_cast<unsigned char>((alloc >> 8) & 0xff);
	m_cdb[8] = static_cast<unsigned char>(alloc & 0xff);
	m_cdb[9] = 0;
}

/* opcode 0x5d, CDB len 12, DATA_OUT.
 * pbuf: dw08=buffer ptr, w0e=structure length (cdb[8..9] BE16), b0c bit0
 * (cdb[1]).
 */
void CScsiDriverBase::SetParamSendEvent(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataOut;
	unsigned short len = U16(pbuf, 0x0e);
	m_xferLen = len;
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	m_cdbLen = 12;
	m_cdb[0] = 0x5d;
	m_cdb[1] = static_cast<unsigned char>(U8(pbuf, 0x0c) & 0x01);
	m_cdb[2] = 0; m_cdb[3] = 0; m_cdb[4] = 0; m_cdb[5] = 0; m_cdb[6] = 0; m_cdb[7] = 0;
	m_cdb[8] = static_cast<unsigned char>((len >> 8) & 0xff);
	m_cdb[9] = static_cast<unsigned char>(len & 0xff);
	m_cdb[10] = 0;
	m_cdb[11] = 0;
}

/* GET PERFORMANCE (0xac), CDB len 12, DATA_IN.
 * pbuf: dw08=buffer ptr, w0c=alloc/xfer length, dw10=starting LBA (BE32,
 * cdb[2..5]), w14=max descriptors (BE16, cdb[8..9]), b0e=data-type byte
 * (cdb[1]), b16=command type (cdb[10]).
 */
void CScsiDriverBase::SetParamGetPerformance(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataIn;
	m_xferLen = U16(pbuf, 0x0c);
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	unsigned long lba = U32(pbuf, 0x10);
	unsigned short w14 = U16(pbuf, 0x14);
	m_cdbLen = 12;
	m_cdb[0] = 0xac;
	m_cdb[1] = U8(pbuf, 0x0e);
	m_cdb[2] = static_cast<unsigned char>((lba >> 24) & 0xff);
	m_cdb[3] = static_cast<unsigned char>((lba >> 16) & 0xff);
	m_cdb[4] = static_cast<unsigned char>((lba >> 8) & 0xff);
	m_cdb[5] = static_cast<unsigned char>(lba & 0xff);
	m_cdb[6] = 0;
	m_cdb[7] = 0;
	m_cdb[8] = static_cast<unsigned char>((w14 >> 8) & 0xff);
	m_cdb[9] = static_cast<unsigned char>(w14 & 0xff);
	m_cdb[10] = U8(pbuf, 0x16);
	m_cdb[11] = 0;
}

/* READ DVD STRUCTURE (0xad), CDB len 12, DATA_IN.
 * pbuf: dw08=buffer ptr, w12=alloc length, dw0c=address (BE32, cdb[2..5]),
 * b10=layer number (cdb[6]), b11=format code (cdb[7]), b14=AGID (cdb[10]<<6).
 */
void CScsiDriverBase::SetParamReadDVDStructure(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataIn;
	unsigned short w12 = U16(pbuf, 0x12);
	m_xferLen = w12;
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	unsigned long addr = U32(pbuf, 0x0c);
	m_cdbLen = 12;
	m_cdb[0] = 0xad;
	m_cdb[1] = 0;
	m_cdb[2] = static_cast<unsigned char>((addr >> 24) & 0xff);
	m_cdb[3] = static_cast<unsigned char>((addr >> 16) & 0xff);
	m_cdb[4] = static_cast<unsigned char>((addr >> 8) & 0xff);
	m_cdb[5] = static_cast<unsigned char>(addr & 0xff);
	m_cdb[6] = U8(pbuf, 0x10);
	m_cdb[7] = U8(pbuf, 0x11);
	m_cdb[8] = static_cast<unsigned char>((w12 >> 8) & 0xff);
	m_cdb[9] = static_cast<unsigned char>(w12 & 0xff);
	m_cdb[10] = static_cast<unsigned char>((U8(pbuf, 0x14) << 6) & 0xff);
	m_cdb[11] = 0;
}

/* SEND DVD STRUCTURE (0xbf), CDB len 12, DATA_OUT.
 * pbuf: dw08=buffer ptr, w0e=structure data length (cdb[8..9]), b0c=format
 * code (cdb[7]).
 */
void CScsiDriverBase::SetParamSendDVDStructure(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataOut;
	unsigned short len = U16(pbuf, 0x0e);
	m_xferLen = len;
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	m_cdbLen = 12;
	m_cdb[0] = 0xbf;
	m_cdb[1] = 0; m_cdb[2] = 0; m_cdb[3] = 0; m_cdb[4] = 0; m_cdb[5] = 0; m_cdb[6] = 0;
	m_cdb[7] = U8(pbuf, 0x0c);
	m_cdb[8] = static_cast<unsigned char>((len >> 8) & 0xff);
	m_cdb[9] = static_cast<unsigned char>(len & 0xff);
	m_cdb[10] = 0;
	m_cdb[11] = 0;
}

/* REPORT KEY (0xa4), CDB len 12, DATA_IN.
 * pbuf: dw08=buffer ptr, w12=alloc length, dw0c=LBA (BE32, cdb[2..5]),
 * b10=key class (cdb[7]), b14/b15 combine into cdb[10] (agid<<6 + key format).
 */
void CScsiDriverBase::SetParamReportKey(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataIn;
	unsigned short w12 = U16(pbuf, 0x12);
	m_xferLen = w12;
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	unsigned long lba = U32(pbuf, 0x0c);
	m_cdbLen = 12;
	m_cdb[0] = 0xa4;
	m_cdb[1] = 0;
	m_cdb[2] = static_cast<unsigned char>((lba >> 24) & 0xff);
	m_cdb[3] = static_cast<unsigned char>((lba >> 16) & 0xff);
	m_cdb[4] = static_cast<unsigned char>((lba >> 8) & 0xff);
	m_cdb[5] = static_cast<unsigned char>(lba & 0xff);
	m_cdb[6] = 0;
	m_cdb[7] = U8(pbuf, 0x10);
	m_cdb[8] = static_cast<unsigned char>((w12 >> 8) & 0xff);
	m_cdb[9] = static_cast<unsigned char>(w12 & 0xff);
	m_cdb[10] = static_cast<unsigned char>(((U8(pbuf, 0x14) << 6) + U8(pbuf, 0x15)) & 0xff);
	m_cdb[11] = 0;
}

/* SEND KEY (0xa3), CDB len 12, DATA_OUT.
 * pbuf: dw08=buffer ptr, w0c=param length (cdb[8..9]), b0e/b0f combine into
 * cdb[10] (agid<<6 + key format).
 */
void CScsiDriverBase::SetParamSendKey(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataOut;
	unsigned short w0c = U16(pbuf, 0x0c);
	m_xferLen = w0c;
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	m_cdbLen = 12;
	m_cdb[0] = 0xa3;
	m_cdb[1] = 0; m_cdb[2] = 0; m_cdb[3] = 0; m_cdb[4] = 0; m_cdb[5] = 0; m_cdb[6] = 0; m_cdb[7] = 0;
	m_cdb[8] = static_cast<unsigned char>((w0c >> 8) & 0xff);
	m_cdb[9] = static_cast<unsigned char>(w0c & 0xff);
	m_cdb[10] = static_cast<unsigned char>(((U8(pbuf, 0x0e) << 6) + U8(pbuf, 0x0f)) & 0xff);
	m_cdb[11] = 0;
}

/* WRITE AND VERIFY (10) (0x2e), CDB len 12, DATA_OUT.
 * pbuf: dw08=buffer ptr, dw0c=LBA (BE32, cdb[2..5]), dw10=BE32 spread across
 * cdb[6..9] (real, transcribed as-is -- not the SCSI-2 spec's usual
 * reserved/length-only shape for this opcode).
 */
void CScsiDriverBase::SetParamWriteAndVerify(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataOut;
	unsigned long dw10 = U32(pbuf, 0x10);
	m_xferLen = dw10;
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	unsigned long lba = U32(pbuf, 0x0c);
	m_cdbLen = 12;
	m_cdb[0] = 0x2e;
	m_cdb[1] = 0;
	m_cdb[2] = static_cast<unsigned char>((lba >> 24) & 0xff);
	m_cdb[3] = static_cast<unsigned char>((lba >> 16) & 0xff);
	m_cdb[4] = static_cast<unsigned char>((lba >> 8) & 0xff);
	m_cdb[5] = static_cast<unsigned char>(lba & 0xff);
	m_cdb[6] = static_cast<unsigned char>((dw10 >> 24) & 0xff);
	m_cdb[7] = static_cast<unsigned char>((dw10 >> 16) & 0xff);
	m_cdb[8] = static_cast<unsigned char>((dw10 >> 8) & 0xff);
	m_cdb[9] = static_cast<unsigned char>(dw10 & 0xff);
	m_cdb[10] = 0;
	m_cdb[11] = 0;
}

/* FORMAT UNIT (0x04), CDB len 6, DATA_OUT.
 * pbuf: dw08=buffer ptr, w0c=xfer length, b0e/b0f/b10 OR together into cdb[1]
 * (FmtData/CmpLst/DefectListFormat-shaped flags), w12=cdb[3..4] BE16.
 */
void CScsiDriverBase::SetParamFormatUnit(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataOut;
	m_xferLen = U16(pbuf, 0x0c);
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	unsigned short w12 = U16(pbuf, 0x12);
	unsigned char flags = static_cast<unsigned char>(
		((U8(pbuf, 0x0e) << 4) | (U8(pbuf, 0x0f) << 3) | U8(pbuf, 0x10)) & 0xff);
	m_cdbLen = 6;
	m_cdb[0] = 0x04;
	m_cdb[1] = flags;
	m_cdb[2] = 0;
	m_cdb[3] = static_cast<unsigned char>((w12 >> 8) & 0xff);
	m_cdb[4] = static_cast<unsigned char>(w12 & 0xff);
	m_cdb[5] = 0;
}

/* opcode 0x23 ("READ FORMAT CAPACITIES"-shaped), CDB len field says 12 but
 * only bytes 0..9 are ever written here (real quirk, transcribed as-is --
 * bytes 10/11 keep whatever a previous call left in m_cdb).
 * pbuf: dw08=buffer ptr, w0c=alloc length (cdb[7..8] BE16).
 */
void CScsiDriverBase::SetParamReadFmtCapacity(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataIn;
	unsigned short w0c = U16(pbuf, 0x0c);
	m_xferLen = w0c;
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	m_cdbLen = 12;
	m_cdb[0] = 0x23;
	m_cdb[1] = 0; m_cdb[2] = 0; m_cdb[3] = 0; m_cdb[4] = 0; m_cdb[5] = 0; m_cdb[6] = 0;
	m_cdb[7] = static_cast<unsigned char>((w0c >> 8) & 0xff);
	m_cdb[8] = static_cast<unsigned char>(w0c & 0xff);
	m_cdb[9] = 0;
}

/* GET CONFIGURATION (0x46), CDB len 10, DATA_IN.
 * pbuf: dw08=buffer ptr, w10=alloc length (cdb[7..8]), b0c=RT (cdb[1]),
 * w0e=starting feature (cdb[2..3]).
 */
void CScsiDriverBase::SetParamGetConfiguration(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataIn;
	unsigned short w10 = U16(pbuf, 0x10);
	m_xferLen = w10;
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	unsigned short w0e = U16(pbuf, 0x0e);
	m_cdbLen = 10;
	m_cdb[0] = 0x46;
	m_cdb[1] = U8(pbuf, 0x0c);
	m_cdb[2] = static_cast<unsigned char>((w0e >> 8) & 0xff);
	m_cdb[3] = static_cast<unsigned char>(w0e & 0xff);
	m_cdb[4] = 0; m_cdb[5] = 0;
	m_cdb[6] = 0;
	m_cdb[7] = static_cast<unsigned char>((w10 >> 8) & 0xff);
	m_cdb[8] = static_cast<unsigned char>(w10 & 0xff);
	m_cdb[9] = 0;
}

/* TEST UNIT READY (0x00), CDB len 6, NO_DATA. pbuf argument is real but
 * entirely unread (only `this`-side globals touched) -- also zeroes 18 bytes
 * of sm_oDataBuf (offsets 0x00,0x04,0x08,0x0c dwords + 0x10 word) as a side
 * effect, transcribed exactly.
 */
void CScsiDriverBase::SetParamTestUnitReady(SDriverIOPbuf * /*pbuf*/)
{
	std::memset(sm_oDataBuf + 0x00, 0, 4);
	std::memset(sm_oDataBuf + 0x04, 0, 4);
	m_xferDir = kScsiNoData;
	m_dataPtr = sm_oDataBuf;
	std::memset(sm_oDataBuf + 0x08, 0, 4);
	std::memset(sm_oDataBuf + 0x0c, 0, 4);
	std::memset(sm_oDataBuf + 0x10, 0, 2);
	m_cdbLen = 6;
	m_cdb[0] = 0; m_cdb[1] = 0; m_cdb[2] = 0; m_cdb[3] = 0; m_cdb[4] = 0; m_cdb[5] = 0;
}

/* INQUIRY (0x12), CDB len 6, DATA_IN, fixed standard-inquiry request -- pbuf
 * argument is real but entirely unread (only `this` and sm_oDataBuf touched).
 * Zeroes sm_oDataBuf[0..0x23] (36 bytes, `rep stos` of 9 dwords) first.
 */
void CScsiDriverBase::SetParamInquiry(SDriverIOPbuf * /*pbuf*/)
{
	std::memset(sm_oDataBuf, 0, 0x24);
	m_xferDir = kScsiDataIn;
	m_xferLen = 0x24;
	m_dataPtr = sm_oDataBuf;
	m_cdbLen = 6;
	m_cdb[0] = 0x12;
	m_cdb[1] = 0; m_cdb[2] = 0; m_cdb[3] = 0;
	m_cdb[4] = 0x24;
	m_cdb[5] = 0;
}

/* PREVENT ALLOW MEDIUM REMOVAL (0x1e), CDB len 6, NO_DATA.
 * pbuf: b05 != 0 -> cdb[4] (Prevent bit) = 1.
 */
void CScsiDriverBase::SetParamRmvLock(SDriverIOPbuf *pbuf)
{
	m_cdbLen = 6;
	m_cdb[0] = 0x1e;
	m_cdb[1] = 0; m_cdb[2] = 0; m_cdb[3] = 0;
	m_xferDir = kScsiNoData;
	m_cdb[4] = (U8(pbuf, 0x05) != 0) ? 1 : 0;
	m_cdb[5] = 0;
}

/* REQUEST SENSE (0x03), CDB len 6, DATA_IN, fixed 18-byte alloc length --
 * pbuf argument is real but entirely unread.
 */
void CScsiDriverBase::SetParamRequestSense(SDriverIOPbuf * /*pbuf*/)
{
	m_xferDir = kScsiDataIn;
	m_xferLen = 0x12;
	m_dataPtr = sm_oDataBuf;
	m_cdbLen = 6;
	m_cdb[0] = 0x03;
	m_cdb[1] = 0; m_cdb[2] = 0; m_cdb[3] = 0;
	m_cdb[4] = 0x12;
	m_cdb[5] = 0;
}

/* MODE SENSE (10) (0x5a), CDB len 10, DATA_IN, response into sm_oDataBuf.
 * pbuf: b06=page code (cdb[2]), b07=extra alloc-length bytes added to the
 * fixed 8-byte base (cdb[7..8] BE16).
 */
void CScsiDriverBase::SetParamModeSense10(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataIn;
	unsigned long alloc = static_cast<unsigned long>(U8(pbuf, 0x07)) + 8;
	m_dataPtr = sm_oDataBuf;
	m_cdbLen = 10;
	m_xferLen = alloc;
	m_cdb[0] = 0x5a;
	m_cdb[1] = 0x08;
	m_cdb[2] = U8(pbuf, 0x06);
	m_cdb[3] = 0; m_cdb[4] = 0; m_cdb[5] = 0; m_cdb[6] = 0;
	m_cdb[7] = static_cast<unsigned char>((alloc >> 8) & 0xff);
	m_cdb[8] = static_cast<unsigned char>(alloc & 0xff);
	m_cdb[9] = 0;
}

/* MODE SELECT (6) (0x15), CDB len 6, DATA_OUT. Builds a mode-parameter
 * header + 1 block descriptor directly in sm_oDataBuf: header byte[3] (block
 * descriptor length) fixed at 8, descriptor bytes [8..0xb] zeroed then
 * [0xa]=pbuf.b07, [0xb]=low byte of pbuf.w06 -- transcribed exactly as
 * ground truth builds it (byte roles beyond that aren't independently
 * confirmed against the SCSI-2 mode-parameter spec).
 */
void CScsiDriverBase::SetParamModeSelect6(SDriverIOPbuf *pbuf)
{
	std::memset(sm_oDataBuf + 0x00, 0, 4);
	std::memset(sm_oDataBuf + 0x04, 0, 4);
	sm_oDataBuf[0x03] = 0x08;
	m_xferDir = kScsiDataOut;
	m_xferLen = 0x0c;
	std::memset(sm_oDataBuf + 0x08, 0, 4);
	m_dataPtr = sm_oDataBuf;
	sm_oDataBuf[0x0a] = U8(pbuf, 0x07);
	sm_oDataBuf[0x0b] = static_cast<unsigned char>(U16(pbuf, 0x06) & 0xff);
	m_cdbLen = 6;
	m_cdb[0] = 0x15;
	m_cdb[1] = 0; m_cdb[2] = 0; m_cdb[3] = 0;
	m_cdb[4] = 0x0c;
	m_cdb[5] = 0;
}

/* MODE SELECT (10) (0x55), CDB len 10, DATA_OUT.
 * pbuf: dw08=buffer ptr, b07=xfer length (cdb[8]).
 */
void CScsiDriverBase::SetParamModeSelect10(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataOut;
	unsigned char b07 = U8(pbuf, 0x07);
	m_xferLen = b07;
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	m_cdbLen = 10;
	m_cdb[0] = 0x55;
	m_cdb[1] = 0x10;
	m_cdb[2] = 0; m_cdb[3] = 0; m_cdb[4] = 0; m_cdb[5] = 0; m_cdb[6] = 0; m_cdb[7] = 0;
	m_cdb[8] = b07;
	m_cdb[9] = 0;
}

/* READ CAPACITY (10) (0x25), CDB len 10, DATA_IN, response into sm_oDataBuf,
 * fixed 8-byte alloc length -- pbuf argument unread.
 */
void CScsiDriverBase::SetParamReadCapacity(SDriverIOPbuf * /*pbuf*/)
{
	m_xferDir = kScsiDataIn;
	m_xferLen = 8;
	m_dataPtr = sm_oDataBuf;
	m_cdbLen = 10;
	m_cdb[0] = 0x25;
	m_cdb[1] = 0; m_cdb[2] = 0; m_cdb[3] = 0; m_cdb[4] = 0; m_cdb[5] = 0;
	m_cdb[6] = 0; m_cdb[7] = 0; m_cdb[8] = 0; m_cdb[9] = 0;
}

/* READ DISC INFORMATION (0x51), CDB len 10, DATA_IN, fixed 24-byte alloc
 * length. pbuf: dw08=buffer ptr.
 */
void CScsiDriverBase::SetParamReadDiskInfo(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataIn;
	m_xferLen = 0x18;
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	m_cdbLen = 10;
	m_cdb[0] = 0x51;
	m_cdb[1] = 0; m_cdb[2] = 0; m_cdb[3] = 0; m_cdb[4] = 0; m_cdb[5] = 0; m_cdb[6] = 0; m_cdb[7] = 0;
	m_cdb[8] = 0x18;
	m_cdb[9] = 0;
}

/* READ TRACK INFORMATION (0x52), CDB len 10, DATA_IN, fixed 28-byte alloc
 * length, address type fixed at LBA (cdb[1]=1).
 * pbuf: dw08=buffer ptr, dw0c=address (BE32, cdb[2..5]).
 */
void CScsiDriverBase::SetParamReadTrackInfo(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataIn;
	m_xferLen = 0x1c;
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	unsigned long addr = U32(pbuf, 0x0c);
	m_cdbLen = 10;
	m_cdb[0] = 0x52;
	m_cdb[1] = 1;
	m_cdb[2] = static_cast<unsigned char>((addr >> 24) & 0xff);
	m_cdb[3] = static_cast<unsigned char>((addr >> 16) & 0xff);
	m_cdb[4] = static_cast<unsigned char>((addr >> 8) & 0xff);
	m_cdb[5] = static_cast<unsigned char>(addr & 0xff);
	m_cdb[6] = 0; m_cdb[7] = 0;
	m_cdb[8] = 0x1c;
	m_cdb[9] = 0;
}

/* RESERVE TRACK (0x53), CDB len 10, NO_DATA, fixed 28-byte xfer length.
 * pbuf: dw0c=data pointer (NOTE: +0xc here, unlike every other method's
 * +0x08 -- real, transcribed as-is), dw08=reserved track length (BE32,
 * cdb[5..8]).
 */
void CScsiDriverBase::SetParamReserveTrack(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiNoData;
	m_xferLen = 0x1c;
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x0c));
	unsigned long len = U32(pbuf, 0x08);
	m_cdbLen = 10;
	m_cdb[0] = 0x53;
	m_cdb[1] = 0; m_cdb[2] = 0; m_cdb[3] = 0; m_cdb[4] = 0;
	m_cdb[5] = static_cast<unsigned char>((len >> 24) & 0xff);
	m_cdb[6] = static_cast<unsigned char>((len >> 16) & 0xff);
	m_cdb[7] = static_cast<unsigned char>((len >> 8) & 0xff);
	m_cdb[8] = static_cast<unsigned char>(len & 0xff);
	m_cdb[9] = 0;
}

/* SEEK (10) (0x2b), CDB len 10, NO_DATA. m_dataPtr/m_xferLen untouched (real
 * -- this command moves no data).
 * pbuf: dw08=LBA (BE32, cdb[2..5]).
 */
void CScsiDriverBase::SetParamSeek(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiNoData;
	unsigned long lba = U32(pbuf, 0x08);
	m_cdbLen = 10;
	m_cdb[0] = 0x2b;
	m_cdb[1] = 0;
	m_cdb[2] = static_cast<unsigned char>((lba >> 24) & 0xff);
	m_cdb[3] = static_cast<unsigned char>((lba >> 16) & 0xff);
	m_cdb[4] = static_cast<unsigned char>((lba >> 8) & 0xff);
	m_cdb[5] = static_cast<unsigned char>(lba & 0xff);
	m_cdb[6] = 0; m_cdb[7] = 0; m_cdb[8] = 0; m_cdb[9] = 0;
}

/* READ(10)/WRITE(10) (0x28/0x2a), CDB len 10.
 * isRead != 0 -> READ(10)/DATA_IN; isRead == 0 -> WRITE(10)/DATA_OUT.
 * pbuf: dw08=buffer ptr, dw10=LBA (BE32, cdb[2..5]), dw14=block count
 * (BE16, cdb[7..8]), dw0c=block size -- m_xferLen = block size * block count
 * (real `imul`, argument order swapped between the two branches but the
 * product is identical).
 */
void CScsiDriverBase::SetParamReadWrite(SDriverIOPbuf *pbuf, int isRead)
{
	unsigned char opcode;
	if (isRead != 0) {
		m_xferDir = kScsiDataIn;
		opcode = 0x28;
	} else {
		m_xferDir = kScsiDataOut;
		opcode = 0x2a;
	}
	m_xferLen = U32(pbuf, 0x0c) * U32(pbuf, 0x14);
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	unsigned long lba = U32(pbuf, 0x10);
	unsigned long blocks = U32(pbuf, 0x14);
	m_cdbLen = 10;
	m_cdb[0] = opcode;
	m_cdb[1] = 0;
	m_cdb[2] = static_cast<unsigned char>((lba >> 24) & 0xff);
	m_cdb[3] = static_cast<unsigned char>((lba >> 16) & 0xff);
	m_cdb[4] = static_cast<unsigned char>((lba >> 8) & 0xff);
	m_cdb[5] = static_cast<unsigned char>(lba & 0xff);
	m_cdb[6] = 0;
	m_cdb[7] = static_cast<unsigned char>((blocks >> 8) & 0xff);
	m_cdb[8] = static_cast<unsigned char>(blocks & 0xff);
	m_cdb[9] = 0;
}

/* SYNCHRONIZE CACHE (10) (0x35), CDB len 10, NO_DATA.
 * pbuf: b05 != 0 -> cdb[1] = 2, else cdb[1] = 0.
 */
void CScsiDriverBase::SetParamSyncCache(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiNoData;
	m_cdbLen = 10;
	m_cdb[0] = 0x35;
	m_cdb[1] = (U8(pbuf, 0x05) != 0) ? 2 : 0;
	m_cdb[2] = 0; m_cdb[3] = 0; m_cdb[4] = 0; m_cdb[5] = 0; m_cdb[6] = 0; m_cdb[7] = 0; m_cdb[8] = 0; m_cdb[9] = 0;
}

/* CLOSE TRACK/SESSION (0x5b), CDB len 10, NO_DATA.
 * pbuf: b0c == 0 -> cdb[2] = 1, else cdb[2] = 2; b0d = track/session number
 * (cdb[5]).
 */
void CScsiDriverBase::SetParamCloseSession(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiNoData;
	m_cdbLen = 10;
	m_cdb[0] = 0x5b;
	m_cdb[1] = 1;
	m_cdb[2] = (U8(pbuf, 0x0c) == 0) ? 1 : 2;
	m_cdb[3] = 0; m_cdb[4] = 0;
	m_cdb[5] = U8(pbuf, 0x0d);
	m_cdb[6] = 0; m_cdb[7] = 0; m_cdb[8] = 0; m_cdb[9] = 0;
}

/* BLANK (0xa1), CDB len 12, NO_DATA.
 * pbuf: cdb[1] = (b07<<4) | b06 (blanking type / Immed-shaped flags).
 */
void CScsiDriverBase::SetParamBlank(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiNoData;
	m_cdbLen = 12;
	m_cdb[0] = 0xa1;
	m_cdb[1] = static_cast<unsigned char>(((U8(pbuf, 0x07) << 4) | U8(pbuf, 0x06)) & 0xff);
	m_cdb[2] = 0; m_cdb[3] = 0; m_cdb[4] = 0; m_cdb[5] = 0; m_cdb[6] = 0;
	m_cdb[7] = 0; m_cdb[8] = 0; m_cdb[9] = 0; m_cdb[10] = 0; m_cdb[11] = 0;
}

/* READ TOC/PMA/ATIP (0x43), CDB len 10, DATA_IN.
 * pbuf: dw08=buffer ptr, w10=alloc length (cdb[7..8]), b0d*2=cdb[1] (MSF-shaped
 * bit), b0e=starting track/session (cdb[2]), b0c=format (cdb[6]).
 */
void CScsiDriverBase::SetParamReadTocAtip(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataIn;
	unsigned short w10 = U16(pbuf, 0x10);
	m_xferLen = w10;
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	m_cdbLen = 10;
	m_cdb[0] = 0x43;
	m_cdb[1] = static_cast<unsigned char>((U8(pbuf, 0x0d) * 2) & 0xff);
	m_cdb[2] = U8(pbuf, 0x0e);
	m_cdb[3] = 0; m_cdb[4] = 0; m_cdb[5] = 0;
	m_cdb[6] = U8(pbuf, 0x0c);
	m_cdb[7] = static_cast<unsigned char>((w10 >> 8) & 0xff);
	m_cdb[8] = static_cast<unsigned char>(w10 & 0xff);
	m_cdb[9] = 0;
}

/* PLAY AUDIO (12) (0xa5), CDB len 12, NO_DATA. m_dataPtr/m_xferLen untouched
 * (real).
 * pbuf: dw08=starting LBA (BE32, cdb[2..5]), dw0c=play length (BE32,
 * cdb[6..9]).
 */
void CScsiDriverBase::SetParamPlayAudio(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiNoData;
	unsigned long lba = U32(pbuf, 0x08);
	unsigned long len = U32(pbuf, 0x0c);
	m_cdbLen = 12;
	m_cdb[0] = 0xa5;
	m_cdb[1] = 0;
	m_cdb[2] = static_cast<unsigned char>((lba >> 24) & 0xff);
	m_cdb[3] = static_cast<unsigned char>((lba >> 16) & 0xff);
	m_cdb[4] = static_cast<unsigned char>((lba >> 8) & 0xff);
	m_cdb[5] = static_cast<unsigned char>(lba & 0xff);
	m_cdb[6] = static_cast<unsigned char>((len >> 24) & 0xff);
	m_cdb[7] = static_cast<unsigned char>((len >> 16) & 0xff);
	m_cdb[8] = static_cast<unsigned char>((len >> 8) & 0xff);
	m_cdb[9] = static_cast<unsigned char>(len & 0xff);
	m_cdb[10] = 0;
	m_cdb[11] = 0;
}

/* READ SUB-CHANNEL (0x42), CDB len 10, DATA_IN, fixed SUBQ/CURRENT_POSITION
 * request, fixed 16-byte alloc length.
 * pbuf: dw08=buffer ptr.
 */
void CScsiDriverBase::SetParamReadSub(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataIn;
	m_xferLen = 0x10;
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	m_cdbLen = 10;
	m_cdb[0] = 0x42;
	m_cdb[1] = 0;
	m_cdb[2] = 0x40;
	m_cdb[3] = 1;
	m_cdb[4] = 0; m_cdb[5] = 0; m_cdb[6] = 0; m_cdb[7] = 0;
	m_cdb[8] = 0x10;
	m_cdb[9] = 0;
}

/* READ CD (0xbe), CDB len 12, DATA_IN.
 * pbuf: dw08=buffer ptr, dw0c=starting LBA (BE32, cdb[2..5]), dw10=24-bit
 * transfer length (cdb[6..8]) with dw14=block count multiplied in for
 * m_xferLen, b18=sync/header/user-data flags (cdb[9]), b19=sub-channel
 * selection (cdb[10]).
 */
void CScsiDriverBase::SetParamReadCD(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataIn;
	m_xferLen = U32(pbuf, 0x14) * U32(pbuf, 0x10);
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	unsigned long lba = U32(pbuf, 0x0c);
	unsigned long len24 = U32(pbuf, 0x10);
	m_cdbLen = 12;
	m_cdb[0] = 0xbe;
	m_cdb[1] = 0;
	m_cdb[2] = static_cast<unsigned char>((lba >> 24) & 0xff);
	m_cdb[3] = static_cast<unsigned char>((lba >> 16) & 0xff);
	m_cdb[4] = static_cast<unsigned char>((lba >> 8) & 0xff);
	m_cdb[5] = static_cast<unsigned char>(lba & 0xff);
	m_cdb[6] = static_cast<unsigned char>((len24 >> 16) & 0xff);
	m_cdb[7] = static_cast<unsigned char>((len24 >> 8) & 0xff);
	m_cdb[8] = static_cast<unsigned char>(len24 & 0xff);
	m_cdb[9] = U8(pbuf, 0x18);
	m_cdb[10] = U8(pbuf, 0x19);
	m_cdb[11] = 0;
}

/* SET CD SPEED (0xbb), CDB len 12, NO_DATA. cdb[2..3]/[4..5] are only
 * written when the corresponding pbuf word is non-zero -- real conditional
 * skip, transcribed as-is (leaves m_cdb stale from a prior call otherwise).
 * pbuf: w06=read speed (cdb[2..3]), w08=write speed (cdb[4..5]).
 */
void CScsiDriverBase::SetParamSetSpeed(SDriverIOPbuf *pbuf)
{
	unsigned short w06 = U16(pbuf, 0x06);
	unsigned short w08 = U16(pbuf, 0x08);
	m_xferDir = kScsiNoData;
	m_cdbLen = 12;
	m_cdb[0] = 0xbb;
	m_cdb[1] = 0;
	if (w06 != 0) {
		m_cdb[2] = static_cast<unsigned char>((w06 >> 8) & 0xff);
		m_cdb[3] = static_cast<unsigned char>(w06 & 0xff);
	}
	if (w08 != 0) {
		m_cdb[4] = static_cast<unsigned char>((w08 >> 8) & 0xff);
		m_cdb[5] = static_cast<unsigned char>(w08 & 0xff);
	}
	m_cdb[6] = 0; m_cdb[7] = 0; m_cdb[8] = 0; m_cdb[9] = 0; m_cdb[10] = 0; m_cdb[11] = 0;
}

/* READ BUFFER CAPACITY (0x5c), CDB len 10, DATA_IN, fixed 12-byte alloc
 * length.
 * pbuf: dw08=buffer ptr.
 */
void CScsiDriverBase::SetParamReadBufferCapacity(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiDataIn;
	m_xferLen = 0x0c;
	m_dataPtr = reinterpret_cast<void *>(U32(pbuf, 0x08));
	m_cdbLen = 10;
	m_cdb[0] = 0x5c;
	m_cdb[1] = 0; m_cdb[2] = 0; m_cdb[3] = 0; m_cdb[4] = 0; m_cdb[5] = 0; m_cdb[6] = 0; m_cdb[7] = 0;
	m_cdb[8] = 0x0c;
	m_cdb[9] = 0;
}

/* START STOP UNIT (0x1b), CDB len 6, NO_DATA.
 * pbuf: b05&1 = cdb[1] (Immed), cdb[4] = (b06<<4) + (b07*2) + b08
 * (PowerCondition/LoEj/Start-shaped flags).
 */
void CScsiDriverBase::SetParamStartStop(SDriverIOPbuf *pbuf)
{
	m_xferDir = kScsiNoData;
	m_cdbLen = 6;
	m_cdb[0] = 0x1b;
	m_cdb[1] = static_cast<unsigned char>(U8(pbuf, 0x05) & 0x01);
	m_cdb[2] = 0; m_cdb[3] = 0;
	m_cdb[4] = static_cast<unsigned char>(
		((U8(pbuf, 0x06) << 4) + (U8(pbuf, 0x07) * 2) + U8(pbuf, 0x08)) & 0xff);
	m_cdb[5] = 0;
}

/* MODE SENSE (10) (0x5a) used as a write-protect probe, CDB len 10, DATA_IN,
 * fixed "all pages" (0x3f) request into sm_oDataBuf -- pbuf argument is real
 * but entirely unread.
 */
void CScsiDriverBase::SetParamCheckWriteProtect(SDriverIOPbuf * /*pbuf*/)
{
	m_xferDir = kScsiDataIn;
	m_xferLen = 0x0c;
	m_dataPtr = sm_oDataBuf;
	m_cdbLen = 10;
	m_cdb[0] = 0x5a;
	m_cdb[1] = 0x08;
	m_cdb[2] = 0x3f;
	m_cdb[3] = 0; m_cdb[4] = 0; m_cdb[5] = 0; m_cdb[6] = 0; m_cdb[7] = 0;
	m_cdb[8] = 0x0c;
	m_cdb[9] = 0;
}

/* opcode 0x1b (same opcode as SetParamStartStop -- a "sleep" variant of
 * START STOP UNIT, real, transcribed as-is), CDB len 6, NO_DATA.
 * pbuf: b05 == 0 -> cdb[4] = 1, else cdb[4] = 0.
 */
void CScsiDriverBase::SetParamSleep(SDriverIOPbuf *pbuf)
{
	m_cdbLen = 6;
	m_cdb[0] = 0x1b;
	m_cdb[1] = 0; m_cdb[2] = 0; m_cdb[3] = 0;
	m_xferDir = kScsiNoData;
	m_cdb[5] = 0;
	m_cdb[4] = (U8(pbuf, 0x05) == 0) ? 1 : 0;
}
