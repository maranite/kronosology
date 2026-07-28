/*
 * test_ram_sample.cpp  -  host-side known-answer test for CRamSample,
 * CMultiSample and CRamSampleRelative (src/editor/ram_sample.cpp). See
 * include/ram_sample.h for full ground-truth provenance.
 *
 * Checks:
 *   [1] CRamSample plain accessor round trips (address family, FS, LoopTune).
 *   [2] CRamSample::GetBank()/SetBank() confirmed-dead-field behavior.
 *   [3] CRamSample flag bits: normalized (IsNotUse2ndStart/IsOneShot) vs.
 *       raw non-normalized (IsPlus12dB/IsReverse) return shapes, plus
 *       GetFlag()/SetFlag() whole-byte access.
 *   [4] CRamSample::GetStartAddress(int) 2nd-start selection logic.
 *   [5] CRamSample::Initialize() computed field values.
 *   [6] CRamSample::GetName() returns a stable, writable pointer.
 *   [7] CMultiSample accessors, including the low-byte-truncation quirk on
 *       SetTopOfRelative().
 *   [8] CRamSampleRelative accessor round trips, the GetTranspose()/GetPan()
 *       storage aliasing, and SetupAsSkipped()/IsSkipped().
 */

#include <cstdio>
#include <cstring>

#include "ram_sample.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	printf("CRamSample / CMultiSample / CRamSampleRelative known-answer test\n");
	printf("===================================================================\n");

	printf("[1] CRamSample plain accessor round trips\n");
	{
		CRamSample s;
		s.SetFS(44100);
		check("GetFS round trip", s.GetFS() == 44100);

		s.SetLoopTune(37);
		check("GetLoopTune round trip", s.GetLoopTune() == 37);

		s.SetStartAddress(0x1000);
		s.Set2ndStartAddress(0x2000);
		s.SetLoopStartAddress(0x3000);
		s.SetEndAddress(0x4000);
		s.SetTopAddress(0x5000);
		s.SetNumOfByte(0x6000);
		check("address family round trip",
			s.GetStartAddress() == 0x1000 && s.Get2ndStartAddress() == 0x2000 &&
			s.GetLoopStartAddress() == 0x3000 && s.GetEndAddress() == 0x4000 &&
			s.GetTopAddress() == 0x5000 && s.GetNumOfByte() == 0x6000);
	}

	printf("[2] CRamSample::GetBank()/SetBank() confirmed-dead-field\n");
	{
		CRamSample s;
		s.SetBank(5);
		check("GetBank() always 0, SetBank() is a real no-op", s.GetBank() == 0);
	}

	printf("[3] CRamSample flag bits: normalized vs raw\n");
	{
		CRamSample s;
		s.SetFlag(0);
		check("IsNotUse2ndStart false at 0", !s.IsNotUse2ndStart());
		s.SetNotUse2ndStart(1);
		check("IsNotUse2ndStart normalized to true (0x04 set)", s.IsNotUse2ndStart() && s.GetFlag() == 0x04);

		s.SetFlag(0);
		s.SetPlus12dB(1);
		check("IsPlus12dB() returns RAW 0x08, not 1", s.IsPlus12dB() == 0x08);
		s.SetPlus12dB(0);
		check("IsPlus12dB() returns 0 when cleared", s.IsPlus12dB() == 0);

		s.SetFlag(0x20);
		check("IsReverse() returns RAW 0x20, not 1", s.IsReverse() == 0x20);

		s.SetFlag(0x80);
		check("IsOneShot() normalized to bool true at bit7", s.IsOneShot());
		s.SetFlag(0x00);
		check("IsOneShot() false when bit7 clear", !s.IsOneShot());

		s.SetFlag(0xd2);
		check("GetFlag() whole-byte read", s.GetFlag() == 0xd2);
	}

	printf("[4] CRamSample::GetStartAddress(int) 2nd-start selection\n");
	{
		CRamSample s;
		s.SetStartAddress(0x100);
		s.Set2ndStartAddress(0x200);

		s.SetFlag(0x00); /* NotUse2ndStart bit clear -> 2nd start considered in use */
		check("flag=0,NotUse2nd clear -> GetStartAddress(0) returns regular start",
			s.GetStartAddress(0) == 0x100);
		check("flag=1,NotUse2nd clear -> GetStartAddress(1) returns 2nd start",
			s.GetStartAddress(1) == 0x200);

		s.SetFlag(0x04); /* NotUse2ndStart bit set -> always regular start */
		check("flag=1,NotUse2nd set -> GetStartAddress(1) returns regular start",
			s.GetStartAddress(1) == 0x100);
	}

	printf("[5] CRamSample::Initialize() computed fields\n");
	{
		CRamSample s;
		s.Initialize(0 /* dead param */, 0x1000, 0x40);
		check("Initialize start/2nd/loop start all = startAddr",
			s.GetStartAddress() == 0x1000 && s.Get2ndStartAddress() == 0x1000 &&
			s.GetLoopStartAddress() == 0x1000);
		check("Initialize end = start+numBytes-1", s.GetEndAddress() == 0x1000 + 0x40 - 1);
		check("Initialize top = start*2", s.GetTopAddress() == 0x2000);
		check("Initialize numOfByte = numBytes*2", s.GetNumOfByte() == 0x80);
		check("Initialize flags = 0xd2", s.GetFlag() == 0xd2);
		/* 0xd2 = 1101 0010b: bit7(OneShot)=1, bit5(Reverse)=0, bit3(Plus12dB)=0,
		 * bit2(NotUse2ndStart)=0; bits 1 and 6 are set but have no accessor
		 * anywhere in this class -- ground truth's own literal, not modeled
		 * further. */
		check("Initialize -> IsPlus12dB raw 0 (bit3 clear in 0xd2)", s.IsPlus12dB() == 0);
		check("Initialize -> IsReverse raw 0 (bit5 clear in 0xd2)", s.IsReverse() == 0);
		check("Initialize -> IsOneShot true", s.IsOneShot());
		check("Initialize -> IsNotUse2ndStart false", !s.IsNotUse2ndStart());
	}

	printf("[6] CRamSample::GetName() stable writable pointer\n");
	{
		CRamSample s;
		char *n = s.GetName();
		std::strncpy(n, "TestSample", 10);
		check("GetName() round trip through raw buffer", std::strncmp(s.GetName(), "TestSample", 10) == 0);
	}

	printf("[7] CMultiSample accessors\n");
	{
		CMultiSample m;
		m.SetTopOfRelative(0x1234);
		check("SetTopOfRelative() truncates to low byte only", m.GetTopOfRelative() == 0x34);

		m.SetTopOfRelative(200);
		check("SetTopOfRelative() in-range value preserved", m.GetTopOfRelative() == 200);

		m.SetNumOfRelative(12);
		check("GetNumOfRelative round trip", m.GetNumOfRelative() == 12);

		m.SetFlag(0);
		check("IsNotUse2ndStart false at 0", !m.IsNotUse2ndStart());
		m.SetFlag(0x01);
		check("IsNotUse2ndStart true at bit0", m.IsNotUse2ndStart());

		char *n = m.GetName();
		std::strncpy(n, "MS", 2);
		check("GetName() writable pointer", std::strncmp(m.GetName(), "MS", 2) == 0);
	}

	printf("[8] CRamSampleRelative accessors + Transpose/Pan aliasing + Skip\n");
	{
		CRamSampleRelative r;
		r.SetSampleNumber(1234);
		r.SetSampleBank(3);
		r.SetTopKey(0x60);
		r.SetOriginalKey(0x3c);
		r.SetTune(10);
		r.SetLevel(99);
		r.SetCutoff(-5);
		r.SetResonance(20);
		r.SetAttack(-30);
		r.SetDecay(40);
		check("plain field round trips",
			r.GetSampleNumber() == 1234 && r.GetSampleBank() == 3 && r.GetTopKey() == 0x60 &&
			r.GetOriginalKey() == 0x3c && r.GetTune() == 10 && r.GetLevel() == 99 &&
			r.GetCutoff() == -5 && r.GetResonance() == 20 && r.GetAttack() == -30 &&
			r.GetDecay() == 40);

		r.SetTranspose(200); /* unsigned view: 200 fits a byte */
		check("GetTranspose() zero-extended read", r.GetTranspose() == 200);
		check("GetPan() sign-extended read of the SAME byte SetTranspose wrote",
			r.GetPan() == (signed char)200);

		r.SetPan(-10);
		check("GetPan() round trip", r.GetPan() == -10);
		check("GetTranspose() reads the SAME byte SetPan just wrote (aliasing)",
			r.GetTranspose() == (unsigned char)(signed char)-10);

		check("not skipped by default construction (arbitrary sample number)", !r.IsSkipped());
		r.SetupAsSkipped();
		check("IsSkipped() true after SetupAsSkipped()", r.IsSkipped());
		check("SetupAsSkipped() sets sample number to 0xffff", r.GetSampleNumber() == 0xffff);
	}

	printf("\n%s\n", g_fail ? "FAILED" : "all checks passed");
	return g_fail ? 1 : 0;
}
