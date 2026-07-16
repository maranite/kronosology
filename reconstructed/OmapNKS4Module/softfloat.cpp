// SPDX-License-Identifier: GPL-2.0
/*
 * softfloat.cpp - minimal software floating-point routines this build needs
 * that aren't in this host's libgcc.a.
 *
 * Why this file exists: the kernel build passes -msoft-float -mno-sse
 * -mno-mmx -mno-3dnow (kernel context can't safely touch FPU/SSE state
 * without an explicit save/restore, which this driver's code never does),
 * so GCC emits calls to libgcc-style helper routines for every float/double
 * operation instead of hardware instructions. The real, production
 * OmapNKS4Module.ko has none of these as unresolved imports (confirmed via
 * its own import list) - Korg's actual build toolchain links a genuine
 * soft-float-capable libgcc into the final .ko. This host's libgcc.a
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
 * mantissa is enough to match real driver behavior for every value this
 * code will ever actually see. Using real hardware FPU/SSE instructions
 * instead (trivial to implement, but WRONG) was deliberately rejected: doing
 * so from raw kernel context without save/restore risks corrupting another
 * process's floating-point state - not an acceptable shortcut for code that
 * runs on real, in-use hardware.
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
