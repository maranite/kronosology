/*
 * edit_server.cpp  -  see include/edit_server.h.
 *
 * CDataHandler::AddDescriptors/FindDescriptor(x2) transcribed from
 * AddDescriptors@0806d390.c/FindDescriptor@0806d8d0.c/FindDescriptor@0806e050.c.
 * The real disassembly for all 3 is an 8-way-unrolled (Duff's-device-shaped)
 * linear scan in every loop -- collapsed to plain loops here, same license
 * used throughout this project (omega_ptr_array.cpp, CDataHandler's own
 * header comment) since the unrolling has no behavioral effect, only order
 * of redundant re-checks.
 *
 * CEditServer::CEditServer/~CEditServer/FindDescriptor/SetDefault/Get/Set/
 * PutNotify transcribed from CEditServer@08070970.c/_CEditServer@08070820.c/
 * FindDescriptor@08070a80.c/SetDefault@0806fa90.c/Get@0806fb40.c/
 * Set@080700b0.c/PutNotify@08c1a680.c. Get()/Set() are intricate,
 * flag-driven dispatch functions (1302/1857 bytes) -- transcribed as
 * literally as reasonably possible (goto used at the real function's own
 * shared join points, matching its own label structure) to minimize
 * transcription risk. Neither is reachable from this reconstruction's own
 * currently-wired boot path yet (see edit_server.h's file header -- the
 * CXxxModuleConstructor::Create() call that would ever construct a live
 * CEditServer instance is gated behind CConfigManager::CreateUserModules(),
 * itself not pursued, config_manager.cpp), so this is "ready infrastructure"
 * rather than something a live kronos_vm boot test can currently exercise --
 * verified here by KAT against hand-built SDescriptor tables instead (see
 * verify/test_edit_server.cpp), same methodology this project already uses
 * for classes without an exercised real ctor (module.h's own precedent,
 * task.h's CTask::CTask() batch).
 */

#include "edit_server.h"
#include "omega_vtables.h"
#include "system_api.h"

#include <cstdlib>
#include <cstring>

#include <new>

extern CSystemApi *Api; /* mains.cpp */

/* ===========================================================================
 * Tier-B link-stubs: real subsystems genuinely out of scope for this batch
 * (see edit_server.h's own file header -- both are their own multi-method
 * classes, neither reconstructed elsewhere yet).
 * ===========================================================================
 */

/* CEditApiInstance -- the real scope broker every CEditServer registers
 * itself with (.text 080d1f70/080d1f30/080d2400). Only ever backed today by
 * the opaque EditApiInstance byte buffer (mains.cpp) -- no real C++ class
 * modeled yet. Stubbed the same way mains.cpp already stubs
 * CChkApiInstance::SetOwnerModule/CRMApiInstance::SetResMan: empty bodies,
 * GetAssignedScope returns 0xff (the same "unassigned" sentinel the real
 * ctor's own default write already uses before this call runs) since
 * nothing in this reconstruction can compute the real scope id without
 * CEditApiInstance's own ~14-method implementation.
 */
namespace {
	void EditApiInstance_RegisterServer(void * /*self*/, CEditServer * /*server*/) {}
	void EditApiInstance_UnregisterServer(void * /*self*/, CEditServer * /*server*/) {}
	unsigned char EditApiInstance_GetAssignedScope(void * /*self*/, const char * /*name*/) { return 0xff; }
}

/* CNotifyList::Put(uchar,uchar,uchar) (.text+0x08071000) -- the real
 * change-notification poster. Out of scope (its own subsystem: GrowEventsList/
 * ReleaseList/GetList, none reconstructed) -- stubbed as a no-op returning
 * "no notify posted" (0), matching PutNotify()'s own real gating (this
 * reconstruction's sm_bNotifyEnabled defaults false anyway, see
 * edit_server.h, so this stub is doubly unreachable today).
 */
namespace {
	int CNotifyList_Put(void * /*list*/, unsigned char /*scope*/, unsigned char /*index*/,
	                     unsigned char /*subIndex*/)
	{
		return 0;
	}
}

bool CEditServer::sm_bNotifyEnabled = false;

/* Real global static (CEditServer::m_oNotifyList, symbols.csv: 0930a248) --
 * the CNotifyList instance PutNotify() posts through. Not modeled as a real
 * object (CNotifyList is Tier-B, see above); kept as an opaque placeholder
 * pointer, matching this project's "declare the real global, stub what it
 * points at" convention used for FMApi/EditApi/etc. in mains.cpp.
 */
static void *g_oNotifyList = 0;

/* ===========================================================================
 * Shared PMF-resolution helper -- the real disassembly's own recurring
 * "tagged function pointer" idiom for a descriptor's custom getter/setter
 * callback (Get@0806fb40.c/Set@080700b0.c, 4 near-identical call sites each):
 * low bit clear -> plain function pointer; low bit set -> Itanium-ABI
 * virtual-thunk encoding, `*(code**)(pcVar5 + *(int*)base - 1)` in the real
 * decompile (note: the vtable-relative offset is added to the TAGGED
 * pointer value itself, not to the dereferenced vtable pointer -- numerically
 * equivalent either way since the tag is (byteOffset+1), transcribed exactly
 * as the real code computes it). Every real descriptor table entry this
 * project has actually seen elsewhere uses the even/direct encoding (same
 * "faithful but dead" caveat already documented for
 * CSTGUnsolMsgHandler::HandleMessage()'s own analogous odd-tag branch) --
 * neither Get() nor Set() have a real, currently-populated descriptor table
 * to observe this against yet (see file header).
 * ===========================================================================
 */
static void *ResolveDescriptorCallback(void *rawFn, void *base)
{
	unsigned long raw = (unsigned long)rawFn;
	if ((raw & 1) == 0)
		return rawFn;
	int vtblVal = *(int *)base;
	return *(void **)((char *)rawFn + vtblVal - 1);
}

static int CallApiNotifySlot(int notifyId, int arg)
{
	typedef int (*Fn)(void *, int, int);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x80);
	return fn(Api, notifyId, arg);
}

/* ===========================================================================
 * CDataHandler
 * ===========================================================================
 */

/* Real 3-word linear-list registration record AddDescriptors()/both
 * FindDescriptor() overloads share (malloc(0xc) in AddDescriptors' own "new
 * registration" path) -- not itself a named type in the real binary, matches
 * the same "recovered from field-write offsets" treatment as SDescriptor.
 */
namespace {
	struct SRegistration {
		CObjectBase  *owner;
		SDescriptor  *descriptors;
		int           count;
	};
}

void CDataHandler::AddDescriptors(CObjectBase *owner, SDescriptor *descriptors, int count,
                                   bool alreadyRegistered)
{
	if (alreadyRegistered) {
		/* De-dup path: find an existing registration with the same
		 * (descriptors,count) pair (reverse scan, most-recent first) and just
		 * retarget its owner. Falls through to the "new registration" path
		 * below if no match is found -- same real behavior as the ground
		 * truth's own final `goto LAB_0806d580` when the scan runs dry.
		 */
		for (int i = (int)mCount - 1; i >= 0; --i) {
			SRegistration *reg = (SRegistration *)mArray[i];
			if (reg && reg->descriptors == descriptors && reg->count == count) {
				reg->owner = owner;
				return;
			}
		}
	}

	SRegistration *reg = (SRegistration *)malloc(sizeof(SRegistration));
	reg->owner = owner;
	reg->descriptors = descriptors;
	reg->count = count;

	if (count >= 1) {
		for (int i = 0; i < count; ++i) {
			SDescriptor *d = &descriptors[i];
			unsigned base = (unsigned)d->baseIndex + 8u + (unsigned)d->group * 256u;
			mTable[base] = d;
			if ((d->flags & kFlagIndexed) && d->count > 1) {
				for (unsigned k = 1; k < d->count; ++k)
					mTable[base + k] = d;
			}
		}
	}

	/* Real: `*(CObjectBase**)(this+0x40020) = owner` -- 1 dword past
	 * CDataHandler's own declared 0x40020-byte bounds. CDataHandler is only
	 * ever embedded as CEditServer's own mData member with that field
	 * (CEditServer::mUnknown40024) immediately following it in memory, so
	 * this poke is safe and matches the real ground-truth write exactly --
	 * same "raw offset access spanning a nominal class boundary" idiom
	 * already used throughout this project (e.g. module.cpp's
	 * CModule::AdjustTaskMask() poking CTask fields via absolute offset).
	 * Never read back by any reconstructed method.
	 */
	*(CObjectBase **)((char *)this + 0x40020) = owner;

	Add(reg);
}

int CDataHandler::FindDescriptor(unsigned char group, unsigned char index, unsigned char subIndex,
                                  SDescriptor **outDescriptor, CObjectBase **outOwner) const
{
	*outDescriptor = 0;
	*outOwner = 0;

	/* Real: both the direct hash-table hit AND every iteration of the
	 * fallback scan independently re-check `this[0x1c] == group` (mScope) --
	 * an invariant condition for the duration of one call, so collapsed to
	 * a single outer check here (matches project convention of collapsing
	 * per-iteration-redundant-but-behaviorally-inert checks, e.g.
	 * CModule::AdjustTaskMask()'s own re-read-every-iteration count field).
	 */
	if (mScope != group)
		return 0;

	unsigned tableIdx = (unsigned)subIndex + 8u + (unsigned)index * 256u;
	if (mTable[tableIdx] != 0) {
		*outDescriptor = mTable[tableIdx];
		*outOwner = *(CObjectBase **)((char *)this + 0x40020); /* see AddDescriptors' own comment */
		return 1;
	}

	if (subIndex == 0xff && index == 0xff)
		return 0; /* reserved wildcard pair: no fallback scan, real behavior */

	for (int i = (int)mCount - 1; i >= 0; --i) {
		SRegistration *reg = (SRegistration *)mArray[i];
		if (!reg)
			continue;
		for (int j = reg->count - 1; j >= 0; --j) {
			SDescriptor *d = &reg->descriptors[j];
			if (d->group != index)
				continue;
			if (d->baseIndex == subIndex) {
				*outDescriptor = d;
				*outOwner = reg->owner;
				return 1;
			}
			if ((d->flags & kFlagIndexed) &&
			    (unsigned)subIndex >= d->baseIndex &&
			    (unsigned)subIndex < (unsigned)d->baseIndex + d->count) {
				*outDescriptor = d;
				*outOwner = reg->owner;
				return 1;
			}
		}
	}

	return 0;
}

int CDataHandler::FindDescriptor(CObjectBase *owner, void *callbackSlot, SDescriptor **outDescriptor) const
{
	*outDescriptor = 0;

	for (int i = (int)mCount - 1; i >= 0; --i) {
		SRegistration *reg = (SRegistration *)mArray[i];
		if (!reg || reg->owner != owner)
			continue;
		for (int j = reg->count - 1; j >= 0; --j) {
			SDescriptor *d = &reg->descriptors[j];
			/* Real: `*(unsigned**)pSVar8 == param_2` -- compares SDescriptor's
			 * own +0x00 field (the same field Get/Set treat as an int
			 * member-offset) directly against the caller's raw pointer/
			 * member-pointer value. Never exercised by any reconstructed
			 * caller (see edit_server.h's own comment on this overload).
			 */
			if (*(void **)&d->offset == callbackSlot) {
				*outDescriptor = d;
				return 1;
			}
		}
	}
	return 0;
}

/* ===========================================================================
 * CEditServer
 * ===========================================================================
 */

CEditServer::CEditServer(const char *name)
{
	mVtbl = (void *)PTR__CEditServer_08e817b0;

	new (&mData) COmegaPtrArray();
	*(void **)&mData = (void *)PTR__TPtrArray_08e817e8;
	mData.mLoopContinue = 1;
	mData.mScope = 0xff;
	memset(mData.mTable, 0, sizeof(mData.mTable));

	mUnknown40024 = 0;

	size_t len = strlen(name);
	char *dup = (char *)malloc(len + 1);
	mName = dup;
	strcpy(dup, name);

	unsigned char scope = EditApiInstance_GetAssignedScope(0 /* EditApiInstance */, mName);
	mAssignedScope = scope;
	mData.mScope = scope;

	EditApiInstance_RegisterServer(0 /* EditApiInstance */, this);

	mNotifyPending = 0;
	mNotifyLatch = 0;
}

CEditServer::~CEditServer()
{
	mVtbl = (void *)PTR__CEditServer_08e817b0;

	EditApiInstance_UnregisterServer(0 /* EditApiInstance */, this);

	if (mName != 0)
		free(mName);

	*(void **)&mData = (void *)PTR__TPtrArray_08e817e8;
	mData.Destroy();
	*(void **)&mData = (void *)PTR__COmegaPtrArray_08e80be0;
}

int CEditServer::FindDescriptor(unsigned char group, unsigned char index, unsigned char subIndex) const
{
	if (mAssignedScope != group)
		return 0;

	SDescriptor *d;
	CObjectBase *owner;
	return mData.FindDescriptor(mAssignedScope, index, subIndex, &d, &owner);
}

int CEditServer::SetDefault(unsigned char group, unsigned char index, unsigned char subIndex)
{
	SDescriptor *d;
	CObjectBase *owner;
	int found = mData.FindDescriptor(group, index, subIndex, &d, &owner);
	if (!found)
		return 0;
	if (!(d->flags & kFlagHasDefault))
		return 0;

	/* Real: dispatches through THIS object's own vtable slot +0xc -- a real
	 * per-derived-class virtual override never reconstructed (opaque
	 * EvaVTableStub dispatch, same treatment as every other undecoded vtable
	 * slot in this project).
	 */
	typedef int (*Fn)(CEditServer *, unsigned char, unsigned char, unsigned char, const void *, unsigned, int);
	Fn fn = *(Fn *)((char *)mVtbl + 0xc);
	return fn(this, group, index, subIndex, &d->defaultValue, 4, 0);
}

int CEditServer::Get(unsigned char group, unsigned char index, unsigned char subIndex,
                      void *buf, unsigned int bufLen)
{
	unsigned rawSubIndex = subIndex; /* uVar8 -- unmodified for the whole call */

	for (;;) {
		mData.mLoopContinue = 1;

		SDescriptor *d = 0;
		CObjectBase *owner = 0;
		int found = mData.FindDescriptor(group, index, subIndex, &d, &owner);

		if (found) {
			unsigned flags = d->flags;
			if (flags & (kFlagCommand | kFlagDisabled))
				return 0;

			unsigned n = 0;
			void *scratchBuf = 0;
			unsigned short scratch16 = 0;
			unsigned scratch32 = 0;
			unsigned char scratch8 = 0;

			if (!(flags & kFlagBlockCopy)) {
				if (flags & kFlagVarLen) {
					if (bufLen != d->count)
						return 0;
					n = bufLen;
					scratchBuf = buf;
				} else {
					n = (unsigned)d->elemSize;
					if (n == 2) scratchBuf = &scratch16;
					else if (n == 4) scratchBuf = &scratch32;
					else if (n == 1) scratchBuf = &scratch8;
					else return 0;
				}
			} else {
				if (bufLen < d->count)
					return 0;
				n = d->count;
				scratchBuf = buf;
			}

			int savedNotify = 1;
			if (d->notifyId != -1) {
				savedNotify = CallApiNotifySlot(d->notifyId, 0);
				flags = d->flags;
			}

			unsigned rawVal = 0;
			bool haveRawVal = false;

			if (flags & kFlagCustomGetter) {
				mNotifyPending = 1;
				flags = d->flags;

				void *self = (unsigned char *)owner + d->getterExtra;
				void *fn = ResolveDescriptorCallback(d->getterFn, self);

				int cbResult;
				typedef int (*Fn2)(void *, int, void *);
				typedef int (*Fn3)(void *, int, int, void *);
				if (!(flags & kFlagIndexed)) {
					if (!(flags & kFlagTwoArgCb))
						cbResult = ((Fn2)fn)(self, (int)rawSubIndex, scratchBuf);
					else
						cbResult = ((Fn3)fn)(self, (int)(unsigned char)index, (int)rawSubIndex, scratchBuf);
				} else {
					int adj = (int)(signed char)subIndex - (int)(signed char)d->baseIndex;
					if (!(flags & kFlagTwoArgCb))
						cbResult = ((Fn2)fn)(self, adj, scratchBuf);
					else
						cbResult = ((Fn3)fn)(self, (int)(unsigned char)index, adj, scratchBuf);
				}

				mNotifyPending = 0;

				if (cbResult < 0) {
					if (d->notifyId != -1)
						CallApiNotifySlot(d->notifyId, savedNotify);
					return 0;
				}

				if (cbResult > 0) {
					flags = d->flags;
					if (!(flags & kFlagVarLen) && !(flags & kFlagBlockCopy)) {
						if (n == 2) rawVal = scratch16;
						else if (n == 4) rawVal = scratch32;
						else if (n == 1) rawVal = scratch8;
						else return 0;
						haveRawVal = true;
					}
					/* kFlagVarLen or kFlagBlockCopy: nothing further, callback
					 * already wrote directly into buf (scratchBuf==buf for
					 * both cases, see the buffer-selection step above). */
				} else {
					/* cbResult == 0: fall back to the raw memory get, same as
					 * the !kFlagCustomGetter path below. */
					flags = d->flags;
					goto do_raw_get;
				}
			} else {
do_raw_get:
				if (!(flags & (kFlagBlockCopy | kFlagVarLen))) {
					int off = (flags & kFlagIndexed)
					        ? d->elemSize * (int)(rawSubIndex - d->baseIndex) + d->offset
					        : d->offset;
					unsigned char *src = (unsigned char *)owner + off;
					if (n == 2) rawVal = *(unsigned short *)src;
					else if (n == 4) rawVal = *(unsigned *)src;
					else if (n == 1) rawVal = (unsigned)*src;
					else return 0;
					haveRawVal = true;
				} else if (!(flags & kFlagIndexed)) {
					memcpy(scratchBuf, (unsigned char *)owner + d->offset, n);
				} else {
					memcpy(scratchBuf,
					       (unsigned char *)owner + d->elemSize * (int)(rawSubIndex - d->baseIndex) + d->offset,
					       n);
				}
				/* memcpy branches: never proceed to scalar widen -- matches
				 * the real code's own shared post-check (`if((flags&0x800)==0)
				 * {uVar4=flags&4; if(uVar4==0){...}}`), which for both memcpy
				 * branches here always evaluates uVar4 as the SAME
				 * kFlagBlockCopy bit that routed us into this half of the
				 * if/else in the first place (0x804 mask) -- never 0, so the
				 * scalar path never runs for a memcpy'd value. */
			}

			if (haveRawVal) {
				if (flags & kFlagSigned) {
					if (n == 2) rawVal = (unsigned)(short)rawVal;
					else if (n == 1) rawVal = (unsigned)(signed char)rawVal;
					else if (n != 4) return 0;
				}
				if (bufLen == 2) *(unsigned short *)buf = (unsigned short)rawVal;
				else if (bufLen == 4) *(unsigned *)buf = rawVal;
				else if (bufLen == 1) *(unsigned char *)buf = (unsigned char)rawVal;
				else return 0;
			}

			if (d->notifyId != -1)
				CallApiNotifySlot(d->notifyId, savedNotify);
		}

		if (mData.mLoopContinue != 0) {
			if (mNotifyLatch != 0) {
				mData.mLoopContinue = 0;
				mNotifyLatch = 0;
				return 1;
			}
			return 1;
		}
		/* mLoopContinue is never cleared by any path above -- faithful
		 * transcription of a real `do { } while(true)` whose own continue
		 * condition is never actually exercised by any reconstructed caller,
		 * same "dead-in-practice defensive loop" shape already documented
		 * for CModule::AdjustTaskMask()/CTaskBuffer's own always-empty walk
		 * (see edit_server.h). Kept for structural fidelity. */
	}
}

unsigned int CEditServer::Set(unsigned char group, unsigned char index, unsigned char subIndex,
                               const void *buf, unsigned int bufLen, EEditSource source)
{
	SDescriptor *d = 0;
	CObjectBase *owner = 0;
	int found = mData.FindDescriptor(group, index, subIndex, &d, &owner);
	if (!found)
		return 1; /* real: not-found is a SUCCESS return for Set(), unlike Get() */

	if (d->flags & kFlagDisabled)
		return 0;

	if (d->flags & kFlagCommand) {
		int saved = 1;
		if (d->notifyId != -1)
			saved = CallApiNotifySlot(d->notifyId, 0);

		void *base = (unsigned char *)owner + d->setterExtra;
		void *fn = ResolveDescriptorCallback(d->setterFn, base);
		typedef int (*Fn)(void *, unsigned, int);
		int r = ((Fn)fn)(base, (unsigned char)subIndex, source);

		if (d->notifyId != -1)
			CallApiNotifySlot(d->notifyId, saved);

		return (unsigned)(~(unsigned)r >> 31); /* real: `~uVar6 >> 0x1f` -- 1 if r<0 else 0 */
	}

	/* Assertion-only checks in the real binary (Api vtable slot +0x94) --
	 * transcribed as documented, non-fatal call-contract dispatch (matches
	 * this project's convention of not actually terminating on a reconstructed
	 * assert path; see system_api.h). Never actually triggered by any
	 * reconstructed caller (buf/bufLen always valid), so left as a
	 * best-effort call-through only.
	 */
	if (buf == 0 || bufLen == 0) {
		typedef void (*AssertFn)(void *, const char *, const char *, int);
		void *vtbl = *(void **)Api;
		AssertFn assertFn = *(AssertFn *)((char *)vtbl + 0x94);
		assertFn(Api, "Assertion failed in module %s, line %i.\n", "EditServer.cpp",
		         buf == 0 ? 0x164 : 0x165);
	}

	unsigned flags = d->flags;
	unsigned resultN;
	const void *srcBuf;
	unsigned scratch = 0;

	if (!(flags & kFlagBlockCopy)) {
		if (flags & kFlagVarLen) {
			if (bufLen != d->count)
				return 0;
			resultN = bufLen;
			srcBuf = buf;
		} else {
			unsigned width = (unsigned)d->elemSize;
			unsigned val;
			if (bufLen == 2) val = *(const unsigned short *)buf;
			else if (bufLen == 4) val = *(const unsigned *)buf;
			else if (bufLen == 1) val = (unsigned)*(const unsigned char *)buf;
			else return 0;

			if (!(flags & kFlagSigned)) {
				if ((unsigned)d->maxValue < val) return 0;
				if (val < (unsigned)d->minValue) return 0;
			} else {
				int sval;
				if (width == 2) sval = (short)val;
				else if (width == 1) sval = (signed char)val;
				else sval = (int)val;
				if (sval > d->maxValue) return 0;
				if (sval < d->minValue) return 0;
				val = (unsigned)sval;
			}

			if (width == 2) { scratch = (unsigned short)val; resultN = 2; }
			else if (width == 4) { scratch = val; resultN = 4; }
			else if (width == 1) { scratch = (unsigned char)val; resultN = 1; }
			else return 0;
			srcBuf = &scratch;
		}
	} else {
		/* Real: `if (count < bufLen) return 0;` -- block-copy allows
		 * bufLen <= count (unlike kFlagVarLen's exact-match requirement). */
		if (d->count < bufLen)
			return 0;
		resultN = bufLen;
		srcBuf = buf;
	}

	int saved = 1;
	if (d->notifyId != -1)
		saved = CallApiNotifySlot(d->notifyId, 0);
	flags = d->flags;

	bool doRawStore = true;
	int cbResult = 0;

	if (flags & kFlagCustomSetter) {
		void *base = (unsigned char *)owner + d->setterExtra;
		void *fn = ResolveDescriptorCallback(d->setterFn, base);
		typedef int (*Fn2)(void *, int, const void *, int);
		typedef int (*Fn3)(void *, int, int, const void *, int);

		if (!(flags & kFlagIndexed)) {
			if (!(flags & kFlagSetterUsesIdxOnly))
				cbResult = ((Fn2)fn)(base, (unsigned char)subIndex, srcBuf, source);
			else
				cbResult = ((Fn3)fn)(base, (int)(unsigned char)index, (unsigned char)subIndex, srcBuf, source);
		} else {
			int adj = (int)(signed char)subIndex - (int)(signed char)d->baseIndex;
			if (!(flags & kFlagSetterUsesIdxOnly))
				cbResult = ((Fn2)fn)(base, adj, srcBuf, source);
			else
				cbResult = ((Fn3)fn)(base, (int)(unsigned char)index, adj, srcBuf, source);
		}

		if (cbResult < 0) {
			if (d->notifyId != -1)
				CallApiNotifySlot(d->notifyId, saved);
			return 0;
		}
		doRawStore = (cbResult == 0);
		if (doRawStore)
			flags = d->flags; /* real: re-read before falling into the raw-store tail */
	}

	if (doRawStore) {
		int off = (flags & kFlagIndexed)
		        ? d->elemSize * (int)((int)subIndex - (int)d->baseIndex) + d->offset
		        : d->offset;
		unsigned char *dst = (unsigned char *)owner + off;
		if (!(flags & (kFlagBlockCopy | kFlagVarLen))) {
			if (resultN == 2) *(unsigned short *)dst = *(const unsigned short *)srcBuf;
			else if (resultN == 4) *(unsigned *)dst = *(const unsigned *)srcBuf;
			else if (resultN == 1) *dst = *(const unsigned char *)srcBuf;
			else return 0;
		} else {
			memcpy(dst, srcBuf, resultN);
		}
	}

	if (d->notifyId != -1)
		CallApiNotifySlot(d->notifyId, saved);

	if (!sm_bNotifyEnabled)
		return 0;

	int posted = CNotifyList_Put(g_oNotifyList, group, index, subIndex);
	return (unsigned)(posted != 0);
}

int CEditServer::PutNotify(unsigned char group, unsigned char subIndex)
{
	if (!sm_bNotifyEnabled)
		return 0;

	int posted = CNotifyList_Put(g_oNotifyList, group, subIndex, 0);
	if (posted == 0)
		return 0;

	if (mNotifyPending != 0) {
		mNotifyLatch = 1;
		return 1;
	}
	mData.mLoopContinue = 0; /* real: `*(undefined4*)(this+0x1c)=0` */
	mNotifyLatch = 0;
	return 1;
}
