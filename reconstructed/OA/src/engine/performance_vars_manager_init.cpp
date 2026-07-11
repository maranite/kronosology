// SPDX-License-Identifier: GPL-2.0
/*
 * performance_vars_manager_init.cpp  -  CSTGPerformanceVarsManager::
 * Initialize() (batch 53, `.text+0xb9f10`, 1431 bytes).
 *
 * Reached from init_module()'s own transitive call graph: setup_global_
 * resources() -> CSTGGlobal::Initialize() -> ((CSTGPerformanceVarsManager*)
 * &CSTGPerformanceVarsManager::sInstance)->Initialize() (the SAME "address
 * of the singleton" idiom already established for this class, sec
 * 10.55/10.56/10.71) -- init_module() step 8.
 *
 * Ground-truthed via a full objdump -d -r --start-address=0xb9f10
 * --stop-address=0xba4a7 disassembly plus relocation resolution.
 *
 * Confirmed shape: a `for (i = 0; i < 2; i++)` outer loop, each iteration
 * allocating one fresh `0xb6d0`-byte object via `CSTGBankMemory::
 * AllocAligned(0xb6d0, 0x40)` -- the SAME object other already-reconstructed
 * code in this project calls "the active CSTGPerformanceVarsManager"/`mgr`
 * (`ResolveActivePerformanceVarsManagerRaw()`'s own return value), whose
 * real C++ type -- per its own mangled demangled argument name in the
 * `CSTGEffectRackVars::Initialize(CSTGPerformanceVars*)` relocation below --
 * is `CSTGPerformanceVars` (a DIFFERENT class from `CSTGPerformanceVarsManager`
 * itself, matching oa_engine_init.h's own pre-existing note on the two
 * classes' relationship). Referred to as `mgr` throughout this file.
 *
 * Per-`mgr` confirmed field layout (raw offset arithmetic -- `CSTGPerformanceVars`
 * has no modeled fields elsewhere in this project, only methods, so this
 * stays consistent with that convention rather than inventing a struct):
 *
 *   +0x000  placement-constructed `CSTGAudioInputMixer` (a thin derived
 *           class over the already-real `CSTGAudioInputMixerBase`, 0x10
 *           bytes of own fields -- see oa_global.h). Immediately after
 *           construction, `+0x0` is unconditionally overwritten with the
 *           literal `8` -- confirmed real, preserved verbatim (whatever
 *           the deferred ctor itself would have written there is
 *           immediately clobbered either way).
 *   +0x020  12 embedded "channel mixer state" sub-objects, `0x210`-byte
 *           stride (ending at `+0x1900`).
 *   +0x18e0 2 more, `0x170`-byte stride (OVERLAPS the tail of the 12-entry
 *           array above by design -- confirmed real, a genuine
 *           record-field-overlap, not a transcription error; see this
 *           project's own established "record-field-overlap" gotcha).
 *   +0x1bc0 2 more, `0x150`-byte stride.
 *   +0x1e60 a 16-entry pointer table (`0x40` bytes) indexing the 16
 *           sub-objects above (12 + 2 + 2) in order -- populated but
 *           never itself READ anywhere within Initialize(), so its
 *           real consumer is some other, not-yet-traced method.
 *   +0x1ea0 2 more sub-objects, `0x144`-byte stride, DIFFERENT field
 *           shape (no `+0x14c`/`+0x14e` word pair, a plain 5-dword
 *           zero-block instead) -- NOT part of the +0x1e60 pointer table.
 *   +0x2128..+0x2130 3 more zeroed dwords.
 *   +0x2140 16 zeroed bytes -- immediately followed by the embedded
 *           `CSTGMasterLRMixer` object at the SAME offset (`Initialize()`
 *           call uses `this = mgr+0x2140`).
 *   +0x2160 embedded `CSetListEQ` (see `CSetList::Activate()`'s own
 *           already-confirmed cross-reference to this exact offset).
 *   +0x23d0 byte = `i` (the outer loop's own 0/1 slot index).
 *   +0x23d1 byte = 0 (written TWICE -- confirmed real double-write,
 *           preserved verbatim, not simplified away).
 *   +0x23d4/+0x23dc/+0x23e0/+0x23e8/+0x2404/+0x2408  more zeroed fields.
 *   +0x23e4 dword = 0x3e8 (1000).
 *   +0x23f0 dword = 0x3f800000 (1.0f).
 *   +0x2410 16 embedded `CSTGChannelValues` objects (`0x92c`-byte stride,
 *           matching oa_engine_init.h's own pre-existing "16 channels"
 *           note), spanning exactly to `+0xb6d0` (the buffer's own end --
 *           an independent confirmation of both the channel count and the
 *           `CSTGChannelValues` stride). A partial (120x12-byte array +
 *           one more 12-byte tail record) INLINE pre-zero pass runs first
 *           over all 16 slots -- CONFIRMED REAL, PRESERVED BUG-FOR-BUG:
 *           every byte it touches is immediately overwritten by
 *           `CSTGChannelValues::Initialize()`'s own unconditional
 *           `sTemplate` memcpy right after (already-real since sec
 *           10.151), so this pass is functionally inert but genuinely
 *           present in the real object code.
 *
 * After all per-`mgr` field-init is done, `mgr` is committed into
 * `CSTGPerformanceVarsManager::sInstance[i*4]` (a packed 32-bit pointer),
 * THEN four confirmed-real, deliberately-deferred sub-object Initialize()
 * calls run, in order: `CSTGAudioInputMixer::Initialize(i)` (this=mgr),
 * `CSTGMasterLRMixer::Initialize(i)` (this=mgr+0x2140),
 * `CSTGEffectRackVars::Initialize(mgr)` (this=mgr+0x20),
 * `CSetListEQ::Initialize(i)` (this=mgr+0x2160) -- THEN the 16x
 * `CSTGChannelValues::Initialize()` calls (already real).
 *
 * `CSTGPerformanceVarsManager::sInstance[8]` (the active-slot selector) is
 * CONFIRMED NOT written anywhere in this function -- `AllocPerformanceVars()`
 * is what actually toggles it (already documented in oa_global.h).
 */

#include "oa_global.h"
#include "oa_engine_init.h"
#include "oa_bank_memory.h"
#include "oa_internal.h"	/* placement operator new(size_t, void*) */

unsigned char CSTGPerformanceVarsManager::sInstance[12];

static inline unsigned int ToU32(void *p)
{
	return (unsigned int)(unsigned long)p;
}

/* Shared body for LOOP1/LOOP2/LOOP3: zero +0x8/+0xc/+0x10, zero the two
 * adjacent 38-entry dword arrays at +0x18/+0xb0, zero the +0x14c/+0x14e
 * word pair. Returns nothing -- caller applies any further per-loop
 * fields (LOOP1/LOOP2 only) itself. */
static void ZeroChannelMixerHead(unsigned char *p)
{
	*(unsigned int *)(p + 0x8) = 0;
	*(unsigned int *)(p + 0xc) = 0;
	*(unsigned int *)(p + 0x10) = 0;
	for (unsigned int k = 0; k < 0x26; k++) {
		*(unsigned int *)(p + k * 4 + 0xb0) = 0;
		*(unsigned int *)(p + k * 4 + 0x18) = 0;
	}
	*(unsigned short *)(p + 0x14c) = 0;
	*(unsigned short *)(p + 0x14e) = 0;
}

static void ZeroBytes(unsigned char *p, unsigned int off, unsigned int n)
{
	for (unsigned int b = 0; b < n; b++)
		p[off + b] = 0;
}

void CSTGPerformanceVarsManager::Initialize()
{
	unsigned char *self = (unsigned char *)this; /* &sInstance itself */

	for (unsigned int i = 0; i < 2; i++) {
		unsigned char *mgr = CSTGBankMemory::AllocAligned(0xb6d0, 0x40);

		new (mgr) CSTGAudioInputMixer();
		*(unsigned int *)(mgr + 0x0) = 8;

		/* ---- 12 entries, stride 0x210, base mgr+0x20 ---- */
		unsigned char *p = mgr + 0x20;
		for (unsigned int n = 0; n < 12; n++, p += 0x210) {
			ZeroChannelMixerHead(p);
			*(unsigned int *)(p + 0x1c0) = 0;
			p[0x1c8] = 0x20; p[0x1c9] = 0;
			p[0x1ca] = 0x20; p[0x1cb] = 0;
			*(unsigned int *)(p + 0x1cc) = 0;
			*(unsigned int *)(p + 0x1d0) = 0;
			p[0x1d8] = 0x20; p[0x1d9] = 0;
			p[0x1da] = 0x20; p[0x1db] = 0;
			*(unsigned int *)(p + 0x1dc) = 0;
			*(unsigned int *)(p + 0x200) = 0x42800000; /* 64.0f */
			ZeroBytes(p, 0x150, 16);
			*(unsigned int *)(p + 0x1f4) = 0x42800000; /* 64.0f */
			ZeroBytes(p, 0x160, 16);
			*(unsigned int *)(p + 0x204) = 0;
			ZeroBytes(p, 0x170, 16);
			*(unsigned int *)(p + 0x1f8) = 0;
			ZeroBytes(p, 0x180, 16);
			*(unsigned int *)(p + 0x208) = 0;
			ZeroBytes(p, 0x190, 16);
			*(unsigned int *)(p + 0x1fc) = 0;
			ZeroBytes(p, 0x1a0, 16);
			ZeroBytes(p, 0x1b0, 16);
		}

		/* ---- 2 entries, stride 0x170, base mgr+0x18e0 (overlaps the
		 * 12-entry array above -- confirmed real, see header comment) ---- */
		unsigned char *loop2Base = mgr + 0x18e0;
		p = loop2Base;
		for (unsigned int n = 0; n < 2; n++, p += 0x170) {
			ZeroChannelMixerHead(p);
			*(unsigned int *)(p + 0x168) = 0;
			ZeroBytes(p, 0x150, 16);
		}

		/* ---- 2 entries, stride 0x150, base mgr+0x1bc0 ---- */
		unsigned char *loop3Base = mgr + 0x1bc0;
		p = loop3Base;
		for (unsigned int n = 0; n < 2; n++, p += 0x150)
			ZeroChannelMixerHead(p);

		/* ---- 2 entries, stride 0x144, base mgr+0x1ea0, different shape ---- */
		p = mgr + 0x1ea0;
		for (unsigned int n = 0; n < 2; n++, p += 0x144) {
			for (unsigned int k = 0; k < 0x26; k++) {
				*(unsigned int *)(p + k * 4 + 0xac) = 0;
				*(unsigned int *)(p + k * 4 + 0x14) = 0;
			}
			*(unsigned int *)(p + 0x0) = 0;
			*(unsigned int *)(p + 0x4) = 0;
			*(unsigned int *)(p + 0x10) = 0;
			*(unsigned int *)(p + 0x8) = 0;
			*(unsigned int *)(p + 0xc) = 0;
		}

		/* ---- 16-entry small-mixer-state pointer table, mgr+0x1e60..+0x1e9c ---- */
		*(unsigned int *)(mgr + 0x1e60) = ToU32(mgr + 0x20);
		*(unsigned int *)(mgr + 0x1e64) = ToU32(mgr + 0x230);
		*(unsigned int *)(mgr + 0x1e68) = ToU32(mgr + 0x440);
		*(unsigned int *)(mgr + 0x1e6c) = ToU32(mgr + 0x650);
		*(unsigned int *)(mgr + 0x1e70) = ToU32(mgr + 0x860);
		*(unsigned int *)(mgr + 0x1e74) = ToU32(mgr + 0xa70);
		*(unsigned int *)(mgr + 0x1e78) = ToU32(mgr + 0xc80);
		*(unsigned int *)(mgr + 0x1e7c) = ToU32(mgr + 0xe90);
		*(unsigned int *)(mgr + 0x1e80) = ToU32(mgr + 0x10a0);
		*(unsigned int *)(mgr + 0x1e84) = ToU32(mgr + 0x12b0);
		*(unsigned int *)(mgr + 0x1e88) = ToU32(mgr + 0x14c0);
		*(unsigned int *)(mgr + 0x1e8c) = ToU32(mgr + 0x16d0);
		*(unsigned int *)(mgr + 0x1e90) = ToU32(loop2Base);		/* mgr+0x18e0 */
		*(unsigned int *)(mgr + 0x1e94) = ToU32(mgr + 0x1a50);
		*(unsigned int *)(mgr + 0x1e98) = ToU32(loop3Base);		/* mgr+0x1bc0 */
		*(unsigned int *)(mgr + 0x1e9c) = ToU32(mgr + 0x1d10);

		/* ---- misc fields ---- */
		*(unsigned int *)(mgr + 0x2128) = 0;
		*(unsigned int *)(mgr + 0x212c) = 0;
		*(unsigned int *)(mgr + 0x2130) = 0;
		ZeroBytes(mgr, 0x2140, 16);
		mgr[0x23d0] = (unsigned char)i;
		mgr[0x23d1] = 0;

		/* ---- 16-channel inline pre-zero pass (functionally inert --
		 * see header comment: every byte here is immediately
		 * overwritten by CSTGChannelValues::Initialize() below) ---- */
		unsigned char *ch = mgr + 0x2410;
		for (unsigned int c = 0; c < 16; c++, ch += 0x92c) {
			unsigned char *e = ch;
			for (unsigned int k = 0; k < 120; k++, e += 0xc) {
				unsigned char b = e[0xb];
				*(unsigned short *)(e + 0x8) = 0;
				b = (b | 1) & ~2;
				*(unsigned int *)(e + 0x0) = 0;
				*(unsigned int *)(e + 0x4) = 0;
				e[0xa] = 1;
				e[0xb] = b;
			}
			unsigned char b2 = ch[0x5ab];
			*(unsigned short *)(ch + 0x5a8) = 0;
			b2 = (b2 | 1) & ~2;
			*(unsigned int *)(ch + 0x5a0) = 0;
			*(unsigned int *)(ch + 0x5a4) = 0;
			ch[0x5aa] = 1;
			ch[0x5ab] = b2;
		}

		mgr[0x23d1] = 0;	/* redundant second write, real, preserved */
		mgr[0x23dc] = 0;
		*(unsigned int *)(mgr + 0x23d4) = 0;
		*(unsigned int *)(mgr + 0x23e0) = 0;
		*(unsigned int *)(mgr + 0x23e4) = 0x3e8;	/* 1000 */
		*(unsigned int *)(mgr + 0x23e8) = 0;
		*(unsigned int *)(mgr + 0x23f0) = 0x3f800000;	/* 1.0f */
		*(unsigned int *)(mgr + 0x2408) = 0;
		*(unsigned int *)(mgr + 0x2404) = 0;

		/* Commit into sInstance[i*4] BEFORE the sub-object Initialize()
		 * calls below -- confirmed real ordering. */
		*(unsigned int *)(self + i * 4) = ToU32(mgr);

		((CSTGAudioInputMixer *)mgr)->Initialize(i);
		((CSTGMasterLRMixer *)(mgr + 0x2140))->Initialize(i);
		((CSTGEffectRackVars *)(mgr + 0x20))->Initialize((CSTGPerformanceVars *)mgr);
		((CSetListEQ *)(mgr + 0x2160))->Initialize(i);

		unsigned char *cv = mgr + 0x2410;
		for (unsigned int c = 0; c < 16; c++, cv += 0x92c)
			((CSTGChannelValues *)cv)->Initialize();
	}
}
