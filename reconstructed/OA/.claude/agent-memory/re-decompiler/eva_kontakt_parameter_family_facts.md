---
name: eva-kontakt-parameter-family-facts
description: Eva CKontaktXxxParameter family (~110 classes) real dispatch mechanism, discovery technique, and current reconstruction status as of commit 009c95b (2026-07-28)
type: reference
---

Real facts about Eva's Kontakt (NKI) import "Parameter" accessor family, sitting
on top of `CKontaktXml` (kontakt_xml.h) and `CKontaktParameter`/
`CKontaktIndexedParameter`/`CKontaktDynamicParameter` (kontakt_parameter_base.h).
See also [[stg_value_getter_family]] — the discovery-technique cross-pollination
that started this ("shared write-sink cross-reference") was applied here too.

## Real XML shape (confirmed, not guessed)

Every real field is a CHILD element `<Parameter name="X" value="Y"/>`, NOT a
direct XML attribute on the parent tag as first assumed (e.g. NOT
`<Zone0 sampleStart="1234" .../>`). `AddAttribute(index, name, value)` on all
3 base classes only ever recognizes the literal attribute names "name"/"value"
(confirmed: identical `{"name","value",0}` .data list at all 3 real call sites,
dumped directly). "name" is stashed (`mAllocatedName = xmlStrdup(value)`); the
following "value" attribute on the SAME child element resolves the stashed name
against the owning class's own field-name list (`mList`, set once by the
concrete subclass's ctor) via one of `CKontaktXml::StringIndex`'s 3 overloads,
then dispatches through vtable slot +0x14 (`AddParameter`/`AddIndexedParameter`/
`AddDynamicParameter` depending which of the 3 bases).

Which of the 3 bases a concrete class uses depends on how instances of a
repeating field are distinguished: plain `CKontaktParameter` for fields that
appear once per parent (Zone/Effect/Filter/Output/Lfo/Loop/Envelope/
PlaybackMode/StartCriteria); `CKontaktIndexedParameter` where the "name" value
carries a trailing NUMBER (e.g. Group's "outRouting_N", `StringIndex`'s 3-arg
numeric-suffix overload); `CKontaktDynamicParameter` where the trailing part is
free TEXT (e.g. Script's "persistent_var_XYZ", `StringIndex`'s 4-arg
text-suffix overload).

## Open, unexplained finding (do not silently resolve, flag any evidence found)

The "CKontaktXxxParameters" (PLURAL) factory-wrapper family (`CKontaktParameters`
/`CKontaktIndexedParameters`/`CKontaktDynamicParameters` + ~7 concrete siblings,
~30 methods, NOT modeled yet) — the classes a parent container's own `AddObject`
would use to construct these singular Parameter objects — has its own `AddObject`
dispatch through a name list that is IDENTICAL (same literal .data pointer) across
all 3 abstract bases, and that list has exactly ONE entry: the single-character
string "V". Confirmed via direct raw hex dump (not a parsing artifact — see the
extraction-tool bug note below, this was checked with the FIXED tool). Does not
match any real Kontakt XML tag name found anywhere else in this sweep. Most
likely genuinely dead/unreachable code in this build (abstract base classes,
only a concrete container like `CKontaktGroup` would ever need its own
`AddObject` for real, and that whole container family is separately still
100% out of scope) — but never independently resolved. If a future pass touches
`CKontaktGroup`/`CKontaktZone`/etc.'s own `AddObject`, check whether IT calls
into the plural factory family with a DIFFERENT list, which would explain this.

## Reconstruction status (commit 009c95b)

DONE (66 methods): the 3 abstract bases, plus concrete siblings
CKontaktGroupParameter (32 fields + 1 special SetOutputRouting case),
CKontaktZoneParameter (33-entry name list but ONLY indices 0-15 are actually
dispatched — indices 7-10 are recognized names whose jump-table slots point
straight at the shared no-op fallback, a confirmed real quirk, not a guess;
indices 16-32 are unreachable, guard rejects them before the jump table),
CKontaktEffectParameter, CKontaktFilterParameter (ALL 19 fields are genuinely
float, even ones named like bools/enums — typeA/typeB/typeC/bypA/bypB/bypC),
CKontaktOutputParameter, CKontaktLfoParameter, CKontaktLoopParameter,
CKontaktEnvelopeParameter (1 field, "attack", parsed via SignedValue then
`fild`-converted to float, NOT FloatValue directly), CKontaktPlaybackModeParameter,
CKontaktStartCriteriaParameter, CKontaktScriptParameter (Dynamic family,
confirms a `new[]`/scalar-`delete` allocator MISMATCH plus an uninitialized
NUL-terminator byte in `CKontaktScript::SetSourceText` — reproduced as-is,
not "fixed").

NOT DONE, explicitly deferred with reasons (good next-batch targets):
- The plural "Parameters" factory family (~30 methods) — blocked on the "V"
  mystery above, or just skip it (nothing else needs it).
- CKontaktSampleParameter / CKontaktProgramParameter / CKontaktContainerParameter
  — each calls the still-deferred `CKontaktXml::UnpackPath` (see kontakt_xml.h).
- CKontaktBankParameter (needs CKontaktBank::SetSlotMute/SetSlotSolo/
  SetSlotMidiChannel/SetSlotReorderIndex/SetSlotAuxSendLevel — all confirmed
  trivial pure-field-write setters, just not reconstructed yet),
  CKontaktOutputsParameter (needs CKontaktOutputs::SetPhysicalOutputMapping),
  CKontaktSendLevelsParameter (needs CKontaktSendLevels::SetLevel),
  CKontaktIntModulatorParameter/CKontaktExtModulatorParameter/
  CKontaktVoiceGroupParameter/CKontaktTargetParameter (each needs a plain
  `SetName(const char*)` strncpy setter on its owner class). All traced via
  a full per-function `objdump -dr` call-target sweep already — genuinely
  the cheapest possible next batch, just needs the owner setters typed in.

## Discovery technique used (reusable)

1. Full call-target sweep per concrete `Add(Indexed)Parameter` body
   (`objdump -dr -M intel <bin> --start-address=X --stop-address=Y | grep call`)
   to confirm self-containment before investing in a class.
2. Each concrete class's ctor passes a literal `.data` pointer (the field-name
   list) to its base ctor — find that literal, then dump the NUL-terminated
   `char*[]` array it points to (see below) to get REAL field names, not guesses.
3. The jump table for the dispatch switch is a separate `.rodata` array
   (`jmp DWORD PTR [eax*4+0xADDR]`) — dump ITS N pointers too (N from the
   `cmp eax,N-1` guard just before), then for each pointed-to code block read
   off (CKontaktXml value-parser called, destination struct offset written) —
   this directly gives the case-index -> (field name, offset, type) mapping
   with high confidence, since the name-list index and the jump-table slot
   index are literally the same number.
4. **Extraction-tool gotcha (cost real time this session, now fixed)**: when
   using `objdump -s --start-address=X` with a non-16-byte-aligned `X`,
   objdump prints the row starting exactly at X with an IRREGULAR first group
   length (not a clean 4-byte boundary) — a naive per-group byte-placement
   parser silently drops/misplaces bytes near the requested start, truncating
   strings by 1+ bytes in a way that's easy to miss (looks like a plausible
   but wrong string). Fix: ALWAYS round the requested `--start-address` down
   to the nearest 16-byte boundary before calling objdump, then slice out the
   actually-wanted range from the correctly-aligned dump. Verified by manual
   cross-check against a raw hex dump before trusting the tool further.
5. Host KAT-test limitation (same as [[eva_client_comm_server...]]-style
   host-only gaps): `libxml2_host_stubs.cpp`'s `xmlStrdup()` always returns
   NULL on this host (no i386 libxml2 to link a real one against) — driving
   `AddAttribute()`'s real "name" case end-to-end in a host test SEGFAULTS
   (NULL `mAllocatedName` fed into `strncasecmp`/`strlen` inside
   `StringIndex`). Work around by adding a tiny test-only `SeedName()` setter
   on a testable subclass that pokes the (protected) `mAllocatedName` field
   directly, then only drive the "value" attribute through `AddAttribute()` —
   exercises the same real production dispatch logic, just skips the
   xmlStrdup-dependent half.
