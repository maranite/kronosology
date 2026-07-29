/*
 * test_pcg_save_info.cpp  -  host-side known-answer test for CPcgSaveInfo's
 * round-53 13-method batch (solo, 2026-07-29). See include/pcg_save_info.h
 * for the full derivation and the deferred-item list.
 */
#include <cstdio>
#include <cstring>
#include "pcg_save_info.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

static unsigned short GetU16At(void *p, int off)
{
	unsigned short v;
	memcpy(&v, (unsigned char *)p + off, 2);
	return v;
}

// A zeroed CPcgSaveInfo-shaped buffer -- avoids memset-over-a-real-ctor
// (harmless here, since the class holds no vtable/pointers, but the buffer
// form matches this project's own convention elsewhere) and gives tests a
// pristine all-zero starting point distinct from the ctor's own nonzero
// defaults.
struct ZeroedInfo {
	unsigned char buf[0x28];
	ZeroedInfo() { memset(buf, 0, sizeof(buf)); }
	CPcgSaveInfo *get() { return reinterpret_cast<CPcgSaveInfo *>(buf); }
};

int main()
{
	CPcgSaveInfo info;

	check("ctor: prog mask (0,4) == (0xffff,0xf)",
	      GetU16At(&info, 0) == 0xffff && GetU16At(&info, 4) == 0xf);
	check("ctor: combi mask (8,0xc) == (0x3fff,0)",
	      GetU16At(&info, 8) == 0x3fff && GetU16At(&info, 0xc) == 0);
	check("ctor: dkit mask (0x10,0x14) == (0x7fff,0)",
	      GetU16At(&info, 0x10) == 0x7fff && GetU16At(&info, 0x14) == 0);
	check("ctor: wseq mask (0x18,0x1c) == (0x7fff,0)",
	      GetU16At(&info, 0x18) == 0x7fff && GetU16At(&info, 0x1c) == 0);

	check("HasNothingToSave: false right after ctor (masks are nonzero)",
	      info.HasNothingToSave() == false);

	{
		CPcgSaveInfo other;
		check("Compare: two fresh ctors are equal", info.Compare(other) == true);
		other.setsavedkitbank(0); // ctor's dkit mask is 0x7fff (bit0 already set) -> clearing it flips a bit
		check("Compare: differing dkit mask -> not equal", info.Compare(other) == false);
	}

	{
		ZeroedInfo target, mask;
		mask.get()->setsaveprogbank(1, 0, 0, 0, 0, 0); // starts all-zero -> ends with prog bit0 only
		target.get()->Clear(*mask.get());
		check("Clear: clears only the bits present in the mask arg (prog bit0)",
		      GetU16At(mask.buf, 0) == 1 && GetU16At(target.buf, 0) == 0);
	}

	{
		CPcgSaveInfo target;
		// Zero everything so HasNothingToSave can go true once flags clear too.
		target.Clear(target);
		check("Clear(self): all masks now zero",
		      GetU16At(&target, 0) == 0 && GetU16At(&target, 4) == 0 &&
		      GetU16At(&target, 8) == 0 && GetU16At(&target, 0xc) == 0 &&
		      GetU16At(&target, 0x10) == 0 && GetU16At(&target, 0x14) == 0 &&
		      GetU16At(&target, 0x18) == 0 && GetU16At(&target, 0x1c) == 0);
		check("HasNothingToSave: true once masks and flags are all zero",
		      target.HasNothingToSave() == true);
	}

	{
		ZeroedInfo p;
		p.get()->setsaveprogbank(1, 1, 0, 1, 0, 0);
		// b0!=0 -> bit0, b1!=0 -> bit1, b2==0 -> bit2 clear, b3!=0 -> bit3: 1+2+8=0xb
		check("setsaveprogbank: bit0+bit1+bit3 set (b0,b1,b3 nonzero)",
		      GetU16At(p.buf, 0) == 0x0b);
	}

	{
		ZeroedInfo p;
		p.get()->setsavecombibank(0, 0, 1, 0, 0, 0, 1);
		check("setsavecombibank: bit2 + bit6 set", GetU16At(p.buf, 8) == 0x44);
	}

	{
		ZeroedInfo p;
		p.get()->setsavedkitbank(1);
		check("setsavedkitbank: sets bit0 of +0x10", GetU16At(p.buf, 0x10) == 1);
		p.get()->setsavedkitbank(0);
		check("setsavedkitbank: clears bit0 of +0x10", GetU16At(p.buf, 0x10) == 0);
	}

	{
		ZeroedInfo p;
		p.get()->setsavewseqbank(1);
		check("setsavewseqbank: sets bit0 of +0x18", GetU16At(p.buf, 0x18) == 1);
	}

	{
		// b0=1,b1=1 (b1!=0, so the "if(b1==0)" branch is NOT taken):
		// u = (-(b0==0) & 0xffffffc0) + 0xc0 = 0 + 0xc0 = 0xc0
		ZeroedInfo p;
		p.get()->setsaveprogbank_exb(1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0);
		check("setsaveprogbank_exb: lo has u(0xc0) + b2(0x100)",
		      GetU16At(p.buf, 0) == (0xc0 | 0x100));
		check("setsaveprogbank_exb: hi bit0 set from b10", GetU16At(p.buf, 4) == 1);
	}
	{
		// b0=1,b1=0 -> takes the "if (b1==0)" branch: u = ~-(b0==0) & 0x40 = 0x40
		ZeroedInfo p;
		p.get()->setsaveprogbank_exb(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		check("setsaveprogbank_exb: b1==0 branch yields u=0x40 (b0!=0)",
		      GetU16At(p.buf, 0) == 0x40);
	}

	{
		ZeroedInfo p;
		p.get()->setsavecombibank_exb(1, 1, 1, 0, 0, 0, 0);
		// b0=1,b1=1 -> u = (-(0)&0xff80)+0x180 = 0x180; + b2(0x200)
		check("setsavecombibank_exb: u(0x180)+b2(0x200)", GetU16At(p.buf, 8) == (0x180 | 0x200));
	}

	{
		ZeroedInfo p;
		p.get()->setsavedkitbank_exb(1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		// b0=1,b1=1 -> u = (-(0)&0xfffe)+6 = 6
		check("setsavedkitbank_exb: u==6 when b0,b1 both nonzero", GetU16At(p.buf, 0x10) == 6);
		ZeroedInfo q;
		q.get()->setsavedkitbank_exb(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		// b0=1,b1=0 -> takes if-branch: u = ~-(0) & 2 = 2
		check("setsavedkitbank_exb: b1==0 branch yields u==2", GetU16At(q.buf, 0x10) == 2);
	}

	{
		ZeroedInfo p;
		p.get()->setsavewseqbank_exb(1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		check("setsavewseqbank_exb: u==6 when b0,b1 both nonzero", GetU16At(p.buf, 0x18) == 6);
	}

	{
		ZeroedInfo p;
		unsigned short raw = 0x1234;
		memcpy(p.buf, &raw, 2);
		p.get()->ProcessEndian();
		check("ProcessEndian: byte-swaps +0x00", GetU16At(p.buf, 0) == 0x3412);
	}

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
