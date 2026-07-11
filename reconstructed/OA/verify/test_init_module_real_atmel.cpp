// SPDX-License-Identifier: GPL-2.0
/*
 * test_init_module_real_atmel.cpp  -  batch 55's direct answer to the
 * sec 10.206 "does init_module() itself now reach return 0" question.
 *
 * verify/test_init_module.cpp (unchanged by this batch) already proves
 * init_module()'s own ORCHESTRATION logic reaches `return 0` given a
 * flat, controllable mock of every step function including
 * SetupAtmelForAuthorizations -- by design (it isolates step ordering/
 * unwind-cascade correctness from any one step's own real body, see
 * that file's own header comment). That test's own step-9 mock has
 * always been able to return 0 on command; it never proved the REAL
 * SetupAtmelForAuthorizations() implementation could ever actually
 * produce that 0 itself -- which, before this batch, it could not (sec
 * 10.206: cm_SetUserZone/nv2ac_dispatch_cmd/nv2ac_enable_cipher/
 * nv2ac_enable_encrypt/cm_ComputeChallenge were all bare `return -1;`
 * stubs in bar2_stubs_c.cpp, making step 9 an unconditional hard-fail).
 *
 * This file closes that gap directly: it reuses test_init_module.cpp's
 * OWN exact mock scaffolding for every step EXCEPT step 9, but links
 * the REAL init_module.cpp AND the REAL SetupAtmelForAuthorizations()
 * chain (atmel_setup.cpp/atmel_zone_io.cpp/atmel_deax.cpp/
 * atmel_primitives.cpp/atmel_challenge.cpp/nv2ac_handshake.cpp -- the
 * exact same files verify/test_atmel_setup.cpp already KATs on their
 * own) -- mocking only the genuine hardware/kernel boundary underneath
 * step 9 (stgNV2AC_sync_cmd/stgNV2AC_sync_read_cmd/msleep/
 * get_random_bytes/the nine __gmpz_* symbols), exactly as
 * test_atmel_setup.cpp does (same host-only GMP shim, same rationale --
 * see that file's own header comment for why bit-exact GMP semantics
 * aren't needed here).
 *
 * Result: with a well-behaved (virtual-chip-equivalent) AT88 responder,
 * init_module() now reaches its real, full `return 0` success path --
 * step 9 included -- using genuinely reconstructed logic the entire
 * way down to the stgNV2AC_sync_cmd/stgNV2AC_sync_read_cmd boundary,
 * not a step-level placeholder. This is the single most direct proof
 * available in this project's own host-testable form of "does
 * init_module() reach return 0" (a full live insmod additionally
 * depends on real kernel/RTAI boot-time behavior no host KAT can
 * exercise, e.g. the sec 10.184 fs_base bzImage bug -- out of scope for
 * a host KAT by construction).
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "oa_init.h"

static char g_log[4096];
static void log_call(const char *name)
{
	strcat(g_log, name);
	strcat(g_log, ";");
}

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-60s %ld\n", label, got); return; }
	printf("  FAIL  %-60s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

/* ============================================================
 * Host-only minimal GMP shim -- identical to verify/test_atmel_setup.cpp's
 * own (see that file's header comment for the full rationale). Kept as
 * a separate copy here rather than shared, matching this project's own
 * established per-test-binary mock convention.
 * ============================================================ */
#define BN_LIMBS 40
typedef unsigned long limb_t;

static void bn_zero(limb_t *d) { for (int i = 0; i < BN_LIMBS; i++) d[i] = 0; }
static int bn_nonzero(const limb_t *d) { for (int i = 0; i < BN_LIMBS; i++) if (d[i]) return 1; return 0; }
static int bn_bitlen(const limb_t *a)
{
	for (int i = BN_LIMBS - 1; i >= 0; i--)
		if (a[i])
			for (int b = 63; b >= 0; b--)
				if ((a[i] >> b) & 1UL) return i * 64 + b + 1;
	return 0;
}
static int bn_cmp(const limb_t *a, const limb_t *b)
{
	for (int i = BN_LIMBS - 1; i >= 0; i--)
		if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
	return 0;
}
static void bn_sub(limb_t *a, const limb_t *b)
{
	__int128 borrow = 0;
	for (int i = 0; i < BN_LIMBS; i++) {
		__int128 v = (__int128)a[i] - (__int128)b[i] - borrow;
		if (v < 0) { v += ((__int128)1 << 64); borrow = 1; } else borrow = 0;
		a[i] = (limb_t)v;
	}
}
static void bn_shl1(limb_t *d)
{
	limb_t carry = 0;
	for (int i = 0; i < BN_LIMBS; i++) {
		limb_t newCarry = d[i] >> 63;
		d[i] = (d[i] << 1) | carry;
		carry = newCarry;
	}
}
static void bn_mod(limb_t *r, const limb_t *n, const limb_t *m)
{
	limb_t rem[BN_LIMBS]; bn_zero(rem);
	int nbits = bn_bitlen(n);
	for (int i = nbits - 1; i >= 0; i--) {
		int limb = i / 64, bit = i % 64;
		bn_shl1(rem);
		rem[0] |= (n[limb] >> bit) & 1UL;
		if (bn_cmp(rem, m) >= 0) bn_sub(rem, m);
	}
	for (int i = 0; i < BN_LIMBS; i++) r[i] = rem[i];
}
static void bn_mul(limb_t *r, const limb_t *a, const limb_t *b)
{
	limb_t res[BN_LIMBS];
	for (int i = 0; i < BN_LIMBS; i++) res[i] = 0;
	for (int i = 0; i < BN_LIMBS; i++) {
		if (a[i] == 0) continue;
		unsigned __int128 carry = 0;
		for (int j = 0; j < BN_LIMBS - i; j++) {
			unsigned __int128 p = (unsigned __int128)a[i] * b[j] + res[i + j] + carry;
			res[i + j] = (limb_t)p;
			carry = p >> 64;
		}
	}
	for (int i = 0; i < BN_LIMBS; i++) r[i] = res[i];
}
static void bn_powm(limb_t *r, const limb_t *base, const limb_t *exp, const limb_t *mod)
{
	limb_t result[BN_LIMBS]; bn_zero(result); result[0] = 1;
	limb_t b[BN_LIMBS];
	bn_mod(b, base, mod);
	int ebits = bn_bitlen(exp);
	for (int i = ebits - 1; i >= 0; i--) {
		limb_t sq[BN_LIMBS];
		bn_mul(sq, result, result);
		bn_mod(result, sq, mod);
		int limb = i / 64, bit = i % 64;
		if ((exp[limb] >> bit) & 1UL) {
			limb_t pr[BN_LIMBS];
			bn_mul(pr, result, b);
			bn_mod(result, pr, mod);
		}
	}
	for (int i = 0; i < BN_LIMBS; i++) r[i] = result[i];
}

extern "C" {

struct oa_mpz_struct_local { int _mp_alloc; int _mp_size; unsigned long *_mp_d; };

void __gmpz_init(oa_mpz_struct_local *x)
{
	x->_mp_d = (unsigned long *)malloc(BN_LIMBS * sizeof(unsigned long));
	bn_zero(x->_mp_d);
	x->_mp_alloc = BN_LIMBS;
	x->_mp_size = 0;
}
int __gmpz_init_set_str(oa_mpz_struct_local *x, const char *s, int base)
{
	(void)base;
	__gmpz_init(x);
	limb_t *d = x->_mp_d;
	limb_t ten[BN_LIMBS]; bn_zero(ten); ten[0] = 10;
	for (const char *p = s; *p; p++) {
		if (*p < '0' || *p > '9') continue;
		limb_t tmp[BN_LIMBS];
		bn_mul(tmp, d, ten);
		unsigned __int128 carry = (unsigned)(*p - '0');
		for (int i = 0; i < BN_LIMBS && carry; i++) {
			unsigned __int128 v = (unsigned __int128)tmp[i] + carry;
			tmp[i] = (limb_t)v;
			carry = v >> 64;
		}
		for (int i = 0; i < BN_LIMBS; i++) d[i] = tmp[i];
	}
	x->_mp_size = bn_nonzero(d) ? 1 : 0;
	return 0;
}
void __gmpz_init_set_ui(oa_mpz_struct_local *x, unsigned long v)
{
	__gmpz_init(x);
	x->_mp_d[0] = v;
	x->_mp_size = v ? 1 : 0;
}
void __gmpz_mul_2exp(oa_mpz_struct_local *r, const oa_mpz_struct_local *u, unsigned long cnt)
{
	limb_t tmp[BN_LIMBS];
	for (int i = 0; i < BN_LIMBS; i++) tmp[i] = u->_mp_d[i];
	for (unsigned long i = 0; i < cnt; i++) bn_shl1(tmp);
	for (int i = 0; i < BN_LIMBS; i++) r->_mp_d[i] = tmp[i];
	r->_mp_size = bn_nonzero(r->_mp_d) ? 1 : 0;
}
void __gmpz_add_ui(oa_mpz_struct_local *r, const oa_mpz_struct_local *u, unsigned long v)
{
	limb_t tmp[BN_LIMBS];
	for (int i = 0; i < BN_LIMBS; i++) tmp[i] = u->_mp_d[i];
	unsigned __int128 carry = v;
	for (int i = 0; i < BN_LIMBS && carry; i++) {
		unsigned __int128 s = (unsigned __int128)tmp[i] + carry;
		tmp[i] = (limb_t)s;
		carry = s >> 64;
	}
	for (int i = 0; i < BN_LIMBS; i++) r->_mp_d[i] = tmp[i];
	r->_mp_size = bn_nonzero(r->_mp_d) ? 1 : 0;
}
void __gmpz_mul(oa_mpz_struct_local *r, const oa_mpz_struct_local *u, const oa_mpz_struct_local *v)
{
	limb_t tmp[BN_LIMBS];
	bn_mul(tmp, u->_mp_d, v->_mp_d);
	for (int i = 0; i < BN_LIMBS; i++) r->_mp_d[i] = tmp[i];
	r->_mp_size = bn_nonzero(r->_mp_d) ? 1 : 0;
}
void __gmpz_tdiv_r(oa_mpz_struct_local *r, const oa_mpz_struct_local *n, const oa_mpz_struct_local *d)
{
	limb_t tmp[BN_LIMBS];
	bn_mod(tmp, n->_mp_d, d->_mp_d);
	for (int i = 0; i < BN_LIMBS; i++) r->_mp_d[i] = tmp[i];
	r->_mp_size = bn_nonzero(r->_mp_d) ? 1 : 0;
}
void __gmpz_powm(oa_mpz_struct_local *r, const oa_mpz_struct_local *b, const oa_mpz_struct_local *e, const oa_mpz_struct_local *m)
{
	limb_t tmp[BN_LIMBS];
	bn_powm(tmp, b->_mp_d, e->_mp_d, m->_mp_d);
	for (int i = 0; i < BN_LIMBS; i++) r->_mp_d[i] = tmp[i];
	r->_mp_size = bn_nonzero(r->_mp_d) ? 1 : 0;
}
void __gmpz_clear(oa_mpz_struct_local *x)
{
	free(x->_mp_d);
	x->_mp_d = 0;
	x->_mp_alloc = 0;
	x->_mp_size = 0;
}

} /* extern "C" */

/* ============================================================
 * AT88 hardware/kernel boundary mocks -- well-behaved chip (every
 * status byte reads back 0xff, the real "verified" sentinel).
 * ============================================================ */
static unsigned char g_fakeChip[32];

extern "C" int stgNV2AC_sync_read_cmd(unsigned char *, unsigned char *out, int)
{
	memcpy(out, g_fakeChip, 32);
	return 0;
}
extern "C" int stgNV2AC_sync_cmd(unsigned char *, int) { return 0; }
extern "C" void msleep(unsigned int) { }
extern "C" void get_random_bytes(void *buf, unsigned int len)
{
	unsigned char *p = (unsigned char *)buf;
	for (unsigned int i = 0; i < len; i++) p[i] = (unsigned char)(0x11 * (i + 1));
}

/* ============================================================
 * init_module()'s OTHER step mocks -- identical bodies/behavior to
 * verify/test_init_module.cpp's own (see that file), EXCEPT
 * SetupAtmelForAuthorizations itself, which is left undefined here so
 * the linker pulls in the REAL body from src/auth/atmel_setup.cpp.
 * ============================================================ */
static unsigned long sLastAffinityMask;

extern "C" {

void oa_debug_marker(int) { }
void init_cpp_support(void) { log_call("init_cpp_support"); }
void cleanup_cpp_support(void) { log_call("cleanup_cpp_support"); }

static unsigned char sFakeTask[0x100];
static unsigned long sOriginalMask = 0xdeadbeef;
void *stg_get_current_task(void)
{
	*(unsigned long *)(sFakeTask + 0xbc) = sOriginalMask;
	return sFakeTask;
}
bool cpu_features_ok(void) { log_call("cpu_features_ok"); return true; }
unsigned long stg_cpumask_of_cpu(unsigned int cpu)
{
	char buf[64];
	snprintf(buf, sizeof buf, "stg_cpumask_of_cpu(%u)", cpu);
	log_call(buf);
	return 1UL << cpu;
}
int stg_set_cpus_allowed(void *task, unsigned long mask)
{
	log_call("stg_set_cpus_allowed");
	sLastAffinityMask = mask;
	*(unsigned long *)((unsigned char *)task + 0xbc) = mask;
	return 0;
}
void *CSTGFile_Open(const char *, int) { log_call("CSTGFile_Open"); return 0; }
unsigned int CSTGFile_GetFileSize(void *) { return 0; }
int  CSTGFile_Read(void *, void *, unsigned int) { return 0; }
int  CSTGFile_Close(void *) { return 0; }
int  kill_proc_info(int, void *, int) { return 0; }
int  sscanf(const char *, const char *, ...) { return 0; }

int InitializeSTGHeap(void) { log_call("InitializeSTGHeap"); return 0; }
void CleanupSharedHeap(void) { log_call("CleanupSharedHeap"); }

int InitSharedMemProcInterface(void) { log_call("InitSharedMemProcInterface"); return 0; }
void CleanupSharedMemProcInterface(void) { log_call("CleanupSharedMemProcInterface"); }

int InitPcmModProcInterface(void) { log_call("InitPcmModProcInterface"); return 0; }
void CleanupPcmModProcInterface(void) { log_call("CleanupPcmModProcInterface"); }

int setup_global_resources(int) { log_call("setup_global_resources"); return 0; }
void cleanup_global_resources(void) { log_call("cleanup_global_resources"); }

int setup_stg_decrypt_daemons(void) { log_call("setup_stg_decrypt_daemons"); return 0; }
void cleanup_stg_decrypt_daemons(void) { }

int load_global_resources(void) { log_call("load_global_resources"); return 0; }

int setup_stg_daemons(void) { log_call("setup_stg_daemons"); return 0; }
void cleanup_stg_daemons(void) { log_call("cleanup_stg_daemons"); }

int CSTGAudioManager_StartAudioEngine(void) { log_call("CSTGAudioManager_StartAudioEngine"); return 1; }
void CSTGAudioManager_StopAudioEngine(void) { log_call("CSTGAudioManager_StopAudioEngine"); }
void CSTGAudioManager_EnableAudioManagerThread(void) { log_call("CSTGAudioManager_EnableAudioManagerThread"); }

int CSTGKeybedInterface_Startup(void) { log_call("CSTGKeybedInterface_Startup"); return 1; }
void CSTGKeybedInterface_Cleanup(void) { log_call("CSTGKeybedInterface_Cleanup"); }

int CSTGDrumPadInterface_Initialize(void) { log_call("CSTGDrumPadInterface_Initialize"); return -1; }
void CSTGDrumPadInterface_Cleanup(void) { log_call("CSTGDrumPadInterface_Cleanup"); }

int stg_rtfifo_init(void) { log_call("stg_rtfifo_init"); return 0; }
void stg_rtfifo_cleanup(void) { }

void IncProgressBar(void) { log_call("IncProgressBar"); }
void SetInstalledOptions(int) { }
void stg_log_startup_error(const char *) { log_call("stg_log_startup_error"); }

int gModuleParam10 = 0;
int gModuleParam14 = 0;
int gModuleParam18 = 0;

int printk(const char *, ...) { return 0; }
void rt_printk(const char *, ...) { }
void __const_udelay(unsigned long) { }
unsigned long long stg_rdtsc(void) { return 0x1122334455667788ULL; }

} /* extern "C" */

int init_module(void);

int main(void)
{
	printf("init_module() -- REAL SetupAtmelForAuthorizations() end to end\n");
	printf("=================================================================\n");
	printf("[1] All OTHER steps mocked-to-succeed (same as test_init_module.cpp's\n"
	       "    own scenario [1]), but step 9 links the REAL AT88 GPA-handshake\n"
	       "    chain against a well-behaved chip -- does init_module() reach a\n"
	       "    genuine return 0?\n");

	for (int i = 0; i < 32; i++) g_fakeChip[i] = 0xff;
	g_log[0] = 0;
	sLastAffinityMask = 0;

	int rc = init_module();

	check_eq("init_module() returns 0 (success, REAL step 9)", rc, 0);
	check_eq("SetupAtmelForAuthorizations reached in the real call chain",
		 strstr(g_log, "setup_global_resources;IncProgressBar;setup_stg_decrypt_daemons") != 0, 1);
	check_eq("final stg_set_cpus_allowed call restored the ORIGINAL mask (0xdeadbeef)",
		 (long)sLastAffinityMask, (long)0xdeadbeef);

	printf("[2] Misbehaving chip (status byte never 0xff) -> step 9 hard-fails for\n"
	       "    real (rc == -3 deep inside SetupAtmelForAuthorizations), init_module()\n"
	       "    correctly unwinds via fail_globalres and returns -1\n");
	for (int i = 0; i < 32; i++) g_fakeChip[i] = 0x00;
	g_log[0] = 0;
	rc = init_module();
	check_eq("init_module() returns -1 (real AT88 handshake failure)", rc, -1);
	check_eq("cascade reached cleanup_global_resources (fail_globalres label)",
		 strstr(g_log, "cleanup_global_resources") != 0, 1);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "ALL TESTS PASSED");
	return g_fail ? 1 : 0;
}
