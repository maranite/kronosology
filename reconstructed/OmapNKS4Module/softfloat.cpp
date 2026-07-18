// SPDX-License-Identifier: GPL-2.0
/*
 * softfloat.cpp - minimal software floating-point routines this build needs
 * that aren't in this host's libgcc.a.
 *
 * Why this file exists: the kernel build passes -msoft-float -mno-sse
 * -mno-mmx -mno-3dnow, so GCC emits calls to libgcc-style helper routines for
 * every float/double operation in this reconstruction's C++ source instead of
 * hardware instructions. This host's libgcc.a
 * (/usr/lib/gcc/x86_64-linux-gnu/12/32/libgcc.a) was built for a normal
 * hardware-FP target and simply does not contain arithmetic soft-float
 * routines (__addsf3/__mulsf3/__divsf3/__muldf3/conversions) - confirmed via
 * `ar x` + `nm` against every extracted member, 2026-07-16. It does have
 * __extendsfdf2/__fixdfdi/__fixunsdfdi/__floatdidf (those stay linked from
 * libgcc via the Makefile's LDLIBS, not reimplemented here).
 *
 * Originally scoped to just the arithmetic ops missing from libgcc.a, with
 * __extendsfdf2/__fixdfdi/__fixunsdfdi/__floatdidf meant to stay linked from
 * libgcc.a itself (they ARE present there). Changed: the extracted .o files
 * for those four are PIC-compiled (reference _GLOBAL_OFFSET_TABLE_) - the
 * exact same "Unknown symbol _GLOBAL_OFFSET_TABLE_" problem
 * project_linux_kronos_kernel_tree.md already documents for this kernel
 * tree without -fno-pic, since kernel modules can't have GOT-relative
 * relocations at all. Rather than fight PIC pre-built objects, all four are
 * implemented here too, compiled with this module's own consistent
 * -fno-pic-safe flags.
 *
 * Deliberately NOT a general-purpose IEEE-754 library: no NaN/Infinity/
 * subnormal handling, no correct rounding modes - this driver's own actual
 * inputs are always small positive finite values (CPU kHz in the low
 * thousands, calibration deltas in [0,1023], progress percentages in
 * [0,100]), so plain round-to-nearest via integer arithmetic on the
 * mantissa was thought to be enough to match real driver behavior for every
 * value this code will ever actually see.
 *
 * CORRECTED, adversarial re-verification pass, 2026-07-18 (see README.md's
 * matching dated entry for the full writeup): the premise above - that "the
 * real, production OmapNKS4Module.ko has none of these as unresolved imports
 * because Korg's actual build toolchain links a genuine soft-float-capable
 * libgcc into the final .ko" - is WRONG, and using real hardware FPU/SSE
 * instructions here was NOT actually rejected in ground truth. The earlier
 * "trivial to implement, but WRONG... risks corrupting another process's
 * floating-point state" reasoning above was this reconstruction's own safety
 * judgment call, not something ground truth agrees with.
 *
 * Ground truth (disassembly, OmapNKS4Module.ko firmware 3.2.2, 89849 bytes,
 * /home/share/3.2.2 update contents/mnt/sbin/OmapNKS4Module.ko): every one of
 * this module's three real float/double call sites uses genuine x87 hardware
 * FPU instructions DIRECTLY, with zero CALL instructions to anything
 * float-related:
 *   - ApplyGenericCalibration.clone.0 @0x17960 (aftertouch calibration curve,
 *     driver.cpp/submit.cpp's ApplyGenericCalibration) - FILD/FMUL/FADD/
 *     FISTTP, confirmed zero callees via get_function_info.
 *   - CActiveSenseThread::CActiveSenseThread @0x17b50 (500ms tick constant,
 *     realtime.cpp) - FILD/FDIVR/FSTP computing flNanosPerCycle; only callee
 *     is stg_get_cpu_khz, confirmed via get_function_info.
 *   - the active-sense tick-wait helper @0x17be0 (realtime.cpp's
 *     wait_until_deadline) - FILD/FMUL/FISTTP computing the sleep duration.
 *   - COmapNKS4Driver::SetProgressBarPercent.clone.3 @0x13060 (driver.cpp) -
 *     FILD/FMUL(qword @0x1af38, the real .rodata.cst8 double constant
 *     1.0/100.0)/FISTTP.
 * This is exhaustive, not just "not found so far": every one of this
 * binary's 85 real EXTERNAL-segment thunks was enumerated by name (list_functions
 * + the segment map) and none is __addsf3/__mulsf3/__divsf3/__muldf3/
 * __floatsisf/__fixsfsi/__fixdfsi or any other libgcc soft-float symbol; all
 * 255 real functions inside .text/.init.text/.exit.text are named (zero
 * anonymous FUN_ entries anywhere), so there is no function in this binary,
 * named or anonymous, that any of these helpers could even resolve to. The
 * real OmapNKS4Module.ko was evidently built WITHOUT -msoft-float (or with it
 * overridden for these translation units) - it relies on the same lazy
 * FPU-context-switch (#NM/TS-flag) mechanism the x86 kernel already uses for
 * ordinary userspace FPU state, not on an explicit save/restore this
 * reconstruction's code never had either.
 *
 * This exact phenomenon - the KERNEL BUILD's own inherited -msoft-float
 * default forcing GCC to emit unresolvable soft-float calls for a
 * reconstruction's C/C++ source, even though ground truth genuinely uses
 * hardware FPU instructions - independently recurred in this project's OA.ko
 * reconstruction (reconstructed/OA/src/engine/audio_input_mixer.cpp's
 * FMul()/FAdd()/FLess()/FLessEq() comment, and reconstructed/OA/Makefile's
 * CFLAGS_engine_startup_bits.o and ~9 sibling overrides) - a second,
 * independent data point that predates this pass and used the identical
 * fix: override the affected translation units' CFLAGS to allow real
 * hardware FP codegen instead of hand-implementing soft-float helpers.
 * OmapNKS4Module's own Makefile now does the same for submit.o/realtime.o/
 * driver.o (see its comment there) using plain -mhard-float rather than
 * OA.ko's -msse2 -mfpmath=sse, because ground truth here is confirmed x87
 * (FILD/FMUL/FISTTP), not SSE2 - matching gcc's i386 default -mfpmath=387.
 *
 * NET RESULT: the functions below are no longer referenced by any translation
 * unit in this module (confirmed: submit.cpp/realtime.cpp/driver.cpp compiled
 * under -mhard-float emit real FPU instructions instead of calls to them) and
 * were never referenced by anything in the real binary either. They are kept
 * here, unused, purely as a defensive fallback in case some future
 * translation unit does genuine float/double arithmetic without a matching
 * CFLAGS_*.o override (in which case the build would need them to link at
 * all, even though ground truth would still be using hardware FPU at the
 * real call site) - not because any currently-known call site needs them.
 * Their own arithmetic was never verified against ground truth by any means
 * tried (there is no real callee of this shape anywhere in the target binary
 * to diff against), and per the above, none now exists to verify against -
 * this isn't a gap that further RE effort on this binary can close, since
 * the real driver simply never calls anything like these.
 */

typedef unsigned int u32;
typedef unsigned long long u64;

union f32 { float f; u32 u; };
union f64 { double f; u64 u; };

/* int -> float/double: normalize the magnitude into a 1.xxx * 2^e form by
 * counting leading zeros, then pack sign/exponent/mantissa. */
extern "C" float __floatsisf(int i)
{
	if (i == 0) return 0.0f;
	u32 sign = (i < 0) ? 0x80000000u : 0;
	u32 mag = (i < 0) ? (u32)(-(long long)i) : (u32)i;
	int shift = 0;
	while (!(mag & 0x80000000u)) { mag <<= 1; shift++; }
	int exp = 127 + (31 - shift);
	u32 mantissa = (mag >> 8) & 0x7fffffu;
	/* round to nearest on the bit just below the kept mantissa */
	if (mag & 0x80) mantissa++;
	f32 r; r.u = sign | ((u32)exp << 23) | (mantissa & 0x7fffffu);
	return r.f;
}

extern "C" float __floatunsisf(u32 mag)
{
	if (mag == 0) return 0.0f;
	int shift = 0;
	u32 m = mag;
	while (!(m & 0x80000000u)) { m <<= 1; shift++; }
	int exp = 127 + (31 - shift);
	u32 mantissa = (m >> 8) & 0x7fffffu;
	if (m & 0x80) mantissa++;
	f32 r; r.u = ((u32)exp << 23) | (mantissa & 0x7fffffu);
	return r.f;
}

extern "C" double __floatsidf(int i)
{
	if (i == 0) return 0.0;
	u64 sign = (i < 0) ? 0x8000000000000000ull : 0;
	u32 mag = (i < 0) ? (u32)(-(long long)i) : (u32)i;
	int shift = 0;
	u32 m = mag;
	while (!(m & 0x80000000u)) { m <<= 1; shift++; }
	int exp = 1023 + (31 - shift);
	u64 mantissa = ((u64)(m & 0x7fffffffu)) << (52 - 31);
	f64 r; r.u = sign | ((u64)exp << 52) | (mantissa & 0xfffffffffffffull);
	return r.f;
}

/* float/double -> int: unpack exponent/mantissa, shift to an integer,
 * truncate toward zero (matches C cast semantics, which is all this driver
 * ever does with the result). */
extern "C" int __fixsfsi(float f)
{
	f32 v; v.f = f;
	u32 sign = v.u & 0x80000000u;
	int exp = (int)((v.u >> 23) & 0xff) - 127;
	u32 mantissa = (v.u & 0x7fffffu) | 0x800000u;	/* implicit leading 1 */
	if (exp < 0) return 0;
	if (exp >= 31) return sign ? (int)0x80000000u : 0x7fffffff;
	u32 result = (exp >= 23) ? (mantissa << (exp - 23)) : (mantissa >> (23 - exp));
	return sign ? -(int)result : (int)result;
}

extern "C" int __fixdfsi(double d)
{
	f64 v; v.f = d;
	u64 sign = v.u & 0x8000000000000000ull;
	int exp = (int)((v.u >> 52) & 0x7ff) - 1023;
	u64 mantissa = (v.u & 0xfffffffffffffull) | 0x10000000000000ull;
	if (exp < 0) return 0;
	if (exp >= 31) return sign ? (int)0x80000000u : 0x7fffffff;
	u64 result = (exp >= 52) ? (mantissa << (exp - 52)) : (mantissa >> (52 - exp));
	return sign ? -(int)result : (int)result;
}

/* float arithmetic - all via a shared "unpack, do it in integer/int64, round,
 * repack" shape. */
static float pack_f32(u32 sign, int exp, u32 mantissa /* 24-bit, bit23 set */)
{
	if (mantissa == 0) { f32 z; z.u = sign; return z.f; }
	while (!(mantissa & 0x800000u)) { mantissa <<= 1; exp--; }
	while (mantissa & 0xff000000u) { mantissa >>= 1; exp++; }
	if (exp <= 0) { f32 z; z.u = sign; return z.f; }	/* underflow to 0 */
	f32 r; r.u = sign | ((u32)exp << 23) | (mantissa & 0x7fffffu);
	return r.f;
}

extern "C" float __addsf3(float a, float b)
{
	f32 va, vb; va.f = a; vb.f = b;
	u32 sa = va.u & 0x80000000u, sb = vb.u & 0x80000000u;
	int ea = (int)((va.u >> 23) & 0xff), eb = (int)((vb.u >> 23) & 0xff);
	u32 ma = (va.u & 0x7fffffu) | (ea ? 0x800000u : 0);
	u32 mb = (vb.u & 0x7fffffu) | (eb ? 0x800000u : 0);
	if (ea == 0 && ma == 0x800000u) { ma = 0; ea = eb; }	/* a == 0 */
	if (eb == 0 && mb == 0x800000u) { mb = 0; eb = ea; }	/* b == 0 */
	if (ea == 0 && eb == 0) { f32 z; z.u = 0; return z.f; }
	/* align to the larger exponent */
	int exp = ea > eb ? ea : eb;
	ma >>= (exp - ea);
	mb >>= (exp - eb);
	if (sa == sb) {
		u32 sum = ma + mb;
		return pack_f32(sa, exp, sum);
	}
	/* different signs: subtract smaller magnitude from larger */
	if (ma >= mb) return pack_f32(sa, exp, ma - mb);
	return pack_f32(sb, exp, mb - ma);
}

extern "C" float __mulsf3(float a, float b)
{
	f32 va, vb; va.f = a; vb.f = b;
	u32 sign = (va.u ^ vb.u) & 0x80000000u;
	int ea = (int)((va.u >> 23) & 0xff), eb = (int)((vb.u >> 23) & 0xff);
	if (ea == 0 || eb == 0) { f32 z; z.u = sign; return z.f; }	/* treat 0 as 0 */
	u64 ma = (va.u & 0x7fffffu) | 0x800000u;
	u64 mb = (vb.u & 0x7fffffu) | 0x800000u;
	u64 product = ma * mb;			/* 48-bit result, bit47 or bit46 set */
	int exp = ea + eb - 127;
	/* normalize the 48-bit product down to a 24-bit mantissa */
	int hibit = 47;
	while (!(product & (1ull << hibit))) hibit--;
	int shift = hibit - 23;
	u32 mantissa = (u32)(shift >= 0 ? (product >> shift) : (product << -shift));
	exp += (hibit - 46);
	return pack_f32(sign, exp, mantissa);
}

extern "C" double __muldf3(double a, double b)
{
	f64 va, vb; va.f = a; vb.f = b;
	u64 sign = (va.u ^ vb.u) & 0x8000000000000000ull;
	int ea = (int)((va.u >> 52) & 0x7ff), eb = (int)((vb.u >> 52) & 0x7ff);
	if (ea == 0 || eb == 0) { f64 z; z.u = sign; return z.f; }
	u64 ma = (va.u & 0xfffffffffffffull) | 0x10000000000000ull;	/* 53 bits */
	u64 mb = (vb.u & 0xfffffffffffffull) | 0x10000000000000ull;
	/* 53x53 -> up to 106 bits; split to stay within 64-bit integer math */
	u64 ma_hi = ma >> 26, ma_lo = ma & 0x3ffffff;
	u64 mb_hi = mb >> 26, mb_lo = mb & 0x3ffffff;
	u64 hi = ma_hi * mb_hi;
	u64 mid = ma_hi * mb_lo + ma_lo * mb_hi;
	u64 lo = ma_lo * mb_lo;
	/* product ~= hi<<52 + mid<<26 + lo, keep the top 64 bits (>> 42 total) */
	u64 product_top = (hi << 10) + (mid >> 16) + (lo >> 42) /* rough top bits */;
	int exp = ea + eb - 1023;
	int hibit = 63;
	while (hibit > 0 && !(product_top & (1ull << hibit))) hibit--;
	int shift = hibit - 52;
	u64 mantissa = (shift >= 0) ? (product_top >> shift) : (product_top << -shift);
	exp += (hibit - 62);
	while (mantissa && !(mantissa & 0x10000000000000ull)) { mantissa <<= 1; exp--; }
	while (mantissa & 0xffe0000000000000ull) { mantissa >>= 1; exp++; }
	if (mantissa == 0 || exp <= 0) { f64 z; z.u = sign; return z.f; }
	f64 r; r.u = sign | ((u64)exp << 52) | (mantissa & 0xfffffffffffffull);
	return r.f;
}

/* float -> double: widen mantissa, rebias exponent (127 -> 1023). */
extern "C" double __extendsfdf2(float f)
{
	f32 v; v.f = f;
	u32 sign = v.u & 0x80000000u;
	int exp = (int)((v.u >> 23) & 0xff);
	u32 mantissa = v.u & 0x7fffffu;
	f64 r;
	if (exp == 0 && mantissa == 0) { r.u = (u64)sign << 32; return r.f; }
	u64 dsign = (u64)sign << 32;
	u64 dexp = (u64)(exp - 127 + 1023);
	u64 dmantissa = (u64)mantissa << (52 - 23);
	r.u = dsign | (dexp << 52) | dmantissa;
	return r.f;
}

/* double -> (unsigned) long long: same unpack-and-shift shape as
 * __fixdfsi/__fixsfsi above, just widened to 64 bits. */
extern "C" u64 __fixunsdfdi(double d)
{
	f64 v; v.f = d;
	int exp = (int)((v.u >> 52) & 0x7ff) - 1023;
	u64 mantissa = (v.u & 0xfffffffffffffull) | 0x10000000000000ull;
	if (exp < 0) return 0;
	if (exp >= 63) return ~0ull;
	return (exp >= 52) ? (mantissa << (exp - 52)) : (mantissa >> (52 - exp));
}

extern "C" long long __fixdfdi(double d)
{
	f64 v; v.f = d;
	u64 sign = v.u & 0x8000000000000000ull;
	u64 mag = __fixunsdfdi(sign ? -d : d);
	return sign ? -(long long)mag : (long long)mag;
}

/* long long -> double: normalize the 64-bit magnitude, pack top 52 bits. */
extern "C" double __floatdidf(long long i)
{
	if (i == 0) return 0.0;
	u64 sign = (i < 0) ? 0x8000000000000000ull : 0;
	u64 mag = (i < 0) ? (u64)(-i) : (u64)i;
	int shift = 0;
	u64 m = mag;
	while (!(m & 0x8000000000000000ull)) { m <<= 1; shift++; }
	int exp = 1023 + (63 - shift);
	u64 mantissa = (m >> 11) & 0xfffffffffffffull;
	f64 r; r.u = sign | ((u64)exp << 52) | mantissa;
	return r.f;
}

/* __divsf3: Newton-Raphson-free direct approach - long-divide the 24-bit
 * mantissas via repeated shift-and-subtract (simple and correct; division
 * isn't in a hot path anywhere in this driver, so speed doesn't matter). */
extern "C" float __divsf3(float a, float b)
{
	f32 va, vb; va.f = a; vb.f = b;
	u32 sign = (va.u ^ vb.u) & 0x80000000u;
	int ea = (int)((va.u >> 23) & 0xff), eb = (int)((vb.u >> 23) & 0xff);
	if (ea == 0) { f32 z; z.u = sign; return z.f; }		/* 0 / x = 0 */
	if (eb == 0) { f32 z; z.u = sign | 0x7f800000u; return z.f; }	/* x / 0 -> "infinity" */
	u32 ma = (va.u & 0x7fffffu) | 0x800000u;
	u32 mb = (vb.u & 0x7fffffu) | 0x800000u;
	/* compute (ma << 24) / mb via shift-and-subtract to get a 24-bit quotient
	 * plus enough guard bits for rounding. */
	u64 numerator = (u64)ma << 25;
	u64 quotient = 0;
	u64 rem = 0;
	for (int i = 24; i >= 0; i--) {
		rem = (rem << 1) | ((numerator >> (24 + i)) & 1);
		quotient <<= 1;
		if (rem >= mb) { rem -= mb; quotient |= 1; }
	}
	int exp = ea - eb + 127;
	return pack_f32(sign, exp, (u32)(quotient >> 1));
}
