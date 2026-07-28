/*
 * kontakt_parameter_base.h  -  CKontaktParameter / CKontaktIndexedParameter /
 * CKontaktDynamicParameter, the three abstract roots of Eva's ~110-class
 * "CKontaktXxxParameter" accessor family that sits directly on top of the
 * already-reconstructed CKontaktXml (kontakt_xml.h). Found 2026-07-28 while
 * chasing the standing lead "CKontaktGroupParameter dispatches through
 * CKontaktXml's value parsers" (kontakt_xml.h's own file header) -- tracing
 * that dispatch chain top-down (AddAttribute -> the real per-class attribute-
 * name lists -> each concrete sibling's own switch) surfaced these 3 shared
 * bases first, since every concrete sibling's ctor/AddAttribute is inherited
 * from exactly one of them unchanged.
 *
 * ALL THREE derive from CKontaktXml (confirmed: each ctor's first act is
 * `call CKontaktXml::CKontaktXml()`) and share an identical 16-byte layout
 * added on top of CKontaktXml's own vptr+mState:
 *   +0x08  mList           const char *const *  -- NUL-terminated attribute-
 *          name list, supplied by each concrete subclass's own ctor as a
 *          literal .data pointer (e.g. CKontaktGroupParameter passes the
 *          32-entry list documented in kontakt_parameter_family.h). Never
 *          owned/freed by this class.
 *   +0x0c  mAllocatedName  unsigned char *  -- heap copy (xmlStrdup) of the
 *          most recent "name" attribute value seen on the current XML
 *          element (see AddAttribute below); freed via `xmlFree()` (the
 *          same libxml2-owned function-pointer variable kontakt_xml.cpp
 *          already declares -- confirmed via `readelf -r`: the dtor's
 *          indirect call target at .data+0x9304d40 is an R_386_COPY
 *          relocation naming `xmlFree`, not a raw GOT/PLT stub) in the dtor.
 *
 * REAL XML SHAPE this confirms: contrary to a first guess that e.g. a Zone's
 * fields would be direct XML attributes on `<Zone0 sampleStart="..." .../>`,
 * AddAttribute below ONLY ever recognizes two special attribute names,
 * "name" and "value" (confirmed: the 2-entry list every concrete AddAttribute
 * call site passes is `{"name","value",0}` byte-for-byte in all 3 places
 * checked) -- i.e. every real field is encoded as a CHILD element:
 *   <Zone0><Parameter name="sampleStart" value="1234"/>...</Zone0>
 * "name" is stashed (mAllocatedName); the following "value" attribute on the
 * SAME child element looks that stashed name up against the owning class's
 * own mList (case-insensitive prefix + optional numeric/text suffix, see
 * CKontaktXml::StringIndex's 3 overloads) and dispatches through the ONE new
 * virtual all three of these base classes add at vtable slot +0x14 --
 * CKontaktParameter's own AddParameter(unsigned,const unsigned char*) /
 * CKontaktIndexedParameter's AddIndexedParameter(unsigned,unsigned,const
 * unsigned char*) / CKontaktDynamicParameter's AddDynamicParameter(unsigned,
 * const char*,const unsigned char*) -- each concrete sibling in
 * kontakt_parameter_family.h overrides exactly this one slot.
 *
 * The `unsigned index` (libxml2 attribute position) argument AddAttribute
 * itself receives is genuinely unused by all 3 real bodies -- reproduced
 * as-is (kept in the signature for ABI/override fidelity, unused in the
 * base bodies too).
 *
 * WHICH SUB-FAMILY A CONCRETE CLASS USES, and why 3 not 1: StringIndex's
 * plain 2-arg overload (index only) backs CKontaktParameter -- used by
 * fields whose owning element repeats at most once per parent (Zone/Effect/
 * Filter/Output/Lfo/Loop/Envelope/PlaybackMode/StartCriteria in
 * kontakt_parameter_family.h); the 3-arg numeric-suffix overload backs
 * CKontaktIndexedParameter -- used where the "name" value itself carries a
 * TRAILING NUMBER selecting among several instances of the same logical
 * field (CKontaktGroupParameter's "outRouting_N" case, see that class's own
 * note); the 4-arg text-suffix overload backs CKontaktDynamicParameter --
 * used where the trailing part is free text, not a number
 * (CKontaktScriptParameter's "persistent_var_" case).
 *
 * NOT MODELED THIS PASS: the sibling "CKontaktXxxParameters" (plural) wrapper
 * family (CKontaktParameters/CKontaktIndexedParameters/
 * CKontaktDynamicParameters + ~7 concrete siblings, ~30 methods total) that
 * factories these singular classes into existence from a parent container's
 * own AddObject. Deliberately deferred: their own AddObject dispatches
 * through a name list that is IDENTICAL across all 3 abstract bases (byte-
 * for-byte the same .data pointer, itself pointing to a 1-entry list
 * containing just the string "V") -- confirmed via direct .data dump, not a
 * parsing artifact -- which does not correspond to any real Kontakt XML tag
 * name found anywhere else in this sweep. Most likely genuinely dead/
 * unreachable code in this build (these 3 base wrapper classes are abstract
 * -- only concrete containers like CKontaktGroup would ever really need
 * their own AddObject, and that container family is itself still 100% out
 * of scope), but flagged as an open, unexplained finding rather than papered
 * over with a guess. Every concrete singular Parameter class below is
 * complete and self-contained without needing the plural family at all.
 *
 * VIRTUAL SLOT LAYOUT (shared with CKontaktXml, see kontakt_xml.h):
 *   +0x00/+0x04  dtor pair (D1/D0)
 *   +0x08        CKontaktXml's own still-unidentified pure virtual (inherited
 *                unchanged -- none of these base classes override it)
 *   +0x0c        AddObject (inherited unchanged from CKontaktXml's default)
 *   +0x10        AddAttribute (OVERRIDDEN here, real body documented above)
 *   +0x14        NEW virtual this pass confirms: AddParameter/
 *                AddIndexedParameter/AddDynamicParameter respectively --
 *                pure dispatch target, this base class's own override is a
 *                literal 1-byte `ret` no-op (confirmed: .text size 1 for all
 *                3 in functions.csv) -- every concrete sibling overrides it
 *                for real.
 */

#ifndef KONTAKT_PARAMETER_BASE_H
#define KONTAKT_PARAMETER_BASE_H

#include "kontakt_xml.h"

/* .text+0x089c0c50 (ctor) / +0x089c0b10,+0x089c0b60 (dtor D1/D0) /
 * +0x089c0bc0 (AddAttribute) / +0x089c0c80 (AddParameter(uch*)) /
 * +0x089c0b00 (AddParameter(uint,uch*), 1-byte no-op default) /
 * +0x089c0cc0..+0x089c0d20 is CKontaktParameters (plural, NOT modeled here).
 */
class CKontaktParameter : public CKontaktXml {
public:
	/* `list` is stored, not copied/owned -- caller (a concrete subclass's
	 * own ctor) always passes a static .data literal. */
	CKontaktParameter(const char **list);
	virtual ~CKontaktParameter();

	/* .text+0x089c0bc0. Real body: `index` unused. StringIndex(list={"name",
	 * "value"}, name) == 0 ("name") -> xmlStrdup(value) into mAllocatedName.
	 * == 1 ("value") -> StringIndex(mList, mAllocatedName) (2-arg, no
	 * suffix) to resolve a field index, then (if found) virtual-dispatch
	 * AddParameter(fieldIndex, value). Anything else: no-op. */
	virtual void AddAttribute(unsigned int index, const unsigned char *name, const unsigned char *value);

	/* .text+0x089c0c80. Same "resolve mAllocatedName against mList, then
	 * dispatch" logic as AddAttribute's "value" case above, but taking the
	 * value directly as a parameter instead of reading the "value"
	 * attribute -- real caller not identified this pass (kept for symbol/
	 * behavior fidelity; harmless if never invoked). */
	void AddParameter(const unsigned char *value);

	/* .text+0x089c0b00. This base class's own default: literal `ret`
	 * no-op. Every concrete sibling in kontakt_parameter_family.h
	 * overrides this for real. */
	virtual void AddParameter(unsigned int index, const unsigned char *value);

protected:
	const char **mList;
	unsigned char *mAllocatedName;
};

/* .text+0x089be810 (ctor) / +0x089be6c0,+0x089be710 (dtor D1/D0) /
 * +0x089be770 (AddAttribute) / +0x089be840 (AddIndexedParameter(uch*)) /
 * +0x089be6b0 (AddIndexedParameter(uint,uint,uch*), 1-byte no-op default) /
 * +0x089be8a0..+0x089be900 is CKontaktIndexedParameters (plural, NOT
 * modeled here). Layout, semantics, and every real body below are IDENTICAL
 * in shape to CKontaktParameter above except for using StringIndex's 3-arg
 * (numeric-suffix) overload -- see file header for why 2 vs 3 vs 4-arg. */
class CKontaktIndexedParameter : public CKontaktXml {
public:
	CKontaktIndexedParameter(const char **list);
	virtual ~CKontaktIndexedParameter();

	/* .text+0x089be770. "value" case: StringIndex(mList, mAllocatedName,
	 * outSuffix) (3-arg numeric-suffix resolver) -> dispatch
	 * AddIndexedParameter(fieldIndex, suffix, value). */
	virtual void AddAttribute(unsigned int index, const unsigned char *name, const unsigned char *value);

	/* .text+0x089be840. Same idea as CKontaktParameter::AddParameter(uch*)
	 * above -- real caller not identified this pass. */
	void AddIndexedParameter(const unsigned char *value);

	/* .text+0x089be6b0. 1-byte no-op default; every concrete sibling
	 * overrides this for real. */
	virtual void AddIndexedParameter(unsigned int index, unsigned int suffix, const unsigned char *value);

protected:
	const char **mList;
	unsigned char *mAllocatedName;
};

/* .text+0x089bbe80 (ctor) / +0x089bbd30,+0x089bbd80 (dtor D1/D0) /
 * +0x089bbde0 (AddAttribute) / +0x089bbeb0 (AddDynamicParameter(uch*)) /
 * +0x089bbd20 (AddDynamicParameter(uint,char const*,uch*), 1-byte no-op
 * default) / +0x089bbf10..+0x089bbf70 is CKontaktDynamicParameters (plural,
 * NOT modeled here). Same shape again, using StringIndex's 4-arg
 * (text-suffix, 0x20-byte stack buffer) overload. */
class CKontaktDynamicParameter : public CKontaktXml {
public:
	CKontaktDynamicParameter(const char **list);
	virtual ~CKontaktDynamicParameter();

	/* .text+0x089bbde0. "value" case: StringIndex(mList, mAllocatedName,
	 * suffixBuf, 0x20) (4-arg text-suffix resolver, real code hardcodes
	 * the buffer size as 0x20) -> dispatch AddDynamicParameter(fieldIndex,
	 * suffixText, value). */
	virtual void AddAttribute(unsigned int index, const unsigned char *name, const unsigned char *value);

	/* .text+0x089bbeb0. Real caller not identified this pass. */
	void AddDynamicParameter(const unsigned char *value);

	/* .text+0x089bbd20. 1-byte no-op default. */
	virtual void AddDynamicParameter(unsigned int index, const char *suffix, const unsigned char *value);

protected:
	const char **mList;
	unsigned char *mAllocatedName;
};

#endif /* KONTAKT_PARAMETER_BASE_H */
