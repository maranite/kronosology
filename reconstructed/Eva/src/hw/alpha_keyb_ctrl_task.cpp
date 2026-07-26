/*
 * alpha_keyb_ctrl_task.cpp  -  see include/alpha_keyb_ctrl_task.h.
 *
 * CAlphaKeybCtrlTask::CAlphaKeybCtrlTask/~CAlphaKeybCtrlTask/Exec/Initialize/
 * SetCtrlCondition/ProcessEvent transcribed from
 * CAlphaKeybCtrlTask@0823f2a0.c/~CAlphaKeybCtrlTask@0823e9d0.c/Exec@0823e930.c/
 * Initialize@0823ef50.c/SetCtrlCondition@0823efc0.c/ProcessEvent@0823f0f0.c.
 *
 * Raw vtable-dispatch helper style (inline function-pointer typedefs at each call
 * site) matches poller.cpp's own established convention for the identical
 * `Api->vtbl[0xac]` named-resource-lookup shape.
 */

#include "alpha_keyb_ctrl_task.h"
#include "keyboard_layout.h"
#include "locale_manager.h"
#include "level_manager_array.h"
#include "out_link.h"
#include "module.h"
#include "omega_vtables.h"
#include "system_api.h"

#include <cstdlib>
#include <cstring>
#include <new>

extern CSystemApi *Api; /* mains.cpp */

namespace {

/* Real ground truth calls the same HAL_GetSystemTime() every other Tier-A HAL stub
 * in this project does (already defined, non-static, in sysex_msg_task_base.cpp --
 * `unsigned HAL_GetSystemTime() { return 0; }`). NOT re-declared `extern` here:
 * not every verify/* KAT binary links sysex_msg_task_base.o, so a cross-TU
 * dependency on that specific file would make this one un-link-able standalone.
 * A local `static` stub with identical behavior avoids both the missing-symbol
 * failure and any ODR/multiple-definition risk when both TUs DO end up linked
 * together (the full Eva binary, some verify targets).
 */
static unsigned HAL_GetSystemTime()
{
	return 0;
}

/* Diagnostic-only trace call Exec()'s own real body makes -- same "log-only, no
 * control-flow effect" status as every Api+0x90/+0x94 soft-assert elsewhere in this
 * project. No real target reconstructed (opaque HAL logging sink); no-op here.
 */
extern "C" void HAL_LLDebugTrace(const char * /*fmt*/, ...)
{
}

/*
 * The "AlphaKeybCode" interface-link sub-object CAlphaKeybCtrlTask's ctor builds at
 * +0x80 (mCodeIfc). See alpha_keyb_ctrl_task.h's header comment for the full
 * "why this is opaque, not a real COutLinkIfcBase/COutLinkIfc<T>/CMarshaller<T>"
 * derivation. Reproduces ground truth's own real byte offsets:
 *   +0x00..0x33  COutLink base subobject -- REAL, placement-constructed via the
 *                already-reconstructed COutLink ctor (out_link.h), then its vtable
 *                slot overridden (matching ground truth's own "install own vtable
 *                after base ctor" idiom, e.g. CPanel::CPanel()).
 *   +0x34        secondary "CMsgSender"-shaped thunk-vtable install (opaque
 *                placeholder, PTR__COutLinkIfc_AlphaKeybCode_08eabd48).
 *   +0x38        interfaceId (IAlphaKeybCode::sm_tInterfaceId -- a real ground-truth
 *                static, itself computed by an un-reconstructed static initializer;
 *                stored here as a plain 0, never read back by anything this
 *                project's own code does).
 *   +0x3c        unmarshallFn (opaque, stored not called).
 *   +0x40        mDirectTarget -- NEVER populated (see GetDirectIfcPtr() below).
 *   +0x44        real ground truth zeroes this too; undecoded.
 *   +0x48        CMarshaller<IAlphaKeybCode> sub-object vtable install (opaque
 *                placeholder, PTR__CMarshaller_AlphaKeybCode_08e89f18).
 *   +0x4c        self-referential back-pointer to +0x34 (matches ground truth's own
 *                `this_01+0x4c = this_01+0x34`).
 * Total 0x50 bytes, matching the real `malloc(0x50)` call site.
 */
void *BuildAlphaKeybCodeIfcLink(CTask *owner)
{
	void *buf = malloc(0x50);
	new (buf) COutLink(*owner, "AlphaKeybCode", COutLink::eDirectionOut, 0x804b, 1);

	*reinterpret_cast<void **>(buf) = (void *)PTR__COutLinkIfc_AlphaKeybCode_08eabd48;

	char *base = reinterpret_cast<char *>(buf);
	*reinterpret_cast<void **>(base + 0x34) = (void *)PTR__COutLinkIfc_AlphaKeybCode_08eabd48;
	*reinterpret_cast<unsigned int *>(base + 0x38) = 0;  /* interfaceId, undecoded */
	*reinterpret_cast<void **>(base + 0x3c) = 0;          /* unmarshallFn, opaque */
	*reinterpret_cast<void **>(base + 0x40) = 0;          /* mDirectTarget */
	*reinterpret_cast<void **>(base + 0x44) = 0;
	*reinterpret_cast<void **>(base + 0x48) = (void *)PTR__CMarshaller_AlphaKeybCode_08e89f18;
	*reinterpret_cast<void **>(base + 0x4c) = (void *)(base + 0x34);

	return buf;
}

/* COutLinkIfcBase::GetDirectIfcPtr(CLevelLocker&) const, .text+0x0807b8e0, 155
 * bytes -- fully reconstructed for real (self-contained, only touches `ifcLink`'s
 * own +0x40/+0x44 fields, which this reconstruction fully controls). `ifcLink` is
 * `mCodeIfc`'s raw buffer; `levelLocker` is the caller's own `{CLevelManager*,
 * SStateRegisterForMsg}`-shaped out-buffer (ProcessEvent()'s own `local_3c`).
 * Always returns NULL here in practice: `mDirectTarget` (ifcLink+0x40) is never
 * populated by BuildAlphaKeybCodeIfcLink() above, matching the real ground-truth
 * field's own "no observed real populator" status this batch's research turned up.
 */
void *GetDirectIfcPtr(void *ifcLink, void *levelLocker)
{
	char *base = reinterpret_cast<char *>(ifcLink);
	void *target = *reinterpret_cast<void **>(base + 0x40);
	void *result = 0;
	if (target == 0)
		return result;

	char *targetBase = reinterpret_cast<char *>(target);
	char haveDirect = *reinterpret_cast<char *>(targetBase + 0x38);
	if (haveDirect != 0)
		result = *reinterpret_cast<void **>(targetBase + 0x34);

	if (*reinterpret_cast<int *>(levelLocker) == 0) {
		if (result != 0) {
			*reinterpret_cast<void **>(levelLocker) = result;
			CLevelManager::StopForMessage(result,
			                                reinterpret_cast<char *>(levelLocker) + 4);
			target = *reinterpret_cast<void **>(base + 0x40);
			haveDirect = *reinterpret_cast<char *>(reinterpret_cast<char *>(target) + 0x38);
		}
	} else {
		/* Real: soft Api+0x94 assertion ("IfcOutLink.cpp", line 100) -- log-only,
		 * no control-flow effect, omitted per this project's established
		 * convention.
		 */
		target = *reinterpret_cast<void **>(base + 0x40);
		haveDirect = *reinterpret_cast<char *>(reinterpret_cast<char *>(target) + 0x38);
	}

	result = 0;
	if (haveDirect != 0)
		result = *reinterpret_cast<void **>(reinterpret_cast<char *>(target) + 0x30);
	return result;
}

} // namespace

void CAlphaKeybCtrlTask::LayoutVector::Append(CKeyboardLayout *layout)
{
	if (mEnd == mCap) {
		size_t curCap = mCap - mBegin;
		size_t newCap = curCap < 10 ? 10 : curCap;
		while (newCap < curCap + 1)
			newCap *= 2;

		CKeyboardLayout **fresh =
			(CKeyboardLayout **)malloc(newCap * sizeof(CKeyboardLayout *));
		size_t used = mEnd - mBegin;
		if (used != 0)
			memcpy(fresh, mBegin, used * sizeof(CKeyboardLayout *));
		free(mBegin);

		mBegin = fresh;
		mEnd = fresh + used;
		mCap = fresh + newCap;
	}

	*mEnd = layout;
	++mEnd;
}

void CAlphaKeybCtrlTask::LayoutVector::FreeAll()
{
	for (CKeyboardLayout **p = mBegin; p != mEnd; ++p)
		free(*p);
	free(mBegin);
	mBegin = mEnd = mCap = 0;
}

CAlphaKeybCtrlTask::CAlphaKeybCtrlTask(const CModule *owner, const char *hidDrvName)
	: CTask(*owner, "AlphaKeybCtrlTask", 4, 1, 0x804b)
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CAlphaKeybCtrlTask_08eabcc8;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
		(void *)&EvaDataPlaceholder_08eabce8;

	mHidResource = 0;
	mLayoutList.mVtbl = 0;
	mLayoutList.mBegin = mLayoutList.mEnd = mLayoutList.mCap = 0;
	mAsciiLayoutList.mVtbl = 0;
	mAsciiLayoutList.mBegin = mAsciiLayoutList.mEnd = mAsciiLayoutList.mCap = 0;
	mUnknown98 = 0;
	mUnknown9c = 0;

	/* Named-resource lookup, same call shape as CPoller::CPoller()'s own
	 * (poller.cpp) -- always returns NULL under this reconstruction's own
	 * LookupResourceStub (omega_vtables.cpp), so mHidResource stays NULL and
	 * buildExtraLayouts stays false on every real path below. Real ground truth's
	 * own "does this device support extra international layouts" out-param
	 * arrives via a compiler stack-slot-aliasing trick (the resource's own
	 * vtbl+8 call writes through what ground truth's disassembly computes as
	 * `local_38+3`, which happens to alias `local_2c[0]`) -- modeled directly
	 * here as a real out-param instead, since the alias itself has no
	 * independent meaning.
	 */
	bool buildExtraLayouts = false;
	if (hidDrvName != 0) {
		typedef void *(*FnLookup)(void *, const char *);
		void *apiVtbl = *(void **)Api;
		FnLookup lookup = *(FnLookup *)((char *)apiVtbl + 0xac);
		void *resource = lookup(Api, hidDrvName);
		mHidResource = resource;

		if (resource != 0) {
			typedef int (*FnGetType)(void *);
			void *resVtbl = *(void **)resource;
			FnGetType getType = *(FnGetType *)((char *)resVtbl + 0x10);
			if (getType(resource) != 10) {
				mHidResource = 0;
			} else {
				typedef int (*FnQuery)(void *, unsigned char *);
				FnQuery query = *(FnQuery *)((char *)resVtbl + 8);
				unsigned char extra = 0;
				if (query(resource, &extra) != 0)
					mHidResource = 0;
				else
					buildExtraLayouts = (extra == 1);
			}
		}
	}

	mUnknown84 = 1;
	mUnknown88 = 0xb;

	mCodeIfc = BuildAlphaKeybCodeIfcLink(this);
	CTask::Add((COutLink *)mCodeIfc);

	/* Default layout -- built unconditionally, appended to mLayoutList. */
	const SKeyboardLayoutDesc &def = kKeyboardLayoutDescs[0];
	CKeyboardLayout *defLayout =
		new (malloc(sizeof(CKeyboardLayout))) CKeyboardLayout(def.type, def.table,
		                                                        def.name, def.flag);
	mLayoutList.Append(defLayout);

	if (buildExtraLayouts) {
		for (int i = 1; i < 14; ++i) {
			const SKeyboardLayoutDesc &d = kKeyboardLayoutDescs[i];
			CKeyboardLayout *layout =
				new (malloc(sizeof(CKeyboardLayout))) CKeyboardLayout(d.type, d.table,
				                                                        d.name, d.flag);
			mLayoutList.Append(layout);
			CLocaleManager::GetInstance()->AddKeyboardLayout(layout);
		}
	}

	/* Custom ASCII layout -- built unconditionally, appended to mAsciiLayoutList
	 * (NOT mLayoutList).
	 */
	CKeyboardLayout *asciiLayout =
		new (malloc(sizeof(CKeyboardLayout)))
			CKeyboardLayout(kCustomAsciiLayoutDesc.type, kCustomAsciiLayoutDesc.table,
			                 kCustomAsciiLayoutDesc.name, kCustomAsciiLayoutDesc.flag);
	mAsciiLayoutList.Append(asciiLayout);

	CLocaleManager *localeMgr = CLocaleManager::GetInstance();
	localeMgr->AddKeyboardLayout(defLayout);
	localeMgr->AddKeyboardLayout(asciiLayout);

	mUnknown90 = 0;
	mUnknown94 = 0;
	mUnknown8c = 0;
}

CAlphaKeybCtrlTask::~CAlphaKeybCtrlTask()
{
	*reinterpret_cast<void **>(this) = (void *)PTR__CAlphaKeybCtrlTask_08eabcc8;
	*reinterpret_cast<void **>(reinterpret_cast<char *>(this) + 8) =
		(void *)&EvaDataPlaceholder_08eabce8;

	mLayoutList.FreeAll();
	mAsciiLayoutList.FreeAll();

	if (mHidResource != 0) {
		typedef void (*FnRelease)(void *, int);
		void *vtbl = *(void **)mHidResource;
		FnRelease release = *(FnRelease *)((char *)vtbl + 0xc);
		release(mHidResource, 0);
	}

	/* mCodeIfc intentionally not freed here -- see header comment (drained via
	 * the base CTask::~CTask()'s own generic mOutLinks loop instead, which never
	 * calls the element's own dtor either -- a real, faithfully-preserved
	 * ground-truth leak).
	 */
}

int CAlphaKeybCtrlTask::Exec()
{
	void *resource = mHidResource;
	if (resource == 0) {
		SetMask(1);
		return -1;
	}

	void *resVtbl = *(void **)resource;
	typedef char (*FnBool)(void *);

	FnBool hasCode = (FnBool)*(void **)((char *)resVtbl + 0x28);
	if (hasCode(resource) != 0) {
		/* Real ground truth builds a 4-dword stack buffer {1, <uninitialized>,
		 * <uninitialized>, 0x7f} and passes it to the resource's own vtbl+0x1c
		 * "notify" slot; on success, the SAME buffer is handed to ProcessEvent()
		 * via this object's own vtable slot 5 (self-dispatch -- see header
		 * comment). ProcessEvent()'s own real argument type is 0x14 (20) bytes
		 * (SKeyboardEvt); ground truth's own buffer is only 16 bytes, so its real
		 * `modifiers` byte read (offset 0x10) is a genuine 1-byte stack overread
		 * of whatever adjacent memory follows -- padded to 5 dwords here with an
		 * explicit 0 instead of reproducing true UB (same "don't reproduce a raw
		 * overread as actual UB in our own code" choice hid_driver.h's own
		 * HIDUsbKeybEvent::reserved4 note already made for the analogous
		 * situation).
		 */
		unsigned int notifyBuf[5] = {1, 0, 0, 0x7f, 0};
		typedef char (*FnNotify)(void *, unsigned int *);
		FnNotify notify = (FnNotify)*(void **)((char *)resVtbl + 0x1c);
		if (notify(resource, notifyBuf) != 0) {
			typedef void (*FnDispatch)(void *, void *);
			void *selfVtbl = *(void **)this;
			FnDispatch dispatch = (FnDispatch)*(void **)((char *)selfVtbl + 0x14);
			dispatch(this, notifyBuf);
		}
	}

	FnBool hasOvercurrentIter = (FnBool)*(void **)((char *)resVtbl + 0x20);
	if (hasOvercurrentIter(resource) != 0) {
		unsigned t = HAL_GetSystemTime();
		HAL_LLDebugTrace(" HID Notify Iter for OC = true - %08d", t);
	}
	return 0;
}

void CAlphaKeybCtrlTask::Initialize()
{
	/* Both dereferences are always valid: mOwnerModule (CTask's own +0x3c field,
	 * task.h) is set unconditionally by the base CTask ctor; mCodeIfc is always
	 * constructed by this class's own ctor above.
	 */
	const CModule *owner = *reinterpret_cast<const CModule *const *>(
		reinterpret_cast<const char *>(this) + 0x3c);
	const char *ownerName = *reinterpret_cast<char *const *>(
		reinterpret_cast<const char *>(owner) + 4);
	const char *taskName = *reinterpret_cast<char *const *>(
		reinterpret_cast<const char *>(this) + 4);
	const char *codeIfcName = static_cast<COutLink *>(mCodeIfc)->GetName();

	typedef void (*FnRegister)(void *, const char *, const char *, const char *,
	                             const char *, const char *, int);
	void *apiVtbl = *(void **)Api;
	FnRegister reg = (FnRegister)*(void **)((char *)apiVtbl + 0x44);
	reg(Api, ownerName, taskName, codeIfcName, "Editor", "AlphaKeybIfcTask", 0);
}

unsigned int CAlphaKeybCtrlTask::SetCtrlCondition(unsigned char keycode, bool down)
{
	static unsigned int s_iStatusBits = 0;

	if (!down) {
		if (keycode == 'X') {
			unsigned int bit = s_iStatusBits & 4;
			s_iStatusBits &= ~8u; /* real ground truth reads bit 4 but clears bit
			                        * 8 -- transcribed literally, not "fixed"
			                        * (same asymmetric-bit class as 'L' below). */
			return bit == 0;
		}
		if (keycode < 0x59) {
			if (keycode == ';') {
				unsigned int bit = s_iStatusBits & 2;
				s_iStatusBits &= ~1u; /* reads bit 2, clears bit 1 -- real ground
				                        * truth, same asymmetric-bit class. */
				return bit == 0;
			}
			if (keycode == 'L') {
				unsigned int bit = s_iStatusBits & 8;
				s_iStatusBits &= ~4u; /* reads bit 8, clears bit 4 -- real ground
				                        * truth, transcribed literally, not
				                        * "fixed". */
				return bit == 0;
			}
		} else if (keycode == 'a') {
			unsigned int bit0 = s_iStatusBits & 1;
			s_iStatusBits &= ~2u;
			return bit0 ^ 1;
		}
		return 1;
	}

	if (keycode == 'X') {
		unsigned int bits = s_iStatusBits & 0xc;
		s_iStatusBits |= 8;
		return bits == 0;
	}
	if (keycode < 0x59) {
		if (keycode == ';') {
			unsigned int result = 0;
			if ((s_iStatusBits & 1) == 0)
				result = (s_iStatusBits & 2) == 0;
			s_iStatusBits |= 1;
			return result;
		}
		if (keycode == 'L') {
			unsigned int bits = s_iStatusBits & 0xc;
			s_iStatusBits |= 4;
			return bits == 0;
		}
	} else if (keycode == 'a') {
		unsigned int result = 0;
		if ((s_iStatusBits & 1) == 0)
			result = (s_iStatusBits & 2) == 0;
		s_iStatusBits |= 2;
		return result;
	}
	return 1;
}

int CAlphaKeybCtrlTask::ProcessEvent(const SKeyboardEvt *evt)
{
	unsigned int isKeyDown = evt->isKeyDown;
	unsigned char modifiers = evt->modifiers;
	unsigned int keycode = evt->keycode;

	bool bit10 = (modifiers & 0x10) != 0;
	unsigned int rowOffset = (modifiers & 0x40) == 0 ? (bit10 ? 2 : 0) : (bit10 ? 3 : 1);

	bool valid;
	switch (keycode) {
	case 0x37: case 0x38: case 0x39:
	case 0x48: case 0x49: case 0x4a:
	case 0x5a: case 0x5b: case 0x5c:
	case 0x65: case 0x66:
		valid = true;
		if ((modifiers & 4) != 0)
			rowOffset = 1;
		break;
	case 0x3b: case 0x4c: case 0x58: case 0x61:
		valid = SetCtrlCondition((unsigned char)keycode, isKeyDown == 1) != 0;
		break;
	default:
		valid = true;
		break;
	}

	void *layout = CLocaleManager::GetInstance()->GetKeyboardLayout(0x8409);
	if (layout == 0)
		return 0;

	/* Real: `*(ushort*)(layout + 2 + (rowOffset + keycode*4) * 2)` -- indexes
	 * directly into the layout's own 1024-byte keymap table (keyboard_layout.h).
	 * Never reached under this reconstruction's own CLocaleManager stub (always
	 * returns NULL above) -- kept for structural fidelity only.
	 */
	unsigned short mapped =
		*reinterpret_cast<unsigned short *>(reinterpret_cast<char *>(layout) + 2 +
		                                      (rowOffset + keycode * 4) * 2);
	if (!valid || mapped == 0xfffe || mapped == 0xffff)
		return 1;

	/* IAlphaKeybCode::SKeyboardCode -- not reconstructed (see header comment);
	 * built here as a minimal, opaque 24-byte local buffer (matching ground
	 * truth's own `local_28[6]`/24-byte reservation) purely so the dispatch call
	 * below has a well-formed pointer to pass. Never read back by any code this
	 * reconstruction controls (the dispatch resolves to EvaVTableStub).
	 */
	unsigned int codeBuf[6] = {mapped, (isKeyDown == 1) ? 1u : 0u, evt->unused8, 0, 0, 0};

	/* {CLevelManager*, SStateRegisterForMsg (16 bytes, opaque)} combined out-buffer --
	 * ground truth's own real locals (`local_3c`/`local_38`) are 2 SEPARATE adjacent
	 * stack variables that GetDirectIfcPtr()'s/StopForMessage()'s own real bodies
	 * treat as one contiguous 20-byte region via address arithmetic (`param_1+4`
	 * landing exactly on the second variable) -- modeled here as one real array to
	 * give that arithmetic a genuinely valid, correctly-sized destination instead of
	 * reproducing the aliasing as true UB.
	 */
	void *levelLocker[5] = {0, 0, 0, 0, 0};
	void *direct = GetDirectIfcPtr(mCodeIfc, levelLocker);

	char *dispatchTarget = reinterpret_cast<char *>(mCodeIfc) + 0x48;
	if (direct != 0)
		dispatchTarget = reinterpret_cast<char *>(direct);

	typedef void (*FnProcessCode)(void *, unsigned int *);
	void *dispatchVtbl = *reinterpret_cast<void **>(dispatchTarget);
	FnProcessCode processCode = (FnProcessCode)*(void **)((char *)dispatchVtbl + 0xc);
	processCode(dispatchTarget, codeBuf);

	if (levelLocker[0] != 0)
		CLevelManager::ResumeAfterMessage(levelLocker[0], &levelLocker[1]);

	return 1;
}
