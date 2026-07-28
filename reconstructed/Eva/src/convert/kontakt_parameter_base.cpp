/*
 * kontakt_parameter_base.cpp  -  CKontaktParameter / CKontaktIndexedParameter
 * / CKontaktDynamicParameter. See include/kontakt_parameter_base.h for the
 * full derivation and object layout.
 */

#include "kontakt_parameter_base.h"

/* xmlFree -- same libxml2-owned function-pointer variable kontakt_xml.cpp
 * already declares extern; re-declared here since this is a separate TU. */
extern "C" {
extern void (*xmlFree)(void *mem);
unsigned char *xmlStrdup(const unsigned char *cur);
}

/* real .data literal: {"name", "value", 0}, shared byte-for-byte across all
 * 3 base classes' own AddAttribute bodies (confirmed via direct .data dump
 * at all 3 real call sites -- 0x91fc008 / 0x91fbe94 / 0x91fbae0, all
 * identical content). Declared once here rather than 3 times. */
static const char *kNameValueList[] = { "name", "value", 0 };

/* ============================== CKontaktParameter ======================= */

CKontaktParameter::CKontaktParameter(const char **list)
	: mList(list)
	, mAllocatedName(0)
{
}

CKontaktParameter::~CKontaktParameter()
{
	if (mAllocatedName) {
		xmlFree(mAllocatedName);
		mAllocatedName = 0;
	}
}

void CKontaktParameter::AddAttribute(unsigned int /*index*/, const unsigned char *name, const unsigned char *value)
{
	int which = CKontaktXml::StringIndex(kNameValueList, name);
	if (which == 0) {
		mAllocatedName = xmlStrdup(value);
		return;
	}
	if (which != 1)
		return;
	int fieldIndex = CKontaktXml::StringIndex(mList, mAllocatedName);
	if (fieldIndex < 0)
		return;
	AddParameter((unsigned int)fieldIndex, value);
}

void CKontaktParameter::AddParameter(const unsigned char *value)
{
	int fieldIndex = CKontaktXml::StringIndex(mList, mAllocatedName);
	if (fieldIndex < 0)
		return;
	AddParameter((unsigned int)fieldIndex, value);
}

void CKontaktParameter::AddParameter(unsigned int /*index*/, const unsigned char * /*value*/)
{
	/* real: literal `ret` no-op default */
}

/* =========================== CKontaktIndexedParameter ==================== */

CKontaktIndexedParameter::CKontaktIndexedParameter(const char **list)
	: mList(list)
	, mAllocatedName(0)
{
}

CKontaktIndexedParameter::~CKontaktIndexedParameter()
{
	if (mAllocatedName) {
		xmlFree(mAllocatedName);
		mAllocatedName = 0;
	}
}

void CKontaktIndexedParameter::AddAttribute(unsigned int /*index*/, const unsigned char *name, const unsigned char *value)
{
	int which = CKontaktXml::StringIndex(kNameValueList, name);
	if (which == 0) {
		mAllocatedName = xmlStrdup(value);
		return;
	}
	if (which != 1)
		return;
	unsigned int suffix = 0;
	int fieldIndex = CKontaktXml::StringIndex(mList, mAllocatedName, suffix);
	if (fieldIndex < 0)
		return;
	AddIndexedParameter((unsigned int)fieldIndex, suffix, value);
}

void CKontaktIndexedParameter::AddIndexedParameter(const unsigned char *value)
{
	unsigned int suffix = 0;
	int fieldIndex = CKontaktXml::StringIndex(mList, mAllocatedName, suffix);
	if (fieldIndex < 0)
		return;
	AddIndexedParameter((unsigned int)fieldIndex, suffix, value);
}

void CKontaktIndexedParameter::AddIndexedParameter(unsigned int /*index*/, unsigned int /*suffix*/, const unsigned char * /*value*/)
{
	/* real: literal `ret` no-op default */
}

/* =========================== CKontaktDynamicParameter ==================== */

CKontaktDynamicParameter::CKontaktDynamicParameter(const char **list)
	: mList(list)
	, mAllocatedName(0)
{
}

CKontaktDynamicParameter::~CKontaktDynamicParameter()
{
	if (mAllocatedName) {
		xmlFree(mAllocatedName);
		mAllocatedName = 0;
	}
}

void CKontaktDynamicParameter::AddAttribute(unsigned int /*index*/, const unsigned char *name, const unsigned char *value)
{
	int which = CKontaktXml::StringIndex(kNameValueList, name);
	if (which == 0) {
		mAllocatedName = xmlStrdup(value);
		return;
	}
	if (which != 1)
		return;
	char suffixText[0x20];
	int fieldIndex = CKontaktXml::StringIndex(mList, mAllocatedName, suffixText, sizeof(suffixText));
	if (fieldIndex < 0)
		return;
	AddDynamicParameter((unsigned int)fieldIndex, suffixText, value);
}

void CKontaktDynamicParameter::AddDynamicParameter(const unsigned char *value)
{
	char suffixText[0x20];
	int fieldIndex = CKontaktXml::StringIndex(mList, mAllocatedName, suffixText, sizeof(suffixText));
	if (fieldIndex < 0)
		return;
	AddDynamicParameter((unsigned int)fieldIndex, suffixText, value);
}

void CKontaktDynamicParameter::AddDynamicParameter(unsigned int /*index*/, const char * /*suffix*/, const unsigned char * /*value*/)
{
	/* real: literal `ret` no-op default */
}
