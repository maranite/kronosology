/*
 * kontakt_xml.cpp  -  CKontaktXml. See include/kontakt_xml.h for the full
 * derivation, object layout, and the four deferred methods.
 */

#include "kontakt_xml.h"

#include <cstdio>
#include <cstring>
#include <strings.h> /* strcasecmp/strncasecmp (POSIX, not in <cstring>) */

/* Minimal extern "C" declarations for the real libxml2 xmlTextReader API this
 * class calls into (confirmed via `nm -D` -- see kontakt_xml.h's file header).
 * Kept local to this TU rather than pulling in a full libxml2 headers
 * dependency, matching this project's "declare only what's called" convention
 * for external libraries. */
extern "C" {
struct _xmlTextReader;
typedef struct _xmlTextReader *xmlTextReaderPtr;

xmlTextReaderPtr xmlNewTextReaderFilename(const char *URI);
void xmlFreeTextReader(xmlTextReaderPtr reader);
int xmlTextReaderRead(xmlTextReaderPtr reader);
int xmlTextReaderNext(xmlTextReaderPtr reader);
int xmlTextReaderNodeType(xmlTextReaderPtr reader);
int xmlTextReaderIsEmptyElement(xmlTextReaderPtr reader);
int xmlTextReaderDepth(xmlTextReaderPtr reader);
int xmlTextReaderAttributeCount(xmlTextReaderPtr reader);
int xmlTextReaderMoveToAttributeNo(xmlTextReaderPtr reader, int no);
int xmlTextReaderGetParserLineNumber(xmlTextReaderPtr reader);
unsigned char *xmlTextReaderName(xmlTextReaderPtr reader);
unsigned char *xmlTextReaderValue(xmlTextReaderPtr reader);
unsigned char *xmlStrdup(const unsigned char *cur);
extern void (*xmlFree)(void *mem); /* real symbol: a libxml2-owned function-pointer variable, called through */
}

/* ---- construction / destruction ------------------------------------------------ */

CKontaktXml::CKontaktXml()
	: mState(eOutside)
{
}

CKontaktXml::~CKontaktXml()
{
}

/* ---- misc statics ---------------------------------------------------------------- */

const char *CKontaktXml::StateString(KontaktState state)
{
	switch (state) {
	case eOutside: return "Outside";
	case eInside:  return "Inside";
	case eDone:    return "Done";
	default:       return "????"; /* real: out-of-range default */
	}
}

/* ---- default virtuals (overridden by every real Parameter-family class) -------- */

bool CKontaktXml::AddObject(_xmlTextReader *reader, const unsigned char * /*name*/)
{
	/* Byte-identical to SkipNode() -- see header. */
	int nextResult = xmlTextReaderNext(reader);
	xmlTextReaderNodeType(reader);        /* real: return value discarded */
	xmlTextReaderIsEmptyElement(reader);  /* real: return value discarded */
	unsigned char *nodeName = xmlTextReaderName(reader);
	if (nodeName != 0)
		xmlFree(nodeName);
	return nextResult != 1;
}

void CKontaktXml::AddAttribute(unsigned int /*index*/, const unsigned char * /*name*/, const unsigned char * /*value*/)
{
	/* real: plain no-op `ret` */
}

/* ---- parse-loop state machine ---------------------------------------------------- */

bool CKontaktXml::ProcessNode(_xmlTextReader *reader)
{
	xmlTextReaderDepth(reader);            /* real: called for its side effect only, return discarded */
	int nodeType = xmlTextReaderNodeType(reader);
	bool isEmptyElement = (xmlTextReaderIsEmptyElement(reader) != 0);

	if (nodeType != 1) {
		if (nodeType == 15) /* END_ELEMENT */
			mState = eDone;
		return false;
	}

	/* ELEMENT */
	unsigned char *name = xmlTextReaderName(reader);
	if (name == 0)
		name = xmlStrdup((const unsigned char *)"--");
	unsigned char *elemValue = xmlTextReaderValue(reader); /* real: called unconditionally, freed below */
	int attrCount = xmlTextReaderAttributeCount(reader);

	bool stoppedOnElement = false;

	if (mState == eOutside) {
		mState = eInside;
		for (int i = 0; i < attrCount; ++i) {
			xmlTextReaderMoveToAttributeNo(reader, i);
			unsigned char *attrName = xmlTextReaderName(reader);
			unsigned char *attrValue = xmlTextReaderValue(reader);
			AddAttribute((unsigned int)i, attrName, attrValue);
			xmlFree(attrName);
			xmlFree(attrValue);
		}
		if (isEmptyElement) /* self-closing tag: no separate END_ELEMENT will follow */
			mState = eDone;
	} else if (mState == eInside) {
		stoppedOnElement = AddObject(0, name);
		if (stoppedOnElement)
			mState = eDone;
	}
	/* else mState == eDone: no-op */

	xmlFree(name);
	if (elemValue != 0)
		xmlFree(elemValue);

	return stoppedOnElement;
}

bool CKontaktXml::ProcessNodes(_xmlTextReader *reader, bool skipFirstRead)
{
	if (!skipFirstRead) {
		if (ProcessNode(reader))
			return true;
		/* falls into the common loop below at its mState check, same as
		 * the loop's own backedge */
	} else {
		/* skipFirstRead: the real code's very first action is a Read(),
		 * bypassing the initial mState check the !skipFirstRead path
		 * takes on entry to the loop below */
		if (xmlTextReaderRead(reader) != 1) {
			xmlTextReaderGetParserLineNumber(reader); /* diagnostic only, discarded */
			return true;
		}
		if (ProcessNode(reader))
			return true;
	}

	for (;;) {
		if (mState == eDone)
			return false;
		if (xmlTextReaderRead(reader) != 1) {
			xmlTextReaderGetParserLineNumber(reader);
			return true;
		}
		if (ProcessNode(reader))
			return true;
	}
}

bool CKontaktXml::Parse(_xmlTextReader *reader)
{
	/* Byte-identical body to ProcessNodes(reader, false) -- see header. */
	return ProcessNodes(reader, false);
}

int CKontaktXml::Parse(const char *filename)
{
	_xmlTextReader *reader = xmlNewTextReaderFilename(filename);
	if (reader == 0)
		return 0; /* real: no error indication on reader-creation failure, reproduced as-is */

	int result;
	for (;;) {
		if (ProcessNode(reader)) {
			result = 1;
			break;
		}
		if (mState == eDone) {
			result = 0;
			break;
		}
		if (xmlTextReaderRead(reader) == 1)
			continue;
		xmlTextReaderGetParserLineNumber(reader); /* diagnostic only, discarded */
		result = 1;
		break;
	}

	xmlFreeTextReader(reader);
	return result;
}

bool CKontaktXml::SkipNode(_xmlTextReader *reader)
{
	int nextResult = xmlTextReaderNext(reader);
	xmlTextReaderNodeType(reader);        /* real: return value discarded */
	xmlTextReaderIsEmptyElement(reader);  /* real: return value discarded */
	unsigned char *name = xmlTextReaderName(reader);
	if (name != 0)
		xmlFree(name);
	return nextResult != 1;
}

/* ---- string / value helpers ------------------------------------------------------ */

int CKontaktXml::StringIndex(const char **list, const unsigned char *name)
{
	int idx = 0;
	const char *entry = list[0];
	if (entry == 0)
		return -1;
	while (strcasecmp(entry, (const char *)name) != 0) {
		++idx;
		entry = list[idx];
		if (entry == 0)
			return -1;
	}
	return idx;
}

int CKontaktXml::StringIndex(const char **list, const unsigned char *name, unsigned int &outSuffix)
{
	outSuffix = 0;
	int idx = 0;
	const char *entry = list[0];
	if (entry == 0)
		return -1;

	for (;;) {
		size_t entryLen = strlen(entry);
		if (strncasecmp(entry, (const char *)name, entryLen) == 0) {
			size_t nameLen = strlen((const char *)name);
			if (nameLen <= entryLen)
				return idx; /* exact match, no trailing suffix */
			if (sscanf((const char *)name + entryLen, "%u", &outSuffix) == 1)
				return idx; /* prefix + valid numeric suffix */
		}
		++idx;
		entry = list[idx];
		if (entry == 0)
			return -1;
	}
}

int CKontaktXml::StringIndex(const char **list, const unsigned char *name, char *outSuffix, unsigned int outSuffixSize)
{
	outSuffix[0] = 0;
	int idx = 0;
	const char *entry = list[0];
	if (entry == 0)
		return -1;

	for (;;) {
		size_t entryLen = strlen(entry);
		if (strncasecmp(entry, (const char *)name, entryLen) == 0) {
			size_t nameLen = strlen((const char *)name);
			if (nameLen <= entryLen)
				return idx; /* exact match, outSuffix stays "" */
			strncpy(outSuffix, (const char *)name + entryLen, outSuffixSize);
			return idx;
		}
		++idx;
		entry = list[idx];
		if (entry == 0)
			return -1;
	}
}

bool CKontaktXml::StringsEqual(const unsigned char *a, const char *b)
{
	return strcasecmp((const char *)a, b) == 0;
}

bool CKontaktXml::BooleanValue(const unsigned char *s)
{
	const char *cs = (const char *)s;
	if (strcasecmp(cs, "no") == 0)
		return false;
	if (strcasecmp(cs, "yes") == 0)
		return true;
	if (strcasecmp(cs, "0") == 0)
		return false;
	return strcasecmp(cs, "1") == 0; /* real: final catch-all, everything else -> false */
}

unsigned int CKontaktXml::UnsignedValue(const unsigned char *s)
{
	unsigned int v; /* real: left uninitialized if sscanf doesn't match -- reproduced as-is */
	sscanf((const char *)s, "%u", &v);
	return v;
}

int CKontaktXml::SignedValue(const unsigned char *s)
{
	int v; /* real: left uninitialized if sscanf doesn't match -- reproduced as-is */
	sscanf((const char *)s, "%d", &v);
	return v;
}

float CKontaktXml::FloatValue(const unsigned char *s)
{
	float v; /* real: left uninitialized if sscanf doesn't match -- reproduced as-is */
	sscanf((const char *)s, "%f", &v);
	return v;
}

/* ---- packed-record length readers ------------------------------------------------ */

unsigned int CKontaktXml::VolumeLength(const unsigned char *packed, unsigned int &pos)
{
	char buf[4];
	buf[0] = (char)packed[pos + 1];
	buf[1] = (char)packed[pos + 2];
	buf[2] = (char)packed[pos + 3];
	buf[3] = 0;
	pos += 4;
	unsigned int v;
	sscanf(buf, "%u", &v);
	return v;
}

unsigned int CKontaktXml::DirectoryLength(const unsigned char *packed, unsigned int &pos)
{
	/* Byte-identical body to VolumeLength() above -- see header. */
	char buf[4];
	buf[0] = (char)packed[pos + 1];
	buf[1] = (char)packed[pos + 2];
	buf[2] = (char)packed[pos + 3];
	buf[3] = 0;
	pos += 4;
	unsigned int v;
	sscanf(buf, "%u", &v);
	return v;
}

unsigned int CKontaktXml::FileLength(const unsigned char *packed, unsigned int &pos)
{
	char buf[4];
	buf[0] = (char)packed[pos + 6];
	buf[1] = (char)packed[pos + 7];
	buf[2] = (char)packed[pos + 8];
	buf[3] = 0;
	pos += 12;
	unsigned int v;
	sscanf(buf, "%u", &v);
	return v;
}

/* ---- buffer / path helpers --------------------------------------------------------*/

void CKontaktXml::Append(const unsigned char *src, unsigned int &pos, unsigned int len, char *dest, unsigned int destCapacity)
{
	char tmp[64];
	strncpy(tmp, (const char *)src + pos, 64); /* real: fixed 64, ignores `len` here -- see header */
	tmp[63] = 0;
	pos += len;
	tmp[len] = 0; /* real: written unconditionally -- overruns `tmp` if len > 64, see header */

	size_t destLen = strlen(dest);
	strncat(dest, tmp, destCapacity - destLen);
	dest[destCapacity - 1] = 0;
}

void CKontaktXml::AbsolutePath(const char *base, const char *rel, char *outBuf, unsigned int outBufSize)
{
	if (rel[0] == '/') {
		strncpy(outBuf, rel, outBufSize);
		outBuf[outBufSize - 1] = 0;
		return;
	}

	strncpy(outBuf, base, outBufSize);
	outBuf[outBufSize - 1] = 0;

	size_t n = strlen(outBuf);
	char *end = outBuf + n - 1;
	if (n > 0 && *end != '/') {
		/* Strip back to and including the last '/' -- i.e.
		 * dirname-with-trailing-slash. Real code: an unrolled 8-byte-at-
		 * a-time backward scan; reproduced here as a plain backward scan
		 * for the same observable end string (the "no '/' anywhere in
		 * base" edge case below was not independently byte-verified
		 * against the real unrolled asm). */
		while (end > outBuf && *end != '/')
			*end-- = 0;
		if (end == outBuf && *end != '/')
			*end = 0;
	}

	size_t baseLen = strlen(outBuf);
	strncat(outBuf, rel, outBufSize - baseLen);
	outBuf[outBufSize - 1] = 0;
}

void CKontaktXml::RemoveNameExtension(char *name, unsigned int /*unusedMaxLen*/)
{
	char *dot = strrchr(name, '.');
	if (dot != 0)
		*dot = 0;
}
