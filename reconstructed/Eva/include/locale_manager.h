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
 * and its `GetLayout(EKeyboardLayout)` overload (.text+0x08079e30, 290 bytes) turned
 * out fully tractable on a fresh look (2026-07-26, CLocaleManager closeout batch) --
 * the "genuine algorithmic depth" label above was the same "large raw byte count"
 * misjudgment this project has now hit repeatedly (`CAlphaKeybCtrl`'s own 4289-byte
 * ctor, `CPoller::FindRegisteredClient()`'s two 2600-byte wrappers). Both are a plain
 * `TVector<CKeyboardLayout const*,1>` push_back-with-grow and linear scan
 * respectively -- see the per-method comments below and locale_manager.cpp for the
 * full derivation. `GetLayout(char const*)` (.text+0x08079f60, 606 bytes, a second
 * name-based lookup overload) stays OUT OF SCOPE: no real ground-truth caller reaches
 * it anywhere in this project's own reconstructed call graph (`CLocaleManager`'s own
 * public-facing `GetKeyboardLayout()` wraps ONLY the `EKeyboardLayout` overload,
 * confirmed via `nm -C` -- ground truth's `CLocaleManager` class genuinely never
 * exposes the string-based lookup at all), not chased further.
 *
 * `CKeyboardLayoutManager` itself is deliberately NOT modeled as a separate C++
 * class here -- `CLocaleManager` stands in directly for the combined real object
 * (same object identity ground truth's own aliasing already implies), avoiding an
 * empty pass-through class with no purpose of its own.
 *
 * REAL CONSEQUENCE, worth flagging: this closes the loop on
 * `CAlphaKeybCtrlTask::ProcessEvent()`'s own previously-"structurally faithful,
 * quiescent" `CLocaleManager::GetKeyboardLayout(0x8409)` call (alpha_keyb_ctrl_task.h)
 * -- since `AddKeyboardLayout()` is now real too, and `CAlphaKeybCtrlTask`'s own real
 * ctor genuinely calls it once per built-in layout (including the Default layout,
 * `kKeyboardLayoutDescs[0]`, whose real `mType == 0x8409`), `GetKeyboardLayout(0x8409)`
 * now genuinely SUCCEEDS once any `CAlphaKeybCtrlTask` has been constructed (the
 * layout list lives on this SINGLETON, shared process-wide, not per-task) --
 * `ProcessEvent()`'s own "layout lookup failed" fast return is no longer the only
 * reachable path under this reconstruction. See alpha_keyb_ctrl_task.h's own updated
 * comment and test_alpha_keyb_ctrl.cpp's updated check [8].
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

	/* .text+0x08079b40, 13 bytes. Tier A (2026-07-26 CLocaleManager closeout
	 * batch) -- one-line tail call into `CKeyboardLayoutManager::AddLayout()`
	 * (.text+0x08079ba0, 648 bytes), now reconstructed for real: a plain
	 * `TVector<CKeyboardLayout const*,1>` push_back-with-grow, INLINED at its
	 * own single real call site (no separate MakeCapacity symbol found in the
	 * disassembly, unlike `CPoller`'s own `CIfcClient*` instantiation). Ground
	 * truth's own body also carries a compiler-emitted self-aliasing-range
	 * guard (comparing the address of the STACK-passed `layout` argument
	 * against the heap-backed `[mBegin,mEnd)` range) -- confirmed dead for
	 * every real call site on this platform, same "a stack-local source range
	 * can never alias a heap-allocated backing array" reasoning already
	 * established for `CPoller::RegisterClient()`'s own analogous guard
	 * (poller.h); omitted here, not modeled. `layout == NULL` is a real no-op
	 * (matches ground truth's own leading null check). Growth policy: min 32
	 * elements then doubling -- the SAME real constant as
	 * `TVector_CIfcClientPtr_MakeCapacity()` (poller.cpp), confirmed
	 * independently via THIS function's own disassembly, not assumed from
	 * that precedent.
	 */
	void AddKeyboardLayout(const CKeyboardLayout *layout);

	/* .text+0x08079b50, 13 bytes. Tier A (2026-07-26 CLocaleManager closeout
	 * batch) -- one-line tail call into
	 * `CKeyboardLayoutManager::GetLayout(EKeyboardLayout) const`
	 * (.text+0x08079e30, 290 bytes), now reconstructed for real: a plain
	 * linear scan over the same `TVector<CKeyboardLayout const*,1>`, comparing
	 * each element's own LEADING `unsigned short` field (`CKeyboardLayout`'s
	 * own `mType`, keyboard_layout.h) against `type`. `CKeyboardLayout` stays
	 * forward-declared here -- read via a raw leading-`unsigned short` offset,
	 * same opaque cross-boundary convention this project uses throughout
	 * (e.g. `CPoller`'s own client `+0x14` reads). Returns the first matching
	 * element, or NULL if the scan completes with no match (including the
	 * genuinely-empty-list case, same code path -- ground truth does not
	 * distinguish "empty" from "not found" here, unlike
	 * `CPoller::FindRegisteredClient()`'s own distinct -1/0 codes).
	 */
	void *GetKeyboardLayout(unsigned int type);

private:
	/* Real body: `*(void***)this = &PTR__TVector_08e81c48; begin=end=cap=0;` --
	 * the embedded `TVector<CKeyboardLayout const*,1>` `AddLayout()`/`GetLayout()`
	 * would manage, zero-initialized. Not otherwise touched by this
	 * reconstruction's own stub methods above.
	 */
	CLocaleManager();

public:
	/* round 63 (2026-07-29, solo): real dtor, found via a done>0/pending>0
	 * manifest scan. Ground truth's `~CKeyboardLayoutManager()` (.text+0x08079b80,
	 * 30 bytes) -- `CLocaleManager` stands in directly for it, see this class's
	 * own header comment -- resets the vtable slot to `PTR__TVector_08e81c48`
	 * (same symbol the ctor now installs) then `operator delete`s `mBegin`
	 * (the TVector's own backing array, freed unconditionally -- ground truth
	 * does not null-check it first, matching `operator delete(NULL)`'s own
	 * defined no-op behavior when the array was never grown).
	 */
	~CLocaleManager();

private:
	static CLocaleManager *sm_poInstance;

	void  *mVtbl;
	void  *mBegin;
	void  *mEnd;
	void  *mCap;
};

#endif /* LOCALE_MANAGER_H */
