/*
 * locale_manager.h  -  CLocaleManager, the global keyboard-layout registry
 * `CAlphaKeybCtrlTask::CAlphaKeybCtrlTask()`/`ProcessEvent()` both call into
 * (Eva CAlphaKeybCtrl/CAlphaKeybCtrlTask batch, 2026-07-26).
 *
 * GROUND TRUTH: `CLocaleManager` (13-byte ctor, 13-byte dtor, 13-byte
 * `AddKeyboardLayout()`, 13-byte `GetKeyboardLayout()`, 82-byte `GetInstance()`, all
 * `CLocaleManager@08079ab0.c`/`GetInstance@08079ad0.c`/`AddKeyboardLayout@08079b40.c`/
 * `GetKeyboardLayout@08079b50.c`) is, in ground truth, a THIN alias over a SEPARATE
 * real class `CKeyboardLayoutManager` -- every one of CLocaleManager's own methods is
 * a one-line tail call into the identically-shaped `CKeyboardLayoutManager` method of
 * the same name (`CLocaleManager::CLocaleManager()` literally IS
 * `CKeyboardLayoutManager::CKeyboardLayoutManager((CKeyboardLayoutManager*)this)`,
 * no added fields). `GetInstance()` (real body: lazy `operator new(0x10)` +
 * placement-construct a `CKeyboardLayoutManager`, cached in a static
 * `sm_poInstance`) IS reconstructed for real here (trivial, self-contained).
 *
 * `CKeyboardLayoutManager::AddLayout(CKeyboardLayout*)` (.text+0x08079ba0, 648 bytes)
 * and its 2 `GetLayout()` overloads (.text+0x08079e30, 290 bytes; .text+0x08079f60,
 * 606 bytes) are genuine algorithmic depth -- each manages its own internal
 * `TVector<CKeyboardLayout const*,1>` map with real insert/lookup logic -- correctly
 * OUT OF SCOPE for this batch (same "genuinely disproportionate sub-piece, deferred"
 * bar as `CAlphaKeybIfcTask::ProcessCode()`, alpha_keyb_ifc_task.h). `AddKeyboardLayout()`/
 * `GetKeyboardLayout()` below are declared with their real signatures but Tier-B
 * stub bodies (same "declare real signature, stub body" convention as
 * `CPoller::InitButtons()`/`InitAnalogs()`, poller.h) -- `AddKeyboardLayout()` is a
 * real no-op here (ground truth's own `AddLayout()` is never exercised by any KAT
 * this batch wrote); `GetKeyboardLayout()` always returns NULL, which drives
 * `CAlphaKeybCtrlTask::ProcessEvent()` down its own real, already-handled
 * "layout lookup failed" branch (`uVar5 = 1;` returned, no crash -- same
 * "structurally faithful, quiescent under this reconstruction's own stub"
 * status as `CPoller`'s own `LookupResourceStub`, poller.cpp).
 *
 * `CKeyboardLayoutManager` itself is deliberately NOT modeled as a separate C++
 * class here -- since neither of its own real, deep methods is reconstructed,
 * `CLocaleManager` stands in directly for the combined real object (same object
 * identity ground truth's own aliasing already implies), avoiding an empty
 * pass-through class with no purpose of its own.
 */

#ifndef LOCALE_MANAGER_H
#define LOCALE_MANAGER_H

class CKeyboardLayout;

class CLocaleManager {
public:
	/* .text+0x08079ad0, 82 bytes. Real: lazy singleton, `operator new(0x10)` +
	 * placement-construct on first call, cached thereafter.
	 */
	static CLocaleManager *GetInstance();

	/* .text+0x08079b40, 13 bytes. Tier-B stub -- real ground truth forwards to
	 * `CKeyboardLayoutManager::AddLayout()` (648 bytes, out of scope, see header
	 * comment). No-op here.
	 */
	void AddKeyboardLayout(const CKeyboardLayout *layout);

	/* .text+0x08079b50, 13 bytes. Tier-B stub -- real ground truth forwards to
	 * `CKeyboardLayoutManager::GetLayout(EKeyboardLayout)` (290 bytes, out of
	 * scope, see header comment). Always returns NULL here -- every real caller
	 * in this project (`CAlphaKeybCtrlTask::ProcessEvent()`) already has a real,
	 * well-defined "lookup failed" path for that.
	 */
	void *GetKeyboardLayout(unsigned int type);

private:
	/* Real body: `*(void***)this = &PTR__TVector_08e81c48; begin=end=cap=0;` --
	 * the embedded `TVector<CKeyboardLayout const*,1>` `AddLayout()`/`GetLayout()`
	 * would manage, zero-initialized. Not otherwise touched by this
	 * reconstruction's own stub methods above.
	 */
	CLocaleManager();

	static CLocaleManager *sm_poInstance;

	void  *mVtbl;
	void  *mBegin;
	void  *mEnd;
	void  *mCap;
};

#endif /* LOCALE_MANAGER_H */
