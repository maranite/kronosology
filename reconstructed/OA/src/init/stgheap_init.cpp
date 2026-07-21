// SPDX-License-Identifier: GPL-2.0
/*
 * stgheap_init.cpp  -  InitializeSTGHeap()/CleanupSharedHeap(): init_module
 * step 5 (hard-fail). See oa_stgheap_init.h for the ground-truthing
 * details (offset, extraction method, symbol-naming caveats).
 *
 * Faithful, instruction-level reconstruction from a full objdump
 * disassembly + `.rel.text` relocation resolution of the real
 * `InitializeSTGHeap` (`.text+0x9bc0`, 640 bytes) in OA_real.ko. Every
 * magic constant below (0x07ffffff/0xf8000000 round-to-128MB, the
 * 0x40000000/0xffc00000 and 0x602000/0xffc00000 masks, the 0x800000/
 * 0x802000/0xa00000 offsets, and the 0.9 multiplier) is copied exactly
 * from the real machine code, not inferred or simplified -- this
 * project's disassembly-first policy applies here just as much as to
 * any cryptographic routine reconstructed earlier. The five diagnostic
 * `printk` format strings were extracted directly from this exact
 * binary's `.rodata.str1.4` section (not guessed), and their
 * placeholder names independently confirm every variable name used
 * below.
 *
 * `sInstalledRAM`/`sPhysicalBankStart`/`sIORemapBase`/`AlignedHeapBase`/
 * `sHeapSize` are real module-level statics in the original binary
 * (confirmed via `.bss`-relative relocations, not stack slots) -- kept
 * as file-scope statics here too, with small accessors below, both for
 * fidelity and so a host KAT can observe the computed result.
 */

#include "oa_stgheap_init.h"

/* asmlinkage (regparm(0)) required -- see init_module.cpp's own
 * comment on this exact fix, sec 10.87. */
extern "C" __attribute__((regparm(0))) int printk(const char *fmt, ...);
#define KERN_INFO "\001" "6"

static unsigned long sInstalledRAM;
static unsigned long sPhysicalBankStart;
static unsigned long sIORemapBase;
static unsigned long sAlignedHeapBase;
static unsigned long sHeapSize;

unsigned long stgheap_get_installed_ram(void) { return sInstalledRAM; }
unsigned long stgheap_get_physical_bank_start(void) { return sPhysicalBankStart; }
unsigned long stgheap_get_ioremap_base(void) { return sIORemapBase; }
unsigned long stgheap_get_aligned_heap_base(void) { return sAlignedHeapBase; }
unsigned long stgheap_get_heap_size(void) { return sHeapSize; }

int InitializeSTGHeap(void)
{
	unsigned long rawMemSize = orig_mem_size[0];

	/* Round rawMemSize UP to the next 128MB boundary: this exact
	 * (x + (128MB-1)) & ~(128MB-1) pattern, confirmed via the literal
	 * 0x07ffffff/0xf8000000 constants (128MB = 0x08000000). */
	sInstalledRAM = (rawMemSize + 0x07ffffffUL) & 0xf8000000UL;

	/*
	 * Two real `ja` (unsigned-greater) guards follow in the disassembly:
	 * one checks a confirmed +4-byte read past `orig_mem_size` itself
	 * (see oa_stgheap_init.h's own note -- modeled as orig_mem_size[1]
	 * here, exact original-source meaning not independently confirmed
	 * beyond "a real guard read the compiler emitted"), the other
	 * re-checks rawMemSize against the just-computed rounded value
	 * (guards against the round-up wrapping, which shouldn't happen in
	 * practice). BOTH failure branches converge on the SAME real target
	 * (`.text+0x9e18`): overwrite sInstalledRAM with -1 as a diagnostic
	 * "invalid" marker, then jump back into the MIDDLE of this same
	 * computation (`.text+0x9bf8`) -- not an early return. The rest of
	 * the function proceeds identically either way (the real code's own
	 * `eax = -1` assignment right before that jump is never read again),
	 * so this is faithfully modeled as a plain overwrite, not a goto.
	 */
	if (orig_mem_size[1] != 0 || rawMemSize > sInstalledRAM)
		sInstalledRAM = (unsigned long)-1;

	/* sPhysicalBankStart: high_memory rebased by -1GB, then aligned down
	 * to a 4MB boundary. */
	sPhysicalBankStart = (high_memory - 0x40000000UL) & 0xffc00000UL;

	/* physMemSize: the gap between rawMemSize and sPhysicalBankStart,
	 * also aligned down to 4MB (a stack-local in the real code -- not
	 * persisted to .bss, so kept as a plain local here too). */
	unsigned long physMemSize = (rawMemSize - sPhysicalBankStart) & 0xffc00000UL;

	printk(KERN_INFO "orig_mem_size %08lx (%lu), sInstalledRAM %08lx (%lu).  "
	       "sPhysicalBankStart %08lx.  physMemSize 0x%lx (%lu)\n",
	       rawMemSize, rawMemSize, sInstalledRAM, sInstalledRAM,
	       sPhysicalBankStart, physMemSize, physMemSize);

	/*
	 * vmallocStart/vmallocEnd: real kernel VMALLOC-area boundary
	 * diagnostics (names confirmed via the second printk's own format
	 * string). vmallocEnd's formula ((__FIXADDR_TOP - 0x602000) &
	 * 0xffc00000) - 0x2000 mirrors the real kernel's own
	 * `FIXADDR_START - 2*PAGE_SIZE`-style VMALLOC_END derivation. Both
	 * are diagnostic-only in this function -- neither is reused below
	 * (the later ceiling check recomputes a related but distinct value
	 * from scratch, see hardCeiling).
	 */
	unsigned long vmallocStart = high_memory + 0x00800000UL;
	unsigned long vmallocEnd = ((__FIXADDR_TOP - 0x602000UL) & 0xffc00000UL) - 0x2000UL;
	printk(KERN_INFO "vmalloc_start=0x%08lx, vmalloc_end=0x%08lx\n",
	       vmallocStart, vmallocEnd);

	/*
	 * 0.9: the real .rodata.cst8 double constant at this call site,
	 * extracted directly (bytes cd cc cc cc cc cc ec 3f), not assumed --
	 * the real code computes this via genuine x87 instructions
	 * (fild/fld/fmulp/fisttp).
	 *
	 * DELIBERATE DEVIATION, not a fidelity gap in the algorithm itself:
	 * built under Kbuild, this file inherits the real kernel's own
	 * `-msoft-float` build flag (confirmed via the actual Kbuild compile
	 * command line), which makes a literal `(double)x * 0.9` compile to
	 * calls to libgcc-style soft-float helpers (`__floatunsidf`,
	 * `__fixunsdfsi`) instead of real FPU instructions -- new unresolved
	 * symbols very unlikely to be exported by a real running kernel.
	 * Plain `((unsigned long long)x * 9) / 10` avoids the float helpers
	 * but trades them for a DIFFERENT libgcc dependency (`__udivdi3`,
	 * 64-bit division) -- this is EXACTLY the scenario the real kernel's
	 * own `do_div()` macro exists to solve (32-bit x86 has no native
	 * 64-bit divide instruction, so plain C 64-bit division always needs
	 * a libgcc call unless done via the CPU's own `divl`, which only
	 * takes a 64-bit dividend with a 32-bit QUOTIENT -- exactly what
	 * `do_div()` wraps). Since physMemSize*9/10 can never exceed
	 * physMemSize itself (a 32-bit value), the quotient always fits in
	 * 32 bits, so a hand-written `divl`-based helper (mirroring
	 * `do_div()`'s own real x86 implementation, not reimplementing
	 * anything novel) is used here instead of including the real kernel
	 * header for it.
	 */
	unsigned long long tmp = (unsigned long long)physMemSize * 9ULL;
	unsigned long requiredSize;
#if defined(__i386__)
	{
		/* Real x86 `divl`: 32-bit-sized operands specifically (not
		 * `unsigned long`, which is only guaranteed 32-bit on this
		 * -m32 target -- explicit-width types keep this correct if
		 * ever compiled elsewhere). */
		unsigned int hi = (unsigned int)(tmp >> 32);
		unsigned int lo = (unsigned int)tmp;
		unsigned int quot, rem;
		__asm__("divl %[div]"
			: "=a"(quot), "=d"(rem)
			: "0"(lo), "1"(hi), [div] "rm"(10U));
		requiredSize = quot;
	}
#else
	/* Host KAT/non-x86-32 builds: plain division is fine here (only the
	 * real Kbuild -m32 target needs to avoid the libgcc __udivdi3 call). */
	requiredSize = (unsigned long)(tmp / 10ULL);
#endif

	/*
	 * Walk iomem_resource's top-level sibling list (struct resource
	 * layout: start@0, end@0x4, sibling@0x14, child@0x18 -- confirmed
	 * real Linux 2.6.32 x86-32 non-PAE offsets), tracking the largest
	 * free gap seen between consecutive resources whose own `end` is at
	 * or above sPhysicalBankStart, and stopping early once a gap at
	 * least `requiredSize` is found.
	 *
	 * NOTE, faithfully reproduced rather than "fixed": the real code's
	 * very first iteration compares the first child resource against
	 * ITSELF (prevEnd is seeded from the first node's own `end`, not
	 * "no previous node"), so the first gap computed is
	 * `firstNode->start - firstNode->end - 1` -- for a normal resource
	 * (start <= end) this underflows to a huge unsigned value, which
	 * will almost always satisfy `gap >= bestGap` and, once clamped by
	 * the trimming step below, immediately exits the search. Whether
	 * this is a deliberate "always take the region right after the
	 * first iomem child" shortcut or an oversight in the original code
	 * is not resolved here -- only that it's exactly what the real
	 * machine code computes.
	 */
	unsigned long candidateStart = 0;
	unsigned long bestGap = 0;
	struct resource *node = iomem_resource.child;
	if (node) {
		unsigned long prevEnd = node->end;
		for (;;) {
			unsigned long curEnd = node->end;
			if (curEnd >= sPhysicalBankStart) {
				unsigned long gap = node->start - prevEnd - 1;
				if (gap >= bestGap) {
					candidateStart = prevEnd + 1;
					bestGap = gap;
				}
				if (bestGap >= requiredSize)
					break;
			}
			prevEnd = curEnd;
			node = node->sibling;
			if (!node)
				break;
		}
	}

	printk(KERN_INFO "found a suitable region at 0x%lx of size 0x%lx (%lu).  "
	       "Our size request is 0x%lx (%lu)\n",
	       candidateStart, bestGap, bestGap, physMemSize, physMemSize);

	/* sPhysicalBankStart is REPURPOSED here to mean "the chosen
	 * candidate start address" for the rest of the function (confirmed:
	 * the 4th/5th printk calls below read it back out and the extracted
	 * format strings label it the same way) -- reusing the same real
	 * global rather than introducing a second one, exactly as
	 * disassembled. */
	sPhysicalBankStart = candidateStart;

	/* Clamp bestGap down to physMemSize if the search somehow found more
	 * than that (a real `cmova`, confirmed). */
	if (bestGap > physMemSize)
		bestGap = physMemSize;

	/* A SECOND, independently-computed ceiling (not a reuse of
	 * vmallocEnd above -- confirmed via a fresh __FIXADDR_TOP reload and
	 * a further subtraction of `high_memory`): trim bestGap down by 10MB
	 * steps until it fits under this ceiling. */
	unsigned long hardCeiling =
		((__FIXADDR_TOP - 0x602000UL) & 0xffc00000UL) - high_memory - 0x00802000UL;
	while (bestGap > hardCeiling)
		bestGap -= 0x00a00000UL;

	void *mapped = ioremap_cache(sPhysicalBankStart, bestGap);
	while (!mapped) {
		/* Real retry-on-failure path: shrink by another 10MB and try
		 * again at the SAME candidate start address. No explicit
		 * bound on retry count in the real code either. */
		bestGap -= 0x00a00000UL;
		mapped = ioremap_cache(sPhysicalBankStart, bestGap);
	}

	sIORemapBase = (unsigned long)mapped;

	printk(KERN_INFO "sPhysicalBankStart = 0x%08lx, physMemSize = 0x%08lx (%lu), "
	       "sIORemapBase = 0x%08lx\n",
	       sPhysicalBankStart, bestGap, bestGap, sIORemapBase);

	/* Inlined memset(mapped, 0, bestGap) -- real code does the same
	 * dword/word/byte-remainder split via `rep stosl` + conditional
	 * trailing stosw/stosb. */
	{
		unsigned char *p = (unsigned char *)mapped;
		unsigned long n = bestGap;
		unsigned long ndw = n >> 2;
		unsigned int *pdw = (unsigned int *)p;
		for (unsigned long i = 0; i < ndw; i++)
			pdw[i] = 0;
		p += ndw << 2;
		if (n & 0x2) {
			*(unsigned short *)p = 0;
			p += 2;
		}
		if (n & 0x1)
			*p = 0;
	}

	unsigned long heapInitResult = CSTGHeapManager_Initialize(sIORemapBase, bestGap);

	/* AlignedHeapBase: real name confirmed via the 5th printk's format
	 * string below. */
	sAlignedHeapBase = heapInitResult - sIORemapBase + sPhysicalBankStart;

	sHeapSize = CSTGHeapManager_GetHeapSize();

	printk(KERN_INFO "%p..%p is ioremapped memory, %lu bytes, "
	       "physicalBankStart @ 0x%08lx\nAlignedHeapBase is 0x%lx\n",
	       (void *)sIORemapBase, (void *)(sIORemapBase + bestGap), bestGap,
	       sPhysicalBankStart, sAlignedHeapBase);

	return 0;
}

/*
 * Confirmed real (`.text+0x9e40`, real-hardware boot regression found
 * 2026-07-21, via `objdump -d -r` on OA_real.ko): a single guarded
 * `iounmap()` call against `sIORemapBase` -- nothing else, no
 * `release_mem_region`/`release_resource` counterpart in ground truth.
 * This project's own prior "not yet disassembled" no-op left
 * `InitializeSTGHeap`'s ~2.7GB `ioremap_cache`'d bank-memory region
 * mapped forever on any init_module failure past step 5 (e.g. a real
 * keybed-handshake failure at step 14) -- confirmed on real hardware:
 * a single such failed insmod left only ~1MB of vmalloc address space
 * free (`VmallocUsed` within 1MB of `VmallocTotal`), starving every
 * subsequent retry within the same boot down to a few-MB heap and
 * crashing early in `CSTGEngine`'s constructor instead.
 */
void CleanupSharedHeap(void)
{
	if (sIORemapBase)
		iounmap((void *)sIORemapBase);
}
