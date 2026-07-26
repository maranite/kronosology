/*
 * keyboard_layout.cpp  -  see include/keyboard_layout.h.
 *
 * CKeyboardLayout::CKeyboardLayout() transcribed from the shared inlined-construction
 * sequence repeated 15 times in CAlphaKeybCtrlTask::CAlphaKeybCtrlTask()
 * (CAlphaKeybCtrlTask@0823f2a0.c) -- see keyboard_layout.h for the un-inlining
 * rationale. Real body per site: `mType = type; memcpy(mTable, table, 1024);
 * SetLayoutName(name); mFlag = flag;` where SetLayoutName is itself
 * `if (strlen(name) >= eMaxNameChars) __assert_fail(...); strcpy(mName, name);`
 * (ground truth's own real `KeyboardLayout.h` line 0x1a / eMaxNameChars = 128,
 * confirmed via the real assert string literal).
 */

#include "keyboard_layout.h"

#include <cassert>
#include <cstring>

CKeyboardLayout::CKeyboardLayout(unsigned short type, const unsigned char *table,
                                   const char *name, unsigned char flag)
{
	mType = type;
	memcpy(mTable, table, sizeof(mTable));

	/* SetLayoutName(name), inlined -- real ground truth:
	 * __assert_fail("kiLen < eMaxNameChars",
	 *   ".../OPOS/KorgPrj/Omega/Kernel/Locale/KeyboardLayout/KeyboardLayout.h", 0x1a,
	 *   "void CKeyboardLayout::SetLayoutName(const char*)");
	 * Never fires for any of this project's own 15 built-in names.
	 */
	size_t len = strlen(name);
	if (len >= sizeof(mName)) {
		__assert_fail("kiLen < eMaxNameChars",
		               "../../../../../OPOS/KorgPrj/Omega/Kernel/Locale/KeyboardLayout/KeyboardLayout.h",
		               0x1a, "void CKeyboardLayout::SetLayoutName(const char*)");
	}
	strcpy(mName, name);

	mFlag = flag;
}
