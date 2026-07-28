/*
 * prog_converter.h  -  CProgConverter, the Program (single-patch) file-format
 * version converter. Found 2026-07-28 while tracing
 * CStorageConverterBase::Open()'s 2 real external callers
 * (.text+0x08df76b9/0x08df778d, storage_converter_base.h) -- both are inside
 * this class's own Open(), confirming the whole storage-converter cluster IS
 * genuinely reachable on a real path, just not through CFilesys as originally
 * guessed in the earlier batch's own lead note.
 *
 * REAL SHAPE: 6 nm -C symbols, .text+0x08df7510-0x08e07de4: Open() (766B),
 * Load()/Save() (50B each), Close() (37B), D1/D0 dtor (25B/74B). No plain ctor
 * symbol found (same "likely elided/inline, or constructed via a derived class
 * not yet identified" situation as CStorageConverterBase itself). THIS PASS
 * reconstructs the dtor pair + Close() (3 of 6); Open() and Load()/Save() are
 * deferred (see their own declarations below for exactly why -- Open() is
 * simply large/intricate, Load()/Save() are fully understood but their real
 * target is itself a dispatcher into the still-unreconstructed ExttoIntXXXX
 * family).
 *
 * VTABLE: `vtable for CProgConverter` @ 0x08fcd820, 296 slots (1184 bytes),
 * confirmed via a direct .rodata dump: slot0/1 = this class's own D1/D0 dtor,
 * slot2 = Open (OVERRIDDEN), slot3 = Load (OVERRIDDEN), slot4 = Save
 * (OVERRIDDEN), slot5 = Close (OVERRIDDEN), slot6+ are byte-identical to
 * `vtable for CStorageConverterBase`'s own slot6+ (i.e. every slot beyond
 * dtor/Open/Load/Save/Close is plain inherited, unmodified) -- CProgConverter
 * genuinely publicly, singly inherits CStorageConverterBase in ground truth.
 *
 * THIS RECONSTRUCTION DOES NOT MODEL THAT INHERITANCE IN C++ (deliberately):
 * CStorageConverterBase is declared non-virtual/stateless in this project's
 * C++ model (storage_converter_base.h's own note), so it contributes 0 bytes
 * to a derived class's layout -- but ground truth's real object layout
 * clearly needs real bytes at +0x00/+0x04/+0x08 (see D1/D0 below, and Open()'s
 * own use of `this+4` as a raw `CStorageConverterBase::Open()` call target).
 * Modeling this via `class CProgConverter : public CStorageConverterBase`
 * would silently shift every member below by less than ground truth actually
 * needs. Declared instead as a plain standalone class with explicit raw
 * offset fields, same discipline as scsi_driver_base.h's own member layout
 * (derived from ctor + cross-checked against every field-writing method).
 *
 * MEMBER LAYOUT (confirmed from D1/D0's own writes + Open()/Load()/Save()/
 * Close()'s own field reads):
 *   +0x00  m_vptr0    -- this class's own vtable pointer slot. D1/D0 both
 *          reset it to `&vtable_for_CStorageConverterBase + 8` (0x08fcc9c8),
 *          NOT this class's own vtable (0x08fcd828) -- see the note on D1/D0
 *          below for why this is reproduced literally rather than "corrected".
 *   +0x04  m_base4    -- reset by D1/D0 alongside m_vptr0 (same value). Also
 *          used by the still-deferred Open() as the raw `this` pointer for a
 *          NON-virtual `call CStorageConverterBase::Open()` (`lea ebp,[esi+4]`
 *          then `call` -- not an indirect/vtable call) -- i.e. ground truth
 *          treats +0x04 as the start of an embedded, non-polymorphically-used
 *          CStorageConverterBase subobject distinct from this class's own
 *          primary vtable slot at +0x00.
 *   +0x08  m_base8    -- reset by D1/D0 alongside the two above; no other
 *          confirmed use found this pass.
 *   +0x0c  m_pFormatConverter -- real, CONFIRMED: a `CStorageConverterBase*`
 *          to a dynamically-selected concrete sub-converter (almost certainly
 *          a CPCMProgConverter or CMOSSProgConverter instance, chosen by the
 *          still-deferred Open() based on a session-context mode-flag byte --
 *          see storage_format_converters.h's note on those 2 classes). Load(),
 *          Save(), and Close() all null-check this pointer and, if non-null,
 *          dispatch through ITS OWN vtable at the exact slot offsets
 *          confirmed against `vtable for CStorageConverterBase`'s own .rodata
 *          layout (+0xc=Load, +0x10=Save, +0x14=Close) -- reproduced here as
 *          direct (non-virtual) calls to CStorageConverterBase's own Load/
 *          Save/Close, since no currently-reconstructed sibling class
 *          overrides any of the three (storage_format_converters.h's own
 *          classes only override Open/ValidateExtXXXX/the Ext{X}toInt{Y}
 *          matrix) -- behavior-identical today, flagged for revisit if that
 *          ever changes. Close() additionally nulls this member out after
 *          forwarding (confirmed: `mov DWORD PTR [ebx+0xc],0x0`), Load()/
 *          Save() do not.
 *   +0x10  m_storedParam -- a full embedded CConvertStorageParam. GENUINE,
 *          CONFIRMED finding: Load()/Save() do NOT forward the caller's own
 *          `param` argument to m_pFormatConverter -- they discard it and pass
 *          `&this->m_storedParam` instead (`lea edx,[edx+0x10]` overwrites
 *          the register that held the incoming `this`, not the incoming
 *          `&param`). This member is populated by the still-deferred Open()
 *          (which builds a normalized copy of its own caller's param here,
 *          with per-field variant-flag logic mirroring the sibling
 *          ValidateExtXXXX family in storage_format_converters.h) -- until a
 *          future batch reconstructs Open(), this member stays whatever the
 *          caller left it as (zero-initialized by nothing in particular;
 *          Load()/Save() are still faithfully reconstructed, they just have
 *          no real data to act on yet in this project's own test harness).
 *          Struct is 0x20 bytes (storage_converter_base.h), giving this class
 *          a total confirmed size of 0x30.
 */

#ifndef PROG_CONVERTER_H
#define PROG_CONVERTER_H

#include "storage_converter_base.h"

class CProgConverter {
public:
	// D1, complete-object destructor. .text+0x08e07d90, 25B. Ground truth
	// resets all 3 raw-pointer slots (+0x00/+0x04/+0x08) to
	// `&vtable_for_CStorageConverterBase + 8` (0x08fcc9c8) -- NOT this
	// class's own vtable. Reproduced literally: this project's own
	// LESSON_missing_vtable_write.md convention is to match observed vptr
	// writes exactly rather than "correct" them against an inferred model,
	// and 3 identical resets across offsets that don't fit a simple
	// single-inheritance shape are exactly the kind of layout this project
	// declines to guess at (see the file header's own note on why C++
	// inheritance isn't used to model this class).
	~CProgConverter();

	// D0, deleting destructor. .text+0x08e07db0, 74B. Same 3 resets as D1,
	// plus a real HAL_DisableInterrupts()/free(this)/HAL_EnableInterrupts()
	// sequence (the same critical-section-guarded free() idiom used
	// elsewhere in this project for heap-allocated polymorphic objects).
	void DeletingDtor();

	// Open() -- .text+0x08df75c0, 766B. NOT reconstructed this pass (see file
	// header). Builds m_storedParam from the caller's param, picks/constructs
	// a concrete sub-converter into m_pFormatConverter based on a session-
	// context mode-flag byte, and non-virtually calls the embedded
	// CStorageConverterBase::Open() (this+4). Confirmed to be the ONLY
	// caller anywhere in the binary of CStorageConverterBase::Open() (2 call
	// sites, one per branch of an internal PCM-vs-MOSS format dispatch).
	// int Open(const CConvertStorageParam &param);

	// Load()/Save() -- .text+0x08df7510/0x08df7550, 50B each. NOT
	// reconstructed this pass, unlike Close() below: both are real,
	// fully-understood forwards -- `if (m_pFormatConverter)
	// m_pFormatConverter->Load(m_storedParam)` (note: NOT the caller's own
	// `param` argument -- ground truth overwrites the register holding the
	// incoming `this` with `this+0x10` and passes THAT as the forwarded
	// call's param, a genuine, confirmed "ignores its own argument, forwards
	// its own internal copy instead" behavior) -- but the confirmed real
	// target, `CStorageConverterBase::Load()`/`Save()`
	// (storage_converter_base.h), are themselves version-dispatch jump
	// tables into the `ExttoInt0000..000F` real per-version conversion
	// bodies (343-379B each), which are still deferred (storage_converter_
	// base.h's own header comment). Implementing Load()/Save() here would
	// mean either fabricating those 16 bodies or stubbing them out, neither
	// of which this project does; left as a documented, well-understood lead
	// for whichever future batch reconstructs `ExttoIntXXXX`.
	// void Load(const CConvertStorageParam &param) const;
	// void Save(const CConvertStorageParam &param) const;

	// .text+0x08df7590, 37B. Real: if (m_pFormatConverter) { forward to
	// m_pFormatConverter->Close(); m_pFormatConverter = 0; } -- unlike
	// Load/Save, Close() DOES null the member out afterward (confirmed).
	void Close();

protected:
	// See file header's "MEMBER LAYOUT" for the full derivation of every
	// field below. `protected` (not `private`) so a test-only subclass can
	// `using CProgConverter::m_pFormatConverter;` etc, same convention as
	// scsi_driver_base.h's own CScsiDriverBase.
	void *m_vptr0;                              // +0x00
	void *m_base4;                              // +0x04
	void *m_base8;                              // +0x08
	CStorageConverterBase *m_pFormatConverter;   // +0x0c
	CConvertStorageParam   m_storedParam;        // +0x10..0x2f
};

#endif // PROG_CONVERTER_H
