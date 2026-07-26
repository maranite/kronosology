/*
 * led_blinker.h  -  CLEDBlinker, Eva Stage 6 batch (2026-07-26, CPoller final-
 * prerequisites follow-up). Reconstructed as the concrete unlock for CPoller's own
 * last 3 still-deferred small handlers (MsgSetLed/MsgSetLed16bits/MsgBackupLEDs,
 * see poller.h) -- flagged but not pursued by the 2026-07-26 broad nm-C sweep batch
 * ([[eva_cpoller_msg_handlers_and_swarm_recheck_2026-07-26]]) as "a whole new
 * external singleton class, bigger than fits a small-handler sweep." On direct
 * inspection it turned out to be genuinely tiny -- 6 methods, all under 100 bytes,
 * ctor/dtor/Register/Unregister(x2 overload)/Exec -- the smallest "whole new class"
 * unlock found in this project so far, nothing like the CBatchDiskMan/
 * CAlphaKeybCtrlTask scale the same sweep also flagged.
 *
 * GROUND TRUTH: `.text+0x089ee1d0..0x089ee360` (ctor 41B, dtor 1B [empty, no vtable
 * -- confirmed via `objdump -dr`: CLEDBlinker has NO virtual functions at all, the
 * dtor body really is a bare `ret`], `Register(ELedCode)` 93B, `Unregister(ELedCode)`
 * 77B, `Unregister(int wordIndex, unsigned short mask)` 60B, `Exec()` 62B). Confirmed
 * via `symbols.csv`/`strings.csv` to live in ground truth's own "LEDBlinker.cpp"
 * translation unit (`.strtab` embeds that literal filename plus the mangled
 * `_ZL13s_oLEDBlinker` -- the leading `L` marks INTERNAL (file-static) linkage) --
 * i.e. ground truth's `CPoller::MsgSetLed()`/`MsgSetLed16bits()`/`MsgBackupLEDs()`
 * (which directly touch this same static) must ALSO live in that same real
 * "LEDBlinker.cpp" TU, not in whatever file holds CPoller's other methods --
 * explains why those 3 handlers reach a `static` object across what looks like a
 * class boundary. This reconstruction doesn't mirror ground truth's file-splitting
 * (CPoller's methods all stay in poller.cpp/poller.h) -- `s_oLEDBlinker` here is a
 * plain externally-linked global instead of file-static, declared in this header so
 * poller.cpp can reach it directly; behaviorally identical, since nothing else in
 * the real binary ever references it (confirmed: `s_oLEDBlinker`'s address
 * `.bss+0x0af09920` appears only inside CLEDBlinker's OWN 6 methods and inside
 * CPoller's 3 LED handlers, per a `.rodata`/`.data` relocation-literal grep).
 *
 * SINGLETON CONSTRUCTION: a real C++ static-initializer call
 * (`.text+0x089f6f90ish`, `CLEDBlinker::CLEDBlinker((CLEDBlinker*)0xaf09920)`) builds
 * `s_oLEDBlinker` in place at program startup, standard global-object-with-
 * nontrivial-ctor idiom, same as every other `s_oXxx`/`sm_poXxx` singleton this
 * project already models as a plain global with a call from an init-time
 * constructor list this reconstruction doesn't reproduce (mains.cpp's own
 * established convention) -- modeled here identically: a plain global `CLEDBlinker
 * s_oLEDBlinker;` (led_blinker.cpp), C++'s own static-init rules run its ctor before
 * `main()`, matching the real behavior with no extra plumbing needed.
 *
 * REAL LAYOUT (no vtable -- confirmed no virtual functions):
 *   +0x00  mCount       int  -- number of LEDs currently registered as "blinking".
 *                                Ctor zeroes it. Register()/Unregister() increment/
 *                                decrement it (Unregister() only if != 0, defensive).
 *   +0x04  mBlinkPhase  int  -- 0/1 toggle Exec() flips every 0x14 (20) calls; reset
 *                                to a computed value (see Exec()) whenever mCount
 *                                transitions from 0 -> nonzero via Register().
 *   +0x08  mDivider     int  -- 0..0x14 tick divider; Exec() increments it each call
 *                                and only flips mBlinkPhase when it wraps past 0x14.
 *   +0x0c  mBitmap[0x20] unsigned short[32] (64 bytes) -- ctor zeroes 16 dwords here
 *                                (== 32 ushorts). One bit per `ELedCode` value: bit
 *                                `ledCode % 16` of word `ledCode / 16` == "this LED is
 *                                currently registered as blinking." A SEPARATE 512-bit
 *                                bitmap from `CPoller::mZeroBlock` (poller.h's own
 *                                `+0x3a0`) -- that one tracks each `CPoller` instance's
 *                                own per-LED ON/OFF display state; this one tracks
 *                                the GLOBAL "is this LED actively blinking" set. Both
 *                                bitmaps are updated together by `CPoller::MsgSetLed()`/
 *                                `MsgSetLed16bits()` (poller.cpp) but serve different
 *                                real purposes -- confirmed by direct disassembly of
 *                                both call sites, not assumed.
 *
 * `Register(ELedCode)`/`Unregister(ELedCode)`: ground truth computes the (wordIndex,
 * bit) pair via a decompiler idiom (`iVar2 = ledCode + 0xf; if (ledCode >= 0) iVar2 =
 * ledCode; wordIndex = iVar2 >> 4;`) that is EXACTLY C's own truncating `ledCode / 16`
 * for every int value (the "+15 before arithmetic-shift when negative" trick converts
 * floor-shift into trunc-division) -- modeled directly as `ledCode / 16` / `ledCode %
 * 16`, mathematically identical, not a simplification. `Register()` only sets the bit
 * and increments `mCount` if the bit was previously CLEAR (no-op if the LED is
 * already registered); additionally, if `mCount` was exactly 0 before this call (the
 * "was completely idle" transition), it also resets `mBlinkPhase`/`mDivider` to 0 --
 * transcribed exactly, a real "restart the blink cycle from a clean phase whenever
 * blinking starts back up from zero" behavior. `Unregister(ELedCode)` mirrors this
 * for clearing, decrementing `mCount` only if the bit was previously SET (and only if
 * `mCount != 0`, a defensive floor that's a real, dead-in-practice guard -- `mCount`
 * can't go negative given `Register()`'s own gating, but ground truth checks it
 * anyway; transcribed as found).
 *
 * `Unregister(int wordIndex, unsigned short mask)`: a SEPARATE real overload that
 * takes an already-resolved absolute word index directly (not an `ELedCode` to
 * divide/mod) and a MULTI-bit mask -- clears every bit in `mBitmap[wordIndex]` that
 * is ALSO set in `mask`, decrementing `mCount` once per bit actually cleared (a
 * `while` loop over the intersection popcounting itself down, not a single
 * decrement) -- this is `CPoller::MsgSetLed16bits()`'s own real callee (poller.cpp),
 * used to bulk-unregister exactly the LEDs whose 16-bit group just changed away from
 * "blink" state in one message.
 *
 * `Exec()` (returns `int`, called every scheduler tick per its own real ground-truth
 * caller -- not yet identified/reconstructed, likely `CPoller::Exec()`'s own 0-arg
 * 3213-byte override per poller.h's still-deferred list; out of scope here): if
 * `mCount == 0`, returns 0 immediately (nothing registered, no blinking to drive). If
 * `mDivider != 0`, increments it (capped/wrapped to 0 once it would reach 0x15,
 * i.e. a 21-tick divider: 1..20 then back to 0) and returns 0 -- the "still counting
 * down" case. Only when `mDivider == 0` does it actually flip: sets `mDivider = 1`,
 * flips `mBlinkPhase` (`mBlinkPhase = (mBlinkPhase == 0)`, i.e. toggles 0<->1), and
 * returns 1 (signaling "phase changed this tick" to its own real, not-yet-identified
 * caller -- presumably the trigger for pushing the new phase out to every registered
 * LED, per-word, via `CPoller`'s own `mResource` notify slot the same way
 * `MsgSetLed()` does; not reconstructed, out of scope).
 */

#ifndef LED_BLINKER_H
#define LED_BLINKER_H

class CLEDBlinker {
public:
	/* .text+0x089ee1d0, 41 bytes. Tier A. Zeroes mCount/mBlinkPhase/mDivider and
	 * all 32 mBitmap words.
	 */
	CLEDBlinker();

	/* .text+0x089ee200, 1 byte (bare `ret` -- no vtable, no members needing
	 * teardown). Tier A.
	 */
	~CLEDBlinker();

	/* .text+0x089ee210, 93 bytes. Tier A -- see header comment. */
	void Register(int ledCode);

	/* .text+0x089ee270, 77 bytes. Tier A -- see header comment. */
	void Unregister(int ledCode);

	/* .text+0x089ee2c0, 60 bytes. Tier A -- see header comment. Separate real
	 * overload, NOT the same as Unregister(int) above (different parameter
	 * semantics: absolute word index + multi-bit mask, not an ELedCode to
	 * divide/mod).
	 */
	void Unregister(int wordIndex, unsigned short mask);

	/* .text+0x089ee300, 62 bytes. Tier A -- see header comment. */
	int Exec();

private:
	int            mCount;        /* +0x00 */
	int            mBlinkPhase;   /* +0x04 */
	int            mDivider;      /* +0x08 */
	unsigned short mBitmap[0x20]; /* +0x0c, 64 bytes -- 512-bit "currently blinking" set */

	friend struct LEDBlinkerTestHooks;
};

/* Real ground-truth global (`.bss+0x0af09920`, file-static in ground truth's own
 * "LEDBlinker.cpp" TU -- see header comment for why it's a plain extern-linkage
 * global here instead). Constructed by ordinary C++ static-init, no explicit call
 * needed in this reconstruction's own init path.
 */
extern CLEDBlinker s_oLEDBlinker;

#endif /* LED_BLINKER_H */
