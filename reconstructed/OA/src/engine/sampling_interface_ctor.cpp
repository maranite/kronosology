// SPDX-License-Identifier: GPL-2.0
/*
 * sampling_interface_ctor.cpp  -  CSTGSamplingInterface::CSTGSamplingInterface()
 * (sec 10.160, `.text+0x4a6f0`, 910 bytes). Placement-constructed at
 * `CSTGGlobal+0x98` by CSTGGlobal::CSTGGlobal() (see global_ctor.cpp,
 * step 4) -- currently still calling the empty stub body prior to this
 * pass.
 *
 * Ground-truthed via a full objdump -d -r disassembly: a completely
 * straight-line sequence (NO branches, NO calls of any kind) -- the
 * safest possible category of ctor to reconstruct, since there is no
 * control flow or external dependency to get wrong, only careful
 * transcription. Confirmed via `readelf -r`: only two relocations in
 * the whole function --
 *   +0x0   vtable pointer install (`_ZTV21CSTGSamplingInterface + 8`,
 *          standard Itanium primary-vtable-pointer pattern; the real
 *          class has a genuine ~0x60-byte vtable of GetNumParams()/
 *          GetValueGetters()/GetMessageHandlers()/GetParamDescriptors()/
 *          dtor-style slots matching the same "ParamsOwner" message-
 *          handler shape as CSTGAudioInput/CSTGControllerInfo, none of
 *          which are dispatched through by this ctor itself -- safe
 *          per the sec 10.153 "install vs dispatch" rule).
 *   +0x98b sInstance = this (CSTGSamplingInterface::sInstance).
 * Every other field write is either a plain immediate (0, 0xffffffff,
 * or a handful of confirmed non-zero defaults) or a byte-wise
 * read-modify-write of the confirmed shape `(orig | 1) & 0xe1` (set
 * bit0, clear bits1-4, preserve the rest) -- the SAME RMW idiom already
 * established for CSTGProgramSlot's own ctor (sec 10.153), applied here
 * at three independent byte offsets (+0x55/+0x150/+0x24b).
 *
 * +0x448 = 0xbb80 (48000, a sample rate) exactly matches the same
 * literal already confirmed for CSTGAudioEvent::CSTGAudioEvent()'s own
 * `sampleRate` field (sec 10.149) -- an independent cross-check that
 * this transcription's constant is real, not a misread immediate.
 *
 * +0x4 is a lone byte write, confirmed real but positioned late in the
 * real instruction stream (right after +0x320, well after the vtable
 * install at +0x0..+0x3) -- reproduced here in its own real relative
 * order like every other field, since nothing else in this ctor
 * aliases it and reordering is purely cosmetic.
 */

#include "oa_global.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

CSTGSamplingInterface *CSTGSamplingInterface::sInstance;

extern "C" unsigned char _ZTV21CSTGSamplingInterface[0x60];
unsigned char _ZTV21CSTGSamplingInterface[0x60];

static unsigned int ToU32(void *p)
{
	return (unsigned int)(unsigned long)p;
}

CSTGSamplingInterface::CSTGSamplingInterface()
{
	unsigned char *base = (unsigned char *)this;

	*(unsigned int *)base = ToU32(_ZTV21CSTGSamplingInterface + 8);

	base[0x55] = (unsigned char)((base[0x55] | 1) & 0xe1);

	*(unsigned int *)(base + 0x18) = 0;
	*(unsigned int *)(base + 0x1c) = 0;
	for (unsigned int off = 0x20; off <= 0x50; off += 4)
		*(unsigned int *)(base + off) = 0;
	base[0x54] = 0;
	*(unsigned short *)(base + 0x56) = 0;

	*(unsigned int *)(base + 0xa0) = 0xffffffff;

	*(unsigned int *)(base + 0x113) = 0;
	*(unsigned int *)(base + 0x117) = 0;
	*(unsigned int *)(base + 0x11b) = 0;
	*(unsigned int *)(base + 0x11f) = 0;
	*(unsigned int *)(base + 0x123) = 0;
	*(unsigned int *)(base + 0x127) = 0;
	*(unsigned int *)(base + 0x12b) = 0;
	*(unsigned int *)(base + 0x12f) = 0;
	*(unsigned int *)(base + 0x133) = 0;
	*(unsigned int *)(base + 0x137) = 0;
	*(unsigned int *)(base + 0x13b) = 0;
	*(unsigned int *)(base + 0x13f) = 0;
	*(unsigned int *)(base + 0x143) = 0;
	*(unsigned int *)(base + 0x147) = 0;

	base[0x150] = (unsigned char)((base[0x150] | 1) & 0xe1);
	*(unsigned int *)(base + 0x14b) = 0;
	base[0x14f] = 0;
	*(unsigned short *)(base + 0x151) = 0;

	*(unsigned int *)(base + 0x19b) = 0xffffffff;

	base[0x24b] = (unsigned char)((base[0x24b] | 1) & 0xe1);
	*(unsigned int *)(base + 0x20e) = 0;
	*(unsigned int *)(base + 0x212) = 0;
	*(unsigned int *)(base + 0x216) = 0;
	*(unsigned int *)(base + 0x21a) = 0;
	*(unsigned int *)(base + 0x21e) = 0;
	*(unsigned int *)(base + 0x222) = 0;
	*(unsigned int *)(base + 0x226) = 0;
	*(unsigned int *)(base + 0x22a) = 0;
	*(unsigned int *)(base + 0x22e) = 0;
	*(unsigned int *)(base + 0x232) = 0;
	*(unsigned int *)(base + 0x236) = 0;
	*(unsigned int *)(base + 0x23a) = 0;
	*(unsigned int *)(base + 0x23e) = 0;
	*(unsigned int *)(base + 0x242) = 0;
	*(unsigned int *)(base + 0x246) = 0;
	base[0x24a] = 0;
	*(unsigned short *)(base + 0x24c) = 0;

	*(unsigned int *)(base + 0x296) = 0xffffffff;
	*(unsigned short *)(base + 0x309) = 0xffff;
	*(unsigned short *)(base + 0x30b) = 0;
	*(unsigned int *)(base + 0x30d) = 0;

	base[0x311] = 0x3c;
	base[0x312] = 0x3c;
	base[0x313] = 0;
	base[0x314] = 0x40;
	base[0x31f] &= 0xfe;
	base[0x315] = 0;
	base[0x316] = 0;
	base[0x317] = 0;
	base[0x318] = 0;
	base[0x319] = 0;
	base[0x31a] = 0;
	base[0x31b] = 0;
	base[0x31c] = 0;
	base[0x31d] = 0;
	base[0x31e] = 0;

	base[0x43c] = 0;
	base[0x43d] = 0;

	CSTGSamplingInterface::sInstance = this;

	base[0x323] = 0;
	base[0x324] = 0;
	*(unsigned short *)(base + 0x326) = 0;
	*(unsigned short *)(base + 0x328) = 0;
	*(unsigned int *)(base + 0x338) = 0;
	*(unsigned int *)(base + 0x32c) = 0;
	*(unsigned int *)(base + 0x334) = 0;
	*(unsigned int *)(base + 0x330) = 0;
	base[0x33c] = 0;
	base[0x441] = 0;
	base[0x320] = 0;
	base[0x4] = 0;
	base[0x444] = 0;
	base[0x322] = 0xff;
	base[0x445] = 0xff;
	*(unsigned short *)(base + 0x44c) = 0xffff;
	*(unsigned int *)(base + 0x448) = 0xbb80;
	*(unsigned int *)(base + 0x45c) = 0;
	*(unsigned int *)(base + 0x454) = 0;
	*(unsigned int *)(base + 0x464) = 0;
	*(unsigned int *)(base + 0x460) = 0;
	base[0x446] = 0;
	base[0x468] = 0;
	base[0x442] = 0;
	base[0x56c] = 0;
	base[0x43e] = 1;
	*(unsigned short *)(base + 0x568) = 0;
	*(unsigned short *)(base + 0x56a) = 0x24b;
	base[0x56d] = 2;
}
