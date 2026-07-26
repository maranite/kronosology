/*
 * edit_server.h  -  CDataHandler + CEditServer, the generic scope-keyed
 * parameter-descriptor engine underlying every one of the 10 CModule+
 * CEditServer "edit server" classes registered by mains.cpp's
 * MMainESCommon/MMainESProg/MMainESEffect/MMainESCombi/MMainESGlobal/
 * MMainESMOSS/MMainESSampling/MMainESSetList/MMainESSong/MMainESDisk (Stage 6
 * breadth sweep, 2026-07-25).
 *
 * Survey finding (this batch): the task's own working hypothesis was that
 * ESCommon/ESGlobal might be more "core/global-state" than the other 8
 * per-editor-page ES classes and therefore more boot-path-relevant. Verified
 * FALSE by direct nm -C inspection of the real binary -- all 10 share the
 * exact same shape:
 *
 *   MMainXxx(CSystemApi*)      registers an opaque "CXxxModuleConstructor"
 *                              descriptor via mains.cpp's RegisterModuleDescriptor()
 *                              (already Tier A) -- never itself constructs anything.
 *   CXxxModuleConstructor::Create(char const*, char const*, int)
 *                              malloc's the real CXxx object (0x40064-0x40068
 *                              bytes) and calls its ctor. Only actually invoked
 *                              from CConfigManager::CreateUserModules()/
 *                              CreateFMDrivers() (config_manager.cpp's own "Not
 *                              pursued" factory-array walk, Stage 6 batch
 *                              2026-07-25b) -- i.e. NONE of the 10 CXxx objects
 *                              are constructed on this reconstruction's own
 *                              currently-wired boot path yet.
 *   CXxx : CModule, CEditServer    ctor/dtor/Setup/Start/Config only (this
 *                              file + es_common.h) -- identical shape across
 *                              all 10, confirmed via nm -C (each has exactly
 *                              10 nm entries: ctor x2, dtor x2 + thunk x2,
 *                              Setup/Start/Config).
 *   CXxx::Setup()              mallocs a per-page "CXxxTask" (CTask-derived)
 *                              and calls CModule::Add() on it -- the ONE part
 *                              of each of the 10 that differs.
 *   CXxxTask                   the real "CSK model layer"/editor-page logic:
 *                              52 (CESEffectTask) to 1092 (CESSongTask) real
 *                              methods each (nm -C counts, all 10 checked:
 *                              CESCommonTask ~180, CESGlobalTask 324,
 *                              CESProgTask 501, CESEffectTask 52,
 *                              CESCombiTask 623, CESMOSSTask 65,
 *                              CESSamplingTask 348, CESSetListTask 213,
 *                              CESSongTask 1092, CESDiskTask 203). Same
 *                              PLAN.md "CForm/CSK-scale, indefinitely
 *                              deferred" boundary as the Peg toolkit -- NOT
 *                              pursued by this batch, same as CFileMan/CResMan
 *                              in mains.cpp.
 *
 * So: the CModule+CEditServer *shell* (this file, es_common.h) is real,
 * shared, tractable infrastructure -- reconstructed here, using CESCommon as
 * the representative instance (the shape is identical for all 10, so building
 * one is not "1 of 10 done", it is "the shared shell done"; see es_common.h).
 * The CXxxTask god-objects behind each Setup() are genuinely out of scope,
 * confirmed by nm -C method-count survey of all 10, not assumed from the name.
 *
 * CDataHandler is the actual generic engine: a direct-indexed
 * (group,subIndex) -> SDescriptor* hash table (0x10000 slots, 0x40000 bytes)
 * plus the inherited COmegaPtrArray's own linear list of "descriptor set"
 * registrations (used only by the group==0xff/index==0xff wildcard-skip case
 * in FindDescriptor(uchar,uchar,uchar,...) and by the second FindDescriptor
 * overload, a reverse lookup from a live SDescriptor* back to its enclosing
 * registration). Real layout confirmed from CDataHandler@0806d390.c/
 * @0806d8d0.c/@0806e050.c: CDataHandler's own "this" pointer IS CEditServer's
 * embedded-subobject pointer (CEditServer+4) -- i.e. CDataHandler is not
 * separately allocated, it's a byte-for-byte reinterpretation of {an embedded
 * COmegaPtrArray (0x18 bytes) + 2 more fields (a dword, a byte) + the
 * 0x40000-byte hash table CEditServer's own ctor memsets immediately after}.
 * Modeled here as CDataHandler : public COmegaPtrArray with those 3 extra
 * pieces as its own trailing members, matching the real memory layout
 * exactly (not a separate allocation).
 *
 * SDescriptor (0x38 = 56 bytes) real field offsets, derived directly from
 * every read site in Get@0806fb40.c/Set@080700b0.c/FindDescriptor@0806d8d0.c/
 * AddDescriptors@0806d390.c/SetDefault@0806fa90.c (no independent struct
 * definition was available in the Ghidra type export -- types.csv lists
 * "SDescriptor" only as an opaque StructureDB entry, no member list).
 * Flag-bit NAMES below are this pass's own inference from how each bit gates
 * Get()/Set()'s control flow -- not independently confirmed against any
 * symbol/string naming them, same "meaning not officially decoded,
 * offsets/behavior faithful" caveat already used throughout this project for
 * undecoded bitfields (e.g. module.h's mScopeId, mains.cpp's DAT_0930b174).
 */

#ifndef EDIT_SERVER_H
#define EDIT_SERVER_H

#include "omega_ptr_array.h"

class CObjectBase; /* opaque -- real class not reconstructed, used only as a
                     * generic owner-object pointer (matches CApiBase's
                     * treatment in sysapi_instance.h). */

/* EEditSource -- real enum not reconstructed (only ever passed through
 * opaquely to a descriptor's custom setter callback); kept as a plain int,
 * same convention this project uses for every other undecoded enum/id arg.
 */
typedef int EEditSource;

struct SDescriptor {
	int            offset;        /* +0x00 direct member offset, or base offset
	                                * for indexed (kFlagIndexed) fields */
	void          *getterFn;      /* +0x04 custom getter callback (kFlagCustomGetter).
	                                * GCC member-function-pointer encoding: if the low
	                                * bit is set, treat as a vtable-relative thunk
	                                * (`*(code**)(getterFn + *(int*)owner - 1)`),
	                                * else a plain function pointer -- same PMF
	                                * decode idiom already used in
	                                * stg_unsol_msg_handler.cpp's HandleMessage(). */
	int            getterExtra;   /* +0x08 addend applied before the getter call */
	void          *setterFn;      /* +0x0c custom setter callback (kFlagCustomSetter
	                                * or kFlagCommand) -- same PMF encoding as getterFn */
	int            setterExtra;   /* +0x10 addend applied before the setter call */
	int            elemSize;      /* +0x14 scalar width in bytes (1/2/4), also the
	                                * per-element stride for kFlagIndexed fields */
	int            unused18;      /* +0x18 never read by Get/Set/FindDescriptor */
	unsigned int   count;         /* +0x1c array length (kFlagIndexed) or exact
	                                * required byte length (kFlagVarLen) */
	int            minValue;      /* +0x20 inclusive range-check minimum (scalar
	                                * fields only) */
	int            maxValue;      /* +0x24 inclusive range-check maximum */
	int            defaultValue;  /* +0x28 SetDefault()'s own 4-byte default value */
	int            notifyId;      /* +0x2c passed to Api's vtable slot +0x80
	                                * (begin/end notify-suspend pair) around every
	                                * Get/Set of this descriptor; -1 = no hook */
	unsigned int   flags;         /* +0x30 see kFlagXxx below (byte 1 = +0x31,
	                                * checked separately by SetDefault() as
	                                * kFlagHasDefault) */
	unsigned char  group;         /* +0x34 outer index (param_2) */
	unsigned char  baseIndex;     /* +0x35 subtracted from the inner index
	                                * (param_3) before any indexed-array or
	                                * callback dispatch */
};

enum {
	kFlagSigned        = 0x001, /* Get(): sign-extend the narrow scalar before
	                              * widening to the caller's requested width */
	kFlagBlockCopy     = 0x004, /* raw memcpy of `count`-ish bytes rather than a
	                              * single scalar load/store (paired with
	                              * kFlagIndexed for a per-element block copy) */
	kFlagIndexed       = 0x008, /* array field: real offset/callback-owner is
	                              * `base + elemSize * (index - baseIndex)` */
	kFlagCustomGetter  = 0x010, /* Get() dispatches through getterFn instead of
	                              * a raw memory load */
	kFlagCustomSetter  = 0x020, /* Set() dispatches through setterFn instead of
	                              * a raw memory store */
	kFlagCommand       = 0x040, /* "trigger" descriptor: Set() always dispatches
	                              * through setterFn (ignoring kFlagCustomSetter);
	                              * combined with kFlagDisabled below (0xc0 mask)
	                              * it makes Get() unconditionally fail */
	kFlagDisabled      = 0x080, /* both Get() and Set() return 0 (fail)
	                              * unconditionally */
	kFlagHasDefault    = 0x100, /* SetDefault(): descriptor has a real default
	                              * value (defaultValue) worth restoring */
	kFlagTwoArgCb      = 0x200, /* custom getter/setter callback takes
	                              * (owner, index, buf[, source]) instead of
	                              * (owner, group, index, buf[, source]) */
	kFlagSetterUsesIdxOnly = 0x400, /* Set()'s own analogue of kFlagTwoArgCb for
	                              * its OWN callback-arg-count branch (kept
	                              * separate: Get and Set gate this independently
	                              * in the real binary) */
	kFlagVarLen        = 0x800  /* variable-length exact-size buffer: caller's
	                              * requested length must equal `count` exactly,
	                              * copied straight to/from the caller's buffer
	                              * with no scalar interpretation */
};

/* CDataHandler -- see file header. Real methods: AddDescriptors, and 2
 * FindDescriptor overloads. No ctor/dtor of its own (never separately
 * constructed -- its "this" is always an existing subobject belonging to
 * some enclosing CEditServer, itself constructed via CEditServer's own ctor
 * calling the inherited COmegaPtrArray's default ctor and then hand-writing
 * the 2 extra trailing scalar fields -- see CEditServer::CEditServer()).
 *
 * Layout (relative to CDataHandler's own "this"), confirmed against every
 * read site in the real disassembly:
 *   +0x00..0x18  inherited COmegaPtrArray (vtbl/mUnknown04/mCapacity/mCount/
 *                mGrowBy/mArray) -- the linear "descriptor set" registration
 *                list AddDescriptors()/FindDescriptor() fall back to
 *   +0x18        mLoopContinue (int) -- CEditServer::Get()'s own per-call
 *                loop-continue latch (Get() itself pokes this by raw offset
 *                relative to ITS OWN "this", which is CDataHandler's this
 *                minus 4 -- see CEditServer's own +0x1c comment)
 *   +0x1c        mScope (byte) -- this server's currently-assigned scope id,
 *                checked by FindDescriptor()'s own exact-hit fast path
 *   +0x20        mTable[0x10000] (SDescriptor*[]) -- the direct-indexed hash
 *                table, index = subIndex + 8 + group*256
 */
class CDataHandler : public COmegaPtrArray {
public:
	/* .text+0x0806d390, 1245 bytes. Registers `count` SDescriptor entries
	 * starting at `descriptors`, owned by `owner`, into the direct-indexed
	 * hash table (each entry's own group/baseIndex/count fields determine
	 * which slots it occupies -- an indexed descriptor with kFlagIndexed and
	 * count>1 claims `count` consecutive slots, one per array element). When
	 * `count<=0` (or after the per-entry table population loop finishes) the
	 * (owner,descriptors,count) triple is appended as one new linear-list
	 * registration UNLESS `alreadyRegistered` is true, in which case an
	 * existing registration matching the same (descriptors,count) pair is
	 * found instead (reverse, most-recently-added-first scan) and just has
	 * its owner pointer updated. Real disassembly is an 8-way-unrolled
	 * reverse scan; collapsed to a plain reverse loop here, same license
	 * used throughout this project for Duff's-device-shaped disassembly.
	 *
	 * CONFIRMED BY KAT: the de-dup path's own owner update does NOT affect
	 * what a direct hash-table hit later observes as the owner -- only the
	 * "new registration" path writes the CDataHandler-relative +0x40020
	 * "last owner" field a hash hit actually returns (see FindDescriptor()'s
	 * own comment below); the de-dup path only updates the linear-list
	 * registration record's own owner field, which the FALLBACK scan reads.
	 * A real, if surprising, ground-truth quirk -- not an artifact of this
	 * reconstruction's own collapsing of the unrolled scan.
	 */
	void AddDescriptors(CObjectBase *owner, SDescriptor *descriptors, int count,
	                     bool alreadyRegistered);

	/* .text+0x0806d8d0, 1879 bytes. Primary 3-key lookup: exact (group,index)
	 * hash-table hit first; on miss, and only when NOT both group==0xff AND
	 * index==0xff (the "reserved/wildcard" pair -- real behavior for that
	 * exact pair is to skip the fallback scan entirely and fail), falls back
	 * to a reverse linear scan of every registered descriptor set looking for
	 * either an exact (group,index) match or an in-range kFlagIndexed match
	 * (index within [baseIndex, baseIndex+count)). Returns the matching
	 * SDescriptor* and its registration's owner object via the two
	 * out-parameters; 0 (not found) leaves both NULL. Real disassembly is an
	 * 8-way-unrolled reverse scan (both over the registration list AND, on an
	 * indexed miss, effectively a linear probe within one registration's own
	 * descriptor array) -- collapsed to plain loops here, same license as
	 * AddDescriptors.
	 */
	int FindDescriptor(unsigned char group, unsigned char index, unsigned char subIndex,
	                    SDescriptor **outDescriptor, CObjectBase **outOwner) const;

	/* .text+0x0806e050, 564 bytes. Reverse lookup: given an `owner` object and
	 * a raw getter/setter-callback-slot address (`callbackSlot` -- always
	 * &SDescriptor::getterFn of some already-known descriptor in the real
	 * caller), scans that owner's own registered descriptor array (found by
	 * matching `owner` against each registration's own owner pointer) for the
	 * one whose own getterFn field equals `callbackSlot`, returning it via
	 * the out-parameter. Never exercised by any reconstructed caller yet
	 * (Get()/Set()/FindDescriptor(3-key) above are the only 3 CDataHandler
	 * callers reconstructed so far) -- included for completeness since it's
	 * one of only 3 real CDataHandler methods total.
	 */
	int FindDescriptor(CObjectBase *owner, void *callbackSlot, SDescriptor **outDescriptor) const;

	int            mLoopContinue; /* +0x18, see class comment */
	unsigned char  mScope;        /* +0x1c */
	SDescriptor   *mTable[0x10000]; /* +0x20, 0x40000 bytes */
};

/* CEditServer -- see file header. Real base every one of the 10 CModule+
 * CEditServer "edit server" classes multiply-inherits (always at a fixed
 * +0x2c offset from the derived class's own `this`, right after CModule's
 * own 0x2c-byte base -- confirmed from CESCommon::CESCommon@08bd1e00.c and
 * identical across the other 9 per this file's own nm -C survey).
 *
 * Real object layout, 0x40038 bytes total (matches CESCommon's own
 * 0x40064-byte malloc minus CModule's 0x2c-byte base exactly):
 *   +0x00        vtbl (CEditServer's own real 7-slot vtable,
 *                 PTR__CEditServer_08e817b0 below -- never dispatched
 *                 through by any reconstructed code, same EvaVTableStub
 *                 placeholder treatment as every other undecoded vtable in
 *                 this project)
 *   +0x04..0x40024  mData (CDataHandler, 0x40020 bytes -- see its own layout
 *                 comment above)
 *   +0x40024     mUnknown40024 (int) -- ctor zeroes; not read back by any of
 *                 Get()/Set()/PutNotify()/SetDefault()/FindDescriptor()
 *   +0x40028     mNotifyPending (int) -- Get()'s custom-getter "in progress"
 *                 latch and PutNotify()'s own toggle (see PutNotify())
 *   +0x4002c     mNotifyLatch (int) -- PutNotify()'s companion toggle
 *   +0x40030     mAssignedScope (byte) -- this server's real assigned scope
 *                 id (mirrors mData.mScope, written identically by the ctor);
 *                 checked by CEditServer::FindDescriptor()'s own outer gate
 *   +0x40034     mName (char*) -- malloc'd copy of the ctor's name argument
 *
 * NOTE: the ctor writes mData.mScope and mAssignedScope with the same value
 * in that literal order (CEditServer@08070970.c) -- transcribed as found,
 * not "cleaned up" into a single write, since this project's own convention
 * (mains.cpp's Mains(), elsewhere) is to preserve real redundant writes
 * rather than optimize them away.
 */
class CEditServer {
public:
	CEditServer(const char *name);
	~CEditServer();

	/* .text+0x08070a80, 122 bytes. Real: `const` in the ground truth binary.
	 * Only succeeds (dispatches to CDataHandler::FindDescriptor) when `group`
	 * matches this server's own assigned scope; otherwise fails immediately
	 * without even calling CDataHandler.
	 */
	int FindDescriptor(unsigned char group, unsigned char index, unsigned char subIndex) const;

	/* .text+0x0806fa90, 162 bytes. Restores a single descriptor's default
	 * value: looks the descriptor up, and if found AND kFlagHasDefault is
	 * set, dispatches through THIS object's own vtable slot +0xc (a real,
	 * per-derived-class virtual override -- never reconstructed, opaque
	 * EvaVTableStub dispatch) with the descriptor's own defaultValue bytes
	 * as the new value.
	 */
	int SetDefault(unsigned char group, unsigned char index, unsigned char subIndex);

	/* .text+0x0806fb40, 1302 bytes. Full flag-driven dispatch (scalar/
	 * indexed/block-copy/custom-getter paths) -- see edit_server.cpp. The
	 * outer `do { } while(true)` from the real disassembly is preserved
	 * literally (goto-based) even though mLoopContinue is never actually
	 * cleared inside the loop body by any reconstructed path, making the
	 * loop always execute exactly once in practice -- same "faithful but
	 * dead-in-practice defensive shape" already documented for
	 * CModule::AdjustTaskMask()/CTaskBuffer's own always-empty walk.
	 *
	 * CONFIRMED BY KAT (verify/test_edit_server.cpp), corrects this batch's
	 * own initial assumption: the return value is NOT a "found" boolean.
	 * When CDataHandler::FindDescriptor() misses, the entire dispatch body is
	 * skipped and execution falls straight to the loop-bottom
	 * `if (mLoopContinue != 0) { ...; return 1; }` tail -- mLoopContinue is
	 * unconditionally set to 1 at the top of every iteration, so a miss
	 * (including a wrong-scope call, which fails the same
	 * CDataHandler::FindDescriptor() check) returns 1, same as success. Get()
	 * only returns 0 for specific validation failures encountered AFTER a
	 * hit (kFlagDisabled/kFlagCommand, or a width mismatch) -- callers must
	 * not treat a nonzero return as "found".
	 */
	int Get(unsigned char group, unsigned char index, unsigned char subIndex,
	        void *buf, unsigned int bufLen);

	/* .text+0x080700b0, 1857 bytes. Set()'s own analogue of Get() above --
	 * same literal-transcription approach, same file.
	 *
	 * CONFIRMED BY KAT: the return value is NOT a success/failure boolean
	 * either. Not-found returns 1 (unlike Get(), this one IS a real
	 * "no descriptor, nothing to do" convention here). kFlagDisabled and
	 * every range-check/width-mismatch rejection return 0 -- the SAME value
	 * a genuinely successful set-with-no-notify-posted returns. The only way
	 * to observe a rejection is that the target memory was not written --
	 * callers must not treat a 0 return as failure. A kFlagCommand
	 * descriptor's return value has its own distinct convention: 1 if the
	 * setter callback returned >=0, 0 if it returned negative (`~r >> 0x1f`).
	 */
	unsigned int Set(unsigned char group, unsigned char index, unsigned char subIndex,
	                  const void *buf, unsigned int bufLen, EEditSource source);

	/* .text+0x08c1a680, 161 bytes. Real: only actually posts a notification
	 * (CNotifyList::Put(), notify_list.h -- promoted Tier B -> Tier A 2026-07-26)
	 * when the class-static sm_bNotifyEnabled is set; on a successful Put(), latches
	 * mNotifyLatch/clears mNotifyPending depending on which of the two is
	 * currently set (a real "was a notify already pending" toggle --
	 * transcribed exactly, not independently confirmed against any other
	 * subsystem).
	 */
	int PutNotify(unsigned char group, unsigned char subIndex);

	/* Trivial public accessors -- no matching .text address of their own; added
	 * for CEditMan::CMainTask (edit_man.h, Stage 6 breadth sweep, 2026-07-25),
	 * which the real binary reads via raw `param_1[0x40030]`/`*(char**)(x+0x40034)`
	 * offset pokes from a DIFFERENT class entirely (same "new external caller
	 * needs raw access this class's own methods never required" reasoning as
	 * omega_ptr_array.h's Count()/Get()).
	 */
	unsigned char GetAssignedScope() const { return mAssignedScope; }
	const char *GetName() const { return mName; }

	/* Raw redispatch through this object's own vtable slots +8/+0xc/+0x10 (Get/
	 * Set/SetDefault, by this class's own established slot convention -- see
	 * class header) -- CEditMan::CMainTask's own external Get/Set/SetDefault
	 * wrappers (edit_man.h) call these directly on a DIFFERENT, already-
	 * FindDescriptor()-confirmed server object, bypassing this class's own
	 * Get()/Set()/SetDefault() member-function logic entirely (confirmed real
	 * ground-truth behavior from CMainTask's own decompile, not a
	 * simplification). Every real derived CEditServer's own override at these
	 * slots is out of scope (EvaVTableStub placeholder, same as this class's
	 * whole vtable), so these calls are functionally inert here -- zero-arg
	 * cdecl-safe shape, same convention as every other undecoded vtable-slot
	 * dispatch in this project (module.h's own header comment).
	 */
	void InvokeGetSlot() const { ((void (*)())(((void **)mVtbl)[2]))(); }
	void InvokeSetSlot() const { ((void (*)())(((void **)mVtbl)[3]))(); }
	void InvokeSetDefaultSlot() const { ((void (*)())(((void **)mVtbl)[4]))(); }

private:
	void          *mVtbl;
	CDataHandler   mData;              /* +0x04 */
	int            mUnknown40024;      /* +0x40024 */
	int            mNotifyPending;     /* +0x40028 */
	int            mNotifyLatch;       /* +0x4002c */
	unsigned char  mAssignedScope;     /* +0x40030 */
	char          *mName;              /* +0x40034 */

	/* Class-static real global (CEditServer::sm_bNotifyEnabled, symbols.csv:
	 * 091ae800) -- shared by every CEditServer instance, not per-object. Its
	 * own static ctor (global.constructors.keyed.to.
	 * CEditServer::sm_bNotifyEnabled@08070b00.c) is not reconstructed (real
	 * initial value not decoded) -- defaults to false here, matching this
	 * project's "unfaithful placeholder that doesn't change control flow on
	 * the currently-unreachable path" license (PutNotify() is unreachable
	 * anyway until some CXxxTask calls it, and those are all out of scope
	 * per this file's header).
	 */
	static bool sm_bNotifyEnabled;

	/* Friend accessor for verify/test_edit_server.cpp -- same extraction
	 * pattern already used by module.h/comm_driver.h/level_manager_array.h
	 * (ModuleTestHooks etc.): lets the KAT register synthetic SDescriptor
	 * tables directly into mData and toggle sm_bNotifyEnabled without a real
	 * CEditApiInstance/CNotifyList to drive it through.
	 */
	friend struct EditServerTestHooks;

	/* CEditor (editor.h) embeds a CEditServer sub-object at +0x38 and, like
	 * ground truth's own ctor, overwrites its vtable pointer with CEditor's
	 * own this-adjusted secondary vtable right after construction -- needs
	 * direct write access to mVtbl for that, same reasoning as module.h's own
	 * `friend class CEditor;`.
	 */
	friend class CEditor;
};

/* PTR__CEditServer_08e817b0[7]/PTR__TPtrArray_08e817e8[3] -- declared in
 * omega_vtables.h (extern "C", matching this project's centralized-vtable
 * convention), not redeclared here.
 */

#endif /* EDIT_SERVER_H */
