/*
 * keyboard_layout.h  -  CKeyboardLayout, the plain-data (non-polymorphic) per-locale
 * keymap record `CAlphaKeybCtrlTask::CAlphaKeybCtrlTask()` builds up to 15 of, once per
 * boot, from a fixed built-in table (Eva CAlphaKeybCtrl/CAlphaKeybCtrlTask batch,
 * 2026-07-26 -- see alpha_keyb_ctrl_task.h for the full derivation of that ctor).
 *
 * GROUND TRUTH: there is no separate `CKeyboardLayout::CKeyboardLayout()` symbol
 * anywhere in `functions.csv` -- the real construction logic (malloc(0x484), set the
 * type word, `memcpy` the 1024-byte keymap table, `SetLayoutName()` the display name
 * with an `eMaxNameChars`-guarded `strcpy` -- confirmed via the real `__assert_fail`
 * call's own literal string, "kiLen < eMaxNameChars", ".../KeyboardLayout.h", line
 * 0x1a, function "void CKeyboardLayout::SetLayoutName(const char*)" -- and set the
 * trailing flag byte) is fully INLINED, 14+1 times, directly into
 * `CAlphaKeybCtrlTask::CAlphaKeybCtrlTask()`'s own 4289-byte body. This header
 * un-inlines it back into a real, named 4-argument constructor -- the natural
 * un-inlining, not a guess: every one of the 15 call sites performs the IDENTICAL
 * sequence of operations differing only in the 4 argument values, and ground truth's
 * own `SetLayoutName()` function name (recovered from the assert string) confirms the
 * name-copy step really is understood as its own named operation by the original
 * source, just compiled away at `-O2`+`-finline-functions`.
 *
 * REAL LAYOUT (0x484 bytes, confirmed via every construction site's own
 * `malloc(0x484)` call and internal field offsets -- +0x402/+0x482 both used
 * directly as byte offsets from the object's own start, confirming natural (not
 * `CKeyboardLayout*`-scaled) pointer arithmetic despite Ghidra's own misleading
 * `CKeyboardLayout *` variable typing in the decompile):
 *   +0x000       mType   unsigned short -- a Windows-LANGID-shaped locale/keyboard
 *                        id (e.g. 0x0409 = US English, 0x0410 = Italian; Default's
 *                        own 0x8409 = 0x8000 | 0x0409, matching
 *                        `CLocaleManager::GetKeyboardLayout(0x8409)`'s own literal
 *                        argument in `ProcessEvent()` -- i.e. ProcessEvent() always
 *                        looks up the Default layout specifically, a real, faithfully-
 *                        preserved ground-truth behavior, not a bug).
 *   +0x002       mTable  unsigned char[1024] -- raw scancode/keycode lookup table,
 *                        see keyboard_layout_tables.cpp's own header comment.
 *   +0x402       mName   char[128] -- display name, `strcpy`'d in (real
 *                        `eMaxNameChars` = 128, confirmed by the assert's own guard
 *                        `kiLen < eMaxNameChars` firing when `strlen(name) > 0x7f`).
 *   +0x482       mFlag   unsigned char -- real meaning not decoded (no consumer of
 *                        this field exists anywhere in this project's own
 *                        reconstructed call graph -- `CKeyboardLayoutManager::
 *                        AddLayout()`/`GetLayout()`, the only real ground-truth
 *                        readers, are themselves out of scope, see locale_manager.h).
 *                        Faithfully preserved regardless (real per-layout constant
 *                        values, byte-read from ground truth, not fabricated).
 * Natural struct alignment (mType is the widest-aligned member at 2 bytes) rounds
 * 0x483 up to 0x484 with zero explicit padding needed -- matches every real
 * `malloc(0x484)` call site exactly.
 */

#ifndef KEYBOARD_LAYOUT_H
#define KEYBOARD_LAYOUT_H

class CKeyboardLayout {
public:
	/* Un-inlined constructor -- see header comment. `table` must point at 1024
	 * bytes (memcpy'd verbatim into mTable). `name` is `strcpy`'d via the same
	 * `eMaxNameChars`-guarded path ground truth's own (inlined)
	 * `SetLayoutName()` uses, including the real `__assert_fail()` call on
	 * overflow (never fires for any of this project's own 15 built-in names,
	 * all well under 128 chars).
	 */
	CKeyboardLayout(unsigned short type, const unsigned char *table, const char *name,
	                 unsigned char flag);

private:
	unsigned short mType;         /* +0x000 */
	unsigned char  mTable[1024];  /* +0x002 */
	char           mName[128];    /* +0x402 */
	unsigned char  mFlag;         /* +0x482 */

	CKeyboardLayout(const CKeyboardLayout &);
	CKeyboardLayout &operator=(const CKeyboardLayout &);
};

/* One row of the built-in-layout table (keyboard_layout_tables.cpp). */
struct SKeyboardLayoutDesc {
	unsigned short      type;
	const unsigned char *table;
	const char          *name;
	unsigned char        flag;
};

extern const SKeyboardLayoutDesc kKeyboardLayoutDescs[14];
extern const SKeyboardLayoutDesc kCustomAsciiLayoutDesc;

#endif /* KEYBOARD_LAYOUT_H */
