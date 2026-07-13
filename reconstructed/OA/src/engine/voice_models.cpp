// SPDX-License-Identifier: GPL-2.0
/*
 * voice_models.cpp  -  CSTGVoiceModel base ctor, CSTGVoiceModelManager::
 * Register(), and the ten derived "Model" ctors (CSTGOffModel/PCMModel/
 * AnalogSyncModel/OrganModel/PluckedModel/MS20Model/PolysixModel/
 * VPMModel/PianoModel/EPModel). Batch 42 -- see oa_engine_init.h's own
 * header comment (right above the `CSTGVoiceModel` class) for the full
 * ground-truthing detail and the sec 10.147/10.154/10.185 policy history
 * this supersedes.
 *
 * Also builds each model's own REAL (correctly-shaped) vtable, matching
 * ground truth's confirmed 0x5c-byte / 23-slot layout exactly the way
 * `kCCostProfileVtbl` (cost_profile.cpp, sec 10.186) does -- header
 * (offset-to-top/RTTI) + D1/D0 (null, never invoked: these are
 * permanent, module-lifetime singletons, same reasoning as
 * CCostProfile's own dtor slots) + slot 2 (`Initialize`, real dispatch
 * target for `CallVtableSlot(obj, 2)` in engine_init.cpp) + 15 more
 * nulls + slot 18 (`ProcessSubRate`) + slot 19 (`ProcessAudioRate`,
 * real dispatch targets for `CSTGVoiceModelManager::ProcessSubRate`/
 * `ProcessAudioRate`, sec 10.137, once `Register()` below actually
 * populates the array those two already-real methods walk) + 3 more
 * nulls. Every other slot (GetId/GetName/GetNumAudioRateParams/...) is
 * confirmed NEVER dispatched by any code path this reconstruction can
 * currently reach, so null is safe there -- same "install vs dispatch"
 * rule this project has applied since sec 10.153.
 */

#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_bank_memory.h"
#include "oa_new_delete.h"

static unsigned int ToU32(void *p) { return (unsigned int)(unsigned long)p; }

typedef void (*oa_vfn)(void);

/*
 * CSTGVoiceModel::CSTGVoiceModel(eSTGVoiceModelType) -- `.text+0x1a9b10`,
 * 338 bytes, confirmed real. See oa_engine_init.h's own header comment
 * for the full field-by-field derivation; this is a direct transliteration
 * of that confirmed layout.
 */
CSTGVoiceModel::CSTGVoiceModel(eSTGVoiceModelType type)
{
	unsigned char *self = (unsigned char *)this;

	/* +0x84..+0xd1 (78 bytes), confirmed zeroed -- the real ctor repeats
	 * this exact same range a second time later (a genuine but
	 * functionally-inert redundancy); collapsed to one write here, see
	 * header comment. */
	for (unsigned int i = 0; i < 0x4e; i++)
		self[0x84 + i] = 0;

	self[0xe2] &= 0xfc;
	self[0xe1] = 0;
	*(unsigned short *)(self + 0xf0) = 0;
	*(unsigned int *)(self + 0xec) = ToU32(CSTGBankMemory::AllocAligned(0xcc0, 0x10));
	*(unsigned short *)(self + 0xf8) = 0;
	*(unsigned int *)(self + 0xf4) = ToU32(CSTGBankMemory::AllocAligned(0x6a0, 0x10));
	*(unsigned int *)(self + 0xfc) = 0;

	/* Per-audio-channel triple array: count = CSTGAudioManager::sInstance's
	 * own +0x18 field (still an opaque, not-yet-individually-named
	 * count within that class's own confirmed _unrecovered_head blob --
	 * read raw, matching this project's established convention for
	 * fields not yet promoted to a named struct member elsewhere). */
	unsigned int channelCount =
		*(unsigned int *)((unsigned char *)CSTGAudioManager::sInstance + 0x18);
	unsigned char *records =
		(unsigned char *)operator new[]((oa_size_t)(channelCount * 0xc));
	for (unsigned int i = 0; i < channelCount; i++) {
		*(unsigned int *)(records + i * 0xc + 0x0) = 0;
		*(unsigned int *)(records + i * 0xc + 0x4) = 0;
		*(unsigned int *)(records + i * 0xc + 0x8) = 0;
	}
	*(unsigned int *)(self + 0xd4) = ToU32(records);

	CSTGVoiceModelManager::sInstance->Register(type, this);

	*(unsigned short *)(self + 0xd8) = 0xffff;
	*(unsigned int *)(self + 0x100) = 0;
	self[0xe0] = 0;

	unsigned char *bufA = CSTGBankMemory::AllocAligned(0x1a80, 0x80);
	for (unsigned int i = 0; i < 0x1a80; i++)
		bufA[i] = 0;
	*(unsigned int *)(self + 0xe4) = ToU32(bufA);

	unsigned char *bufB = CSTGBankMemory::AllocAligned(0x3300, 0x80);
	for (unsigned int i = 0; i < 0x3300; i++)
		bufB[i] = 0;
	*(unsigned int *)(self + 0xe8) = ToU32(bufB);
}

/*
 * CSTGVoiceModelManager::Register(type, model) -- `.text+0x1a9a30`, 25
 * bytes, confirmed real (see oa_engine.h's own header comment for the
 * confirmed instruction-by-instruction shape).
 */
void CSTGVoiceModelManager::Register(eSTGVoiceModelType type, CSTGVoiceModel *model)
{
	/* +8/+0x30 are 4-byte (32-bit-target-pointer-wide) slot arrays, NOT
	 * native-pointer-wide -- must use the packed-32-bit ToU32 convention
	 * here, not a native `CSTGVoiceModel **` write, or a 64-bit host
	 * build stomps the ADJACENT slot's own value (confirmed by a real
	 * KAT failure this batch -- see test_voice_models.cpp). */
	unsigned char *p = (unsigned char *)this;
	unsigned int packed = ToU32(model);

	*(unsigned int *)(p + 8 + (unsigned int)type * 4) = packed;

	unsigned short n = *(unsigned short *)(p + 0x58);
	*(unsigned int *)(p + 0x30 + n * 4) = packed;
	*(unsigned short *)(p + 0x58) = (unsigned short)(n + 1);
}

/*
 * CSTGOffModel's own Initialize()/ProcessSubRate()/ProcessAudioRate()
 * are confirmed literally 1 byte (a bare `ret`) in ground truth --
 * reconstructed here for real, not deferred.
 */
extern "C" void OA_VoiceModel_Off_Initialize(void *self) { (void)self; }
extern "C" void OA_VoiceModel_Off_ProcessSubRate(void *self, unsigned int tick) { (void)self; (void)tick; }
extern "C" void OA_VoiceModel_Off_ProcessAudioRate(void *self, unsigned int tick) { (void)self; (void)tick; }

/*
 * GetId() (real ABI slot 3, immediately after Initialize) -- confirmed
 * real and TRIVIAL in ground truth for all ten models
 * (`.text._ZNK<Model>5GetIdEv`, 3-5 bytes: `xor eax,eax; ret` for Off,
 * `mov $N,%eax; ret` for the rest, N = this model's own
 * eSTGVoiceModelType ordinal, matching the Register(type,...) call each
 * ctor already makes below exactly). Newly REACHABLE now that
 * CSTGKLMManager::AuthorizeBuiltins() (klm_manager.cpp) is wired for
 * real (sec 10.234): its own `VM_GET_ID` vcall(vm, 0x0c) dispatches
 * through exactly this slot for every loaded voice model whose +0x104
 * flag is clear, and this project's own vtable left it null -- a live
 * boot crash (EIP=CR2=0, one instruction into AuthorizeBuiltins' first
 * loop) that this batch fixes by reconstructing the real body (cheaper
 * than Initialize() itself), not a stand-in.
 */
extern "C" unsigned int OA_VoiceModel_Off_GetId(const void *self) { (void)self; return 0; }
extern "C" unsigned int OA_VoiceModel_PCM_GetId(const void *self) { (void)self; return 1; }
extern "C" unsigned int OA_VoiceModel_AnalogSync_GetId(const void *self) { (void)self; return 2; }
extern "C" unsigned int OA_VoiceModel_Organ_GetId(const void *self) { (void)self; return 3; }
extern "C" unsigned int OA_VoiceModel_Plucked_GetId(const void *self) { (void)self; return 4; }
extern "C" unsigned int OA_VoiceModel_MS20_GetId(const void *self) { (void)self; return 5; }
extern "C" unsigned int OA_VoiceModel_Polysix_GetId(const void *self) { (void)self; return 6; }
extern "C" unsigned int OA_VoiceModel_VPM_GetId(const void *self) { (void)self; return 7; }
extern "C" unsigned int OA_VoiceModel_Piano_GetId(const void *self) { (void)self; return 8; }
extern "C" unsigned int OA_VoiceModel_EP_GetId(const void *self) { (void)self; return 9; }

/*
 * GetAuthField()const / SetAuthField(int) / SetProductId(unsigned long) --
 * real ABI slots 13/14/15 (byte offsets 0x34/0x38/0x3c from the adjusted
 * vtable pointer, i.e. array indices 15/16/17). Confirmed via
 * `objdump -r` on `OA_real.ko`'s own `.rodata._ZTV12CSTGOffModel`
 * relocations (sec 10.234, same pass as GetId above) to be
 * `CSTGVoiceModel` BASE-CLASS methods, not per-derived-model overrides
 * (identical weak symbols at every model's own slot) -- confirmed
 * trivial one-instruction accessors: `GetAuthField() const` reads
 * `this+0x100`, `SetAuthField(int)` writes `edx` to `this+0x100`,
 * `SetProductId(unsigned long)` writes `edx` to `this+0x104` (==
 * `VM_EXTRA_OFF`, `oa_internal.h` -- the SAME field AuthorizeBuiltins'
 * own loop already gates on being clear and IsAuthorizedVoiceModel's
 * own `extra` read, now confirmed to also be this real slot's target).
 * Newly REACHABLE for the identical reason as GetId: `klm_manager.cpp`'s
 * `stamp_object()` (called from `AuthorizeBuiltins()`) dispatches
 * through exactly these two slots (SET_AUTH/RECOMPUTE) for every stamped
 * voice model -- previously null, the SECOND live-boot NULL-call crash
 * this same session hit (one call further into `AuthorizeBuiltins()`
 * once GetId's own crash was fixed). One shared implementation per
 * method (matching ground truth's own base-class sharing), not ten.
 */
extern "C" unsigned int OA_VoiceModel_GetAuthField(const void *self)
{
	return *(const unsigned int *)((const unsigned char *)self + 0x100);
}
extern "C" void OA_VoiceModel_SetAuthField(void *self, unsigned int value)
{
	*(unsigned int *)((unsigned char *)self + 0x100) = value;
}
extern "C" void OA_VoiceModel_SetProductId(void *self, unsigned int value)
{
	*(unsigned int *)((unsigned char *)self + 0x104) = value;
}

/*
 * Each model's own real vtable. Layout (23 slots, matching ground
 * truth's confirmed 0x5c-byte size exactly):
 *   [0..1]  offset-to-top, RTTI (0, `-fno-rtti`)
 *   [2..3]  D1/D0 (null -- singletons, never destructed)
 *   [4]     slot 2 = Initialize()
 *   [5]     slot 3 = GetId() const -- REAL (sec 10.234, see above)
 *   [6..14] slots 4..12 (null -- confirmed never dispatched)
 *   [15]    slot 13 = GetAuthField() const -- REAL (sec 10.234)
 *   [16]    slot 14 = SetAuthField(int) -- REAL (sec 10.234)
 *   [17]    slot 15 = SetProductId(unsigned long) -- REAL (sec 10.234)
 *   [18..19] slots 16..17 (null -- confirmed never dispatched)
 *   [20]    slot 18 = ProcessSubRate(unsigned int)
 *   [21]    slot 19 = ProcessAudioRate(unsigned int)
 *   [22]    slot 20 (null -- confirmed never dispatched)
 */
#define OA_VOICE_MODEL_VTABLE(name) \
	static const oa_vfn kVoiceModel_##name##_Vtbl[23] = { \
		0, 0, \
		0, 0, \
		(oa_vfn)&OA_VoiceModel_##name##_Initialize, \
		(oa_vfn)&OA_VoiceModel_##name##_GetId, \
		0, 0, 0, 0, 0, 0, 0, 0, 0, \
		(oa_vfn)&OA_VoiceModel_GetAuthField, \
		(oa_vfn)&OA_VoiceModel_SetAuthField, \
		(oa_vfn)&OA_VoiceModel_SetProductId, \
		0, 0, \
		(oa_vfn)&OA_VoiceModel_##name##_ProcessSubRate, \
		(oa_vfn)&OA_VoiceModel_##name##_ProcessAudioRate, \
		0, \
	}

OA_VOICE_MODEL_VTABLE(Off);
OA_VOICE_MODEL_VTABLE(PCM);
OA_VOICE_MODEL_VTABLE(AnalogSync);
OA_VOICE_MODEL_VTABLE(Organ);
OA_VOICE_MODEL_VTABLE(Plucked);
OA_VOICE_MODEL_VTABLE(MS20);
OA_VOICE_MODEL_VTABLE(Polysix);
OA_VOICE_MODEL_VTABLE(VPM);
OA_VOICE_MODEL_VTABLE(Piano);
OA_VOICE_MODEL_VTABLE(EP);

CSTGOffModel *CSTGOffModel::sInstance;
CSTGPCMModel *CSTGPCMModel::sInstance;
CSTGAnalogSyncModel *CSTGAnalogSyncModel::sInstance;
CSTGOrganModel *CSTGOrganModel::sInstance;
CSTGPluckedModel *CSTGPluckedModel::sInstance;
CSTGMS20Model *CSTGMS20Model::sInstance;
CSTGPolysixModel *CSTGPolysixModel::sInstance;
CSTGVPMModel *CSTGVPMModel::sInstance;
/* CSTGPianoModel::sInstance's storage is NOT defined here -- see
 * oa_engine_init.h's own header note on this class: it already lives in
 * src/auth/process_oacmd.cpp (that TU's separate, pre-existing,
 * incompatible `CSTGPianoModel` declaration ecosystem), and defining it
 * again here would be a real duplicate-symbol link error at `make ko`. */
CSTGEPModel *CSTGEPModel::sInstance;

/*
 * The ten derived ctors. Each: call the base ctor (installs the common
 * state above), overwrite the vtable pointer with this model's own real
 * (correctly-shaped) vtable, `+0x104 = 0`, `sInstance = this`, then this
 * model's own confirmed `+0xe1`/`+0xe2` flag-byte write(s) -- see
 * oa_engine_init.h's own header comment for the full per-model
 * confirmed byte values.
 */
CSTGOffModel::CSTGOffModel() : CSTGVoiceModel(eVoiceModel_Off)
{
	_vtablePtr = (void *)&kVoiceModel_Off_Vtbl[2];
	*(unsigned int *)((unsigned char *)this + 0x104) = 0;
	sInstance = this;
	unsigned char *self = (unsigned char *)this;
	self[0xe1] |= 0x3f;
}

CSTGPCMModel::CSTGPCMModel() : CSTGVoiceModel(eVoiceModel_PCM)
{
	_vtablePtr = (void *)&kVoiceModel_PCM_Vtbl[2];
	*(unsigned int *)((unsigned char *)this + 0x104) = 0;
	sInstance = this;
	unsigned char *self = (unsigned char *)this;
	self[0xe1] |= 0x7f;
}

CSTGAnalogSyncModel::CSTGAnalogSyncModel() : CSTGVoiceModel(eVoiceModel_AnalogSync)
{
	_vtablePtr = (void *)&kVoiceModel_AnalogSync_Vtbl[2];
	*(unsigned int *)((unsigned char *)this + 0x104) = 0;
	sInstance = this;
	unsigned char *self = (unsigned char *)this;
	self[0xe1] |= 0x57;
}

CSTGOrganModel::CSTGOrganModel() : CSTGVoiceModel(eVoiceModel_Organ)
{
	_vtablePtr = (void *)&kVoiceModel_Organ_Vtbl[2];
	*(unsigned int *)((unsigned char *)this + 0x104) = 0;
	sInstance = this;
	unsigned char *self = (unsigned char *)this;
	self[0xe1] = 0xc1;
}

CSTGPluckedModel::CSTGPluckedModel() : CSTGVoiceModel(eVoiceModel_Plucked)
{
	_vtablePtr = (void *)&kVoiceModel_Plucked_Vtbl[2];
	unsigned char *self = (unsigned char *)this;
	*(unsigned int *)(self + 0x104) = 0;
	sInstance = this;
	self[0xe1] = (unsigned char)((self[0xe1] & 0x80) | 0x77);
	self[0xe2] |= 0x1;
}

CSTGMS20Model::CSTGMS20Model() : CSTGVoiceModel(eVoiceModel_MS20)
{
	_vtablePtr = (void *)&kVoiceModel_MS20_Vtbl[2];
	unsigned char *self = (unsigned char *)this;
	*(unsigned int *)(self + 0x104) = 0;
	sInstance = this;
	self[0xe1] = 0xd7;
	self[0xe2] |= 0x1;
}

CSTGPolysixModel::CSTGPolysixModel() : CSTGVoiceModel(eVoiceModel_Polysix)
{
	_vtablePtr = (void *)&kVoiceModel_Polysix_Vtbl[2];
	*(unsigned int *)((unsigned char *)this + 0x104) = 0;
	sInstance = this;
	unsigned char *self = (unsigned char *)this;
	self[0xe1] = 0xd7;
}

CSTGVPMModel::CSTGVPMModel() : CSTGVoiceModel(eVoiceModel_VPM)
{
	_vtablePtr = (void *)&kVoiceModel_VPM_Vtbl[2];
	unsigned char *self = (unsigned char *)this;
	*(unsigned int *)(self + 0x104) = 0;
	sInstance = this;
	self[0xe1] = (unsigned char)((self[0xe1] & 0x80) | 0x77);
	self[0xe2] |= 0x1;
}

CSTGPianoModel::CSTGPianoModel() : CSTGVoiceModel(eVoiceModel_Piano)
{
	_vtablePtr = (void *)&kVoiceModel_Piano_Vtbl[2];
	*(unsigned int *)((unsigned char *)this + 0x104) = 0;
	sInstance = this;
	unsigned char *self = (unsigned char *)this;
	self[0xe1] = 0xd1;
}

CSTGEPModel::CSTGEPModel() : CSTGVoiceModel(eVoiceModel_EP)
{
	_vtablePtr = (void *)&kVoiceModel_EP_Vtbl[2];
	*(unsigned int *)((unsigned char *)this + 0x104) = 0;
	sInstance = this;
	unsigned char *self = (unsigned char *)this;
	self[0xe1] = 0xd1;
	self[0xe2] |= 0x2;
}
