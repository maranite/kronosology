/*
 * global_object_base.h  -  CGlobalObjectBase, the tiny common base every "XxxApiInstance"
 * global-scope singleton in this binary (SysApiInstance, EditApiInstance, SeqApiInstance,
 * ChkApiInstance, DumpApiInstance, RMApiInstance, RTRouterApiInstance, g_oSysExApiInstance,
 * BDApiInstance -- 9 confirmed via their own `global.constructors.keyed.to.<Name>@<addr>.c`
 * decompile, Decomp/EVA_Decomp/eva_export/functions/) placement-constructs as its very
 * first step, before overwriting its own vtable slot with the derived class's real one --
 * same base-construct-then-vtable-swap idiom as CModule/CNamedObjectBase throughout this
 * project.
 *
 * THE REAL MECHANISM BEHIND Api/SysApiInstance (traced 2026-07-23, resolves the
 * MMainEditMan() NULL-Api crash found via a live kronos_vm boot test):
 *
 *   CGlobalObjectBase::CGlobalObjectBase()        .text+0x080632e0, 23 bytes
 *     -> installs PTR__CGlobalObjectBase_08e80f08, then calls CKernel::AddGlobalObject(this)
 *   CKernel::AddGlobalObject(CGlobalObjectBase*)  .text+0x0805da90, 123 bytes (ckernel.h/.cpp)
 *     -> lazily creates sm_poGlobalObjectList (a COmegaPtrArray growBy=1/cap=0/ownFlag=0,
 *        vtable-swapped to PTR__TPtrArray_08e80bc8) on first call, then COmegaPtrArray::Add()s
 *   ~CGlobalObjectBase()/CKernel::RemoveGlobalObject() -- symmetric teardown, not
 *     exercised on this pass's own traced boot path (nothing destructs one of these).
 *
 * This constructor runs automatically, before main(), for EVERY one of the 9
 * XxxApiInstance globals -- real C++ static/global initialization (.init_array),
 * confirmed by reading each one's own `global.constructors.keyed.to.<Name>` decompile.
 * SysApiInstance's own copy of this sequence (sysapi_instance.cpp) is the one that
 * actually sets `Api = SysApiInstance;` -- this is what makes Api non-null by the time
 * CKernel::InitSystemLayer() calls MMainEditMan(), fixing the crash this stage was about.
 * Implemented here as ordinary `__attribute__((constructor))` init functions (one per
 * XxxApiInstance global, at the same file scope as that global's own byte-buffer
 * definition -- sysapi_instance.cpp for SysApiInstance, mains.cpp for the other 7 this
 * project's traced boot path actually reaches) rather than real C++ static object
 * syntax, since every one of these globals is (per this project's own established
 * convention) a raw byte buffer manually vtable-swapped into shape, not a genuine C++
 * object of a reconstructed class -- __attribute__((constructor)) is the closest
 * faithful match: a real GCC .init_array entry that genuinely runs pre-main(), same
 * mechanism the original binary's own compiler-generated `_GLOBAL__I_*`/
 * `global.constructors.keyed.to.*` functions use.
 *
 * All 4 "phase hook" vtable slots (PreKernelConstructor/PostKernelConstructor/
 * PreKernelDestructor/PostKernelDestructor, vtable+8/+0xc/+0x10/+0x14) are confirmed, by
 * direct raw-byte read of the real Eva binary's own installed vtables -- this base
 * class's own (08e80f08) AND CSysApiInstance's/CEditApiInstance's/CSeqApiInstance's own
 * derived vtables at these same 4 byte offsets, spot-checked -- to be the unmodified
 * base no-ops below (`return 0`) in every XxxApiInstance class this project touches;
 * none override them. CKernel::CKernel()/~CKernel() (ckernel.cpp) already dispatch
 * through these 4 slots on every sm_poGlobalObjectList entry; that dispatch code was
 * always real and correct, it just never had anything to iterate before (the list was
 * permanently empty, since nothing called AddGlobalObject). It's live data now.
 *
 * BDApiInstance (the 9th XxxApiInstance-style global, `global.constructors.keyed.to.
 * BDApiInstance@08243a40.c`) is deliberately NOT wired here -- nothing on this
 * project's own traced boot path (MMainBatchDiskMan, mains.cpp) ever reads BDApi or
 * BDApiInstance; it's a real sibling of this same family, just out of scope for this
 * pass (same "stay bounded" license as OmegaExitThread being left unreconstructed).
 */

#ifndef GLOBAL_OBJECT_BASE_H
#define GLOBAL_OBJECT_BASE_H

class CGlobalObjectBase {
public:
	/* .text+0x080632e0, 23 bytes. */
	CGlobalObjectBase();

	/* .text+0x08063270, 23 bytes -- the "complete object" (D1) destructor. This
	 * project never deletes a CGlobalObjectBase* through a base pointer, so the
	 * "deleting" (D0) variant (.text+0x08063290, 47 bytes -- identical to D1 plus a
	 * trailing free(this)) isn't given its own separate C++ method here; it's still
	 * installed into the real vtable slot (see .cpp) for shape-fidelity, in case
	 * something not yet traced ever dispatches through it.
	 */
	~CGlobalObjectBase();

protected:
	void *mVtbl;
};

#endif /* GLOBAL_OBJECT_BASE_H */
