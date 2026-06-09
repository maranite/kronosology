/*
 * oa_authgen.ko — EX authorisation string generator for Korg Kronos
 *
 * Reads the 24-byte per-device Atmel NV2AC chip secret using a native
 * implementation of the GPA (Group Authentication Protocol) — reverse-
 * engineered from OA_322.ko.  Only stgNV2AC_sync_cmd and
 * stgNV2AC_sync_read_cmd (OmapNKS4Module.ko) are required; OA.ko need not
 * be present.  Compatible with Korg Kronos UpdateOS context.
 *
 * Interface: /proc/.oaauth
 *   write "GEN:<option_id>"  e.g. "GEN:S285"  → auth string becomes available
 *   read                     returns 24-char auth string + newline from last GEN
 *
 * See kronosology/docs/crypto/auth_string_algorithm.md for full algorithm spec.
 *
 * Build:  make -C <kronos-kernel-src> M=$(pwd) ARCH=i386 modules
 * Load:   insmod oa_authgen.ko
 * Unload: rmmod oa_authgen
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/random.h>
MODULE_LICENSE("GPL");
MODULE_AUTHOR("kronos-re");
MODULE_DESCRIPTION("EX auth string generator for Kronos OA.ko");

/*
 * setup_atmel_addr — OA.ko's SetupAtmelForAuthorizations (optional).
 * chip_read_addr   — OA.ko's fFfFfFfFfFfF13 encrypted chip-read (optional).
 *
 * If both are omitted the module runs its own native GPA implementation and
 * needs no OA.ko — suitable for UpdateOS where OA.ko is not loaded.
 *
 * If OA.ko IS loaded and addresses are known, pass them for a faster path:
 *   SETUP=$(grep ' SetupAtmelForAuthorizations' /proc/kallsyms | cut -d' ' -f1)
 *   READ=$(grep ' fFfFfFfFfFfF13.*\[OA\]' /proc/kallsyms | cut -d' ' -f1)
 *   insmod oa_authgen.ko setup_atmel_addr=0x${SETUP} chip_read_addr=0x${READ}
 */
static unsigned long setup_atmel_addr;
module_param(setup_atmel_addr, ulong, 0444);

static unsigned long chip_read_addr;
module_param(chip_read_addr, ulong, 0444);

/* oa_mode_addr: runtime address of OA.ko BSS mode variable (0x5c90c0 area).
 * If set, CHIP command also shows mode byte and XOR byte values.
 * Find: grep -w OA /proc/kallsyms | grep oa_mode  (if exported), or compute
 * from nm address 0x5c90c0 + OA.ko text base. */
static unsigned long oa_mode_addr;
module_param(oa_mode_addr, ulong, 0444);

/* verify_auth_addr: runtime address of OA.ko VerifyAuthorizationString (nm 0x207de0).
 * VERIFY:<auth_str> calls it directly and reports the return code + extracted opt id.
 *   VERIFY_ADDR=$(grep ' VerifyAuthorizationString' /proc/kallsyms | cut -d' ' -f1)
 *   (re-insmod with verify_auth_addr=0x${VERIFY_ADDR})
 * If rc=0, auth string crypto is correct; problem is downstream in product lookup.
 * If rc!=0, auth string is being rejected by the decode/Blowfish/MD5 step. */
static unsigned long verify_auth_addr;
module_param(verify_auth_addr, ulong, 0444);

/* decode_addr: runtime address of OA.ko DecodeBytesFromAscii (nm 0x4f39d0, runtime 0x5a1d99c0).
 * DECODE:<auth_str> calls it and returns the 15 decoded bytes as hex.
 * Signature: int DecodeBytesFromAscii(char *out_buf, const char *auth_str) regparm(3) */
static unsigned long decode_addr;
module_param(decode_addr, ulong, 0444);

/* --------------------------------------------------------------------------
 * stgNV2AC_sync_read_cmd — exported by OmapNKS4Module.ko
 * Calling convention: __regparm(3) — eax=cmd_data ptr, edx=response_dest ptr
 *   cmd_data:      pointer to a 16-byte command packet {0xb2, 0x00, addr, count, zeros}
 *   response_dest: pointer to a static kernel buffer that receives the DMA response
 * Returns 0 on success, negative error code on failure.
 * -------------------------------------------------------------------------- */
extern int stgNV2AC_sync_read_cmd(void *cmd_data, void *response_dest);
extern int stgNV2AC_sync_cmd(void *cmd_data, int cmd_size);

/* 16-byte Atmel CryptoMemory Read Config Zone command packet */
struct nv2ac_cmd {
	u8 opcode;   /* 0xb2 = Read Config Zone */
	u8 zero;     /* 0x00 */
	u8 addr;     /* byte address in config zone */
	u8 count;    /* number of bytes to read */
	u8 pad[12];  /* must be zero */
};

/* --------------------------------------------------------------------------
 * Blowfish implementation
 * Copied verbatim from linux/crypto/blowfish.c (the same file the Kronos
 * kernel was built from), so we don't depend on CONFIG_CRYPTO_BLOWFISH.
 * Only the key-schedule (bf_setkey_ctx) and ECB encrypt (bf_encrypt_block)
 * are exposed; decrypt is not needed.
 * -------------------------------------------------------------------------- */

#define BF_BLOCK_SIZE 8

struct bf_ctx {
	u32 p[18];
	u32 s[1024];
};

static const u32 bf_pbox[18] = {
	0x243f6a88, 0x85a308d3, 0x13198a2e, 0x03707344,
	0xa4093822, 0x299f31d0, 0x082efa98, 0xec4e6c89,
	0x452821e6, 0x38d01377, 0xbe5466cf, 0x34e90c6c,
	0xc0ac29b7, 0xc97c50dd, 0x3f84d5b5, 0xb5470917,
	0x9216d5d9, 0x8979fb1b,
};

static const u32 bf_sbox[1024] = {
	0xd1310ba6, 0x98dfb5ac, 0x2ffd72db, 0xd01adfb7,
	0xb8e1afed, 0x6a267e96, 0xba7c9045, 0xf12c7f99,
	0x24a19947, 0xb3916cf7, 0x0801f2e2, 0x858efc16,
	0x636920d8, 0x71574e69, 0xa458fea3, 0xf4933d7e,
	0x0d95748f, 0x728eb658, 0x718bcd58, 0x82154aee,
	0x7b54a41d, 0xc25a59b5, 0x9c30d539, 0x2af26013,
	0xc5d1b023, 0x286085f0, 0xca417918, 0xb8db38ef,
	0x8e79dcb0, 0x603a180e, 0x6c9e0e8b, 0xb01e8a3e,
	0xd71577c1, 0xbd314b27, 0x78af2fda, 0x55605c60,
	0xe65525f3, 0xaa55ab94, 0x57489862, 0x63e81440,
	0x55ca396a, 0x2aab10b6, 0xb4cc5c34, 0x1141e8ce,
	0xa15486af, 0x7c72e993, 0xb3ee1411, 0x636fbc2a,
	0x2ba9c55d, 0x741831f6, 0xce5c3e16, 0x9b87931e,
	0xafd6ba33, 0x6c24cf5c, 0x7a325381, 0x28958677,
	0x3b8f4898, 0x6b4bb9af, 0xc4bfe81b, 0x66282193,
	0x61d809cc, 0xfb21a991, 0x487cac60, 0x5dec8032,
	0xef845d5d, 0xe98575b1, 0xdc262302, 0xeb651b88,
	0x23893e81, 0xd396acc5, 0x0f6d6ff3, 0x83f44239,
	0x2e0b4482, 0xa4842004, 0x69c8f04a, 0x9e1f9b5e,
	0x21c66842, 0xf6e96c9a, 0x670c9c61, 0xabd388f0,
	0x6a51a0d2, 0xd8542f68, 0x960fa728, 0xab5133a3,
	0x6eef0b6c, 0x137a3be4, 0xba3bf050, 0x7efb2a98,
	0xa1f1651d, 0x39af0176, 0x66ca593e, 0x82430e88,
	0x8cee8619, 0x456f9fb4, 0x7d84a5c3, 0x3b8b5ebe,
	0xe06f75d8, 0x85c12073, 0x401a449f, 0x56c16aa6,
	0x4ed3aa62, 0x363f7706, 0x1bfedf72, 0x429b023d,
	0x37d0d724, 0xd00a1248, 0xdb0fead3, 0x49f1c09b,
	0x075372c9, 0x80991b7b, 0x25d479d8, 0xf6e8def7,
	0xe3fe501a, 0xb6794c3b, 0x976ce0bd, 0x04c006ba,
	0xc1a94fb6, 0x409f60c4, 0x5e5c9ec2, 0x196a2463,
	0x68fb6faf, 0x3e6c53b5, 0x1339b2eb, 0x3b52ec6f,
	0x6dfc511f, 0x9b30952c, 0xcc814544, 0xaf5ebd09,
	0xbee3d004, 0xde334afd, 0x660f2807, 0x192e4bb3,
	0xc0cba857, 0x45c8740f, 0xd20b5f39, 0xb9d3fbdb,
	0x5579c0bd, 0x1a60320a, 0xd6a100c6, 0x402c7279,
	0x679f25fe, 0xfb1fa3cc, 0x8ea5e9f8, 0xdb3222f8,
	0x3c7516df, 0xfd616b15, 0x2f501ec8, 0xad0552ab,
	0x323db5fa, 0xfd238760, 0x53317b48, 0x3e00df82,
	0x9e5c57bb, 0xca6f8ca0, 0x1a87562e, 0xdf1769db,
	0xd542a8f6, 0x287effc3, 0xac6732c6, 0x8c4f5573,
	0x695b27b0, 0xbbca58c8, 0xe1ffa35d, 0xb8f011a0,
	0x10fa3d98, 0xfd2183b8, 0x4afcb56c, 0x2dd1d35b,
	0x9a53e479, 0xb6f84565, 0xd28e49bc, 0x4bfb9790,
	0xe1ddf2da, 0xa4cb7e33, 0x62fb1341, 0xcee4c6e8,
	0xef20cada, 0x36774c01, 0xd07e9efe, 0x2bf11fb4,
	0x95dbda4d, 0xae909198, 0xeaad8e71, 0x6b93d5a0,
	0xd08ed1d0, 0xafc725e0, 0x8e3c5b2f, 0x8e7594b7,
	0x8ff6e2fb, 0xf2122b64, 0x8888b812, 0x900df01c,
	0x4fad5ea0, 0x688fc31c, 0xd1cff191, 0xb3a8c1ad,
	0x2f2f2218, 0xbe0e1777, 0xea752dfe, 0x8b021fa1,
	0xe5a0cc0f, 0xb56f74e8, 0x18acf3d6, 0xce89e299,
	0xb4a84fe0, 0xfd13e0b7, 0x7cc43b81, 0xd2ada8d9,
	0x165fa266, 0x80957705, 0x93cc7314, 0x211a1477,
	0xe6ad2065, 0x77b5fa86, 0xc75442f5, 0xfb9d35cf,
	0xebcdaf0c, 0x7b3e89a0, 0xd6411bd3, 0xae1e7e49,
	0x00250e2d, 0x2071b35e, 0x226800bb, 0x57b8e0af,
	0x2464369b, 0xf009b91e, 0x5563911d, 0x59dfa6aa,
	0x78c14389, 0xd95a537f, 0x207d5ba2, 0x02e5b9c5,
	0x83260376, 0x6295cfa9, 0x11c81968, 0x4e734a41,
	0xb3472dca, 0x7b14a94a, 0x1b510052, 0x9a532915,
	0xd60f573f, 0xbc9bc6e4, 0x2b60a476, 0x81e67400,
	0x08ba6fb5, 0x571be91f, 0xf296ec6b, 0x2a0dd915,
	0xb6636521, 0xe7b9f9b6, 0xff34052e, 0xc5855664,
	0x53b02d5d, 0xa99f8fa1, 0x08ba4799, 0x6e85076a,
	/* S-box 1 */
	0x4b7a70e9, 0xb5b32944, 0xdb75092e, 0xc4192623,
	0xad6ea6b0, 0x49a7df7d, 0x9cee60b8, 0x8fedb266,
	0xecaa8c71, 0x699a17ff, 0x5664526c, 0xc2b19ee1,
	0x193602a5, 0x75094c29, 0xa0591340, 0xe4183a3e,
	0x3f54989a, 0x5b429d65, 0x6b8fe4d6, 0x99f73fd6,
	0xa1d29c07, 0xefe830f5, 0x4d2d38e6, 0xf0255dc1,
	0x4cdd2086, 0x8470eb26, 0x6382e9c6, 0x021ecc5e,
	0x09686b3f, 0x3ebaefc9, 0x3c971814, 0x6b6a70a1,
	0x687f3584, 0x52a0e286, 0xb79c5305, 0xaa500737,
	0x3e07841c, 0x7fdeae5c, 0x8e7d44ec, 0x5716f2b8,
	0xb03ada37, 0xf0500c0d, 0xf01c1f04, 0x0200b3ff,
	0xae0cf51a, 0x3cb574b2, 0x25837a58, 0xdc0921bd,
	0xd19113f9, 0x7ca92ff6, 0x94324773, 0x22f54701,
	0x3ae5e581, 0x37c2dadc, 0xc8b57634, 0x9af3dda7,
	0xa9446146, 0x0fd0030e, 0xecc8c73e, 0xa4751e41,
	0xe238cd99, 0x3bea0e2f, 0x3280bba1, 0x183eb331,
	0x4e548b38, 0x4f6db908, 0x6f420d03, 0xf60a04bf,
	0x2cb81290, 0x24977c79, 0x5679b072, 0xbcaf89af,
	0xde9a771f, 0xd9930810, 0xb38bae12, 0xdccf3f2e,
	0x5512721f, 0x2e6b7124, 0x501adde6, 0x9f84cd87,
	0x7a584718, 0x7408da17, 0xbc9f9abc, 0xe94b7d8c,
	0xec7aec3a, 0xdb851dfa, 0x63094366, 0xc464c3d2,
	0xef1c1847, 0x3215d908, 0xdd433b37, 0x24c2ba16,
	0x12a14d43, 0x2a65c451, 0x50940002, 0x133ae4dd,
	0x71dff89e, 0x10314e55, 0x81ac77d6, 0x5f11199b,
	0x043556f1, 0xd7a3c76b, 0x3c11183b, 0x5924a509,
	0xf28fe6ed, 0x97f1fbfa, 0x9ebabf2c, 0x1e153c6e,
	0x86e34570, 0xeae96fb1, 0x860e5e0a, 0x5a3e2ab3,
	0x771fe71c, 0x4e3d06fa, 0x2965dcb9, 0x99e71d0f,
	0x803e89d6, 0x5266c825, 0x2e4cc978, 0x9c10b36a,
	0xc6150eba, 0x94e2ea78, 0xa5fc3c53, 0x1e0a2df4,
	0xf2f74ea7, 0x361d2b3d, 0x1939260f, 0x19c27960,
	0x5223a708, 0xf71312b6, 0xebadfe6e, 0xeac31f66,
	0xe3bc4595, 0xa67bc883, 0xb17f37d1, 0x018cff28,
	0xc332ddef, 0xbe6c5aa5, 0x65582185, 0x68ab9802,
	0xeecea50f, 0xdb2f953b, 0x2aef7dad, 0x5b6e2f84,
	0x1521b628, 0x29076170, 0xecdd4775, 0x619f1510,
	0x13cca830, 0xeb61bd96, 0x0334fe1e, 0xaa0363cf,
	0xb5735c90, 0x4c70a239, 0xd59e9e0b, 0xcbaade14,
	0xeecc86bc, 0x60622ca7, 0x9cab5cab, 0xb2f3846e,
	0x648b1eaf, 0x19bdf0ca, 0xa02369b9, 0x655abb50,
	0x40685a32, 0x3c2ab4b3, 0x319ee9d5, 0xc021b8f7,
	0x9b540b19, 0x875fa099, 0x95f7997e, 0x623d7da8,
	0xf837889a, 0x97e32d77, 0x11ed935f, 0x16681281,
	0x0e358829, 0xc7e61fd6, 0x96dedfa1, 0x7858ba99,
	0x57f584a5, 0x1b227263, 0x9b83c3ff, 0x1ac24696,
	0xcdb30aeb, 0x532e3054, 0x8fd948e4, 0x6dbc3128,
	0x58ebf2ef, 0x34c6ffea, 0xfe28ed61, 0xee7c3c73,
	0x5d4a14d9, 0xe864b7e3, 0x42105d14, 0x203e13e0,
	0x45eee2b6, 0xa3aaabea, 0xdb6c4f15, 0xfacb4fd0,
	0xc742f442, 0xef6abbb5, 0x654f3b1d, 0x41cd2105,
	0xd81e799e, 0x86854dc7, 0xe44b476a, 0x3d816250,
	0xcf62a1f2, 0x5b8d2646, 0xfc8883a0, 0xc1c7b6a3,
	0x7f1524c3, 0x69cb7492, 0x47848a0b, 0x5692b285,
	0x095bbf00, 0xad19489d, 0x1462b174, 0x23820e00,
	0x58428d2a, 0x0c55f5ea, 0x1dadf43e, 0x233f7061,
	0x3372f092, 0x8d937e41, 0xd65fecf1, 0x6c223bdb,
	0x7cde3759, 0xcbee7460, 0x4085f2a7, 0xce77326e,
	0xa6078084, 0x19f8509e, 0xe8efd855, 0x61d99735,
	0xa969a7aa, 0xc50c06c2, 0x5a04abfc, 0x800bcadc,
	0x9e447a2e, 0xc3453484, 0xfdd56705, 0x0e1e9ec9,
	0xdb73dbd3, 0x105588cd, 0x675fda79, 0xe3674340,
	0xc5c43465, 0x713e38d8, 0x3d28f89e, 0xf16dff20,
	0x153e21e7, 0x8fb03d4a, 0xe6e39f2b, 0xdb83adf7,
	/* S-box 2 */
	0xe93d5a68, 0x948140f7, 0xf64c261c, 0x94692934,
	0x411520f7, 0x7602d4f7, 0xbcf46b2e, 0xd4a20068,
	0xd4082471, 0x3320f46a, 0x43b7d4b7, 0x500061af,
	0x1e39f62e, 0x97244546, 0x14214f74, 0xbf8b8840,
	0x4d95fc1d, 0x96b591af, 0x70f4ddd3, 0x66a02f45,
	0xbfbc09ec, 0x03bd9785, 0x7fac6dd0, 0x31cb8504,
	0x96eb27b3, 0x55fd3941, 0xda2547e6, 0xabca0a9a,
	0x28507825, 0x530429f4, 0x0a2c86da, 0xe9b66dfb,
	0x68dc1462, 0xd7486900, 0x680ec0a4, 0x27a18dee,
	0x4f3ffea2, 0xe887ad8c, 0xb58ce006, 0x7af4d6b6,
	0xaace1e7c, 0xd3375fec, 0xce78a399, 0x406b2a42,
	0x20fe9e35, 0xd9f385b9, 0xee39d7ab, 0x3b124e8b,
	0x1dc9faf7, 0x4b6d1856, 0x26a36631, 0xeae397b2,
	0x3a6efa74, 0xdd5b4332, 0x6841e7f7, 0xca7820fb,
	0xfb0af54e, 0xd8feb397, 0x454056ac, 0xba489527,
	0x55533a3a, 0x20838d87, 0xfe6ba9b7, 0xd096954b,
	0x55a867bc, 0xa1159a58, 0xcca92963, 0x99e1db33,
	0xa62a4a56, 0x3f3125f9, 0x5ef47e1c, 0x9029317c,
	0xfdf8e802, 0x04272f70, 0x80bb155c, 0x05282ce3,
	0x95c11548, 0xe4c66d22, 0x48c1133f, 0xc70f86dc,
	0x07f9c9ee, 0x41041f0f, 0x404779a4, 0x5d886e17,
	0x325f51eb, 0xd59bc0d1, 0xf2bcc18f, 0x41113564,
	0x257b7834, 0x602a9c60, 0xdff8e8a3, 0x1f636c1b,
	0x0e12b4c2, 0x02e1329e, 0xaf664fd1, 0xcad18115,
	0x6b2395e0, 0x333e92e1, 0x3b240b62, 0xeebeb922,
	0x85b2a20e, 0xe6ba0d99, 0xde720c8c, 0x2da2f728,
	0xd0127845, 0x95b794fd, 0x647d0862, 0xe7ccf5f0,
	0x5449a36f, 0x877d48fa, 0xc39dfd27, 0xf33e8d1e,
	0x0a476341, 0x992eff74, 0x3a6f6eab, 0xf4f8fd37,
	0xa812dc60, 0xa1ebddf8, 0x991be14c, 0xdb6e6b0d,
	0xc67b5510, 0x6d672c37, 0x2765d43b, 0xdcd0e804,
	0xf1290dc7, 0xcc00ffa3, 0xb5390f92, 0x690fed0b,
	0x667b9ffb, 0xcedb7d9c, 0xa091cf0b, 0xd9155ea3,
	0xbb132f88, 0x515bad24, 0x7b9479bf, 0x763bd6eb,
	0x37392eb3, 0xcc115979, 0x8026e297, 0xf42e312d,
	0x6842ada7, 0xc66a2b3b, 0x12754ccc, 0x782ef11c,
	0x6a124237, 0xb79251e7, 0x06a1bbe6, 0x4bfb6350,
	0x1a6b1018, 0x11caedfa, 0x3d25bdd8, 0xe2e1c3c9,
	0x44421659, 0x0a121386, 0xd90cec6e, 0xd5abea2a,
	0x64af674e, 0xda86a85f, 0xbebfe988, 0x64e4c3fe,
	0x9dbc8057, 0xf0f7c086, 0x60787bf8, 0x6003604d,
	0xd1fd8346, 0xf6381fb0, 0x7745ae04, 0xd736fccc,
	0x83426b33, 0xf01eab71, 0xb0804187, 0x3c005e5f,
	0x77a057be, 0xbde8ae24, 0x55464299, 0xbf582e61,
	0x4e58f48f, 0xf2ddfda2, 0xf474ef38, 0x8789bdc2,
	0x5366f9c3, 0xc8b38e74, 0xb475f255, 0x46fcd9b9,
	0x7aeb2661, 0x8b1ddf84, 0x846a0e79, 0x915f95e2,
	0x466e598e, 0x20b45770, 0x8cd55591, 0xc902de4c,
	0xb90bace1, 0xbb8205d0, 0x11a86248, 0x7574a99e,
	0xb77f19b6, 0xe0a9dc09, 0x662d09a1, 0xc4324633,
	0xe85a1f02, 0x09f0be8c, 0x4a99a025, 0x1d6efe10,
	0x1ab93d1d, 0x0ba5a4df, 0xa186f20f, 0x2868f169,
	0xdcb7da83, 0x573906fe, 0xa1e2ce9b, 0x4fcd7f52,
	0x50115e01, 0xa70683fa, 0xa002b5c4, 0x0de6d027,
	0x9af88c27, 0x773f8641, 0xc3604c06, 0x61a806b5,
	0xf0177a28, 0xc0f586e0, 0x006058aa, 0x30dc7d62,
	0x11e69ed7, 0x2338ea63, 0x53c2dd94, 0xc2c21634,
	0xbbcbee56, 0x90bcb6de, 0xebfc7da1, 0xce591d76,
	0x6f05e409, 0x4b7c0188, 0x39720a3d, 0x7c927c24,
	0x86e3725f, 0x724d9db9, 0x1ac15bb4, 0xd39eb8fc,
	0xed545578, 0x08fca5b5, 0xd83d7cd3, 0x4dad0fc4,
	0x1e50ef5e, 0xb161e6f8, 0xa28514d9, 0x6c51133c,
	0x6fd5c7e7, 0x56e14ec4, 0x362abfce, 0xddc6c837,
	0xd79a3234, 0x92638212, 0x670efa8e, 0x406000e0,
	/* S-box 3 */
	0x3a39ce37, 0xd3faf5cf, 0xabc27737, 0x5ac52d1b,
	0x5cb0679e, 0x4fa33742, 0xd3822740, 0x99bc9bbe,
	0xd5118e9d, 0xbf0f7315, 0xd62d1c7e, 0xc700c47b,
	0xb78c1b6b, 0x21a19045, 0xb26eb1be, 0x6a366eb4,
	0x5748ab2f, 0xbc946e79, 0xc6a376d2, 0x6549c2c8,
	0x530ff8ee, 0x468dde7d, 0xd5730a1d, 0x4cd04dc6,
	0x2939bbdb, 0xa9ba4650, 0xac9526e8, 0xbe5ee304,
	0xa1fad5f0, 0x6a2d519a, 0x63ef8ce2, 0x9a86ee22,
	0xc089c2b8, 0x43242ef6, 0xa51e03aa, 0x9cf2d0a4,
	0x83c061ba, 0x9be96a4d, 0x8fe51550, 0xba645bd6,
	0x2826a2f9, 0xa73a3ae1, 0x4ba99586, 0xef5562e9,
	0xc72fefd3, 0xf752f7da, 0x3f046f69, 0x77fa0a59,
	0x80e4a915, 0x87b08601, 0x9b09e6ad, 0x3b3ee593,
	0xe990fd5a, 0x9e34d797, 0x2cf0b7d9, 0x022b8b51,
	0x96d5ac3a, 0x017da67d, 0xd1cf3ed6, 0x7c7d2d28,
	0x1f9f25cf, 0xadf2b89b, 0x5ad6b472, 0x5a88f54c,
	0xe029ac71, 0xe019a5e6, 0x47b0acfd, 0xed93fa9b,
	0xe8d3c48d, 0x283b57cc, 0xf8d56629, 0x79132e28,
	0x785f0191, 0xed756055, 0xf7960e44, 0xe3d35e8c,
	0x15056dd4, 0x88f46dba, 0x03a16125, 0x0564f0bd,
	0xc3eb9e15, 0x3c9057a2, 0x97271aec, 0xa93a072a,
	0x1b3f6d9b, 0x1e6321f5, 0xf59c66fb, 0x26dcf319,
	0x7533d928, 0xb155fdf5, 0x03563482, 0x8aba3cbb,
	0x28517711, 0xc20ad9f8, 0xabcc5167, 0xccad925f,
	0x4de81751, 0x3830dc8e, 0x379d5862, 0x9320f991,
	0xea7a90c2, 0xfb3e7bce, 0x5121ce64, 0x774fbe32,
	0xa8b6e37e, 0xc3293d46, 0x48de5369, 0x6413e680,
	0xa2ae0810, 0xdd6db224, 0x69852dfd, 0x09072166,
	0xb39a460a, 0x6445c0dd, 0x586cdecf, 0x1c20c8ae,
	0x5bbef7dd, 0x1b588d40, 0xccd2017f, 0x6bb4e3bb,
	0xdda26a7e, 0x3a59ff45, 0x3e350a44, 0xbcb4cdd5,
	0x72eacea8, 0xfa6484bb, 0x8d6612ae, 0xbf3c6f47,
	0xd29be463, 0x542f5d9e, 0xaec2771b, 0xf64e6370,
	0x740e0d8d, 0xe75b1357, 0xf8721671, 0xaf537d5d,
	0x4040cb08, 0x4eb4e2cc, 0x34d2466a, 0x0115af84,
	0xe1b00428, 0x95983a1d, 0x06b89fb4, 0xce6ea048,
	0x6f3f3b82, 0x3520ab82, 0x011a1d4b, 0x277227f8,
	0x611560b1, 0xe7933fdc, 0xbb3a792b, 0x344525bd,
	0xa08839e1, 0x51ce794b, 0x2f32c9b7, 0xa01fbac9,
	0xe01cc87e, 0xbcc7d1f6, 0xcf0111c3, 0xa1e8aac7,
	0x1a908749, 0xd44fbd9a, 0xd0dadecb, 0xd50ada38,
	0x0339c32a, 0xc6913667, 0x8df9317c, 0xe0b12b4f,
	0xf79e59b7, 0x43f5bb3a, 0xf2d519ff, 0x27d9459c,
	0xbf97222c, 0x15e6fc2a, 0x0f91fc71, 0x9b941525,
	0xfae59361, 0xceb69ceb, 0xc2a86459, 0x12baa8d1,
	0xb6c1075e, 0xe3056a0c, 0x10d25065, 0xcb03a442,
	0xe0ec6e0e, 0x1698db3b, 0x4c98a0be, 0x3278e964,
	0x9f1f9532, 0xe0d392df, 0xd3a0342b, 0x8971f21e,
	0x1b0a7441, 0x4ba3348c, 0xc5be7120, 0xc37632d8,
	0xdf359f8d, 0x9b992f2e, 0xe60b6f47, 0x0fe3f11d,
	0xe54cda54, 0x1edad891, 0xce6279cf, 0xcd3e7e6f,
	0x1618b166, 0xfd2c1d05, 0x848fd2c5, 0xf6fb2299,
	0xf523f357, 0xa6327623, 0x93a83531, 0x56cccd02,
	0xacf08162, 0x5a75ebb5, 0x6e163697, 0x88d273cc,
	0xde966292, 0x81b949d0, 0x4c50901b, 0x71c65614,
	0xe6c6c7bd, 0x327a140a, 0x45e1d006, 0xc3f27b9a,
	0xc9aa53fd, 0x62a80f00, 0xbb25bfe2, 0x35bdd2f6,
	0x71126905, 0xb2040222, 0xb6cbcf7c, 0xcd769c2b,
	0x53113ec0, 0x1640e3d3, 0x38abbd60, 0x2547adf0,
	0xba38209c, 0xf746ce76, 0x77afa1c5, 0x20756060,
	0x85cbfe4e, 0x8ae88dd8, 0x7aaaf9b0, 0x4cf9aa7e,
	0x1948c25c, 0x02fb8a8c, 0x01c36ae4, 0xd6ebe1f9,
	0x90d4f869, 0xa65cdea0, 0x3f09252d, 0xc208e69f,
	0xb74e6132, 0xce77e25b, 0x578fdfe3, 0x3ac372e6,
};

#define GET32_3(x) (((x) & 0xff))
#define GET32_2(x) (((x) >> 8) & 0xff)
#define GET32_1(x) (((x) >> 16) & 0xff)
#define GET32_0(x) (((x) >> 24) & 0xff)
#define bf_F(x) (((S[GET32_0(x)] + S[256 + GET32_1(x)]) ^ \
		  S[512 + GET32_2(x)]) + S[768 + GET32_3(x)])
#define ROUND(a, b, n)  (b) ^= P[n]; (a) ^= bf_F(b)

static void bf_encrypt_block(struct bf_ctx *ctx, u32 *out, const u32 *in)
{
	const u32 *P = ctx->p;
	const u32 *S = ctx->s;
	u32 yl = in[0], yr = in[1];

	ROUND(yr, yl,  0); ROUND(yl, yr,  1);
	ROUND(yr, yl,  2); ROUND(yl, yr,  3);
	ROUND(yr, yl,  4); ROUND(yl, yr,  5);
	ROUND(yr, yl,  6); ROUND(yl, yr,  7);
	ROUND(yr, yl,  8); ROUND(yl, yr,  9);
	ROUND(yr, yl, 10); ROUND(yl, yr, 11);
	ROUND(yr, yl, 12); ROUND(yl, yr, 13);
	ROUND(yr, yl, 14); ROUND(yl, yr, 15);

	yl ^= P[16];
	yr ^= P[17];
	out[0] = yr;
	out[1] = yl;
}

/* Standard Blowfish key schedule (identical to linux/crypto/blowfish.c) */
static void bf_setkey_ctx(struct bf_ctx *ctx, const u8 *key, unsigned int keylen)
{
	u32 *P = ctx->p;
	u32 *S = ctx->s;
	int i, j, count;
	u32 data[2], temp;

	for (i = 0, count = 0; i < 256; i++)
		for (j = 0; j < 4; j++, count++)
			S[count] = bf_sbox[count];

	for (i = 0; i < 18; i++)
		P[i] = bf_pbox[i];

	for (j = 0, i = 0; i < 18; i++) {
		temp = ((u32)key[j] << 24) |
		       ((u32)key[(j + 1) % keylen] << 16) |
		       ((u32)key[(j + 2) % keylen] <<  8) |
		       ((u32)key[(j + 3) % keylen]);
		P[i] ^= temp;
		j = (j + 4) % keylen;
	}

	data[0] = data[1] = 0;
	for (i = 0; i < 18; i += 2) {
		bf_encrypt_block(ctx, data, data);
		P[i] = data[0]; P[i + 1] = data[1];
	}
	for (i = 0; i < 4; i++) {
		for (j = 0, count = i * 256; j < 256; j += 2, count += 2) {
			bf_encrypt_block(ctx, data, data);
			S[count] = data[0]; S[count + 1] = data[1];
		}
	}
}

#undef ROUND
#undef bf_F
#undef GET32_0
#undef GET32_1
#undef GET32_2
#undef GET32_3

/* --------------------------------------------------------------------------
 * CFB-64 encrypt using Blowfish (full 8-byte block feedback, ciphertext mode)
 *
 * Matches OA.ko's moancjsd82 algorithm exactly:
 *   - 16-byte Blowfish key (chip[0:16])
 *   - 8-byte IV / shift register (chip[16:24])
 *   - New keystream block every 8 bytes: ks = BF_ECB(current_sr)
 *   - sr is updated with ciphertext bytes (standard CFB-64)
 *   - OA.ko verifies by passing ciphertext as input; same code handles both
 *     directions since CFB-64 en/decrypt are symmetric in XOR step.
 * -------------------------------------------------------------------------- */
static void blowfish_cfb64_encrypt(const u8 *key, unsigned int key_len,
				   const u8 *iv_8,
				   const u8 *plaintext, u8 *ciphertext,
				   unsigned int len)
{
	struct bf_ctx *ctx;
	u8 sr[8];
	u8 ks[8];
	u32 block_in[2], block_out[2];
	unsigned int i;

	ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return;

	bf_setkey_ctx(ctx, key, key_len);
	memcpy(sr, iv_8, 8);

	for (i = 0; i < len; i++) {
		if (i % 8 == 0) {
			block_in[0] = (u32)sr[0] << 24 | (u32)sr[1] << 16 |
				      (u32)sr[2] <<  8 | (u32)sr[3];
			block_in[1] = (u32)sr[4] << 24 | (u32)sr[5] << 16 |
				      (u32)sr[6] <<  8 | (u32)sr[7];
			bf_encrypt_block(ctx, block_out, block_in);
			ks[0] = (u8)(block_out[0] >> 24); ks[1] = (u8)(block_out[0] >> 16);
			ks[2] = (u8)(block_out[0] >>  8); ks[3] = (u8)(block_out[0]);
			ks[4] = (u8)(block_out[1] >> 24); ks[5] = (u8)(block_out[1] >> 16);
			ks[6] = (u8)(block_out[1] >>  8); ks[7] = (u8)(block_out[1]);
		}
		ciphertext[i] = plaintext[i] ^ ks[i % 8];
		sr[i % 8] = ciphertext[i];  /* CFB-64: feedback is ciphertext */
	}

	memset(ctx, 0, sizeof(*ctx));
	kfree(ctx);
	memset(ks, 0, sizeof(ks));
	memset(sr, 0, sizeof(sr));
}

/* --------------------------------------------------------------------------
 * Custom base32 encode — 5 bytes → 8 chars, 15 bytes → 24 chars
 * Alphabet: "0123456789ACDEFGHJKLMNPQRTUVWXYZ"
 * -------------------------------------------------------------------------- */
static const char b32_alpha[32] = "0123456789ACDEFGHJKLMNPQRTUVWXYZ";

static void base32_encode_15(const u8 *in, char *out)
{
	int chunk, i;
	for (chunk = 0; chunk < 3; chunk++) {
		const u8 *b = in + chunk * 5;
		/* OA.ko DecodeBytesFromAscii stores the 32-bit part as little-endian:
		 * out[0]=(val>>8), out[1]=(val>>16), out[2]=(val>>24), out[3]=(val>>32), out[4]=val&FF
		 * So pack bytes 0-3 in reversed order so OA decodes them back correctly. */
		u64 val = ((u64)b[3] << 32) | ((u64)b[2] << 24) |
			  ((u64)b[1] << 16) | ((u64)b[0] <<  8) | b[4];
		char *o = out + chunk * 8;
		for (i = 7; i >= 0; i--) {
			o[i] = b32_alpha[val & 0x1F];
			val >>= 5;
		}
	}
}

/* --------------------------------------------------------------------------
 * Inline MD5 — no kernel crypto API dependency (Kronos kernel has no md5.ko)
 * -------------------------------------------------------------------------- */
#define MD5_BLOCK  64

struct md5_state {
	u32 h[4];
	u64 nbytes;
	u8  buf[MD5_BLOCK];
	u32 fill;
};

static const u32 md5_T[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
	0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
	0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
	0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
	0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
	0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
	0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
	0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

static void md5_compress(u32 h[4], const u8 *blk)
{
	u32 W[16], a, b, c, d;
	int i;
	for (i = 0; i < 16; i++)
		W[i] = (u32)blk[i*4] | ((u32)blk[i*4+1] << 8) |
		       ((u32)blk[i*4+2] << 16) | ((u32)blk[i*4+3] << 24);
	a = h[0]; b = h[1]; c = h[2]; d = h[3];
#define ROL(x,n) (((x)<<(n))|((x)>>(32-(n))))
#define FF(a,b,c,d,x,s,t) a=b+ROL(a+((b&c)|(~b&d))+W[x]+(t),(s))
#define GG(a,b,c,d,x,s,t) a=b+ROL(a+((b&d)|(c&~d))+W[x]+(t),(s))
#define HH(a,b,c,d,x,s,t) a=b+ROL(a+(b^c^d)+W[x]+(t),(s))
#define II(a,b,c,d,x,s,t) a=b+ROL(a+(c^(b|~d))+W[x]+(t),(s))
	FF(a,b,c,d, 0, 7,md5_T[ 0]); FF(d,a,b,c, 1,12,md5_T[ 1]);
	FF(c,d,a,b, 2,17,md5_T[ 2]); FF(b,c,d,a, 3,22,md5_T[ 3]);
	FF(a,b,c,d, 4, 7,md5_T[ 4]); FF(d,a,b,c, 5,12,md5_T[ 5]);
	FF(c,d,a,b, 6,17,md5_T[ 6]); FF(b,c,d,a, 7,22,md5_T[ 7]);
	FF(a,b,c,d, 8, 7,md5_T[ 8]); FF(d,a,b,c, 9,12,md5_T[ 9]);
	FF(c,d,a,b,10,17,md5_T[10]); FF(b,c,d,a,11,22,md5_T[11]);
	FF(a,b,c,d,12, 7,md5_T[12]); FF(d,a,b,c,13,12,md5_T[13]);
	FF(c,d,a,b,14,17,md5_T[14]); FF(b,c,d,a,15,22,md5_T[15]);
	GG(a,b,c,d, 1, 5,md5_T[16]); GG(d,a,b,c, 6, 9,md5_T[17]);
	GG(c,d,a,b,11,14,md5_T[18]); GG(b,c,d,a, 0,20,md5_T[19]);
	GG(a,b,c,d, 5, 5,md5_T[20]); GG(d,a,b,c,10, 9,md5_T[21]);
	GG(c,d,a,b,15,14,md5_T[22]); GG(b,c,d,a, 4,20,md5_T[23]);
	GG(a,b,c,d, 9, 5,md5_T[24]); GG(d,a,b,c,14, 9,md5_T[25]);
	GG(c,d,a,b, 3,14,md5_T[26]); GG(b,c,d,a, 8,20,md5_T[27]);
	GG(a,b,c,d,13, 5,md5_T[28]); GG(d,a,b,c, 2, 9,md5_T[29]);
	GG(c,d,a,b, 7,14,md5_T[30]); GG(b,c,d,a,12,20,md5_T[31]);
	HH(a,b,c,d, 5, 4,md5_T[32]); HH(d,a,b,c, 8,11,md5_T[33]);
	HH(c,d,a,b,11,16,md5_T[34]); HH(b,c,d,a,14,23,md5_T[35]);
	HH(a,b,c,d, 1, 4,md5_T[36]); HH(d,a,b,c, 4,11,md5_T[37]);
	HH(c,d,a,b, 7,16,md5_T[38]); HH(b,c,d,a,10,23,md5_T[39]);
	HH(a,b,c,d,13, 4,md5_T[40]); HH(d,a,b,c, 0,11,md5_T[41]);
	HH(c,d,a,b, 3,16,md5_T[42]); HH(b,c,d,a, 6,23,md5_T[43]);
	HH(a,b,c,d, 9, 4,md5_T[44]); HH(d,a,b,c,12,11,md5_T[45]);
	HH(c,d,a,b,15,16,md5_T[46]); HH(b,c,d,a, 2,23,md5_T[47]);
	II(a,b,c,d, 0, 6,md5_T[48]); II(d,a,b,c, 7,10,md5_T[49]);
	II(c,d,a,b,14,15,md5_T[50]); II(b,c,d,a, 5,21,md5_T[51]);
	II(a,b,c,d,12, 6,md5_T[52]); II(d,a,b,c, 3,10,md5_T[53]);
	II(c,d,a,b,10,15,md5_T[54]); II(b,c,d,a, 1,21,md5_T[55]);
	II(a,b,c,d, 8, 6,md5_T[56]); II(d,a,b,c,15,10,md5_T[57]);
	II(c,d,a,b, 6,15,md5_T[58]); II(b,c,d,a,13,21,md5_T[59]);
	II(a,b,c,d, 4, 6,md5_T[60]); II(d,a,b,c,11,10,md5_T[61]);
	II(c,d,a,b, 2,15,md5_T[62]); II(b,c,d,a, 9,21,md5_T[63]);
#undef ROL
#undef FF
#undef GG
#undef HH
#undef II
	h[0] += a; h[1] += b; h[2] += c; h[3] += d;
}

static void md5_init(struct md5_state *ctx)
{
	ctx->h[0] = 0x67452301; ctx->h[1] = 0xefcdab89;
	ctx->h[2] = 0x98badcfe; ctx->h[3] = 0x10325476;
	ctx->nbytes = 0; ctx->fill = 0;
}

static void md5_update(struct md5_state *ctx, const u8 *data, size_t len)
{
	u32 avail = MD5_BLOCK - ctx->fill;
	ctx->nbytes += len;
	if (len < avail) {
		memcpy(ctx->buf + ctx->fill, data, len);
		ctx->fill += len;
		return;
	}
	memcpy(ctx->buf + ctx->fill, data, avail);
	md5_compress(ctx->h, ctx->buf);
	data += avail; len -= avail; ctx->fill = 0;
	while (len >= MD5_BLOCK) {
		md5_compress(ctx->h, data);
		data += MD5_BLOCK; len -= MD5_BLOCK;
	}
	if (len) { memcpy(ctx->buf, data, len); ctx->fill = len; }
}

static void md5_final(struct md5_state *ctx, u8 digest[16])
{
	u64 bits = ctx->nbytes * 8;
	u8 pad[64];
	u32 pad_len;
	int i;
	memset(pad, 0, sizeof(pad));
	pad[0] = 0x80;
	pad_len = (ctx->fill < 56) ? (56 - ctx->fill) : (120 - ctx->fill);
	md5_update(ctx, pad, pad_len);
	memset(pad, 0, 8);
	for (i = 0; i < 8; i++) pad[i] = (u8)(bits >> (i * 8));
	md5_update(ctx, pad, 8);
	for (i = 0; i < 4; i++) {
		digest[i*4+0] = (u8)(ctx->h[i]);
		digest[i*4+1] = (u8)(ctx->h[i] >> 8);
		digest[i*4+2] = (u8)(ctx->h[i] >> 16);
		digest[i*4+3] = (u8)(ctx->h[i] >> 24);
	}
	memset(ctx, 0, sizeof(*ctx));
}

/* --------------------------------------------------------------------------
 * MD5 of (plain_12 || option_file_bytes), extract bytes [3],[7],[11]
 * -------------------------------------------------------------------------- */
static int compute_fingerprint(const u8 *plain_12, const u8 *file_buf,
				size_t file_len, u8 fp[3])
{
	struct md5_state ctx;
	u8 digest[16];

	md5_init(&ctx);
	md5_update(&ctx, plain_12, 12);
	md5_update(&ctx, file_buf, file_len);
	md5_final(&ctx, digest);
	fp[0] = digest[3];
	fp[1] = digest[7];
	fp[2] = digest[11];
	return 0;
}

/* --------------------------------------------------------------------------
 * Read a file from the VFS into a kernel buffer.
 * Returns number of bytes read, or negative errno.
 * -------------------------------------------------------------------------- */
static ssize_t read_option_file(const char *opt_id, u8 *buf, size_t max_len)
{
	char path[80];
	struct file *filp;
	ssize_t ret;

	snprintf(path, sizeof(path), "/korg/rw/Options/%s", opt_id);
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(filp))
		return PTR_ERR(filp);

	ret = kernel_read(filp, 0, buf, max_len);
	filp_close(filp, NULL);
	return ret;
}

/* ==========================================================================
 * Native GPA (Group Authentication Protocol) implementation
 * ==========================================================================
 * Reverse-engineered from OA_322.ko:
 *   SetupAtmelForAuthorizations   nm 0x207a50
 *   fFfFfFfFfFfF13                nm 0x4f4850  authenticated zone read
 *   fFfFfFfFfFfF1C                nm 0x4f4a90  multi-byte read (used in setup)
 *   bzzzzzzzzzzzt12               nm 0x4f3d10  cipher state advance
 *   bzzzzzzzzzzzt16               nm 0x4f3fe0  MAC proof sender
 *   sdflkjsvnd2g                  nm 0x4f61d0  challenge generator
 *   fFfFfFfFfFfF11                            MAC computation
 *
 * Only stgNV2AC_sync_cmd / stgNV2AC_sync_read_cmd (OmapNKS4.ko) are used.
 * Both are available during UpdateOS even when OA.ko is absent.
 * ========================================================================== */

static u8 gpa_mode;         /* 0=unauthenticated, 1=mode1, 2=mode2 */
static u8 gpa_cs[20];       /* cipher state */
static u8 gpa_rxbuf[64];    /* DMA receive buffer */

/* --------------------------------------------------------------------------
 * gpa_bzt12 — advance the GPA cipher state by one input byte.
 * RE from OA_322.ko bzzzzzzzzzzzt12 (nm 0x4f3d10).
 * -------------------------------------------------------------------------- */
static void gpa_bzt12(u8 input)
{
	u8 s0  = gpa_cs[0],  s1  = gpa_cs[1],  s2  = gpa_cs[2],  s3  = gpa_cs[3];
	u8 s4  = gpa_cs[4],  s5  = gpa_cs[5],  s6  = gpa_cs[6],  s7  = gpa_cs[7];
	u8 s8  = gpa_cs[8],  s9  = gpa_cs[9],  s10 = gpa_cs[10], s11 = gpa_cs[11];
	u8 s12 = gpa_cs[12], s13 = gpa_cs[13], s14 = gpa_cs[14], s15 = gpa_cs[15];
	u8 s16 = gpa_cs[16], s17 = gpa_cs[17], s18 = gpa_cs[18], s19 = gpa_cs[19];

	u8 t     = input ^ s0;
	u8 lo4   = t & 0x0f;
	u8 lo5   = t & 0x1f;
	u8 hi3   = (u8)(t >> 5);
	u8 rot_t = hi3 | (u8)(lo4 << 3);
	u8 mid   = (u8)(t >> 3);

	u8 rots1, sum1, rots8, sum2, norm3, cl_final, mux;

	rots1 = (u8)((s1 >> 4) | ((s1 & 0x0f) << 1));
	sum1  = rots1 + s2;
	if (sum1 >= 0x20) sum1 -= 0x1f;

	rots8 = (u8)((s8 >> 6) | ((s8 & 0x3f) << 1));
	sum2  = rots8 + s9;
	if (sum2 & 0x80) sum2 -= 0x7f;

	norm3 = s16 + s15;
	if (norm3 >= 0x20) norm3 -= 0x1f;

	cl_final = sum1 ^ s2;
	mux      = ((norm3 ^ s16) & sum2) | ((u8)(~sum2) & cl_final);

	gpa_cs[0]  = (u8)(((s0 & 0x0f) << 4) | (mux & 0x0f));
	gpa_cs[1]  = s3;   gpa_cs[2]  = lo5 ^ s5;  gpa_cs[3]  = s4;
	gpa_cs[4]  = s2;   gpa_cs[5]  = s6;         gpa_cs[6]  = s7;
	gpa_cs[7]  = sum1; gpa_cs[8]  = s9;         gpa_cs[9]  = rot_t ^ s10;
	gpa_cs[10] = s11;  gpa_cs[11] = s12;        gpa_cs[12] = s13;
	gpa_cs[13] = s14;  gpa_cs[14] = sum2;       gpa_cs[15] = s17;
	gpa_cs[16] = mid ^ s18; gpa_cs[17] = s16;  gpa_cs[18] = s19;
	gpa_cs[19] = norm3;
}

/* --------------------------------------------------------------------------
 * Soft 128-bit arithmetic — replaces GMP used inside sdflkjsvnd2g.
 * __int128 is unavailable on 32-bit kernel targets; use {hi,lo} u64 pairs.
 *
 * All operands in addmod/double_mod/mulmod are maintained < m by invariant.
 * Safe for moduli > 2^127 (proven: overflow in addmod always yields < m).
 * -------------------------------------------------------------------------- */
struct u128 { u64 hi; u64 lo; };

/* GPA moduli from OA_322.ko .rodata string concatenation (nm 0x4f61d0) */
static const struct u128 GPA_P = { 0xcf8aa5362f182eeeULL, 0x7089aef5be7ba844ULL };
static const struct u128 GPA_Q = { 0xcf8aa5362f182ef0ULL, 0x3d8ac5ebdfe1fc69ULL };

static int u128_lt(struct u128 a, struct u128 b)
{
	return (a.hi < b.hi) || (a.hi == b.hi && a.lo < b.lo);
}

static int u128_ge(struct u128 a, struct u128 b)
{
	return !u128_lt(a, b);
}

static int u128_zero(struct u128 a)
{
	return (a.hi == 0) && (a.lo == 0);
}

static int u128_lsb(struct u128 a) { return (int)(a.lo & 1); }

static struct u128 u128_add(struct u128 a, struct u128 b)
{
	struct u128 r;
	r.lo = a.lo + b.lo;
	r.hi = a.hi + b.hi + (r.lo < a.lo ? 1 : 0);
	return r;
}

static struct u128 u128_sub(struct u128 a, struct u128 b)
{
	struct u128 r;
	r.lo = a.lo - b.lo;
	r.hi = a.hi - b.hi - (a.lo < b.lo ? 1 : 0);
	return r;
}

static struct u128 u128_shl1(struct u128 a)
{
	struct u128 r;
	r.hi = (a.hi << 1) | (a.lo >> 63);
	r.lo = a.lo << 1;
	return r;
}

static struct u128 u128_shr1(struct u128 a)
{
	struct u128 r;
	r.lo = (a.lo >> 1) | (a.hi << 63);
	r.hi = a.hi >> 1;
	return r;
}

static struct u128 u128_from_u64(u64 v)
{
	struct u128 r; r.hi = 0; r.lo = v; return r;
}

/* addmod: a+b mod m. Safe for a,b < m and m > 2^127 (overflow => result < m) */
static struct u128 gpa_addmod(struct u128 a, struct u128 b, struct u128 m)
{
	struct u128 r = u128_add(a, b);
	/* if carry occurred: r = a+b-2^128 < m (since a+b < 2m and 2m > 2^128) */
	if (u128_lt(r, a)) return r;
	if (u128_ge(r, m)) r = u128_sub(r, m);
	return r;
}

/* double_mod: 2a mod m */
static struct u128 gpa_double_mod(struct u128 a, struct u128 m)
{
	struct u128 r = u128_shl1(a);
	if (u128_lt(r, a)) return r;  /* overflow: 2a-2^128 < m */
	if (u128_ge(r, m)) r = u128_sub(r, m);
	return r;
}

/* mulmod: a*b mod m. All operands < m by invariant — no need for initial a%=m. */
static struct u128 gpa_mulmod(struct u128 a, struct u128 b, struct u128 m)
{
	struct u128 result = u128_from_u64(0);
	/* b is at most ~127 bits (acc < P < 2^128) */
	while (!u128_zero(b)) {
		if (u128_lsb(b)) result = gpa_addmod(result, a, m);
		a = gpa_double_mod(a, m);
		b = u128_shr1(b);
	}
	return result;
}

static struct u128 gpa_powmod(struct u128 base, struct u128 exp, struct u128 m)
{
	struct u128 result = u128_from_u64(1);
	while (!u128_zero(exp)) {
		if (u128_lsb(exp)) result = gpa_mulmod(result, base, m);
		base = gpa_mulmod(base, base, m);
		exp = u128_shr1(exp);
	}
	return result;
}

/* --------------------------------------------------------------------------
 * gpa_challenge — compute 8-byte GPA challenge from 7-byte nonce.
 * RE from OA_322.ko sdflkjsvnd2g (nm 0x4f61d0).
 *
 * Byte order (confirmed via disasm): nonce[6] is MSB, nonce[0] is LSB.
 * Output selection (confirmed): buf[1,2,3,5,7,11,13,15] of 16-byte buffer.
 * mode_arg=0 in all SetupAtmelForAuthorizations call sites.
 * -------------------------------------------------------------------------- */
static void gpa_challenge(const u8 *nonce7, int mode_arg, u8 *out8)
{
	struct u128 X, acc;
	u8  buf[16];
	int bi, bit, i;
	static const int sel[8] = {1, 2, 3, 5, 7, 11, 13, 15};
	u64 x_raw = 0;

	/* nonce[6] is MSB (bytes loaded high-index-first then left-shifted) */
	for (i = 6; i >= 0; i--)
		x_raw = (x_raw << 8) | nonce7[i];
	X = u128_from_u64(x_raw);
	X = gpa_mulmod(X, X, GPA_Q);  /* X = nonce^2 mod Q */

	/* acc = 2^((mode_arg+17)*128) mod P */
	{
		struct u128 two = u128_from_u64(2);
		u32 e = (u32)(mode_arg + 17) * 128;
		acc = gpa_powmod(two, u128_from_u64(e), GPA_P);
	}

	memset(buf, 0, sizeof(buf));
	for (bi = 0; bi < 16; bi++) {
		for (bit = 0x01; bit <= 0x80; bit <<= 1) {
			struct u128 r;
			acc = gpa_double_mod(acc, GPA_P);
			r   = gpa_powmod(X, acc, GPA_Q);
			if (u128_lsb(r)) buf[bi] |= (u8)bit;
		}
	}
	for (i = 0; i < 8; i++)
		out8[i] = buf[sel[i]];
}

/* --------------------------------------------------------------------------
 * gpa_read_n — read `count` bytes from chip addr with cipher advances.
 * RE from OA_322.ko fFfFfFfFfFfF1C (nm 0x4f4a90).
 * ALWAYS advances cipher (even in mode 0) — used during setup.
 * XOR decryption only for addr > 0xaf with mode==2.
 * -------------------------------------------------------------------------- */
static int gpa_read_n(u8 addr, u8 count, u8 *out)
{
	u8 cmd[4];
	int i, j;

	cmd[0] = 0xb6; cmd[1] = 0x00; cmd[2] = addr; cmd[3] = count;

	/* 12 pre-advances: 5×0, addr, 5×0, count */
	for (i = 0; i < 5; i++) gpa_bzt12(0);
	gpa_bzt12(addr);
	for (i = 0; i < 5; i++) gpa_bzt12(0);
	gpa_bzt12(count);

	if (stgNV2AC_sync_read_cmd(cmd, gpa_rxbuf))
		return -EIO;

	for (i = 0; i < (int)count; i++) {
		u8 raw = gpa_rxbuf[i];
		if (addr > 0xaf && gpa_mode == 2) {
			out[i] = raw ^ gpa_cs[0];
			gpa_bzt12(out[i]);
		} else {
			gpa_bzt12(raw);
			out[i] = raw;
		}
		for (j = 0; j < 5; j++) gpa_bzt12(0);
	}
	return 0;
}

/* --------------------------------------------------------------------------
 * gpa_mac — compute 8-byte MAC from three 8-byte inputs.
 * RE from OA_322.ko fFfFfFfFfFfF11.
 * Resets gpa_cs to zero before computation (synchronized cipher reset).
 * buf1: random bytes, buf2: challenge (or zeros), buf3: chip_B.
 * -------------------------------------------------------------------------- */
static void gpa_mac(const u8 *buf1, const u8 *buf2, const u8 *buf3, u8 *out8)
{
	int k, j;

	memset(gpa_cs, 0, sizeof(gpa_cs));

	/* 8 rounds: rounds 0-3 pair consecutive bytes of buf3, rounds 4-7 of buf2 */
	for (k = 0; k < 8; k++) {
		const u8 *bp = (k < 4) ? (buf3 + k * 2) : (buf2 + (k - 4) * 2);
		for (j = 0; j < 3; j++) gpa_bzt12(bp[0]);
		for (j = 0; j < 3; j++) gpa_bzt12(bp[1]);
		gpa_bzt12(buf1[k]);
	}

	/* Extract 8 output bytes from cs[0] after 6 + 7k zero-advances */
	for (j = 0; j < 6; j++) gpa_bzt12(0);
	out8[0] = gpa_cs[0];
	for (k = 1; k < 8; k++) {
		for (j = 0; j < 7; j++) gpa_bzt12(0);
		out8[k] = gpa_cs[0];
	}
}

/* --------------------------------------------------------------------------
 * gpa_send_proof — send 8-byte MAC proof to chip and check acceptance.
 * RE from OA_322.ko bzzzzzzzzzzzt16 (nm 0x4f3fe0).
 * cmd1_byte: 0x00 for first proof, 0x10 for second.
 * Returns 0 if chip accepted (addr 0x50 reads 0xFF), -1 otherwise.
 * Does NOT touch gpa_cs.
 * -------------------------------------------------------------------------- */
static int gpa_send_proof(u8 cmd1_byte, const u8 *rand8, const u8 *mac8)
{
	u8 cmd[20];
	u8 rsp[4];

	cmd[0] = 0xb8; cmd[1] = cmd1_byte; cmd[2] = 0x00; cmd[3] = 0x10;
	memcpy(cmd + 4,  rand8, 8);
	memcpy(cmd + 12, mac8,  8);
	stgNV2AC_sync_cmd(cmd, 20);

	/* addr 0x50 == 0xFF means chip accepted */
	rsp[0] = 0xb2; rsp[1] = 0x00; rsp[2] = 0x50; rsp[3] = 0x01;
	stgNV2AC_sync_read_cmd(rsp, gpa_rxbuf);
	return (gpa_rxbuf[0] == 0xFF) ? 0 : -1;
}

/* --------------------------------------------------------------------------
 * gpa_setup — run the full GPA authentication handshake with the Atmel chip.
 * RE from OA_322.ko SetupAtmelForAuthorizations (nm 0x207a50).
 * On success: gpa_mode = 2 (or 1); subsequent gpa_read_zone calls decrypt.
 * Returns 0 on success, -EIO if chip rejects both MAC proofs.
 * -------------------------------------------------------------------------- */
static int gpa_setup(void)
{
	u8 nonce[7], challenge[8], chip_B[8], rand_buf[8], mac_out[8];
	static const u8 zeros8[8] = {0};
	u8 cmd[4];
	int ret;

	gpa_mode = 0;
	memset(gpa_cs, 0, sizeof(gpa_cs));

	/* Read 7-byte nonce from addr 0x19 (cipher advances from zero state) */
	ret = gpa_read_n(0x19, 7, nonce);
	if (ret) return ret;

	/* Send init command {0xb8,0,0,0}; read addr 0x50 (result ignored) */
	cmd[0] = 0xb8; cmd[1] = 0x00; cmd[2] = 0x00; cmd[3] = 0x00;
	stgNV2AC_sync_cmd(cmd, 4);
	{
		u8 tmp[4] = {0xb2, 0x00, 0x50, 0x01};
		stgNV2AC_sync_read_cmd(tmp, gpa_rxbuf);
	}

	/* Send activate command {0xb4,0x03,0,0} */
	cmd[0] = 0xb4; cmd[1] = 0x03; cmd[2] = 0x00; cmd[3] = 0x00;
	stgNV2AC_sync_cmd(cmd, 4);

	/* Compute 8-byte challenge from nonce */
	gpa_challenge(nonce, 0, challenge);

	/* Read chip's 8-byte response from addr 0x50 */
	ret = gpa_read_n(0x50, 8, chip_B);
	if (ret) return ret;

	/* First MAC proof */
	get_random_bytes(rand_buf, 8);
	gpa_mac(rand_buf, challenge, chip_B, mac_out);
	if (gpa_send_proof(0x00, rand_buf, mac_out) == 0)
		gpa_mode = 1;

	/* Second MAC proof */
	get_random_bytes(rand_buf, 8);
	gpa_mac(rand_buf, zeros8, chip_B, mac_out);
	if (gpa_send_proof(0x10, rand_buf, mac_out) == 0)
		gpa_mode = 2;

	pr_info("oa_authgen: gpa_setup mode=%d\n", (int)gpa_mode);
	return (gpa_mode > 0) ? 0 : -EIO;
}

/* --------------------------------------------------------------------------
 * gpa_read_zone — authenticated read of `count` bytes from chip addr.
 * RE from OA_322.ko fFfFfFfFfFfF13 (nm 0x4f4850).
 * mode==0: raw read (non-deterministic without prior gpa_setup).
 * mode==1: cipher-synchronized, no XOR decryption.
 * mode==2: cipher-synchronized, XOR-decrypt each byte with cs[0].
 * -------------------------------------------------------------------------- */
static int gpa_read_zone(u8 addr, u8 count, u8 *out)
{
	u8 cmd[4];
	int i, j;

	if (gpa_mode == 0) {
		cmd[0] = 0xb2; cmd[1] = 0x00; cmd[2] = addr; cmd[3] = count;
		if (stgNV2AC_sync_read_cmd(cmd, gpa_rxbuf))
			return -EIO;
		memcpy(out, gpa_rxbuf, count);
		return 0;
	}

	cmd[0] = 0xb6; cmd[1] = 0x00; cmd[2] = addr; cmd[3] = count;

	for (i = 0; i < 5; i++) gpa_bzt12(0);
	gpa_bzt12(addr);
	for (i = 0; i < 5; i++) gpa_bzt12(0);
	gpa_bzt12(count);

	if (stgNV2AC_sync_read_cmd(cmd, gpa_rxbuf))
		return -EIO;

	for (i = 0; i < (int)count; i++) {
		u8 raw = gpa_rxbuf[i];
		if (gpa_mode == 2) {
			out[i] = raw ^ gpa_cs[0];
			gpa_bzt12(out[i]);
		} else {
			gpa_bzt12(raw);
			out[i] = raw;
		}
		for (j = 0; j < 5; j++) gpa_bzt12(0);
	}
	return 0;
}

/* --------------------------------------------------------------------------
 * Chip authentication via OA.ko's SetupAtmelForAuthorizations.
 *
 * The Atmel NV2AC chip requires a host-authentication handshake (opcode 0xb6
 * challenge-response using GPA cipher) before reads from protected zones
 * return valid data.  OA.ko calls this in its init_module, but that call may
 * have failed if the chip was in a wedged state at boot time.  Calling it
 * here (via kallsyms) ensures the chip is authenticated before we read it,
 * making our module self-sufficient rather than depending on OA.ko's init
 * having succeeded.
 *
 * Returns 0 on success, negative on failure.  A non-zero return does NOT
 * prevent us from trying the reads — OA.ko may have already authenticated
 * the chip and the state persists.
 * -------------------------------------------------------------------------- */
typedef int (*setup_atmel_fn_t)(void);
/* regparm(3): EAX=start_addr (e.g. 0x10), EDX=count (e.g. 8), ECX=output_buf */
typedef int (*chip_read_fn_t)(unsigned int start_addr, unsigned int count, void *out);
/* regparm(3): EAX=auth_string ptr, EDX=5-byte opt_id output buf; returns 0 on success */
typedef int (*verify_auth_fn_t)(const char *auth_str, char *opt_id_out);
/* regparm(3): EAX=out_buf ptr, EDX=auth_string ptr; decodes base32 auth string to 15 bytes */
typedef int (*decode_bytes_fn_t)(char *out_buf, const char *auth_str);

static int authenticate_chip(void)
{
	if (setup_atmel_addr) {
		setup_atmel_fn_t fn = (setup_atmel_fn_t)setup_atmel_addr;
		return fn();
	}
	/* No OA.ko address: run native GPA implementation (works without OA.ko) */
	return gpa_setup();
}

/*
 * Read 8 bytes from a chip secret zone.  Uses OA.ko's fFfFfFfFfFfF13 if the
 * address was provided (preferred — handles the encrypted session cipher),
 * otherwise falls back to raw stgNV2AC_sync_read_cmd.
 */
static int read_chip_zone(u8 addr, u8 *buf)
{
	if (chip_read_addr) {
		chip_read_fn_t fn = (chip_read_fn_t)chip_read_addr;
		return fn(addr, 8, buf);
	}
	/* No OA.ko address: use native GPA authenticated read */
	return gpa_read_zone(addr, 8, buf);
}

/* --------------------------------------------------------------------------
 * Main auth string computation
 * option_id: null-terminated string, e.g. "S285" (max 15 chars)
 * out_24:    caller-supplied buffer of at least 24 bytes for the auth string
 * Returns 0 on success, negative errno on failure.
 * -------------------------------------------------------------------------- */
#define OPT_FILE_MAX 512

static int generate_auth_string(const char *option_id, char *out_24)
{
	u8 chip[24];
	u8 file_buf[OPT_FILE_MAX];
	u8 plain_12[12];
	u8 fp[3];
	u8 plaintext[15];
	u8 ciphertext[15];
	ssize_t file_len;
	int ret;

	/* Step 1: Read secret zones (0x10, 0x18, 0x20) via OA.ko's existing cipher state.
	 *
	 * Do NOT call SetupAtmelForAuthorizations here.  VerifyAuthorizationString
	 * also reads the chip without re-running SetupAtmelForAuthorizations, so both
	 * reads must happen at the same cipher-stream position.  Calling Setup would
	 * reset OA.ko's cipher state and cause a mismatch between the key we encrypt
	 * with and the key OA.ko decrypts with during the AU: verification.
	 */
	{
		static const u8 addrs[3] = { 0x10, 0x18, 0x20 };
		int a;

		for (a = 0; a < 3; a++) {
			ret = read_chip_zone(addrs[a], chip + a * 8);
			if (ret) {
				pr_err("oa_authgen: chip read 0x%02x failed (%d)\n",
				       addrs[a], ret);
				return -EIO;
			}
		}
		pr_info("oa_authgen: GEN chip=%02x%02x%02x%02x%02x%02x%02x%02x"
			"%02x%02x%02x%02x%02x%02x%02x%02x"
			"%02x%02x%02x%02x%02x%02x%02x%02x\n",
			chip[0],chip[1],chip[2],chip[3],chip[4],chip[5],chip[6],chip[7],
			chip[8],chip[9],chip[10],chip[11],chip[12],chip[13],chip[14],chip[15],
			chip[16],chip[17],chip[18],chip[19],chip[20],chip[21],chip[22],chip[23]);
	}

	/* Step 2: Read option file */
	file_len = read_option_file(option_id, file_buf, sizeof(file_buf));
	if (file_len < 0) {
		pr_err("oa_authgen: cannot read /korg/rw/Options/%s (%zd)\n",
		       option_id, file_len);
		return (int)file_len;
	}

	/* Step 3: Build plain_12 = salt_8 (zeros) || option_id_4 */
	memset(plain_12, 0, 8);
	memset(plain_12 + 8, 0, 4);
	strncpy(plain_12 + 8, option_id, 4);  /* e.g. "S285" */

	/* Step 4: MD5 fingerprint */
	ret = compute_fingerprint(plain_12, file_buf, (size_t)file_len, fp);
	if (ret)
		return ret;

	/* Step 5: Assemble plaintext_15 = plain_12 || fingerprint_3 */
	memcpy(plaintext, plain_12, 12);
	plaintext[12] = fp[0];
	plaintext[13] = fp[1];
	plaintext[14] = fp[2];

	/* Step 6: Blowfish-CFB-64 encrypt (16-byte key, chip[16:24] as IV) */
	blowfish_cfb64_encrypt(chip, 16, chip + 16, plaintext, ciphertext, 15);

	/* Step 7: Custom base32 encode */
	base32_encode_15(ciphertext, out_24);

	return 0;
}

/* Same as generate_auth_string but also returns the 24-byte chip used */
static int generate_auth_string_dbg(const char *option_id, char *out_24, u8 *chip_out)
{
	u8 chip[24];
	u8 file_buf[OPT_FILE_MAX];
	u8 plain_12[12];
	u8 fp[3];
	u8 plaintext[15];
	u8 ciphertext[15];
	ssize_t file_len;
	int ret;
	static const u8 addrs[3] = { 0x10, 0x18, 0x20 };
	int a;

	for (a = 0; a < 3; a++) {
		ret = read_chip_zone(addrs[a], chip + a * 8);
		if (ret) return -EIO;
	}
	memcpy(chip_out, chip, 24);

	file_len = read_option_file(option_id, file_buf, sizeof(file_buf));
	if (file_len < 0) return (int)file_len;

	memset(plain_12, 0, 8);
	memset(plain_12 + 8, 0, 4);
	strncpy(plain_12 + 8, option_id, 4);

	ret = compute_fingerprint(plain_12, file_buf, (size_t)file_len, fp);
	if (ret) return ret;

	memcpy(plaintext, plain_12, 12);
	plaintext[12] = fp[0]; plaintext[13] = fp[1]; plaintext[14] = fp[2];

	blowfish_cfb64_encrypt(chip, 16, chip + 16, plaintext, ciphertext, 15);
	base32_encode_15(ciphertext, out_24);
	return 0;
}

/* --------------------------------------------------------------------------
 * /proc/.oaauth file operations
 * -------------------------------------------------------------------------- */
static char g_result[128]; /* auth string (24), DBG hex (48), GENDBG (48+1+24=73) */
static DEFINE_MUTEX(g_lock);
static struct proc_dir_entry *g_pde;

static ssize_t oaauth_read(struct file *f, char __user *buf, size_t n,
			   loff_t *off)
{
	ssize_t len;
	if (*off > 0)
		return 0;
	mutex_lock(&g_lock);
	len = strnlen(g_result, sizeof(g_result) - 1);
	if (len == 0) {
		mutex_unlock(&g_lock);
		return -ENODATA;
	}
	if (n < (size_t)(len + 1)) {
		mutex_unlock(&g_lock);
		return -EINVAL;
	}
	if (copy_to_user(buf, g_result, len)) {
		mutex_unlock(&g_lock);
		return -EFAULT;
	}
	mutex_unlock(&g_lock);
	*off = len;
	return len;
}

static ssize_t oaauth_write(struct file *f, const char __user *buf, size_t n,
			    loff_t *off)
{
	char cmd[32];
	char opt_id[20];
	char result[25];
	size_t copy_len = min(n, sizeof(cmd) - 1);
	int ret;

	if (copy_from_user(cmd, buf, copy_len))
		return -EFAULT;
	cmd[copy_len] = '\0';

	/* Strip trailing newline */
	if (copy_len > 0 && cmd[copy_len - 1] == '\n')
		cmd[--copy_len] = '\0';

	/* CHIP: read chip WITHOUT calling authenticate_chip — same path as GEN */
	if (strncmp(cmd, "CHIP", 4) == 0) {
		static const u8 addrs[3] = { 0x10, 0x18, 0x20 };
		static const char hex[] = "0123456789abcdef";
		u8 chip[24];
		char hexbuf[49];
		int a, i;
		char modebuf[32];

		for (a = 0; a < 3; a++) {
			ret = read_chip_zone(addrs[a], chip + a * 8);
			if (ret) {
				pr_err("oa_authgen: CHIP read 0x%02x failed (%d)\n",
				       addrs[a], ret);
				return ret;
			}
		}
		for (i = 0; i < 24; i++) {
			hexbuf[i*2]   = hex[(chip[i] >> 4) & 0xf];
			hexbuf[i*2+1] = hex[chip[i] & 0xf];
		}
		hexbuf[48] = '\0';

		modebuf[0] = '\0';
		if (oa_mode_addr) {
			u8 mode = *(volatile u8 *)oa_mode_addr;
			u8 xorb = *(volatile u8 *)(oa_mode_addr + 1);
			snprintf(modebuf, sizeof(modebuf), " mode=%d xor=%02x", (int)mode, (unsigned)xorb);
		}

		mutex_lock(&g_lock);
		snprintf(g_result, sizeof(g_result) - 1, "%s%s", hexbuf, modebuf);
		g_result[sizeof(g_result) - 1] = '\0';
		mutex_unlock(&g_lock);
		return (ssize_t)n;
	}

	/* DBG: read raw chip bytes and return as 48-char hex string */
	if (strncmp(cmd, "DBG", 3) == 0) {
		static const u8 addrs[3] = { 0x10, 0x18, 0x20 };
		static const char hex[] = "0123456789abcdef";
		u8 chip[24];
		char hexbuf[49];
		int a, i, auth_ret;

		auth_ret = authenticate_chip();
		for (a = 0; a < 3; a++) {
			ret = read_chip_zone(addrs[a], chip + a * 8);
			if (ret) {
				pr_err("oa_authgen: DBG chip read 0x%02x failed (%d)\n",
				       addrs[a], ret);
				return ret;
			}
		}
		for (i = 0; i < 24; i++) {
			hexbuf[i*2]   = hex[(chip[i] >> 4) & 0xf];
			hexbuf[i*2+1] = hex[chip[i] & 0xf];
		}
		hexbuf[48] = '\0';
		mutex_lock(&g_lock);
		/* format: 48-char hex + " auth=<N>" */
		snprintf(g_result, sizeof(g_result) - 1, "%s auth=%d", hexbuf, auth_ret);
		g_result[sizeof(g_result) - 1] = '\0';
		mutex_unlock(&g_lock);
		return (ssize_t)n;
	}

	/* GENDBG: like GEN but result is "chip_hex:auth_string" for debugging */
	if (strncmp(cmd, "GENDBG:", 7) == 0) {
		static const char hex[] = "0123456789abcdef";
		char opt_id2[20];
		char result2[25];
		u8 chip2[24];
		char hexbuf[49];
		int i2;

		strncpy(opt_id2, cmd + 7, sizeof(opt_id2) - 1);
		opt_id2[sizeof(opt_id2) - 1] = '\0';
		memset(result2, 0, sizeof(result2));
		ret = generate_auth_string_dbg(opt_id2, result2, chip2);
		if (ret) return ret;

		for (i2 = 0; i2 < 24; i2++) {
			hexbuf[i2*2]   = hex[(chip2[i2] >> 4) & 0xf];
			hexbuf[i2*2+1] = hex[chip2[i2] & 0xf];
		}
		hexbuf[48] = '\0';

		mutex_lock(&g_lock);
		snprintf(g_result, sizeof(g_result) - 1, "%s:%s", hexbuf, result2);
		g_result[sizeof(g_result) - 1] = '\0';
		mutex_unlock(&g_lock);
		return (ssize_t)n;
	}

	/* DECODE:<auth_str>: call OA.ko DecodeBytesFromAscii and return hex of 15 decoded bytes.
	 * Compare against our base32_encode_15 output to detect encoding mismatches. */
	if (strncmp(cmd, "DECODE:", 7) == 0) {
		char auth_str[28];
		char decoded[20];
		static const char hex[] = "0123456789abcdef";
		char hexbuf[35];
		int dret, i;

		if (!decode_addr) {
			mutex_lock(&g_lock);
			snprintf(g_result, sizeof(g_result) - 1,
				 "ERR:decode_addr not set");
			g_result[sizeof(g_result) - 1] = '\0';
			mutex_unlock(&g_lock);
			return (ssize_t)n;
		}
		strncpy(auth_str, cmd + 7, 24);
		auth_str[24] = '\0';
		memset(decoded, 0, sizeof(decoded));
		{
			decode_bytes_fn_t dfn = (decode_bytes_fn_t)decode_addr;
			dret = dfn(decoded, auth_str);
		}
		for (i = 0; i < 15; i++) {
			hexbuf[i*2]   = hex[((unsigned char)decoded[i] >> 4) & 0xf];
			hexbuf[i*2+1] = hex[(unsigned char)decoded[i] & 0xf];
		}
		hexbuf[30] = '\0';
		pr_info("oa_authgen: DECODE %s → rc=%d bytes=%s\n",
			auth_str, dret, hexbuf);
		mutex_lock(&g_lock);
		snprintf(g_result, sizeof(g_result) - 1, "rc=%d bytes=%s", dret, hexbuf);
		g_result[sizeof(g_result) - 1] = '\0';
		mutex_unlock(&g_lock);
		return (ssize_t)n;
	}

	/* VERIFY:<auth_str>: call OA.ko VerifyAuthorizationString directly.
	 * Returns "rc=N opt=XXXX" — if rc=0 the auth string decrypts OK;
	 * if rc!=0 the crypto itself is wrong. */
	if (strncmp(cmd, "VERIFY:", 7) == 0) {
		char auth_str[28];
		char opt_out[8];
		int vret = -ENOSYS;

		if (!verify_auth_addr) {
			mutex_lock(&g_lock);
			snprintf(g_result, sizeof(g_result) - 1,
				 "ERR:verify_auth_addr not set");
			g_result[sizeof(g_result) - 1] = '\0';
			mutex_unlock(&g_lock);
			return (ssize_t)n;
		}
		strncpy(auth_str, cmd + 7, 24);
		auth_str[24] = '\0';
		memset(opt_out, 0, sizeof(opt_out));
		{
			verify_auth_fn_t vfn = (verify_auth_fn_t)verify_auth_addr;
			vret = vfn(auth_str, opt_out);
		}
		pr_info("oa_authgen: VERIFY %s → rc=%d opt=%.4s\n",
			auth_str, vret, opt_out);
		mutex_lock(&g_lock);
		snprintf(g_result, sizeof(g_result) - 1,
			 "rc=%d opt=%.4s", vret, opt_out);
		g_result[sizeof(g_result) - 1] = '\0';
		mutex_unlock(&g_lock);
		return (ssize_t)n;
	}

	if (strncmp(cmd, "GEN:", 4) != 0)
		return -EINVAL;

	strncpy(opt_id, cmd + 4, sizeof(opt_id) - 1);
	opt_id[sizeof(opt_id) - 1] = '\0';

	if (strlen(opt_id) == 0 || strlen(opt_id) > 15)
		return -EINVAL;

	memset(result, 0, sizeof(result));
	ret = generate_auth_string(opt_id, result);
	if (ret)
		return ret;
	pr_info("oa_authgen: GEN %s → %s\n", opt_id, result);

	mutex_lock(&g_lock);
	memcpy(g_result, result, 24);
	g_result[24] = '\0';
	/* zero rest of buffer to avoid stale DBG output leaking into auth reads */
	memset(g_result + 25, 0, 24);
	mutex_unlock(&g_lock);

	return (ssize_t)n;
}

static const struct file_operations oaauth_fops = {
	.owner = THIS_MODULE,
	.read  = oaauth_read,
	.write = oaauth_write,
};

/* --------------------------------------------------------------------------
 * Module init / exit
 * -------------------------------------------------------------------------- */
static int __init oa_authgen_init(void)
{
	memset(g_result, 0, sizeof(g_result));

	/* Run chip authentication (SetupAtmelForAuthorizations or native GPA).
	 * Required to get deterministic reads from encrypted zones — always. */
	authenticate_chip();

	g_pde = proc_create(".oaauth", 0666, NULL, &oaauth_fops);
	if (!g_pde) {
		pr_err("oa_authgen: failed to create /proc/.oaauth\n");
		return -ENOMEM;
	}
	pr_info("oa_authgen: /proc/.oaauth ready\n");
	return 0;
}

static void __exit oa_authgen_exit(void)
{
	if (g_pde)
		remove_proc_entry(".oaauth", NULL);
	pr_info("oa_authgen: unloaded\n");
}

module_init(oa_authgen_init);
module_exit(oa_authgen_exit);
