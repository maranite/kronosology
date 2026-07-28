/*
 * test_scsi_driver_base.cpp  -  host-side known-answer test for CScsiDriverBase
 * (src/hw/scsi_driver_base.cpp, 2026-07-28 storage-cluster follow-up).
 *
 * Exercises a representative sample of the 35 reconstructed SetParamXxx CDB
 * builders: a no-pbuf-read fixed command (TestUnitReady), a variable-field
 * DATA_IN command (ReadCapacity's sibling GetEventStatusNtf), the READ(10)/
 * WRITE(10) direction+opcode branch, two confirmed real quirks (ReserveTrack's
 * m_dataPtr sourced from pbuf+0xc instead of the usual +0x8, and SetSpeed's
 * conditional per-word CDB-byte skip when the corresponding pbuf field is
 * zero), and ReadCD's 24-bit transfer-length field. A small test-only
 * subclass exposes the protected CDB/xfer fields for inspection -- same
 * pattern as accessing any other protected-member reconstruction under test.
 */

#include <cstdio>
#include <cstring>

#include "scsi_driver_base.h"

namespace {

class TestScsi : public CScsiDriverBase {
public:
	TestScsi() : CScsiDriverBase(0) {}
	using CScsiDriverBase::m_cdb;
	using CScsiDriverBase::m_cdbLen;
	using CScsiDriverBase::m_xferDir;
	using CScsiDriverBase::m_dataPtr;
	using CScsiDriverBase::m_xferLen;
};

SDriverIOPbuf MakePbuf()
{
	SDriverIOPbuf p;
	std::memset(&p, 0, sizeof p);
	return p;
}

void PutU32(SDriverIOPbuf &p, unsigned off, unsigned long v)
{
	std::memcpy(p.opaque + off, &v, sizeof v);
}

void PutU16(SDriverIOPbuf &p, unsigned off, unsigned short v)
{
	std::memcpy(p.opaque + off, &v, sizeof v);
}

} // namespace

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CScsiDriverBase known-answer test\n");
	printf("==================================\n");

	/* TestUnitReady: fixed all-zero CDB, len 6, NO_DATA, dataPtr ==
	 * sm_oDataBuf, pbuf argument entirely unread.
	 */
	{
		TestScsi d;
		SDriverIOPbuf p = MakePbuf();
		d.SetParamTestUnitReady(&p);
		check("TestUnitReady cdb[0]==0x00", d.m_cdb[0] == 0x00);
		check("TestUnitReady cdbLen==6", d.m_cdbLen == 6);
		check("TestUnitReady dir==NO_DATA(2)", d.m_xferDir == 2);
		check("TestUnitReady dataPtr==sm_oDataBuf", d.m_dataPtr == CScsiDriverBase::sm_oDataBuf);
	}

	/* Inquiry: fixed standard-inquiry CDB, 36-byte alloc length, response
	 * into sm_oDataBuf.
	 */
	{
		TestScsi d;
		SDriverIOPbuf p = MakePbuf();
		d.SetParamInquiry(&p);
		check("Inquiry cdb[0]==0x12", d.m_cdb[0] == 0x12);
		check("Inquiry cdb[4]==0x24 (alloc len)", d.m_cdb[4] == 0x24);
		check("Inquiry xferLen==0x24", d.m_xferLen == 0x24UL);
		check("Inquiry dir==DATA_IN(0)", d.m_xferDir == 0);
	}

	/* GetEventStatusNtf: BE16 alloc length split across cdb[7]/cdb[8],
	 * b0c/b0d passthrough fields.
	 */
	{
		TestScsi d;
		SDriverIOPbuf p = MakePbuf();
		PutU32(p, 0x08, 0xdeadbeefUL);          /* buffer ptr */
		PutU32(p, 0x10, 0x00000123UL);          /* alloc length */
		p.opaque[0x0c] = 0x01;                   /* polled flag -> cdb[1] */
		p.opaque[0x0d] = 0x10;                   /* notif class mask -> cdb[4] */
		d.SetParamGetEventStatusNtf(&p);
		check("GetEventStatusNtf cdb[0]==0x4a", d.m_cdb[0] == 0x4a);
		check("GetEventStatusNtf cdb[1]==0x01", d.m_cdb[1] == 0x01);
		check("GetEventStatusNtf cdb[4]==0x10", d.m_cdb[4] == 0x10);
		check("GetEventStatusNtf cdb[7]==0x01 (BE hi)", d.m_cdb[7] == 0x01);
		check("GetEventStatusNtf cdb[8]==0x23 (BE lo)", d.m_cdb[8] == 0x23);
		check("GetEventStatusNtf dataPtr==0xdeadbeef",
		      d.m_dataPtr == reinterpret_cast<void *>(0xdeadbeefUL));
		check("GetEventStatusNtf xferLen==0x123", d.m_xferLen == 0x123UL);
	}

	/* READ(10)/WRITE(10): direction+opcode branch on `isRead`, LBA BE32,
	 * block-count BE16, xferLen = blockSize * blockCount.
	 */
	{
		TestScsi d;
		SDriverIOPbuf p = MakePbuf();
		PutU32(p, 0x08, 0x12345678UL);   /* buffer ptr */
		PutU32(p, 0x0c, 2048UL);         /* block size */
		PutU32(p, 0x10, 0x01020304UL);   /* LBA */
		PutU32(p, 0x14, 5UL);            /* block count */

		d.SetParamReadWrite(&p, 1 /* isRead */);
		check("ReadWrite(read) cdb[0]==0x28", d.m_cdb[0] == 0x28);
		check("ReadWrite(read) dir==DATA_IN(0)", d.m_xferDir == 0);
		check("ReadWrite(read) cdb[2..5]==LBA BE32",
		      d.m_cdb[2] == 0x01 && d.m_cdb[3] == 0x02 && d.m_cdb[4] == 0x03 && d.m_cdb[5] == 0x04);
		check("ReadWrite(read) cdb[7..8]==blockCount BE16",
		      d.m_cdb[7] == 0x00 && d.m_cdb[8] == 0x05);
		check("ReadWrite(read) xferLen==10240", d.m_xferLen == 10240UL);

		d.SetParamReadWrite(&p, 0 /* !isRead -> write */);
		check("ReadWrite(write) cdb[0]==0x2a", d.m_cdb[0] == 0x2a);
		check("ReadWrite(write) dir==DATA_OUT(1)", d.m_xferDir == 1);
	}

	/* ReserveTrack: confirmed real quirk -- m_dataPtr is sourced from
	 * pbuf+0xc, not the usual pbuf+0x8.
	 */
	{
		TestScsi d;
		SDriverIOPbuf p = MakePbuf();
		PutU32(p, 0x08, 0x11223344UL);   /* -> cdb[5..8] BE32 */
		PutU32(p, 0x0c, 0xaabbccddUL);   /* -> m_dataPtr (NOT p+0x08) */
		d.SetParamReserveTrack(&p);
		check("ReserveTrack cdb[0]==0x53", d.m_cdb[0] == 0x53);
		check("ReserveTrack cdb[5..8]==0x11223344 BE",
		      d.m_cdb[5] == 0x11 && d.m_cdb[6] == 0x22 && d.m_cdb[7] == 0x33 && d.m_cdb[8] == 0x44);
		check("ReserveTrack dataPtr==pbuf+0xc value (0xaabbccdd), not pbuf+0x8",
		      d.m_dataPtr == reinterpret_cast<void *>(0xaabbccddUL));
	}

	/* SetSpeed: confirmed real quirk -- cdb[2..3]/cdb[4..5] are only
	 * overwritten when the corresponding pbuf word is non-zero.
	 */
	{
		TestScsi d;
		d.m_cdb[2] = 0xee;
		d.m_cdb[3] = 0xff;
		SDriverIOPbuf p = MakePbuf();
		PutU16(p, 0x06, 0);        /* read speed: zero -> cdb[2..3] left untouched */
		PutU16(p, 0x08, 0x1234);   /* write speed: non-zero -> cdb[4..5] written */
		d.SetParamSetSpeed(&p);
		check("SetSpeed cdb[0]==0xbb", d.m_cdb[0] == 0xbb);
		check("SetSpeed cdb[2..3] left stale (0xee,0xff) when pbuf word is zero",
		      d.m_cdb[2] == 0xee && d.m_cdb[3] == 0xff);
		check("SetSpeed cdb[4..5]==0x12,0x34 when pbuf word is non-zero",
		      d.m_cdb[4] == 0x12 && d.m_cdb[5] == 0x34);
	}

	/* ReadCD: 24-bit transfer length spread across cdb[6..8], sync/header
	 * and sub-channel selection bytes passthrough, xferLen = dw14 * dw10.
	 */
	{
		TestScsi d;
		SDriverIOPbuf p = MakePbuf();
		PutU32(p, 0x08, 0x99UL);         /* buffer ptr */
		PutU32(p, 0x0c, 0x00010203UL);   /* starting LBA */
		PutU32(p, 0x10, 0x00abcdefUL);   /* 24-bit transfer length */
		PutU32(p, 0x14, 2UL);            /* block-count multiplier */
		p.opaque[0x18] = 0xf8;            /* sync/header/user-data flags */
		p.opaque[0x19] = 0x02;            /* sub-channel selection */
		d.SetParamReadCD(&p);
		check("ReadCD cdb[0]==0xbe", d.m_cdb[0] == 0xbe);
		check("ReadCD cdb[2..5]==LBA BE32",
		      d.m_cdb[2] == 0x00 && d.m_cdb[3] == 0x01 && d.m_cdb[4] == 0x02 && d.m_cdb[5] == 0x03);
		check("ReadCD cdb[6..8]==24-bit length",
		      d.m_cdb[6] == 0xab && d.m_cdb[7] == 0xcd && d.m_cdb[8] == 0xef);
		check("ReadCD cdb[9]==sync/header flags", d.m_cdb[9] == 0xf8);
		check("ReadCD cdb[10]==sub-channel sel", d.m_cdb[10] == 0x02);
		check("ReadCD xferLen==2*0xabcdef", d.m_xferLen == 2UL * 0x00abcdefUL);
	}

	/* ReadFmtCapacity: confirmed real quirk -- m_cdbLen says 12 but only
	 * bytes 0..9 are ever written.
	 */
	{
		TestScsi d;
		d.m_cdb[10] = 0x77;
		d.m_cdb[11] = 0x88;
		SDriverIOPbuf p = MakePbuf();
		PutU16(p, 0x0c, 0x0008);
		d.SetParamReadFmtCapacity(&p);
		check("ReadFmtCapacity cdbLen==12 despite only 10 bytes written", d.m_cdbLen == 12);
		check("ReadFmtCapacity cdb[10..11] left stale (0x77,0x88)",
		      d.m_cdb[10] == 0x77 && d.m_cdb[11] == 0x88);
	}

	/* GetResultOfScsiCommandAsync: unconditional 0. */
	{
		TestScsi d;
		check("GetResultOfScsiCommandAsync()==0", d.GetResultOfScsiCommandAsync() == 0);
	}

	printf("\n%d checks failed\n", g_fail);
	return g_fail ? 1 : 0;
}
