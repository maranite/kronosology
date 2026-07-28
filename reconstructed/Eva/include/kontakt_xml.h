/*
 * kontakt_xml.h  -  CKontaktXml, the shared libxml2-xmlTextReader wrapper and
 * value-parsing helper underneath Eva's entire Kontakt (NKI/NKM/NKX) import
 * subsystem. 2026-07-28 class-inventory sweep: fresh `nm -C` over the current
 * Eva static export surfaced ~180 previously-100%-untouched `CKontakt*`
 * classes (the NKI XML importer/converter -- CKontaktGroup/CKontaktZone/
 * CKontaktProgram/... plus a matching family of ~110 tiny
 * `CKontaktXxxParameter` accessor classes, e.g. CKontaktGroupParameter,
 * CKontaktZoneParameter). Before touching any of those, traced what they
 * actually call into: `objdump -dr -M intel` on CKontaktGroupParameter::
 * AddIndexedParameter shows a big `jmp *jumptable(,%eax,4)` dispatch whose
 * cases all funnel through CKontaktXml::UnsignedValue/SignedValue --
 * confirming CKontaktXml is the shared "value-parsing write-sink" the whole
 * Parameter family depends on, directly analogous to the OA-side
 * sValueGetterTemp discovery (see agent-memory stg_value_getter_family.md) --
 * so it's reconstructed here FIRST, on its own, as the natural root of any
 * future pass over the ~180-class family (each `CKontaktXxxParameter::
 * AddIndexedParameter` deferred, unstarted, out of scope this pass).
 *
 * Real call-xref-traced before committing: CKontaktXml's own disassembled
 * bytes (every method in this file) call ONLY libxml2's `xmlTextReader*`
 * API + `xmlFree`/`xmlStrdup` (all real externs, `nm -D` confirms
 * xmlTextReaderRead/Next/NodeType/Name/Value/Depth/IsEmptyElement/
 * AttributeCount/MoveToAttributeNo/GetParserLineNumber, xmlNewTextReaderFilename,
 * xmlFreeTextReader) plus plain libc (strcasecmp/strncasecmp/strlen/strncpy/
 * strncat/sscanf/strrchr) -- zero touch on CZ/CStorage/the ES-family task
 * god-objects/Peg GUI/the virtual-driver subsystem.
 *
 * OBJECT LAYOUT (8 bytes, confirmed from the real ctor @.text+0x089c4bc0):
 *   +0x00  vtable ptr (real vtable @.rodata+0x08f798d8, dumped directly:
 *          slot+0x0/+0x4 = the virtual dtor pair (complete-object D2 @
 *          0x089c4b20, deleting D0 @ 0x089c4b40); slot+0x8 =
 *          `__cxa_pure_virtual@plt` -- CKontaktXml declares a THIRD pure
 *          virtual this pass never identified a name/signature for (no real
 *          caller inside CKontaktXml's own methods reaches it) -- declared
 *          below as an unnamed placeholder, TODO; slot+0xc = AddObject,
 *          slot+0x10 = AddAttribute, both confirmed by matching these exact
 *          vtable addresses against the real CKontaktXml::AddObject/
 *          AddAttribute symbols).
 *   +0x04  KontaktState mState  ctor-inits to eOutside(0). StateString's own
 *          3-entry jump table (@.rodata+0x08f771b0, dumped directly)
 *          confirms the enum's real string values: 0="Outside", 1="Inside",
 *          2="Done" (out-of-range default "????").
 *
 * ProcessNode/ProcessNodes/Parse -- the real parse-loop state machine: on an
 * ELEMENT node (libxml2 XML_READER_TYPE_ELEMENT=1) never seen before
 * (mState==eOutside), a run of attributes is walked via
 * MoveToAttributeNo/Name/Value and each one dispatched through the virtual
 * `AddAttribute(index, name, value)` (default impl here is a plain `ret`
 * no-op -- every real behavior comes from a derived Parameter class's
 * override); mState becomes eInside, and becomes eDone immediately after if
 * the element is self-closing (IsEmptyElement -- libxml2's reader model
 * never emits a separate END_ELEMENT for those). A SECOND ELEMENT node seen
 * while mState==eInside is instead dispatched through the virtual
 * `AddObject(reader=NULL, name)` (this base class's own default impl is
 * byte-identical to SkipNode() below -- confirmed by direct disassembly
 * comparison -- i.e. "advance one node via xmlTextReaderNext() and report
 * whether that failed to move forward", used here as a generic "consume and
 * move on" default); if it reports true, mState becomes eDone. An
 * END_ELEMENT node (type 15) sets mState=eDone directly; any other node
 * type is a no-op. `Parse(_xmlTextReader*)` is byte-identical to
 * `ProcessNodes(reader, false)` (confirmed by direct disassembly comparison)
 * -- kept as a separate real symbol/method here since the binary emits both,
 * but their bodies are provably the same state machine.
 *
 * StringIndex(list, name, unsigned int&) is the actual "TagN" indexed-name
 * resolver used throughout the Parameter family (e.g. matching XML element
 * name "Group3" against a caller-supplied `{"Group","Zone",...,0}` list):
 * case-insensitively prefix-matches `name` against each `list[i]`
 * (strncasecmp over strlen(list[i]) bytes); on a match, if `name` has
 * trailing characters beyond the matched prefix they must parse as a plain
 * "%u" (the out unsigned int&) for the match to count, otherwise that
 * candidate is rejected and the scan continues -- i.e. list[i] must be an
 * exact match OR an exact-numeric-suffix match. Returns the matching index,
 * or -1. The (char*, unsigned int) overload is the same idea but copies the
 * (non-numeric-constrained) suffix text instead of parsing a number.
 *
 * SCOPE NOTE: four real methods are DEFERRED, each for a documented reason
 * (not a blanket "ran out of time") -- declared here for linkage only (no
 * body in kontakt_xml.cpp; `make link`'s expected-unresolved-symbol
 * convention applies same as every other class in this project):
 *   `TruncateName(char const*, char*, unsigned int)` -- .text+0x089c5920,
 *     9731 bytes (functions.csv), by FAR the largest method in this class
 *     (every other method here is under ~300 bytes) -- a genuinely complex
 *     algorithm (name-shortening/ellipsis logic judging by its many nested
 *     branches over character classes) that warrants its own dedicated pass
 *     rather than a rushed, low-confidence transcription.
 *   `UnpackPath(unsigned char const*, char*, unsigned int)` -- .text+
 *     0x089c5340, a packed-path token-decoder state machine (dispatches on
 *     marker bytes 'F'/'b'/'d'/'v' after a leading '@', each case reading a
 *     fixed run of trailing digit bytes via sscanf("%u",...) and rebuilding
 *     a path string) -- mechanically traceable but the real-world meaning of
 *     each token type is not yet pinned down with enough confidence for a
 *     byte-exact port; a closer look is a clean, self-contained follow-up.
 *   `RemoveTrailingCharacters(char*, char, char)` -- .text+0x089c8110, a
 *     backward byte-range scan (`minChar <= c <= maxChar`) that resolves to
 *     exactly ONE `strcpy`-shift-left-by-one at the end regardless of scan
 *     length; the precise boundary rule (which single character actually
 *     gets deleted) was not disambiguated with confidence this pass.
 *   `PathName(char const*, char*, unsigned int, bool)` -- .text+0x089c83d0.
 *     Cleanly traced through its own directory/filename-splitting setup
 *     (two strrchr('/') scans building a "just the immediate parent
 *     directory's own name" buffer when `full` is set), but its shared tail
 *     mixes an inlined SWAR strlen() with a strncat() whose count argument
 *     is computed from that SWAR result in a way not confidently
 *     disambiguated this pass -- AND it tail-calls the also-deferred
 *     TruncateName() at the very end regardless, so a from-scratch
 *     redo makes more sense as a single follow-up covering both.
 * All four are declared here for linkage only (no bodies in kontakt_xml.cpp);
 * none of them are called by any other method reconstructed in this file.
 */

#ifndef KONTAKT_XML_H
#define KONTAKT_XML_H

struct _xmlTextReader;

class CKontaktXml {
public:
	/* Real enum, StateString's own 3-entry jump table confirms the string
	 * values cited above. */
	enum KontaktState {
		eOutside = 0,
		eInside  = 1,
		eDone    = 2
	};

	/* .text+0x089c4bc0. mState = eOutside. */
	CKontaktXml();

	/* .text+0x089c4b20 (complete-object) / 0x089c4b40 (deleting, calls
	 * operator delete). Virtual -- see vtable note above. */
	virtual ~CKontaktXml();

	/* .text+0x089c4be0. Static; the 3-entry KontaktState -> string table. */
	static const char *StateString(KontaktState state);

	/* vtable slot +0x8, __cxa_pure_virtual in this class's own vtable --
	 * name/signature not identified this pass (no real caller inside
	 * CKontaktXml's own reconstructed methods reaches it). TODO. */
	virtual void UnidentifiedPureVirtual_slot8() = 0;

	/* vtable slot +0xc, .text+0x089c4b60 (this class's default impl).
	 * Byte-identical body to SkipNode() below: advances the reader one
	 * node (xmlTextReaderNext), frees the new node's own Name() string,
	 * and returns true iff xmlTextReaderNext()'s OWN return value was not
	 * exactly 1 (libxml2 convention: 1 = advanced successfully, 0 = no
	 * more nodes, -1 = error) -- i.e. true means "nothing left to
	 * advance to / an error occurred". `reader` is passed 0/NULL at
	 * ProcessNode's own call site (real behavior, reproduced as-is);
	 * relies on libxml2's own xmlTextReader* accessors being NULL-safe
	 * (real libxml2 source: each one checks `reader == NULL` and returns
	 * a safe default, e.g. -1) -- not a bug in this base default impl. */
	virtual bool AddObject(_xmlTextReader *reader, const unsigned char *name);

	/* vtable slot +0x10, .text+0x089c4b30 (this class's default impl is a
	 * plain no-op `ret`). */
	virtual void AddAttribute(unsigned int index, const unsigned char *name, const unsigned char *value);

	/* .text+0x089c4c00. Real parse-loop state machine -- see file header.
	 * Returns true only from the mState==eInside/AddObject dispatch path,
	 * when AddObject reports it failed to advance to a further node
	 * (which also drives mState to eDone in that case); every other path
	 * (END_ELEMENT, first-time attribute processing, non-ELEMENT nodes)
	 * returns false. */
	bool ProcessNode(_xmlTextReader *reader);

	/* .text+0x089c4ed0. If skipFirstRead, does one Read() before the first
	 * ProcessNode() call; otherwise calls ProcessNode() immediately. Then
	 * loops: while mState != eDone, Read() (returning true -- an error
	 * indication, after logging via GetParserLineNumber -- if Read()
	 * itself fails) then ProcessNode() (breaking the loop early if it
	 * returns true). Returns false on normal completion (mState reached
	 * eDone), true only on a Read() failure. */
	bool ProcessNodes(_xmlTextReader *reader, bool skipFirstRead);

	/* .text+0x089c4f30. Byte-identical to ProcessNodes(reader, false) --
	 * see file header. Kept as its own real method/symbol. */
	bool Parse(_xmlTextReader *reader);

	/* .text+0x089c4f80. Creates a reader via xmlNewTextReaderFilename();
	 * if that fails, returns 0 WITHOUT any error indication (real binary
	 * behavior, reproduced as-is). Otherwise runs the same ProcessNode-
	 * first / Read-then-loop-until-eDone state machine as ProcessNodes/
	 * Parse above, always calling xmlFreeTextReader() before returning. */
	int Parse(const char *filename);

	/* .text+0x089c4ff0. Advances the reader one node (xmlTextReaderNext),
	 * frees the node's own Name() string via xmlFree() (matches libxml2's
	 * caller-frees-on-Name() contract), and returns true iff
	 * xmlTextReaderNext()'s own return value was not exactly 1 (i.e.
	 * "failed to advance / no more nodes / error"). NodeType()/
	 * IsEmptyElement() are also called on the new current node but their
	 * results are discarded (real code, reproduced as-is).
	 * CKontaktXml::AddObject's default body above is byte-identical to
	 * this. */
	static bool SkipNode(_xmlTextReader *reader);

	/* .text+0x089c5050. Case-insensitive exact match against a
	 * NULL-terminated `list`; returns the matching index or -1. Static --
	 * no `this` argument in the real disassembly. */
	static int StringIndex(const char **list, const unsigned char *name);

	/* .text+0x089c50b0. Prefix-match + numeric-suffix resolver -- see file
	 * header ("TagN" resolution). `outSuffix` is written 0 up front and
	 * only meaningfully set on a match with a nonempty numeric suffix. */
	static int StringIndex(const char **list, const unsigned char *name, unsigned int &outSuffix);

	/* .text+0x089c5170. Same prefix-match idea, but copies the (untyped)
	 * suffix text into `outSuffix` (strncpy, up to `outSuffixSize` -- NOT
	 * guaranteed NUL-terminated by the real code if the suffix is exactly
	 * outSuffixSize bytes) instead of requiring/parsing a number. */
	static int StringIndex(const char **list, const unsigned char *name, char *outSuffix, unsigned int outSuffixSize);

	/* .text+0x089c5210. Plain case-insensitive strcasecmp()==0 wrapper. */
	static bool StringsEqual(const unsigned char *a, const char *b);

	/* .text+0x089c5240. True for (case-insensitive) "yes"/"1"; false for
	 * "no"/"0" OR anything else (real code: only those two negatives are
	 * even checked -- any other unrecognized string also yields false via
	 * the same fallthrough as "no"/"0", reproduced as-is). */
	static bool BooleanValue(const unsigned char *s);

	/* .text+0x089c52b0/0x089c52e0/0x089c5310. Plain sscanf("%u"/"%d"/"%f")
	 * wrappers; real code does not check sscanf's return value (an
	 * unparsable string yields an uninitialized-stack-garbage result in
	 * the real binary -- reproduced as-is, NOT defensively zeroed). */
	static unsigned int UnsignedValue(const unsigned char *s);
	static int SignedValue(const unsigned char *s);
	static float FloatValue(const unsigned char *s);

	/* .text+0x089c5610/0x089c5680. BYTE-IDENTICAL bodies in the real
	 * binary (confirmed by direct disassembly comparison) despite being
	 * two separate symbols: given a packed buffer and a cursor `pos`
	 * (read/write reference), skip 1 byte, read the next 3 bytes as an
	 * ASCII decimal triplet (sscanf "%u"), advance pos by 4 total, return
	 * the parsed value. */
	static unsigned int VolumeLength(const unsigned char *packed, unsigned int &pos);
	static unsigned int DirectoryLength(const unsigned char *packed, unsigned int &pos);

	/* .text+0x089c56f0. Same idea as VolumeLength/DirectoryLength but a
	 * larger packed-record header: skip 6 bytes, read the next 3 as the
	 * ASCII decimal triplet, advance pos by 12 total. */
	static unsigned int FileLength(const unsigned char *packed, unsigned int &pos);

	/* .text+0x089c5760. Copies up to 64 bytes from `src + pos` into a
	 * fixed 64-byte stack temp (real code hardcodes the strncpy count as
	 * 0x40 regardless of `len` -- if `len` > 64 the real code's own
	 * `tmpBuf[len] = 0` NUL-terminate write would overrun that 64-byte
	 * temp; reproduced as-is, callers are assumed to honor len<=64),
	 * advances `pos` by `len`, then strncat()s the temp onto `dest`
	 * (bounded by `destCapacity - strlen(dest)`), finally force-NUL-
	 * terminating dest[destCapacity-1] defensively. */
	static void Append(const unsigned char *src, unsigned int &pos, unsigned int len, char *dest, unsigned int destCapacity);

	/* .text+0x089c57f0. If `rel` is already absolute (rel[0]=='/'),
	 * strncpy's it straight into outBuf. Otherwise: copies `base` into
	 * outBuf, and if outBuf doesn't already end in '/', strips backward to
	 * (and keeping) the last '/' -- i.e. dirname-with-trailing-slash --
	 * (real code: an unrolled 8-byte-at-a-time backward scan; reproduced
	 * here as a plain backward scan for the same observable end string;
	 * the "no '/' anywhere in base" edge case was not independently
	 * byte-verified against the real unrolled asm, flagged rather than
	 * silently guessed), then strncat()s `rel` onto the result -- always
	 * force-NUL-terminating outBuf[outBufSize-1] at the very end. */
	static void AbsolutePath(const char *base, const char *rel, char *outBuf, unsigned int outBufSize);

	/* .text+0x089c58f0. strrchr(name,'.') -> NUL it out if found. The
	 * `unsigned int` 2nd parameter is genuinely unused in the real body
	 * (kept for signature fidelity). */
	static void RemoveNameExtension(char *name, unsigned int unusedMaxLen);

	/* DEFERRED -- see file header SCOPE NOTE. Declared for linkage only
	 * (the also-deferred PathName() below is its only real caller). */
	static void TruncateName(const char *name, char *outBuf, unsigned int outBufSize);

	/* DEFERRED -- see file header SCOPE NOTE. Declared for linkage only;
	 * not called by anything else in this file. */
	static bool RemoveTrailingCharacters(char *str, char maxChar, char minChar);

	/* DEFERRED -- see file header SCOPE NOTE. Declared for linkage only;
	 * not called by anything else in this file (it is a leaf entry point
	 * for the Parameter family, not something ProcessNode/Parse/etc.
	 * above call into). */
	static void PathName(const char *fullPath, char *outBuf, unsigned int outBufSize, bool full);

private:
	KontaktState mState;
};

#endif /* KONTAKT_XML_H */
