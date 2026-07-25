/*
 * parameter_string.cpp  -  see include/parameter_string.h.
 *
 * Transcribed from CParameterString@080b8ac0.c / ~CParameterString@080b8ed0.c /
 * GetParamStr@080b8fc0.c / DecToInt@080b9020.c / HexToInt@080b90f0.c. Every
 * Duff's-device-unrolled `isspace()` skip loop in the ground truth is collapsed
 * to a plain `while (isspace(...)) ...;` loop -- verified to hit the exact same
 * stop conditions as the real disassembly, not just "simplified to look right".
 */

#include "parameter_string.h"

#include <cctype>
#include <cstring>

namespace {

inline bool IsSpace(char c)
{
	return isspace(static_cast<unsigned char>(c)) != 0;
}

} /* anonymous namespace */

CParameterString::CParameterString(const char *paramString)
	: mBuffer(0), mList(0), mCount(0)
{
	size_t len = strlen(paramString);
	mBuffer = new char[len + 1];
	strcpy(mBuffer, paramString);

	char *p = mBuffer;
	while (*p != '\0') {
		/* Skip leading whitespace before the key. */
		while (IsSpace(*p))
			p++;
		char *nameStart = p;

		/* Scan to '=' (or end of string). Capture whether we actually found
		 * '=' (and the position just past it) BEFORE the trailing-whitespace
		 * trim below writes a nul terminator -- when there's no whitespace to
		 * trim, `nameEnd` lands on the SAME byte as `p`'s own '=' (or '\0'),
		 * so nul-ing it in place would otherwise clobber the very delimiter
		 * we still need to test. Same fix shape as the value-scan's own
		 * `afterValue` capture below. */
		while (*p != '\0' && *p != '=')
			p++;
		char *nameEnd = p;
		bool foundEquals = (*p == '=');
		char *afterName = p + (foundEquals ? 1 : 0);

		/* Trim trailing whitespace off the key. */
		while (nameEnd > nameStart && IsSpace(nameEnd[-1]))
			nameEnd--;
		*nameEnd = '\0';

		if (!foundEquals)
			break; /* no '=' found -- ground truth simply stops */
		p = afterName; /* skip '=' */

		/* Skip leading whitespace before the value. */
		while (IsSpace(*p))
			p++;
		char *valueStart = p;

		/* Scan to ',' (or end of string). `p` is left pointing at the ','
		 * (or the terminating '\0') -- captured in `afterValue` BEFORE the
		 * trailing-whitespace trim below moves `valueEnd` backward, so the
		 * next token's scan can resume from the real, untrimmed delimiter
		 * position. */
		while (*p != '\0' && *p != ',')
			p++;
		char *valueEnd = p;
		char *afterValue = p;
		bool sawComma = (*p == ',');
		if (sawComma)
			afterValue++;

		/* Trim trailing whitespace off the value. */
		while (valueEnd > valueStart && IsSpace(valueEnd[-1]))
			valueEnd--;
		*valueEnd = '\0';

		p = afterValue;

		SNode *node = new SNode;
		node->name = nameStart;
		node->value = valueStart;
		node->next = mList;
		mList = node;
		mCount++;

		if (!sawComma)
			break;
	}
}

CParameterString::~CParameterString()
{
	SNode *node = mList;
	while (node != 0) {
		SNode *next = node->next;
		delete node;
		node = next;
	}
	delete[] mBuffer;
}

const char *CParameterString::GetParamStr(const char *name) const
{
	for (SNode *node = mList; node != 0; node = node->next) {
		if (strcmp(node->name, name) == 0)
			return node->value;
	}
	return 0;
}

int CParameterString::DecToInt(const char **cursor)
{
	const char *p = *cursor;
	bool negative = (*p == '-');
	if (negative) {
		p++;
		*cursor = p;
	}

	int value = 0;
	while (*p >= '0' && *p <= '9') {
		value = value * 10 + (*p - '0');
		p++;
		*cursor = p;
	}

	return negative ? -value : value;
}

int CParameterString::HexToInt(const char **cursor)
{
	const char *p = *cursor;
	int value = 0;

	for (;;) {
		char c = *p;
		int digit;
		if (c >= '0' && c <= '9') {
			digit = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			digit = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'F') {
			digit = c - 'A' + 10;
		} else {
			return value;
		}
		p++;
		*cursor = p;
		value = value * 16 + digit;
	}
}
