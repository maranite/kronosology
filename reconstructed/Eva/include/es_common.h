/*
 * es_common.h  -  CESCommon, the representative instance of the 10-member
 * CModule+CEditServer "edit server" shell family (see edit_server.h's file
 * header for the full survey finding: ESGlobal/ESProg/ESEffect/ESCombi/
 * ESMOSS/ESSampling/ESSetList/ESSong/ESDisk all share this exact shape,
 * confirmed via nm -C -- 10 nm entries each: ctor x2, dtor x2 + thunk x2,
 * Setup/Start/Config).
 *
 * Real layout (0x40064 bytes total malloc'd by
 * CESCommonModuleConstructor::Create(), confirmed against
 * CESCommon@08bd1e00.c/Create@08bea580.c): CModule base (0x2c bytes) +
 * CEditServer base (0x40038 bytes) = 0x40064, multiply-inherited (not
 * virtually) at a fixed +0x2c offset -- matches this project's existing
 * `class CFileMan : public CModule` precedent (mains.cpp) for using real
 * C++ inheritance as a layout/composition tool (CModule/CEditServer declare
 * no real C++ virtual methods -- both use the project's own raw `mVtbl`
 * convention -- so multiple inheritance here doesn't fight that convention).
 *
 * CESCommonTask (Setup()'s own owned task, real .text+0x08bd2a20 ctor) is
 * genuinely out of scope: 180 real methods per nm -C (ExecuteCopyProg,
 * CopyToneAdjust, InitializeKarmaModule, ...) -- the actual "CSK model
 * layer" logic PLAN.md excludes, same as the other 9 ES-family Task
 * classes. Modeled here the same way mains.cpp already models
 * CFileMan/CResMan's own out-of-scope derived ctors: a minimal,
 * deliberately-simplified substitute that satisfies the real CTask base
 * construction contract Setup()'s own `CModule::Add()` call needs, not the
 * real 4289-byte ctor body.
 */

#ifndef ES_COMMON_H
#define ES_COMMON_H

#include "module.h"
#include "edit_server.h"
#include "task.h"

/* Tier-B substitute for the real, out-of-scope CESCommonTask (see file
 * header) -- reuses CTask's own already-reconstructed base ctor
 * (task.h) rather than reproducing any of the real 4289-byte
 * CESCommonTask::CESCommonTask() body (13 CKeyboardLayout-shaped keyboard-
 * table constructions are the AlphaKeybCtrlTask analogue this project
 * already surveyed-and-deferred for the same reason; CESCommonTask's own
 * real ctor is unrelated in content but equally out of this pass's scope).
 * `level`/`scheduleFlag`/`lastArg` are unfaithful placeholders (real values
 * not decoded) -- same "doesn't change this pass's own control flow" license
 * already used for CEditMan_SysName/CFileMan/CResMan in mains.cpp.
 */
class CESCommonTask : public CTask {
public:
	explicit CESCommonTask(const CModule &owner)
		: CTask(owner, "ESCommonTask", 0, 0, 0) {}
};

class CESCommon : public CModule, public CEditServer {
public:
	/* .text+0x08bd1e00, 73 bytes. Real ctor signature takes a 2nd argument
	 * (Create@08bea580.c's own param_3, an int) that CESCommon's own ctor
	 * body never references (confirmed from the decompile -- not a
	 * reconstruction gap, genuinely unused by this particular ES-family
	 * member's ctor). Kept for signature fidelity with
	 * CESCommonModuleConstructor::Create()'s own call site.
	 */
	CESCommon(const char *name, int unusedExtra);
	~CESCommon();

	/* .text+0x08bd1d80, 79 bytes. Mallocs+constructs the real, out-of-scope
	 * CESCommonTask (see class comment above) and registers it via
	 * CModule::Add() -- the SAME `CModule::Add(CTask*)` mechanism already
	 * confirmed boot-path-reachable via CEditor::Setup() (module.h) -- but
	 * unreachable on THIS reconstruction's own currently-wired path, since
	 * nothing yet calls CESCommon::Setup() (see edit_server.h: gated behind
	 * CConfigManager::CreateUserModules(), not pursued).
	 */
	int Setup();

	/* .text+0x08bd1bd0, 3 bytes. Confirmed genuinely empty (`return 0;`) in
	 * the real binary -- same "read every one, don't assume" treatment
	 * CSTGUnsolMsgHandler's own confirmed-empty slots got. */
	int Start();

	/* .text+0x08bd1bc0, 3 bytes. Confirmed genuinely empty. */
	int Config();
};

#endif /* ES_COMMON_H */
