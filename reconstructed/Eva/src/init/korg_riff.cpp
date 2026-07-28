/*
 * korg_riff.cpp  -  CKorgRiff + CKorgRiff::CNameChunk. See include/korg_riff.h
 * for full ground-truth provenance (vtable slot layout, object layout, tag-
 * normalization direction, and the rejected/deferred-sibling writeup).
 */

#include "korg_riff.h"

#include <cstring>

namespace {

/* Portable equivalent of the real ground-truth `movbe` (hardware byte-swap
 * load/store) -- see korg_riff.h's own "TAG NORMALIZATION" note.
 */
inline unsigned int Bswap32(unsigned int v)
{
	return ((v & 0x000000ffu) << 24) |
	       ((v & 0x0000ff00u) << 8)  |
	       ((v & 0x00ff0000u) >> 8)  |
	       ((v & 0xff000000u) >> 24);
}

inline unsigned short Bswap16(unsigned short v)
{
	return (unsigned short)(((v & 0x00ffu) << 8) | ((v & 0xff00u) >> 8));
}

/* .text+0x08e4dnnn-shaped RIFF chunk header the real fread/fwrite calls both
 * use: 4-byte tag immediately followed by a 4-byte length, no padding.
 */
struct SChunkHeader {
	unsigned int tag;
	unsigned int len;
};

const unsigned int kNameTag = 0x4e414d45u; /* Bswap32(load_le("NAME")) */

} // namespace

void CKorgRiff::CNameChunk::GetName(char *dest, unsigned int /*maxLen*/) const
{
	strncpy(dest, mName, 0x19);
	dest[0x18] = 0;
}

void CKorgRiff::CNameChunk::SetName(const char *name)
{
	strncpy(mName, name, 0x18);
}

CKorgRiff::CKorgRiff(const char *name, const char *ext)
	: CKorgFile(name, ext)
{
	/* Real ground truth clears only mChunkName's own first byte here
	 * (`mov BYTE PTR [this+0x110],0x0`) -- NOT a full SetName("") (which
	 * would strncpy-zero-pad the whole 24-byte buffer instead).
	 */
	mChunkName.mName[0] = 0;
}

CKorgRiff::~CKorgRiff()
{
}

int CKorgRiff::ReadFile(FILE *file)
{
	int result = 0;

	for (;;) {
		SChunkHeader hdr;
		if (fread(&hdr, 8, 1, file) != 1)
			break;

		hdr.tag = Bswap32(hdr.tag);
		if (IsBigEndian())
			hdr.len = Bswap32(hdr.len);

		if (hdr.tag == kNameTag) {
			fread(&mChunkName, 0x18, 1, file);
		} else {
			result = ReadChunk(hdr.tag, hdr.len, file);
		}

		if (feof(file))
			break;
	}

	return result;
}

int CKorgRiff::WriteFile(FILE *file)
{
	SChunkHeader hdr;
	hdr.tag = 0x454d414eu; /* raw "NAME" bytes, natural on-disk order */
	hdr.len = 0x18;
	if (IsBigEndian())
		hdr.len = Bswap32(hdr.len);

	fwrite(&hdr, 8, 1, file);
	fwrite(&mChunkName, 0x18, 1, file);

	/* Real ground-truth trailing side effect -- copies mChunkName into a
	 * local scratch buffer that is never subsequently used. See file
	 * header's own "inert strncpy" note; preserved faithfully.
	 */
	char scratch[0x19];
	strncpy(scratch, reinterpret_cast<const char *>(&mChunkName), 0x19);

	return 0;
}

int CKorgRiff::ReadChunk(unsigned int /*id*/, unsigned int len, FILE *file)
{
	fseek(file, (long)len, SEEK_CUR);
	return 0;
}

bool CKorgRiff::IsBigEndian() const
{
	return false;
}

void CKorgRiff::WriteHeader(unsigned int id, unsigned int len, FILE *file)
{
	SChunkHeader hdr;
	hdr.len = len;
	hdr.tag = Bswap32(id);
	if (IsBigEndian())
		hdr.len = Bswap32(hdr.len);

	fwrite(&hdr, 8, 1, file);
}

void CKorgRiff::SwapFile(short &ref)
{
	if (IsBigEndian())
		ref = (short)Bswap16((unsigned short)ref);
}

void CKorgRiff::SwapFile(unsigned short &ref)
{
	if (IsBigEndian())
		ref = Bswap16(ref);
}

void CKorgRiff::SwapFile(unsigned int &ref)
{
	if (IsBigEndian())
		ref = Bswap32(ref);
}

void CKorgRiff::SwapLittleEndian(short & /*ref*/)
{
}

void CKorgRiff::SwapLittleEndian(unsigned short & /*ref*/)
{
}

void CKorgRiff::SwapLittleEndian(unsigned int & /*ref*/)
{
}

void CKorgRiff::SwapBigEndian(short &ref)
{
	ref = (short)Bswap16((unsigned short)ref);
}

void CKorgRiff::SwapBigEndian(unsigned short &ref)
{
	ref = Bswap16(ref);
}

void CKorgRiff::SwapBigEndian(unsigned int &ref)
{
	ref = Bswap32(ref);
}

void CKorgRiff::Swap(short &ref)
{
	ref = (short)Bswap16((unsigned short)ref);
}

void CKorgRiff::Swap(unsigned short &ref)
{
	ref = Bswap16(ref);
}

void CKorgRiff::Swap(unsigned int &ref)
{
	ref = Bswap32(ref);
}
