/*
 * parameter_string.h  -  CParameterString, a small self-contained "key=value,
 * key=value, ..." command-line-style parser. Real ground truth: CEditor's own
 * ctor (CEditor@08249cd0.c) builds one from its `param_2` argument (the alpha
 * keyboard config string main() passes through) whenever that argument is
 * non-null, then CEditor::Setup() (CEditor@08249b60.c) queries it for the
 * "ALPHAKEYBOARD" key to decide whether to construct the optional
 * CAlphaKeybIfcTask sibling task. Genuinely boot-path-adjacent (Stage 6 CEditor
 * batch, 2026-07-25) but entirely self-contained -- no dependency on Peg, on
 * CModule/CTask, or on anything else out of scope.
 *
 * Real layout (CParameterString@080b8ac0.c, ~CParameterString@080b8ed0.c,
 * GetParamStr@080b8fc0.c):
 *   +0x00  mBuffer  char*   -- new[]'d copy of the ctor's whole input string
 *                              (the linked list below points INTO this buffer,
 *                              null-terminating each token in place -- never
 *                              freed piecemeal, just `delete[] mBuffer` in the
 *                              dtor)
 *   +0x04  mList    SNode*  -- singly linked list head, MOST RECENTLY parsed
 *                              entry first (ctor prepends, matching a real
 *                              `,`-separated "last one wins" precedence for
 *                              duplicate keys -- GetParamStr() returns the
 *                              first match walking from the head)
 *   +0x08  mCount   int     -- number of parsed entries (incremented by the
 *                              ctor, never read back by any reconstructed
 *                              method)
 *
 * SNode (the ctor's `operator new(0xc)` allocation) is exactly:
 *   +0x00  name   char*  (points into mBuffer, nul-terminated in place)
 *   +0x04  value  char*  (points into mBuffer, nul-terminated in place --
 *                          NOT a copy)
 *   +0x08  next   SNode*
 *
 * The real ctor's own token scanner is an 8x Duff's-device-unrolled pair of
 * `isspace()` skip loops (leading/trailing whitespace trim around both the
 * `=` and the `,` delimiters) -- collapsed here to plain `while (isspace(*p))
 * p++;` loops, same license as every other unrolled loop in this project
 * (mains.cpp's Mains(), edit_man.h's GetServerScope(), etc.). Verified
 * semantically identical against the real disassembly's own control flow
 * (same skip/stop conditions, same order of operations), not merely
 * "simplified to look right".
 *
 * DecToInt()/HexToInt() (CParameterString::DecToInt(char const*&) /
 * HexToInt(char const*&), .text+0x080b9020/0x080b90f0) are real, standalone
 * static parsing helpers taking a `const char *&` cursor they advance past
 * the digits consumed -- also Duff's-device-unrolled in the real binary,
 * collapsed to plain loops here. No confirmed caller in this reconstruction's
 * own call graph yet (not exercised by CEditor::Setup() itself), included
 * for completeness since they're part of the same small, fully tractable
 * class and cost nothing extra to reconstruct faithfully.
 */

#ifndef PARAMETER_STRING_H
#define PARAMETER_STRING_H

class CParameterString {
public:
	/* .text+0x080b8ac0, 1001 bytes. */
	explicit CParameterString(const char *paramString);

	/* .text+0x080b8ed0, 226 bytes. */
	~CParameterString();

	/* .text+0x080b8fc0, 69 bytes. Real: `const` in ground truth. Returns the
	 * value string for the FIRST list entry (walking from the most-recently-
	 * parsed head) whose name matches `name`, or null if not found / list
	 * empty.
	 */
	const char *GetParamStr(const char *name) const;

	/* .text+0x080b9020, 191 bytes. Advances `*cursor` past a signed decimal
	 * run (`-` sign + digits) and returns its value; stops at the first
	 * non-digit. No overflow handling, matching ground truth.
	 */
	static int DecToInt(const char **cursor);

	/* .text+0x080b90f0, 63 bytes. Advances `*cursor` past a hex-digit run
	 * (upper OR lower case a-f) and returns its value; stops at the first
	 * non-hex-digit (never negative, no leading "0x" handling -- ground
	 * truth's own caller, not reconstructed here, presumably skips any "0x"
	 * prefix itself before calling this).
	 */
	static int HexToInt(const char **cursor);

private:
	struct SNode {
		char  *name;
		char  *value;
		SNode *next;
	};

	char  *mBuffer;
	SNode *mList;
	int    mCount;

	/* Not implemented -- real ground truth never copies/assigns a
	 * CParameterString (only ever constructed once by CEditor's own ctor and
	 * held by pointer); this project's convention (e.g. CommDriverTestHooks)
	 * is to leave copy/assign undeclared-but-unused rather than fabricate
	 * semantics ground truth doesn't exercise. Declared private with no body
	 * to make accidental copies a link error instead of silent double-free.
	 */
	CParameterString(const CParameterString &);
	CParameterString &operator=(const CParameterString &);
};

#endif /* PARAMETER_STRING_H */
