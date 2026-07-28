/*
 * storage_converter_base.h  -  CStorageConverterBase, the internal<->external
 * storage-format version converter beneath CFilesys/CDiskUtil (part of the
 * storage cluster surfaced by file_io_base.h/scsi_driver_base.h's own
 * "OUT OF SCOPE" leads). Reconstructed 2026-07-28, found via a fresh pending-
 * manifest class-count sweep (same technique as OA.ko's CKGSeqBackupCommonParam/
 * CKGSeqBackupModuleParam batch, commit efa0926) that surfaced this as a dense,
 * previously-untouched 312-method cluster -- the single largest one left in the
 * storage cluster survey (bigger than CDDriverIO's 85 or CFilesys's 87).
 *
 * SCOPE OF THIS PASS: the 256-method Ext{X}toInt{Y} combinatorial matrix
 * (X,Y both 0000..000F) plus the one real diagonal implementation it bottoms
 * out on. Reconstructed by a scripted `objdump -dr -M intel` -> Python
 * instruction-pattern decoder (not hand-transcribed) that classified all 256
 * bodies by exact byte size (1/19/22/37, with ZERO anomalies against those four
 * templates across the whole matrix) and, for every 19/22-byte forwarding thunk,
 * mechanically resolved its embedded `mov eax,[edx+OFF]` vtable-slot read
 * against a direct `.rodata` dump of `vtable for CStorageConverterBase`
 * (0x08fcc9c0, vptr = symbol+8 per standard Itanium ABI) to confirm exactly
 * which sibling method it tail-calls. This produced a complete, INDEPENDENTLY
 * verified rule with zero exceptions across all 256 entries:
 *
 *   - (X=0000, Y=0000): the one real body -- unconditional
 *     `memcpy(dst, src, size)` using param's own fields. Int0000 is defined to
 *     be byte-identical to Ext0000, so any external format collapses to this
 *     one raw copy once you're targeting Int0000.
 *   - X > Y: a genuine tail-call (`jmp`, not `call`+`ret`) thunk to
 *     Ext{Y:04X}toInt{Y:04X} (the DIAGONAL entry for that same target Y) --
 *     confirmed for all 120 such thunks via the vtable-slot resolution above,
 *     not assumed from the X=2/X=3 samples that first suggested the pattern.
 *     The source version X is read from nowhere in the thunk body itself; only
 *     Y determines the target.
 *   - X <= Y (excluding the one real (0,0) case): a bare 1-byte `ret`, i.e. a
 *     genuine, unconditional no-op. This includes every diagonal
 *     Ext{Y}toInt{Y} for Y=0001..000F -- confirmed these are themselves empty,
 *     meaning EVERY combination that targets an internal version other than
 *     0000 is completely unimplemented in this real binary. 135 such stubs.
 *
 * NET EFFECT: in the real, current build, this entire 256-method matrix reduces
 * to exactly one live behavior -- "converting to Int0000 always does a raw
 * memcpy, regardless of source version" -- with every other (X,Y) combination
 * being live-but-inert dead weight (either a direct no-op or a chain of tail
 * calls that bottoms out in one). Verified independently, not asserted: see
 * verify/test_storage_converter_base.cpp, which computes expected behavior from
 * this Y==0 rule alone (an independent black-box "did the destination buffer
 * change" check), not from re-deriving the generator's own per-method
 * classification.
 *
 * NOT reconstructed this pass (real, documented leads for a future batch, same
 * "precisely documented rather than guessed" discipline as every other deferred
 * item in this project):
 *   int  CheckVersion(const CConvertStorageParam&) const;   // .text+0x08dea950, 46B -- version-compatibility predicate, reads param+0x8/+0xa/+0x14/+0x16
 *   bool ValidateExt(const CConvertStorageParam&) const;    // .text+0x08dea980, 389B
 *   void Save(const CConvertStorageParam&) const;           // .text+0x08de8f20, 320B
 *   void Load(const CConvertStorageParam&) const;           // .text+0x08dec670, 318B
 *   int  Open(const CConvertStorageParam&) const;            // .text+0x08deab30, 103B -- HAS real external callers (.text+0x08df76b9, 0x08df778d), calls ValidateExt; genuinely reachable, unlike the matrix above
 *   void Close();                                            // .text+0x08e07ba0, 1B -- NOW RECONSTRUCTED, see below
 *   void ExttoInt0000(...)..ExttoInt000F(...);               // .text+0x08deaba0..0x08dec4c0, 16 methods, 343-379B each -- real per-version conversion bodies, NOT the same symbols as the Ext{X}toInt{Y} matrix (no X digits in the name) and NOT called by anything in the matrix either; a second, parallel, apparently-independent real implementation family
 *   ValidateExt0000(...)..ValidateExt000F(...);              // .text+0x08e07bb0..0x08e07ca0, 16 methods, 3-14B each -- NOW RECONSTRUCTED, see below
 *   ctor/dtor (2 dtor overloads found; no plain ctor symbol in this export --
 *     likely elided/inline, or only ever constructed via a derived class not
 *     yet identified)
 * `Open()`'s two real external callers (0x08df76b9/0x08df778d) are now traced
 * (2026-07-28 follow-up batch): both are inside `CProgConverter::Open()`
 * (prog_converter.h/.cpp) -- CFilesys/CDiskUtil are NOT involved. `CProgConverter`
 * turned out to be the first of a whole discovered family of ~32 concrete
 * `CStorageConverterBase`-derived per-file-format converter classes (CProgConverter/
 * CCombiConverter/CSongConverter/CDrumKitConverter/CGEConverter/... --
 * storage_format_converters.h), confirming this cluster IS genuinely live, just not
 * through CFilesys directly.
 *
 * CStorageConverterBase is NOT declared with C++ `virtual`/a vtable-swap install
 * here, deliberately, even though ground truth's own vtable is real (294 slots,
 * `vtable for CStorageConverterBase` @ 0x08fcc9c0) -- per this project's own
 * recurring "vtable-dispatch-stub-gap" bug-class lesson (see
 * HARDWARE_REVIEW_LOG.md), declaring a fresh reconstruction `virtual` without an
 * accompanying ground-truth vtable install risks silently colliding with
 * whatever install convention a LATER batch picks for this class. UPDATE
 * (2026-07-28): `CProgConverter` now DOES hold a real `CStorageConverterBase*`
 * member (`m_pFormatConverter`, prog_converter.h) and ground truth genuinely
 * dispatches Load/Save/Close through it via its vtable (confirmed via a direct
 * `.rodata` dump of `vtable for CStorageConverterBase`: slot @ vptr+0xc = Load,
 * vptr+0x10 = Save, vptr+0x14 = Close, byte-exact against the addresses declared
 * here). Rather than retrofitting `virtual` onto this class (still a real collision
 * risk given the matrix's own 294-slot numbering, per the lesson above),
 * `CProgConverter`'s forwarding methods call these slots' current, only-known
 * target directly (a compile-time-resolved, non-virtual call) -- behavior-identical
 * to the real indirect call as long as no currently-reconstructed sibling class
 * overrides Load/Save/Close (none does; every sibling found this batch only
 * overrides Open/ValidateExtXXXX/the Ext{X}toInt{Y} matrix). Flagged explicitly in
 * prog_converter.h; revisit together if a future batch finds a real override.
 *
 * CConvertStorageParam: extended 2026-07-28 with 3 more real confirmed fields
 * (`m_extFormatId` +0x10, `m_skipValidate` +0x19, `m_variantFlag` +0x1a -- see the
 * struct's own per-field comments for how each was confirmed, including a genuine
 * unresolved dual-use finding on `m_size` +0x0c). Struct now sized to 0x20
 * (highest confirmed-live offset +0x1a, rounded up) -- same "declare uncertain
 * fields clearly" convention as scsi_driver_base.h's SDriverIOPbuf.
 */

#ifndef STORAGE_CONVERTER_BASE_H
#define STORAGE_CONVERTER_BASE_H

#include <cstddef>

struct CConvertStorageParam {
	void       *m_internalBuf;     // +0x00, confirmed: memcpy dst in Ext0000toInt0000
	const void *m_externalBuf;     // +0x04, confirmed: memcpy src in Ext0000toInt0000
	unsigned short m_internalVersion; // +0x08, confirmed: CheckVersion's `cx` compare target (not used this pass)
	unsigned char  m_unknown_0a;      // +0x0a, confirmed touched by CheckVersion, meaning not recovered
	unsigned char  m_pad_0b;          // +0x0b, unconfirmed padding
	unsigned long  m_size;            // +0x0c, DUAL-USE (2026-07-28 finding): CStorageConverterBase's own
	                                   // Ext0000toInt0000 reads this as a plain memcpy byte count (confirmed,
	                                   // unchanged from before), but EVERY concrete-subclass ValidateExtXXXX
	                                   // in CPCMProgConverter/CMOSSProgConverter instead dereferences it as a
	                                   // `void*` to a large (~0xa00+ byte) owning/session context object and
	                                   // reads a 3-bit mode flag at that object's own +0x9f2/+0x9fe. Both uses
	                                   // are independently confirmed via direct disassembly; NOT reconciled --
	                                   // most likely this field is genuinely reused for different purposes by
	                                   // different converter subclasses/call sites (the base class's own
	                                   // Ext0000toInt0000 is itself confirmed dead code on the real boot path,
	                                   // so its "size" reading never has to coexist with a live "pointer"
	                                   // reading in practice). Left as unsigned long; the pointer-shaped uses
	                                   // reinterpret_cast it locally rather than retyping this field.
	unsigned long  m_extFormatId;     // +0x10, confirmed 2026-07-28: the format/subtype magic number checked
	                                   // by essentially every concrete converter subclass's ValidateExtXXXX()
	                                   // (48 methods across ~20 classes, storage_format_converters.h) -- was
	                                   // previously `m_reserved_10[4]`/"unconfirmed, never touched"; that was
	                                   // simply because nothing reconstructed yet read it.
	unsigned short m_externalVersion; // +0x14, confirmed: CheckVersion's own version field
	unsigned char  m_unknown_16;      // +0x16, confirmed touched by CheckVersion, meaning not recovered
	unsigned char  m_pad_17;          // +0x17, unconfirmed padding
	unsigned char  m_skipValidate;    // +0x19, confirmed 2026-07-28: CStorageConverterBase::Open() (still
	                                   // deferred) returns success immediately without calling ValidateExt()
	                                   // when this is non-zero; also written by CProgConverter::Open() (also
	                                   // deferred) when building its own persistent param copy.
	unsigned char  m_variantFlag;     // +0x1a, confirmed 2026-07-28: a 0/1 selector read by several concrete
	                                   // converters' ValidateExtXXXX() to pick between two candidate magic
	                                   // values/branches (CCombiConverter, CGEConverter, CPCMProgConverter,
	                                   // CMOSSProgConverter) -- real per-class meaning (platform? byte order?
	                                   // old-vs-new file revision?) not recovered.
	unsigned char  m_pad_1b[5];       // +0x1b..0x1f, unconfirmed padding, rounds struct to 0x20; highest
	                                   // confirmed-live offset remains +0x1a (CProgConverter::Open() itself
	                                   // writes up to here into its own copy, still deferred).
};

class CStorageConverterBase {
public:
	// -- target Int0000 --
	void Ext0000toInt0000(const CConvertStorageParam &param) const;  // REAL identity copy, .text+0x08dea8f0, 37B
	void Ext0001toInt0000(const CConvertStorageParam &param) const;  // thunk -> Ext0000toInt0000, .text+0x08de9180, 19B
	void Ext0002toInt0000(const CConvertStorageParam &param) const;  // thunk -> Ext0000toInt0000, .text+0x08de91a0, 19B
	void Ext0003toInt0000(const CConvertStorageParam &param) const;  // thunk -> Ext0000toInt0000, .text+0x08de91c0, 19B
	void Ext0004toInt0000(const CConvertStorageParam &param) const;  // thunk -> Ext0000toInt0000, .text+0x08de91e0, 19B
	void Ext0005toInt0000(const CConvertStorageParam &param) const;  // thunk -> Ext0000toInt0000, .text+0x08de9200, 19B
	void Ext0006toInt0000(const CConvertStorageParam &param) const;  // thunk -> Ext0000toInt0000, .text+0x08de9220, 19B
	void Ext0007toInt0000(const CConvertStorageParam &param) const;  // thunk -> Ext0000toInt0000, .text+0x08de9240, 19B
	void Ext0008toInt0000(const CConvertStorageParam &param) const;  // thunk -> Ext0000toInt0000, .text+0x08de9260, 19B
	void Ext0009toInt0000(const CConvertStorageParam &param) const;  // thunk -> Ext0000toInt0000, .text+0x08de9280, 19B
	void Ext000AtoInt0000(const CConvertStorageParam &param) const;  // thunk -> Ext0000toInt0000, .text+0x08de92a0, 19B
	void Ext000BtoInt0000(const CConvertStorageParam &param) const;  // thunk -> Ext0000toInt0000, .text+0x08de92c0, 19B
	void Ext000CtoInt0000(const CConvertStorageParam &param) const;  // thunk -> Ext0000toInt0000, .text+0x08de92e0, 19B
	void Ext000DtoInt0000(const CConvertStorageParam &param) const;  // thunk -> Ext0000toInt0000, .text+0x08de9300, 19B
	void Ext000EtoInt0000(const CConvertStorageParam &param) const;  // thunk -> Ext0000toInt0000, .text+0x08de9320, 19B
	void Ext000FtoInt0000(const CConvertStorageParam &param) const;  // thunk -> Ext0000toInt0000, .text+0x08de9340, 19B
	// -- target Int0001 --
	void Ext0000toInt0001(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9360, 1B
	void Ext0001toInt0001(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9370, 1B
	void Ext0002toInt0001(const CConvertStorageParam &param) const;  // thunk -> Ext0001toInt0001, .text+0x08de9380, 22B
	void Ext0003toInt0001(const CConvertStorageParam &param) const;  // thunk -> Ext0001toInt0001, .text+0x08de93a0, 22B
	void Ext0004toInt0001(const CConvertStorageParam &param) const;  // thunk -> Ext0001toInt0001, .text+0x08de93c0, 22B
	void Ext0005toInt0001(const CConvertStorageParam &param) const;  // thunk -> Ext0001toInt0001, .text+0x08de93e0, 22B
	void Ext0006toInt0001(const CConvertStorageParam &param) const;  // thunk -> Ext0001toInt0001, .text+0x08de9400, 22B
	void Ext0007toInt0001(const CConvertStorageParam &param) const;  // thunk -> Ext0001toInt0001, .text+0x08de9420, 22B
	void Ext0008toInt0001(const CConvertStorageParam &param) const;  // thunk -> Ext0001toInt0001, .text+0x08de9440, 22B
	void Ext0009toInt0001(const CConvertStorageParam &param) const;  // thunk -> Ext0001toInt0001, .text+0x08de9460, 22B
	void Ext000AtoInt0001(const CConvertStorageParam &param) const;  // thunk -> Ext0001toInt0001, .text+0x08de9480, 22B
	void Ext000BtoInt0001(const CConvertStorageParam &param) const;  // thunk -> Ext0001toInt0001, .text+0x08de94a0, 22B
	void Ext000CtoInt0001(const CConvertStorageParam &param) const;  // thunk -> Ext0001toInt0001, .text+0x08de94c0, 22B
	void Ext000DtoInt0001(const CConvertStorageParam &param) const;  // thunk -> Ext0001toInt0001, .text+0x08de94e0, 22B
	void Ext000EtoInt0001(const CConvertStorageParam &param) const;  // thunk -> Ext0001toInt0001, .text+0x08de9500, 22B
	void Ext000FtoInt0001(const CConvertStorageParam &param) const;  // thunk -> Ext0001toInt0001, .text+0x08de9520, 22B
	// -- target Int0002 --
	void Ext0000toInt0002(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9540, 1B
	void Ext0001toInt0002(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9550, 1B
	void Ext0002toInt0002(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9560, 1B
	void Ext0003toInt0002(const CConvertStorageParam &param) const;  // thunk -> Ext0002toInt0002, .text+0x08de9570, 22B
	void Ext0004toInt0002(const CConvertStorageParam &param) const;  // thunk -> Ext0002toInt0002, .text+0x08de9590, 22B
	void Ext0005toInt0002(const CConvertStorageParam &param) const;  // thunk -> Ext0002toInt0002, .text+0x08de95b0, 22B
	void Ext0006toInt0002(const CConvertStorageParam &param) const;  // thunk -> Ext0002toInt0002, .text+0x08de95d0, 22B
	void Ext0007toInt0002(const CConvertStorageParam &param) const;  // thunk -> Ext0002toInt0002, .text+0x08de95f0, 22B
	void Ext0008toInt0002(const CConvertStorageParam &param) const;  // thunk -> Ext0002toInt0002, .text+0x08de9610, 22B
	void Ext0009toInt0002(const CConvertStorageParam &param) const;  // thunk -> Ext0002toInt0002, .text+0x08de9630, 22B
	void Ext000AtoInt0002(const CConvertStorageParam &param) const;  // thunk -> Ext0002toInt0002, .text+0x08de9650, 22B
	void Ext000BtoInt0002(const CConvertStorageParam &param) const;  // thunk -> Ext0002toInt0002, .text+0x08de9670, 22B
	void Ext000CtoInt0002(const CConvertStorageParam &param) const;  // thunk -> Ext0002toInt0002, .text+0x08de9690, 22B
	void Ext000DtoInt0002(const CConvertStorageParam &param) const;  // thunk -> Ext0002toInt0002, .text+0x08de96b0, 22B
	void Ext000EtoInt0002(const CConvertStorageParam &param) const;  // thunk -> Ext0002toInt0002, .text+0x08de96d0, 22B
	void Ext000FtoInt0002(const CConvertStorageParam &param) const;  // thunk -> Ext0002toInt0002, .text+0x08de96f0, 22B
	// -- target Int0003 --
	void Ext0000toInt0003(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9710, 1B
	void Ext0001toInt0003(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9720, 1B
	void Ext0002toInt0003(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9730, 1B
	void Ext0003toInt0003(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9740, 1B
	void Ext0004toInt0003(const CConvertStorageParam &param) const;  // thunk -> Ext0003toInt0003, .text+0x08de9750, 22B
	void Ext0005toInt0003(const CConvertStorageParam &param) const;  // thunk -> Ext0003toInt0003, .text+0x08de9770, 22B
	void Ext0006toInt0003(const CConvertStorageParam &param) const;  // thunk -> Ext0003toInt0003, .text+0x08de9790, 22B
	void Ext0007toInt0003(const CConvertStorageParam &param) const;  // thunk -> Ext0003toInt0003, .text+0x08de97b0, 22B
	void Ext0008toInt0003(const CConvertStorageParam &param) const;  // thunk -> Ext0003toInt0003, .text+0x08de97d0, 22B
	void Ext0009toInt0003(const CConvertStorageParam &param) const;  // thunk -> Ext0003toInt0003, .text+0x08de97f0, 22B
	void Ext000AtoInt0003(const CConvertStorageParam &param) const;  // thunk -> Ext0003toInt0003, .text+0x08de9810, 22B
	void Ext000BtoInt0003(const CConvertStorageParam &param) const;  // thunk -> Ext0003toInt0003, .text+0x08de9830, 22B
	void Ext000CtoInt0003(const CConvertStorageParam &param) const;  // thunk -> Ext0003toInt0003, .text+0x08de9850, 22B
	void Ext000DtoInt0003(const CConvertStorageParam &param) const;  // thunk -> Ext0003toInt0003, .text+0x08de9870, 22B
	void Ext000EtoInt0003(const CConvertStorageParam &param) const;  // thunk -> Ext0003toInt0003, .text+0x08de9890, 22B
	void Ext000FtoInt0003(const CConvertStorageParam &param) const;  // thunk -> Ext0003toInt0003, .text+0x08de98b0, 22B
	// -- target Int0004 --
	void Ext0000toInt0004(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de98d0, 1B
	void Ext0001toInt0004(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de98e0, 1B
	void Ext0002toInt0004(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de98f0, 1B
	void Ext0003toInt0004(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9900, 1B
	void Ext0004toInt0004(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9910, 1B
	void Ext0005toInt0004(const CConvertStorageParam &param) const;  // thunk -> Ext0004toInt0004, .text+0x08de9920, 22B
	void Ext0006toInt0004(const CConvertStorageParam &param) const;  // thunk -> Ext0004toInt0004, .text+0x08de9940, 22B
	void Ext0007toInt0004(const CConvertStorageParam &param) const;  // thunk -> Ext0004toInt0004, .text+0x08de9960, 22B
	void Ext0008toInt0004(const CConvertStorageParam &param) const;  // thunk -> Ext0004toInt0004, .text+0x08de9980, 22B
	void Ext0009toInt0004(const CConvertStorageParam &param) const;  // thunk -> Ext0004toInt0004, .text+0x08de99a0, 22B
	void Ext000AtoInt0004(const CConvertStorageParam &param) const;  // thunk -> Ext0004toInt0004, .text+0x08de99c0, 22B
	void Ext000BtoInt0004(const CConvertStorageParam &param) const;  // thunk -> Ext0004toInt0004, .text+0x08de99e0, 22B
	void Ext000CtoInt0004(const CConvertStorageParam &param) const;  // thunk -> Ext0004toInt0004, .text+0x08de9a00, 22B
	void Ext000DtoInt0004(const CConvertStorageParam &param) const;  // thunk -> Ext0004toInt0004, .text+0x08de9a20, 22B
	void Ext000EtoInt0004(const CConvertStorageParam &param) const;  // thunk -> Ext0004toInt0004, .text+0x08de9a40, 22B
	void Ext000FtoInt0004(const CConvertStorageParam &param) const;  // thunk -> Ext0004toInt0004, .text+0x08de9a60, 22B
	// -- target Int0005 --
	void Ext0000toInt0005(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9a80, 1B
	void Ext0001toInt0005(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9a90, 1B
	void Ext0002toInt0005(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9aa0, 1B
	void Ext0003toInt0005(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9ab0, 1B
	void Ext0004toInt0005(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9ac0, 1B
	void Ext0005toInt0005(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9ad0, 1B
	void Ext0006toInt0005(const CConvertStorageParam &param) const;  // thunk -> Ext0005toInt0005, .text+0x08de9ae0, 22B
	void Ext0007toInt0005(const CConvertStorageParam &param) const;  // thunk -> Ext0005toInt0005, .text+0x08de9b00, 22B
	void Ext0008toInt0005(const CConvertStorageParam &param) const;  // thunk -> Ext0005toInt0005, .text+0x08de9b20, 22B
	void Ext0009toInt0005(const CConvertStorageParam &param) const;  // thunk -> Ext0005toInt0005, .text+0x08de9b40, 22B
	void Ext000AtoInt0005(const CConvertStorageParam &param) const;  // thunk -> Ext0005toInt0005, .text+0x08de9b60, 22B
	void Ext000BtoInt0005(const CConvertStorageParam &param) const;  // thunk -> Ext0005toInt0005, .text+0x08de9b80, 22B
	void Ext000CtoInt0005(const CConvertStorageParam &param) const;  // thunk -> Ext0005toInt0005, .text+0x08de9ba0, 22B
	void Ext000DtoInt0005(const CConvertStorageParam &param) const;  // thunk -> Ext0005toInt0005, .text+0x08de9bc0, 22B
	void Ext000EtoInt0005(const CConvertStorageParam &param) const;  // thunk -> Ext0005toInt0005, .text+0x08de9be0, 22B
	void Ext000FtoInt0005(const CConvertStorageParam &param) const;  // thunk -> Ext0005toInt0005, .text+0x08de9c00, 22B
	// -- target Int0006 --
	void Ext0000toInt0006(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9c20, 1B
	void Ext0001toInt0006(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9c30, 1B
	void Ext0002toInt0006(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9c40, 1B
	void Ext0003toInt0006(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9c50, 1B
	void Ext0004toInt0006(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9c60, 1B
	void Ext0005toInt0006(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9c70, 1B
	void Ext0006toInt0006(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9c80, 1B
	void Ext0007toInt0006(const CConvertStorageParam &param) const;  // thunk -> Ext0006toInt0006, .text+0x08de9c90, 22B
	void Ext0008toInt0006(const CConvertStorageParam &param) const;  // thunk -> Ext0006toInt0006, .text+0x08de9cb0, 22B
	void Ext0009toInt0006(const CConvertStorageParam &param) const;  // thunk -> Ext0006toInt0006, .text+0x08de9cd0, 22B
	void Ext000AtoInt0006(const CConvertStorageParam &param) const;  // thunk -> Ext0006toInt0006, .text+0x08de9cf0, 22B
	void Ext000BtoInt0006(const CConvertStorageParam &param) const;  // thunk -> Ext0006toInt0006, .text+0x08de9d10, 22B
	void Ext000CtoInt0006(const CConvertStorageParam &param) const;  // thunk -> Ext0006toInt0006, .text+0x08de9d30, 22B
	void Ext000DtoInt0006(const CConvertStorageParam &param) const;  // thunk -> Ext0006toInt0006, .text+0x08de9d50, 22B
	void Ext000EtoInt0006(const CConvertStorageParam &param) const;  // thunk -> Ext0006toInt0006, .text+0x08de9d70, 22B
	void Ext000FtoInt0006(const CConvertStorageParam &param) const;  // thunk -> Ext0006toInt0006, .text+0x08de9d90, 22B
	// -- target Int0007 --
	void Ext0000toInt0007(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9db0, 1B
	void Ext0001toInt0007(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9dc0, 1B
	void Ext0002toInt0007(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9dd0, 1B
	void Ext0003toInt0007(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9de0, 1B
	void Ext0004toInt0007(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9df0, 1B
	void Ext0005toInt0007(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9e00, 1B
	void Ext0006toInt0007(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9e10, 1B
	void Ext0007toInt0007(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9e20, 1B
	void Ext0008toInt0007(const CConvertStorageParam &param) const;  // thunk -> Ext0007toInt0007, .text+0x08de9e30, 22B
	void Ext0009toInt0007(const CConvertStorageParam &param) const;  // thunk -> Ext0007toInt0007, .text+0x08de9e50, 22B
	void Ext000AtoInt0007(const CConvertStorageParam &param) const;  // thunk -> Ext0007toInt0007, .text+0x08de9e70, 22B
	void Ext000BtoInt0007(const CConvertStorageParam &param) const;  // thunk -> Ext0007toInt0007, .text+0x08de9e90, 22B
	void Ext000CtoInt0007(const CConvertStorageParam &param) const;  // thunk -> Ext0007toInt0007, .text+0x08de9eb0, 22B
	void Ext000DtoInt0007(const CConvertStorageParam &param) const;  // thunk -> Ext0007toInt0007, .text+0x08de9ed0, 22B
	void Ext000EtoInt0007(const CConvertStorageParam &param) const;  // thunk -> Ext0007toInt0007, .text+0x08de9ef0, 22B
	void Ext000FtoInt0007(const CConvertStorageParam &param) const;  // thunk -> Ext0007toInt0007, .text+0x08de9f10, 22B
	// -- target Int0008 --
	void Ext0000toInt0008(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9f30, 1B
	void Ext0001toInt0008(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9f40, 1B
	void Ext0002toInt0008(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9f50, 1B
	void Ext0003toInt0008(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9f60, 1B
	void Ext0004toInt0008(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9f70, 1B
	void Ext0005toInt0008(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9f80, 1B
	void Ext0006toInt0008(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9f90, 1B
	void Ext0007toInt0008(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9fa0, 1B
	void Ext0008toInt0008(const CConvertStorageParam &) const;  // no-op stub, .text+0x08de9fb0, 1B
	void Ext0009toInt0008(const CConvertStorageParam &param) const;  // thunk -> Ext0008toInt0008, .text+0x08de9fc0, 22B
	void Ext000AtoInt0008(const CConvertStorageParam &param) const;  // thunk -> Ext0008toInt0008, .text+0x08de9fe0, 22B
	void Ext000BtoInt0008(const CConvertStorageParam &param) const;  // thunk -> Ext0008toInt0008, .text+0x08dea000, 22B
	void Ext000CtoInt0008(const CConvertStorageParam &param) const;  // thunk -> Ext0008toInt0008, .text+0x08dea020, 22B
	void Ext000DtoInt0008(const CConvertStorageParam &param) const;  // thunk -> Ext0008toInt0008, .text+0x08dea040, 22B
	void Ext000EtoInt0008(const CConvertStorageParam &param) const;  // thunk -> Ext0008toInt0008, .text+0x08dea060, 22B
	void Ext000FtoInt0008(const CConvertStorageParam &param) const;  // thunk -> Ext0008toInt0008, .text+0x08dea080, 22B
	// -- target Int0009 --
	void Ext0000toInt0009(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea0a0, 1B
	void Ext0001toInt0009(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea0b0, 1B
	void Ext0002toInt0009(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea0c0, 1B
	void Ext0003toInt0009(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea0d0, 1B
	void Ext0004toInt0009(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea0e0, 1B
	void Ext0005toInt0009(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea0f0, 1B
	void Ext0006toInt0009(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea100, 1B
	void Ext0007toInt0009(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea110, 1B
	void Ext0008toInt0009(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea120, 1B
	void Ext0009toInt0009(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea130, 1B
	void Ext000AtoInt0009(const CConvertStorageParam &param) const;  // thunk -> Ext0009toInt0009, .text+0x08dea140, 22B
	void Ext000BtoInt0009(const CConvertStorageParam &param) const;  // thunk -> Ext0009toInt0009, .text+0x08dea160, 22B
	void Ext000CtoInt0009(const CConvertStorageParam &param) const;  // thunk -> Ext0009toInt0009, .text+0x08dea180, 22B
	void Ext000DtoInt0009(const CConvertStorageParam &param) const;  // thunk -> Ext0009toInt0009, .text+0x08dea1a0, 22B
	void Ext000EtoInt0009(const CConvertStorageParam &param) const;  // thunk -> Ext0009toInt0009, .text+0x08dea1c0, 22B
	void Ext000FtoInt0009(const CConvertStorageParam &param) const;  // thunk -> Ext0009toInt0009, .text+0x08dea1e0, 22B
	// -- target Int000A --
	void Ext0000toInt000A(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea200, 1B
	void Ext0001toInt000A(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea210, 1B
	void Ext0002toInt000A(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea220, 1B
	void Ext0003toInt000A(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea230, 1B
	void Ext0004toInt000A(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea240, 1B
	void Ext0005toInt000A(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea250, 1B
	void Ext0006toInt000A(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea260, 1B
	void Ext0007toInt000A(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea270, 1B
	void Ext0008toInt000A(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea280, 1B
	void Ext0009toInt000A(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea290, 1B
	void Ext000AtoInt000A(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea2a0, 1B
	void Ext000BtoInt000A(const CConvertStorageParam &param) const;  // thunk -> Ext000AtoInt000A, .text+0x08dea2b0, 22B
	void Ext000CtoInt000A(const CConvertStorageParam &param) const;  // thunk -> Ext000AtoInt000A, .text+0x08dea2d0, 22B
	void Ext000DtoInt000A(const CConvertStorageParam &param) const;  // thunk -> Ext000AtoInt000A, .text+0x08dea2f0, 22B
	void Ext000EtoInt000A(const CConvertStorageParam &param) const;  // thunk -> Ext000AtoInt000A, .text+0x08dea310, 22B
	void Ext000FtoInt000A(const CConvertStorageParam &param) const;  // thunk -> Ext000AtoInt000A, .text+0x08dea330, 22B
	// -- target Int000B --
	void Ext0000toInt000B(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea350, 1B
	void Ext0001toInt000B(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea360, 1B
	void Ext0002toInt000B(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea370, 1B
	void Ext0003toInt000B(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea380, 1B
	void Ext0004toInt000B(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea390, 1B
	void Ext0005toInt000B(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea3a0, 1B
	void Ext0006toInt000B(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea3b0, 1B
	void Ext0007toInt000B(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea3c0, 1B
	void Ext0008toInt000B(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea3d0, 1B
	void Ext0009toInt000B(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea3e0, 1B
	void Ext000AtoInt000B(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea3f0, 1B
	void Ext000BtoInt000B(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea400, 1B
	void Ext000CtoInt000B(const CConvertStorageParam &param) const;  // thunk -> Ext000BtoInt000B, .text+0x08dea410, 22B
	void Ext000DtoInt000B(const CConvertStorageParam &param) const;  // thunk -> Ext000BtoInt000B, .text+0x08dea430, 22B
	void Ext000EtoInt000B(const CConvertStorageParam &param) const;  // thunk -> Ext000BtoInt000B, .text+0x08dea450, 22B
	void Ext000FtoInt000B(const CConvertStorageParam &param) const;  // thunk -> Ext000BtoInt000B, .text+0x08dea470, 22B
	// -- target Int000C --
	void Ext0000toInt000C(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea490, 1B
	void Ext0001toInt000C(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea4a0, 1B
	void Ext0002toInt000C(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea4b0, 1B
	void Ext0003toInt000C(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea4c0, 1B
	void Ext0004toInt000C(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea4d0, 1B
	void Ext0005toInt000C(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea4e0, 1B
	void Ext0006toInt000C(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea4f0, 1B
	void Ext0007toInt000C(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea500, 1B
	void Ext0008toInt000C(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea510, 1B
	void Ext0009toInt000C(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea520, 1B
	void Ext000AtoInt000C(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea530, 1B
	void Ext000BtoInt000C(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea540, 1B
	void Ext000CtoInt000C(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea550, 1B
	void Ext000DtoInt000C(const CConvertStorageParam &param) const;  // thunk -> Ext000CtoInt000C, .text+0x08dea560, 22B
	void Ext000EtoInt000C(const CConvertStorageParam &param) const;  // thunk -> Ext000CtoInt000C, .text+0x08dea580, 22B
	void Ext000FtoInt000C(const CConvertStorageParam &param) const;  // thunk -> Ext000CtoInt000C, .text+0x08dea5a0, 22B
	// -- target Int000D --
	void Ext0000toInt000D(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea5c0, 1B
	void Ext0001toInt000D(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea5d0, 1B
	void Ext0002toInt000D(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea5e0, 1B
	void Ext0003toInt000D(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea5f0, 1B
	void Ext0004toInt000D(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea600, 1B
	void Ext0005toInt000D(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea610, 1B
	void Ext0006toInt000D(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea620, 1B
	void Ext0007toInt000D(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea630, 1B
	void Ext0008toInt000D(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea640, 1B
	void Ext0009toInt000D(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea650, 1B
	void Ext000AtoInt000D(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea660, 1B
	void Ext000BtoInt000D(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea670, 1B
	void Ext000CtoInt000D(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea680, 1B
	void Ext000DtoInt000D(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea690, 1B
	void Ext000EtoInt000D(const CConvertStorageParam &param) const;  // thunk -> Ext000DtoInt000D, .text+0x08dea6a0, 22B
	void Ext000FtoInt000D(const CConvertStorageParam &param) const;  // thunk -> Ext000DtoInt000D, .text+0x08dea6c0, 22B
	// -- target Int000E --
	void Ext0000toInt000E(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea6e0, 1B
	void Ext0001toInt000E(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea6f0, 1B
	void Ext0002toInt000E(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea700, 1B
	void Ext0003toInt000E(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea710, 1B
	void Ext0004toInt000E(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea720, 1B
	void Ext0005toInt000E(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea730, 1B
	void Ext0006toInt000E(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea740, 1B
	void Ext0007toInt000E(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea750, 1B
	void Ext0008toInt000E(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea760, 1B
	void Ext0009toInt000E(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea770, 1B
	void Ext000AtoInt000E(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea780, 1B
	void Ext000BtoInt000E(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea790, 1B
	void Ext000CtoInt000E(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea7a0, 1B
	void Ext000DtoInt000E(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea7b0, 1B
	void Ext000EtoInt000E(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea7c0, 1B
	void Ext000FtoInt000E(const CConvertStorageParam &param) const;  // thunk -> Ext000EtoInt000E, .text+0x08dea7d0, 22B
	// -- target Int000F --
	void Ext0000toInt000F(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea7f0, 1B
	void Ext0001toInt000F(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea800, 1B
	void Ext0002toInt000F(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea810, 1B
	void Ext0003toInt000F(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea820, 1B
	void Ext0004toInt000F(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea830, 1B
	void Ext0005toInt000F(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea840, 1B
	void Ext0006toInt000F(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea850, 1B
	void Ext0007toInt000F(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea860, 1B
	void Ext0008toInt000F(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea870, 1B
	void Ext0009toInt000F(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea880, 1B
	void Ext000AtoInt000F(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea890, 1B
	void Ext000BtoInt000F(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea8a0, 1B
	void Ext000CtoInt000F(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea8b0, 1B
	void Ext000DtoInt000F(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea8c0, 1B
	void Ext000EtoInt000F(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea8d0, 1B
	void Ext000FtoInt000F(const CConvertStorageParam &) const;  // no-op stub, .text+0x08dea8e0, 1B

	// -- 2026-07-28 follow-up batch: ValidateExtXXXX + Close(), see header comment --
	bool ValidateExt0000(const CConvertStorageParam &param) const;  // .text+0x08e07bb0, 14B
	bool ValidateExt0001(const CConvertStorageParam &) const;  // .text+0x08e07bc0, 3B, always false
	bool ValidateExt0002(const CConvertStorageParam &) const;  // .text+0x08e07bd0, 3B, always false
	bool ValidateExt0003(const CConvertStorageParam &) const;  // .text+0x08e07be0, 3B, always false
	bool ValidateExt0004(const CConvertStorageParam &) const;  // .text+0x08e07bf0, 3B, always false
	bool ValidateExt0005(const CConvertStorageParam &) const;  // .text+0x08e07c00, 3B, always false
	bool ValidateExt0006(const CConvertStorageParam &) const;  // .text+0x08e07c10, 3B, always false
	bool ValidateExt0007(const CConvertStorageParam &) const;  // .text+0x08e07c20, 3B, always false
	bool ValidateExt0008(const CConvertStorageParam &) const;  // .text+0x08e07c30, 3B, always false
	bool ValidateExt0009(const CConvertStorageParam &) const;  // .text+0x08e07c40, 3B, always false
	bool ValidateExt000A(const CConvertStorageParam &) const;  // .text+0x08e07c50, 3B, always false
	bool ValidateExt000B(const CConvertStorageParam &) const;  // .text+0x08e07c60, 3B, always false
	bool ValidateExt000C(const CConvertStorageParam &) const;  // .text+0x08e07c70, 3B, always false
	bool ValidateExt000D(const CConvertStorageParam &) const;  // .text+0x08e07c80, 3B, always false
	bool ValidateExt000E(const CConvertStorageParam &) const;  // .text+0x08e07c90, 3B, always false
	bool ValidateExt000F(const CConvertStorageParam &) const;  // .text+0x08e07ca0, 3B, always false
	void Close();  // .text+0x08e07ba0, 1B -- real no-op body (ground truth: bare `ret`); kept as its own
	               // method (not folded into a no-op inline) since CProgConverter::Close() (prog_converter.h)
	               // genuinely calls through this exact vtable slot on whatever concrete object its own
	               // m_pFormatConverter member points at.
};

#endif // STORAGE_CONVERTER_BASE_H
