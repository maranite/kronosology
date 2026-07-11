// SPDX-License-Identifier: GPL-2.0
/*
 * init_performances.cpp  -  CSTGGlobal::InitializePerformances() (batch 54,
 * init_module() step 8 chain -- CSTGGlobal::Initialize() -> InitializePerformances(),
 * see oa_global.h's own class comment for the full derivation).
 *
 * Also homes CKorgPreloadFile::CKorgPreloadFile(const char*)/
 * ~CKorgPreloadFile() and CKorgProgBankFile::CKorgProgBankFile(const char*)
 * -- their own real, tiny ctor bodies (10/40 bytes in ground truth,
 * fully disassembled) -- matching this project's established convention
 * of homing a brand-new class's real ctor alongside its first real
 * caller (CStartupFile precedent, sec 10.148, startup_file.cpp) rather
 * than in bar2_stubs.cpp. `Load()` (genuine SSD file I/O) is the one
 * deliberately DSP/filesystem-stub-callee-deferred method, living in
 * bar2_stubs.cpp alongside CSTGProgramBank::Initialize()/GetPatchSize(),
 * CSTGProgram::Initialize(), and PopulateDefaultProgramSlotTemplates().
 */

#include "oa_global.h"
#include "oa_setup_global_resources.h"	/* for STGAPIFrontPanelStatus::sInstance */

/* snprintf -- confirmed genuinely `U` (unresolved external) in ground
 * truth too (kernel-exported), same treatment as rt_printk
 * (oa_rtfifo_init.h): the real call site (InitializePerformances()'s
 * own inlined CKorgProgBankFile::GetFileName()) pushes EVERY argument
 * (dest/size/fmt/varargs) onto the stack with no register setup at all,
 * confirming `regparm(0)` (a variadic function is never register-mapped
 * under -mregparm=3 in this project's own established convention). */
extern "C" __attribute__((regparm(0))) int snprintf(char *buf, unsigned long size, const char *fmt, ...);

/*
 * CKorgPreloadFile::CKorgPreloadFile(const char *name) -- ground-truthed
 * byte-exact, .text+0x44e40 (10 bytes): regparm(3) this=EAX, name=EDX.
 *   movl $(_ZTV16CKorgPreloadFile+8),(%eax)
 *   mov  %edx,0x4(%eax)
 *   ret
 * Real vtable data auto-emitted here since ~CKorgPreloadFile() (below)
 * is this class's own Itanium "key function" -- same "extern C byte-
 * array trick, no separate definition needed" technique as
 * startup_file.cpp's CStartupFile.
 */
extern "C" unsigned char _ZTV16CKorgPreloadFile[];

CKorgPreloadFile::CKorgPreloadFile(const char *name)
{
	_vtablePtr = _ZTV16CKorgPreloadFile + 8;
	_name = name;
}

/*
 * ~CKorgPreloadFile() (D1/D2, .text+0x44cc0, 7 bytes -- both fold to the
 * same address, confirmed via nm, no virtual bases) confirmed real body:
 * ONLY a vtable-pointer reset, no other cleanup (`_name` isn't owned/
 * freed). Written out explicitly (rather than left compiler-implicit)
 * so this TU is this class's own key function, matching the
 * CStartupFile precedent exactly.
 */
CKorgPreloadFile::~CKorgPreloadFile()
{
	_vtablePtr = _ZTV16CKorgPreloadFile + 8;
}

/*
 * CKorgProgBankFile::CKorgProgBankFile(const char *name) -- ground-
 * truthed byte-exact, .text+0x44fe0 (40 bytes): regparm(3) this=EAX
 * (saved to EBX across the base-ctor call), name=EDX.
 *   call CKorgPreloadFile::CKorgPreloadFile(this, name)   ; base ctor first
 *   movl $(_ZTV17CKorgProgBankFile+8),(%ebx)               ; own vtable install
 *   movl $0x2,0x8(%ebx)                                    ; _field8 = 2 (literal)
 *   ret
 * `InitializePerformances()`'s own destructor call site only invokes
 * `CKorgPreloadFile::~CKorgPreloadFile()` directly (ground truth inlines
 * CKorgProgBankFile's own dtor down to just a vtable-pointer reset, no
 * separate out-of-line D2 symbol) -- reproduced here by NOT declaring an
 * explicit ~CKorgProgBankFile() at all, letting the compiler-synthesized
 * (implicit) derived dtor chain to the base dtor automatically, same
 * technique as the base class's own dtor above.
 */
extern "C" unsigned char _ZTV17CKorgProgBankFile[];

CKorgProgBankFile::CKorgProgBankFile(const char *name)
	: CKorgPreloadFile(name)
{
	_vtablePtr = _ZTV17CKorgProgBankFile + 8;
	_field8 = 2;
}

/*
 * kBankInfo's own two fields, per bank id (ground-truthed byte-exact
 * from `.data+0x380`, `nm`-confirmed 0xb8-byte/23-entry span -- see
 * InitializePerformances()'s own class comment in oa_global.h):
 *   [0] = default "type" value (also the CSTGProgramBank::Initialize()
 *         `type` argument, and the per-bank bitmask polarity selector);
 *         conditionally overwritten with CKorgProgBankFile::_field8 (the
 *         confirmed real constant `2`) if this bank's own Load() fails.
 *   [1] = default "flags" byte (only the LOW BYTE is ever read -- becomes
 *         CSTGProgramBank::Initialize()'s own `bool` argument); NEVER
 *         written by this function, always the static default below.
 */
void CSTGGlobal::InitializePerformances()
{
	static int kBankInfo[23][2] = {
		{1, 0}, {0, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {0, 1}, {0, 1},
		{0, 1}, {1, 0}, {1, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
		{0, 0}, {0, 0}, {1, 0}, {1, 0}, {1, 0}, {0, 0}, {0, 0},
	};

	unsigned char *self = (unsigned char *)this;

	/* Block 1: real -- 23-bank file-loading loop (see class comment). */
	for (unsigned int bankId = 0; bankId < 23; bankId++) {
		char buf[11];

		if (bankId <= 0xf)
			snprintf(buf, sizeof(buf), "PROG%c.BIN", (char)(bankId + 0x41));
		else
			snprintf(buf, sizeof(buf), "PROG%c%c.BIN",
				 (char)(bankId + 0x31), (char)(bankId + 0x31));

		CKorgProgBankFile file(buf);
		if (!file.Load())
			kBankInfo[bankId][0] = (int)file._field8;

		int typeArg = kBankInfo[bankId][0];
		unsigned char flagByte = (unsigned char)kBankInfo[bankId][1];

		unsigned int *bitmask = (unsigned int *)(STGAPIFrontPanelStatus::sInstance + 0x294f8);
		if (typeArg == 0)
			*bitmask |= (1u << bankId);
		else
			*bitmask &= ~(1u << bankId);

		CSTGProgramBank *bank = (CSTGProgramBank *)(self + 0x132e4d0 + bankId * 0x67603u);
		bank->Initialize(bankId, (unsigned int)typeArg, flagByte != 0);
		/* `file`'s destructor runs here, at loop-body scope exit --
		 * matches ground truth's own call ordering exactly (Initialize()
		 * before the destructor). */
	}

	/* Block 2: real -- GetPatchSize()/CSTGProgram::Initialize() on the
	 * literal, non-loop-carried bank id 6. */
	{
		CSTGProgramBank *bank6 = (CSTGProgramBank *)(self + 0x132e4d0 + 6u * 0x67603u);
		unsigned int patchSize = bank6->GetPatchSize();

		CSTGProgram *prog = (CSTGProgram *)(self + 0x2976e33u);
		prog->Initialize(6, patchSize, 1);
	}

	/* Block 3: DEFERRED -- see PopulateDefaultProgramSlotTemplates()'s
	 * own comment (oa_global.h) and InitializePerformances()'s own class
	 * comment for the full two-nested-loop derivation. */
	PopulateDefaultProgramSlotTemplates();

	/* Block 4: real -- a simple (non-nested) 200-item virtual-dispatch
	 * loop over a DIFFERENT array (`this+0x27cd024`, stride 0x1cad),
	 * same vtable slot 0x58 as block 3's own call. Zero-explicit-
	 * argument call (`this` only) -- confirmed via register tracing.
	 * Modeled via this project's own established raw-vtable-dispatch
	 * idiom (see klm_manager.cpp's vcall()/stamp_object()). */
	{
		unsigned char *arrBase = self + 0x27cd024u;
		for (unsigned int i = 0; i < 200; i++) {
			unsigned char *obj = arrBase + i * 0x1cadu;
			void *const *vtbl = *(void *const *const *)obj;
			((void (*)(void *))vtbl[0x58 / 4])(obj);
		}
	}

	/* Block 5: real -- fully deterministic CSTGPerfChangeRequest, every
	 * field a literal immediate (zero relocations in ground truth). */
	{
		CSTGPerfChangeRequest request;
		request.tag = 0;
		request.mode = 0;
		request.value1 = 0;
		request.value2 = 0;
		request.source = 3;
		request.field14 = 0;
		request.field18 = 0;
		SubmitPerfChangeRequest(request);
	}
}
