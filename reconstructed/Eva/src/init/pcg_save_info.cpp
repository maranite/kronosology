/*
 * pcg_save_info.cpp  -  see pcg_save_info.h for the full derivation and the
 * deferred-item list. All field access goes through raw byte offsets on
 * `this` (opaque-past-what's-needed convention) since the header only
 * names the 3 real flag bytes and the 4-byte tail field individually.
 */
#include <cstring>

#include "pcg_save_info.h"

static inline unsigned short GetU16(void *base, int off)
{
	unsigned short v;
	memcpy(&v, (unsigned char *)base + off, 2);
	return v;
}

static inline void SetU16(void *base, int off, unsigned short v)
{
	memcpy((unsigned char *)base + off, &v, 2);
}

CPcgSaveInfo::CPcgSaveInfo()
{
	SetU16(this, 2, 0x15);
	SetU16(this, 0xa, 0xe);
	SetU16(this, 0xc, 0);
	SetU16(this, 0x12, 0xf);
	SetU16(this, 0x14, 0);
	SetU16(this, 0x1a, 0xf);
	SetU16(this, 0x1c, 0);
	_unknown_24 = 1;
	SetU16(this, 0, 0xffff);
	SetU16(this, 4, 0xf);
	SetU16(this, 8, 0x3fff);
	SetU16(this, 0x10, 0x7fff);
	SetU16(this, 0x18, 0x7fff);
	flagC = 1;
	flagA = 1;
	flagB = 1;
}

void CPcgSaveInfo::Clear(CPcgSaveInfo &other)
{
	unsigned short v;
	v = GetU16(&other, 0);
	SetU16(this, 4, GetU16(this, 4) & (unsigned short)~GetU16(&other, 4));
	SetU16(this, 0, GetU16(this, 0) & (unsigned short)~v);
	v = GetU16(&other, 8);
	SetU16(this, 0xc, GetU16(this, 0xc) & (unsigned short)~GetU16(&other, 0xc));
	SetU16(this, 8, GetU16(this, 8) & (unsigned short)~v);
	v = GetU16(&other, 0x10);
	SetU16(this, 0x14, GetU16(this, 0x14) & (unsigned short)~GetU16(&other, 0x14));
	SetU16(this, 0x10, GetU16(this, 0x10) & (unsigned short)~v);
	v = GetU16(&other, 0x18);
	SetU16(this, 0x1c, GetU16(this, 0x1c) & (unsigned short)~GetU16(&other, 0x1c));
	SetU16(this, 0x18, GetU16(this, 0x18) & (unsigned short)~v);

	if (other.flagA != 0)
		flagA = 0;
	if (other.flagB != 0)
		flagB = 0;
	if (other.flagC != 0)
		flagC = 0;
}

bool CPcgSaveInfo::HasNothingToSave() const
{
	if (GetU16((void *)this, 0) == 0 && GetU16((void *)this, 4) == 0 &&
	    GetU16((void *)this, 8) == 0 && GetU16((void *)this, 0xc) == 0 &&
	    GetU16((void *)this, 0x10) == 0 && GetU16((void *)this, 0x14) == 0 &&
	    GetU16((void *)this, 0x18) == 0 && GetU16((void *)this, 0x1c) == 0 &&
	    flagA == 0 && flagB == 0) {
		return flagC == 0;
	}
	return false;
}

bool CPcgSaveInfo::Compare(CPcgSaveInfo &other) const
{
	if (GetU16((void *)this, 0) == GetU16(&other, 0) &&
	    GetU16((void *)this, 4) == GetU16(&other, 4) &&
	    GetU16((void *)this, 8) == GetU16(&other, 8) &&
	    GetU16((void *)this, 0xc) == GetU16(&other, 0xc) &&
	    GetU16((void *)this, 0x10) == GetU16(&other, 0x10) &&
	    GetU16((void *)this, 0x14) == GetU16(&other, 0x14) &&
	    GetU16((void *)this, 0x18) == GetU16(&other, 0x18) &&
	    GetU16((void *)this, 0x1c) == GetU16(&other, 0x1c)) {
		unsigned int mine, theirs;
		memcpy(&mine, (const unsigned char *)this + 0x20, 4);
		memcpy(&theirs, (const unsigned char *)&other + 0x20, 4);
		return (mine & 0xff00ffff) == (theirs & 0xff00ffff);
	}
	return false;
}

void CPcgSaveInfo::ProcessEndian()
{
	for (int off = 0; off <= 0x1e; off += 2) {
		unsigned short v = GetU16(this, off);
		SetU16(this, off, (unsigned short)((v >> 8) | (v << 8)));
	}
	unsigned short v = (unsigned short)_unknown_24;
	_unknown_24 = (unsigned short)((v >> 8) | ((v & 0xff) << 8));
}

void CPcgSaveInfo::setsaveprogbank(int b0, int b1, int b2, int b3, int b4, int b5)
{
	unsigned short v = (unsigned short)(~-(unsigned short)(b1 == 0) & 2) |
	                    (unsigned short)(b0 != 0) |
	                    (unsigned short)(~-(unsigned short)(b2 == 0) & 4) |
	                    (unsigned short)(~-(unsigned short)(b3 == 0) & 8) |
	                    (unsigned short)(~-(unsigned short)(b4 == 0) & 0x10) |
	                    (unsigned short)(~-(unsigned short)(b5 == 0) & 0x20) |
	                    (GetU16(this, 0) & 0xffc0);
	SetU16(this, 0, v);
}

void CPcgSaveInfo::setsavecombibank(int b0, int b1, int b2, int b3, int b4, int b5, int b6)
{
	unsigned short v = (unsigned short)(~-(unsigned short)(b1 == 0) & 2) |
	                    (unsigned short)(b0 != 0) |
	                    (unsigned short)(~-(unsigned short)(b2 == 0) & 4) |
	                    (unsigned short)(~-(unsigned short)(b3 == 0) & 8) |
	                    (unsigned short)(~-(unsigned short)(b4 == 0) & 0x10) |
	                    (unsigned short)(~-(unsigned short)(b5 == 0) & 0x20) |
	                    (unsigned short)(~-(unsigned short)(b6 == 0) & 0x40) |
	                    (GetU16(this, 8) & 0xff80);
	SetU16(this, 8, v);
}

void CPcgSaveInfo::setsavedkitbank(int enable)
{
	SetU16(this, 0x10, (unsigned short)(enable != 0) | (GetU16(this, 0x10) & 0xfffe));
}

void CPcgSaveInfo::setsavewseqbank(int enable)
{
	SetU16(this, 0x18, (unsigned short)(enable != 0) | (GetU16(this, 0x18) & 0xfffe));
}

void CPcgSaveInfo::setsaveprogbank_exb(int b0, int b1, int b2, int b3, int b4, int b5, int b6,
                                        int b7, int b8, int b9, int b10, int b11, int b12, int b13)
{
	unsigned int u = (unsigned int)(-(unsigned int)(b0 == 0) & 0xffffffc0u) + 0xc0u;
	if (b1 == 0)
		u = (unsigned int)(~-(unsigned int)(b0 == 0)) & 0x40u;

	unsigned short lo = (unsigned short)(GetU16(this, 0) & 0x3f) |
	                     (unsigned short)(~-(unsigned short)(b2 == 0) & 0x100) |
	                     (unsigned short)u |
	                     (unsigned short)(~-(unsigned short)(b3 == 0) & 0x200) |
	                     (unsigned short)(~-(unsigned short)(b4 == 0) & 0x400) |
	                     (unsigned short)(~-(unsigned short)(b5 == 0) & 0x800) |
	                     (unsigned short)(~-(unsigned short)(b6 == 0) & 0x1000) |
	                     (unsigned short)(~-(unsigned short)(b7 == 0) & 0x2000) |
	                     (unsigned short)(~-(unsigned short)(b8 == 0) & 0x4000) |
	                     (unsigned short)(~-(unsigned short)(b9 == 0) & 0x8000);
	SetU16(this, 0, lo);

	unsigned short hi = (unsigned short)(GetU16(this, 4) & 0xfff0) |
	                     (unsigned short)(u >> 0x10) |
	                     (unsigned short)(~(unsigned short)(-(unsigned int)(b10 == 0) >> 0x10) & 1) |
	                     (unsigned short)(~(unsigned short)(-(unsigned int)(b11 == 0) >> 0x10) & 2) |
	                     (unsigned short)(~(unsigned short)(-(unsigned int)(b12 == 0) >> 0x10) & 4) |
	                     (unsigned short)(~(unsigned short)(-(unsigned int)(b13 == 0) >> 0x10) & 8);
	SetU16(this, 4, hi);
}

void CPcgSaveInfo::setsavecombibank_exb(int b0, int b1, int b2, int b3, int b4, int b5, int b6)
{
	unsigned short u = (unsigned short)(-(unsigned short)(b0 == 0) & 0xff80) + 0x180;
	if (b1 == 0)
		u = (unsigned short)(~-(unsigned short)(b0 == 0)) & 0x80;

	unsigned short v = (unsigned short)(~-(unsigned short)(b2 == 0) & 0x200) | u |
	                    (unsigned short)(~-(unsigned short)(b3 == 0) & 0x400) |
	                    (unsigned short)(~-(unsigned short)(b4 == 0) & 0x800) |
	                    (unsigned short)(~-(unsigned short)(b5 == 0) & 0x1000) |
	                    (unsigned short)(~-(unsigned short)(b6 == 0) & 0x2000) |
	                    (GetU16(this, 8) & 0xc07f);
	SetU16(this, 8, v);
}

void CPcgSaveInfo::setsavedkitbank_exb(int b0, int b1, int b2, int b3, int b4, int b5, int b6,
                                        int b7, int b8, int b9, int b10, int b11, int b12, int b13)
{
	unsigned short u = (unsigned short)(-(unsigned short)(b0 == 0) & 0xfffe) + 6;
	if (b1 == 0)
		u = (unsigned short)(~-(unsigned short)(b0 == 0)) & 2;

	unsigned short v = (unsigned short)(~-(unsigned short)(b2 == 0) & 8) | u |
	                    (unsigned short)(~-(unsigned short)(b3 == 0) & 0x10) |
	                    (unsigned short)(~-(unsigned short)(b4 == 0) & 0x20) |
	                    (unsigned short)(~-(unsigned short)(b5 == 0) & 0x40) |
	                    (unsigned short)(~-(unsigned short)(b6 == 0) & 0x80) |
	                    (unsigned short)(~-(unsigned short)(b7 == 0) & 0x100) |
	                    (unsigned short)(~-(unsigned short)(b8 == 0) & 0x200) |
	                    (unsigned short)(~-(unsigned short)(b9 == 0) & 0x400) |
	                    (unsigned short)(~-(unsigned short)(b10 == 0) & 0x800) |
	                    (unsigned short)(~-(unsigned short)(b11 == 0) & 0x1000) |
	                    (unsigned short)(~-(unsigned short)(b12 == 0) & 0x2000) |
	                    (unsigned short)(~-(unsigned short)(b13 == 0) & 0x4000) |
	                    (GetU16(this, 0x10) & 0x8001);
	SetU16(this, 0x10, v);
}

void CPcgSaveInfo::setsavewseqbank_exb(int b0, int b1, int b2, int b3, int b4, int b5, int b6,
                                        int b7, int b8, int b9, int b10, int b11, int b12, int b13)
{
	unsigned short u = (unsigned short)(-(unsigned short)(b0 == 0) & 0xfffe) + 6;
	if (b1 == 0)
		u = (unsigned short)(~-(unsigned short)(b0 == 0)) & 2;

	unsigned short v = (unsigned short)(~-(unsigned short)(b2 == 0) & 8) | u |
	                    (unsigned short)(~-(unsigned short)(b3 == 0) & 0x10) |
	                    (unsigned short)(~-(unsigned short)(b4 == 0) & 0x20) |
	                    (unsigned short)(~-(unsigned short)(b5 == 0) & 0x40) |
	                    (unsigned short)(~-(unsigned short)(b6 == 0) & 0x80) |
	                    (unsigned short)(~-(unsigned short)(b7 == 0) & 0x100) |
	                    (unsigned short)(~-(unsigned short)(b8 == 0) & 0x200) |
	                    (unsigned short)(~-(unsigned short)(b9 == 0) & 0x400) |
	                    (unsigned short)(~-(unsigned short)(b10 == 0) & 0x800) |
	                    (unsigned short)(~-(unsigned short)(b11 == 0) & 0x1000) |
	                    (unsigned short)(~-(unsigned short)(b12 == 0) & 0x2000) |
	                    (unsigned short)(~-(unsigned short)(b13 == 0) & 0x4000) |
	                    (GetU16(this, 0x18) & 0x8001);
	SetU16(this, 0x18, v);
}
