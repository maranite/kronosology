---
name: ckg-midi-msg-processor-evtdisp-2026-07-28
description: CKGMIDIMsgProcessor (13) + CKGEventDisplayManager (15) reconstructed, commit 1ccac6d, manifest 3144->3182/21689; discovered a whole new CKGMIDIOutMsgHandler dependency web the prior batch's "low-risk" framing missed; inline-stub-to-avoid-manifest-credit technique reused for real
metadata:
  type: project
---

## What this is

Continuation of the CKG/CSK KARMA cluster sweep, picking the two targets
the prior batch (`ckg_msgprocessor_paramchange_2026-07-28.md`) flagged as
"already partially-wired opaque stand-ins, same low-risk pattern as this
batch's own 2 targets". That framing was TRUE for `CKGEventDisplayManager`
(genuinely simple once reached) but UNDERSOLD `CKGMIDIMsgProcessor` -- its
real 705-byte ctor placement-constructs 4 distinct sub-handler singletons
plus 16 `CKGCCResetHandler` instances, and NONE of those 6 dependency
classes existed in this project before this batch. Verify real vtable/
inheritance from real relocation dumps rather than trusting a prior
summary's risk assessment, not just its factual claims.

Files: `include/oa_ckg_midi_msg_handler.h` (CKGEventDisplayManager
expanded in place; CKGMIDIOutMsgHandler family + CKGMIDIMsgProcessor
added as new classes at the end), `include/oa_ckg_control_ui_msg.h`
(CKGMIDIMsgProcessor's old 3-method opaque stub removed, replaced by a
pointer comment), `include/oa_ckg_module_param_msg_handler.h`
(`CKGEngine::IsKarmaOn()`/`GetRealOutputChannel(int)` added), `src/engine/
ckg_midi_msg_handler.cpp`, `verify/test_ckg_midi_msg_handler.cpp`. Commit
`1ccac6d`. Manifest 3144 -> 3182/21,689 (+38, 0 regressions, verified via
full `git stash -u` baseline exact-name-set diff).

## The CKGMIDIOutMsgHandler dependency web (all confirmed via `objdump -r`
against `.rodata._ZTV*` in `/home/share/Decomp/OA.ko_Decomp/OA.ko`)

```
CSKMIDIMsgHandler (already real, 15-slot base)
  +-- CKGMIDIOutMsgHandler          (NEW intermediate, 14 own slots, rodata 0x44-0x78)
  |     +-- CKGMIDIKarmaGeneratedMsgHandler  (overrides ShouldSendChannelMessageToMIDIPortInEachMode -> true, CheckDyingNoteForMIDIPort -> true)
  |     +-- CKGMIDITimbreThruMsgHandler      (overrides ShouldSendChannelMessageToMIDIPortInEachMode -> CMIDIFlowParamHolder-gated)
  |     +-- CKGMIDIKarmaResetCCMsgHandler    (overrides ShouldSendChannelMessageToSTG -> false)
  +-- CKGBendRangeHandler           (direct child, own SINGLE new slot at rodata 0x44
                                      only -- NOT via CKGMIDIOutMsgHandler despite the
                                      similar role, settled by vtable size: 16 slots
                                      total vs the other three's 30)

CKGCCResetHandler (unrelated root, own 12-slot vtable, owned 16x by CKGMIDIMsgProcessor)
```

`CKGMIDIOutMsgHandler::Process()` (rodata 0x44) turned out fully
self-contained -- every call inside it targets an already-declared
sibling virtual (own or CSKMIDIMsgHandler's), so it got a REAL body
(117 bytes, `.text+0x3bb6a0`) even though the class itself is mostly
out of scope:

```cpp
void CKGMIDIOutMsgHandler::Process()
{
	if ((unsigned char)m_status > 0xef) return;
	if (CheckDyingNoteForMIDIPort())
		if (ShouldSendChannelMessageToMIDIPort()) SendChannelMessageToMIDIPort();
	if (ShouldRecChannelMessageToSequencer()) RecChannelMessageToSequencer();
	if (!ShouldSendChannelMessageToSTG()) return;
	ConvertPostMIDIAfterTouch(); ConvertPostMIDIVelocity(); SendChannelMessageToSTG();
}
```

This is the function 3 of `CKGMIDIMsgProcessor`'s own methods invoke via
`m_karmaGen/m_timbreThru/m_karmaResetCC->Process()` -- its own siblings
(`ShouldSendChannelMessageToMIDIPort()` etc.) stay inline `{ return
false; }` stubs, so in THIS reconstruction Process() will always take the
"do nothing further" branches; that's an accepted, documented limitation
(the SIBLING methods' own real bodies are a separate, not-yet-attempted
class of their own), not a bug in Process() itself.

## The inline-stub-to-dodge-manifest-credit technique, reused and reconfirmed

For every out-of-scope dependency virtual (14 in `CKGMIDIOutMsgHandler`,
10 in `CKGCCResetHandler`), the body is declared INLINE, directly in the
class definition (`virtual bool Foo() { return false; }`), NOT as an
out-of-line `ClassName::Foo() { ... }` in the .cpp. This is required for
2 reasons at once: (1) C++ instantiability -- every leaf class must have
a concrete (non-pure) override for every inherited virtual, so a stub
body is mandatory; (2) manifest hygiene -- `gen_oa_manifest.py`'s NAME
heuristic only matches qualified out-of-line `Class::Method(...) {`
text, so an in-class inline body never gets credited even though its
signature is real and its NAME would otherwise match. Also deliberately
did NOT cite a `.text+0x` address anywhere near these 24 stub
declarations, since the ADDRESS heuristic matches ANY such literal found
ANYWHERE in a comment, regardless of context (see the `KGMain_Initialize`
gotcha below for what happens when this rule is broken by accident).

Contrast: the handful of dependency methods that WERE reconstructed for
real (`CKGMIDIOutMsgHandler::Process()`, `CKGBendRangeHandler`'s ctor +
`Process()`, 3 leaf-class 3-38-byte overrides, `CKGCCResetHandler`'s ctor
+ `Initialize()`) DO cite their real address and DO get manifest credit
-- that's correct, not a leak, since their bodies are genuinely faithful
transcriptions (or, for the 4 trivial ctors, exactly what the compiler's
own default `ClassName() {}` already produces by chaining to the base
ctor and installing the vtable pointer -- confirmed equivalent to real
ground truth's own ctor bytes, not just "close enough").

## CKGMIDIMsgProcessor's real field layout (from the 705-byte ctor)

```
+0x00 CKGMIDIKarmaGeneratedMsgHandler *m_karmaGen    AllocAligned(0x3090,0x10)
+0x04 CKGMIDITimbreThruMsgHandler *m_timbreThru      AllocAligned(0x3090,0x10)
+0x08 CKGMIDIKarmaResetCCMsgHandler *m_karmaResetCC  AllocAligned(0x3090,0x10)
+0x0c CKGBendRangeHandler *m_bendRange               AllocAligned(0xc,0x10)
+0x10..+0x4c CKGCCResetHandler *m_ccReset[16]        AllocAligned(0xe0,0x10) each,
      CKGCCResetHandler(i) i=0..15, each IMMEDIATELY followed by a real
      vtable-slot-0 call (->Initialize()) right after construction.
+0x50 unsigned char m_bSuspended -- every Process*ChannelMessage() is a
      full no-op while nonzero; ctor sets 0. Nothing reconstructed here
      ever sets it nonzero -- whatever re-suspends it is out of scope.
```

Real dtor (`.text+0x3baa10`, 11 bytes) ONLY clears `ms_poInstance` -- none
of the 20 owned sub-objects are ever deleted, confirmed by there being no
further instructions at all. Same deliberate-leak idiom as
`CSKMIDIMsgHandler::m_special` from an earlier batch -- transcribed
as-is.

## Real per-method quirks caught via register-by-register reading

1. **`ProcessResetControllerChannelMessage()` reuses `m_timbreThru`** (the
   SAME sub-object as `ProcessTimbreThruChannelMessage()`), tagged with a
   fixed low-flags-nibble literal `1` instead of that other method's own
   `CSKMIDIMsgProcessor::ms_poInstance[+0x18]`-derived byte. Confirmed via
   disasm (`or eax,1` vs `or al,0x18(ecx)`), not assumed from either
   method's name.
2. **`ProcessKarmaGeneratedBendRangeChannelMessage()`'s `m_status` is
   `channel - 0x20`, NOT `channel`** -- a real `lea edx,[edx-0x20]` on the
   raw register before the store, easy to miss since every OTHER
   Process*ChannelMessage() in this class stores `channel+status`
   (an ADD, not a SUBTRACT-from-a-different-operand). This method also
   has NO `IsKarmaOn()`/changeSource gating at all (unconditionally sets
   flags bit `0x10` and calls `Process()`), unlike the other 4 siblings.
3. **`StoreCCMessage(CMIDIMessage*)` reads `msg`'s OWN byte 0 directly**
   (`*(unsigned char*)msg & 0xf`), NOT the `+4`-offset
   `m_status`/`m_data1`/`m_data2`/`m_flags` convention every
   `CSKMIDIMsgHandler`-derived class in this file uses -- `CMIDIMessage`
   has a genuinely different raw layout (status-byte-at-offset-0), not a
   transcription slip.
4. **`ResetKarmaGeneratedCCValue()`'s 2 overloads have real, different
   scopes**: the 0-arg version searches ALL live KARMA modules for
   whichever one's `GetRealOutputChannel()` matches each of the 16 MIDI
   channels; the 1-arg version skips the search entirely and resets that
   exact channel index directly. Not "the same thing with an optional
   parameter" -- genuinely different real bodies.

## CKGEventDisplayManager: no recoverable field names, flat-array model

Real class has NO vtable (plain, non-virtual dispatch confirmed) and NO
symbol-recoverable field layout at all -- every access in every one of
its 15 methods is raw dword-indexed arithmetic on `this`. Modeled as one
`int m_flat[0x3c0]` array; every method transcribes the EXACT real index
formula (verified independently per method, not assumed to generalize
from one to the next -- 2 of the 15 initially looked "obviously the same
shape" as a sibling and were NOT, see below).

```
[0x000,0x280) m_flat[objectIndex*128+note]           live note-on refcount,
    5 objectIndex rows: 0=direct/local, 1-4=GetNoteObjectIndex(module)
[0x280,0x2c0) m_flat[module*16+groupIndex]           live CC/bend refcount,
    16-dword-stride rows (only module 0-3 real), groupIndex = cc/8 for CC,
    bendValue/1024 for pitch bend -- CONFIRMED the exact SAME array for
    both event kinds via disasm, not inferred from naming.
[0x2c0,0x388) m_flat[0x2c0+ring*20+objectIndex*4+(note>>5)]  10-slot ring
    buffer (ring 0-9) of 128-bit "note touched this write-window" bitmasks.
[0x388,~0x3b0) m_flat[0x388+ring*4+module]           10-slot ring buffer
    of 16-bit "CC/bend group touched" bitmasks, one dword/module.
0x3b0 (+0xec0) "now" tick counter -- WRITTEN ELSEWHERE (out of scope),
    Idle() only ever reads it.
0x3b1 (+0xec4) note-aging tick checkpoint.
0x3b2 (+0xec8) CC/bend-aging tick checkpoint.
0x3b3 (+0xecc) NOTE ring WRITE cursor (NoteOn/NoteOff mark into this slot).
0x3b4 (+0xed0) NOTE ring READ cursor (CheckAndProcessNoteStatus ages this
    slot out next); Initialize() seeds this to 1 (every other dword to 0).
0x3b5 (+0xed4) CC ring WRITE cursor.
0x3b6 (+0xed8) CC ring READ cursor; also seeded to 1.
```

Also touches `CKGBankManager::ms_poInstance[+8]` (the SAME "note-display
sub-object" pointer established in a prior batch's `NotifyNoteEventToUI()`,
own layout out of scope): `sub+0x723c` is a `unsigned int[5][4]` UI-facing
"note visibly on" bitmask (`[objectIndex][note>>5]`), `sub+0x728c` a
`unsigned int[4]` "CC/bend group visibly on" bitmask (`[module]`) --
these 2 ranges are contiguous and immediately precede the already-known
`+0x147a6` region from the prior batch, all inside the SAME giant
sub-object.

### Real gotcha: NoteOn-shape vs NoteOff-shape debounce are NOT the same pattern

`NoteOn()`/`NoteOnByKarma()`/`NoteOn(objectIndex,note)` (3 real
functions) ALWAYS increment the live count AND unconditionally OR the
ring bit in, no gating at all. `NoteOff()`/`NoteOffByKarma()`/
`NoteOff(objectIndex,note)` (3 more) are ring-GATED instead: if the ring
bit for this exact note is not yet set this write-window, mark it
(without touching the count -- decrement is deferred to
`CheckAndProcessNoteStatus()`'s later aging pass); if the bit IS already
set (a redundant NoteOff within the same window), decrement the count
directly instead. `CCOnByKarma()`/`CCOn()`/`BendOnByKarma()` (3 more, all
"On" events like NoteOn but CC/bend-shaped) use a THIRD pattern: always
increment, then if the ring bit was ALREADY set this window, immediately
decrement back (net: only the FIRST mark per window survives). Assuming
any 2 of these 3 shapes are interchangeable would silently corrupt the
KAT and the real behavior -- each was verified independently against its
own raw disassembly.

### Real gotcha: `Initialize()`'s zero range is contiguous, not "call it 3 different loops"

Real `Initialize()` (3408 bytes!) is a 5x128 double-loop (compiler didn't
unroll, real loop) covering `[0,0xA00)` bytes, immediately followed by
~240 individually-unrolled `movl $0,...` stores covering `[0xA00,0xEDC)`
with exactly 2 exceptions (`+0xed0`/`+0xed8` set to `1`, not `0`). Despite
looking like 2+ separate initialization passes in the raw disassembly
(the compiler emitted the `[0xA00,0xB00)` stores OUT OF ADDRESS ORDER,
interleaved after the `[0xB00,0xE1C)` block), the real observable effect
is one contiguous `memset(this, 0, 0xedc)` plus the 2 explicit `=1`
overwrites -- confirmed by manually walking every single store's own
destination offset before writing the C++, not by trusting the
instruction stream's own visual ordering. `Initialize()` ALSO writes 96
bytes on the foreign sub-object (`sub+0x723c`..`sub+0x729c`) -- the exact
combined size of the 2 UI-bitmask ranges documented above (5x4 + 4
dwords = 24 dwords = 96 bytes), independently confirming both ranges'
real extents.

## Manifest false-credit caught (same class of bug as before, new trigger)

Wrote a class-comment describing `CKGEventDisplayManager`'s own region as
spanning `.text+0x3b40a0`..`.text+0x3b5580` (start of ctor to "one past"
the last method) -- the END address happened to be the exact real start
address of an UNRELATED function, `KGMain_Initialize` (own class-graph
investigation earlier turned this address up while tracing
`CKGMIDIMsgProcessor`'s own construction site, unrelated to this class).
`gen_oa_manifest.py`'s ADDRESS heuristic matched the literal and
false-credited `KGMain_Initialize` as reconstructed (confirmed via
`match_method=address` in the regenerated CSV). This is the SAME bug
class already documented in `ckg_control_ui_msg_family.md` gotcha #2
("a class-region-END address comment coincidentally matched a DIFFERENT
class's own address") -- caught this time by comparing the raw
newly-added-name list against expectation (39 names added, but the list
included an obviously-unrelated free function) rather than trusting the
raw count delta alone. Fixed by rewording to cite the class's own LAST
REAL method's address (`Idle()` at `.text+0x3b5500`) instead of an
unbounded "start..end" range. **Lesson reconfirmed**: never cite a
region's END address as "one past the last real thing here" -- always
cite a specific method you actually implemented, and if you must
describe a range's extent, say so in prose without a second literal
`.text+0x` citation.

## Test-harness fixes required before any KAT could run

Both classes were already reached, unconditionally, through their own
`ms_poInstance`/singleton pointer by ALREADY-COMMITTED real code
(`CSKMIDIMsgHandler::SendChannelMessageToSTG()` calls
`CKGMIDIMsgProcessor::ms_poInstance->StoreCCMessage(...)`;
`CSKMIDIInMsgHandler::NotifyNoteEventToUI()` calls
`CKGEventDisplayManager`'s `NoteOn`/`NoteOff`) -- both singletons were
either null or backed by trivial `{}` test-double mocks in
`verify/test_ckg_midi_msg_handler.cpp`, harmless until this batch gave
them real bodies that dereference real fields. This is the THIRD time
this exact bug class has been hit in this file's own test history
(see `ckg_msgprocessor_paramchange_2026-07-28.md`'s own writeup of the
first two instances) -- fixed proactively this time, before running any
test, by (1) wiring a real `CKGMIDIMsgProcessor` test singleton via
manual field assignment (NOT the real ctor, since
`CSTGBankMemory::AllocAligned()`'s own test mock always returns the SAME
fixed buffer regardless of requested size, and calling the real ctor
would have aliased all 20 owned sub-objects onto that one buffer), and
(2) replacing the old 16-byte `g_eventDisplayBuf` mock with a real
`static CKGEventDisplayManager g_eventDisplayObj`, `Initialize()`'d
AFTER (not before) `CKGBankManager::ms_poInstance[+8]`'s own sub-buffer
is wired, since `Initialize()` itself dereferences it. **Lesson for
future batches touching this file**: before giving a real body to ANY
method reachable through an existing singleton, grep the WHOLE already-
committed codebase (not just this batch's own new call sites) for other
callers of that exact singleton -- this is now a 3-for-3 pattern in this
one file alone.

## Verification

`make verify`: all binaries green, 0 failures. New KAT coverage
(`oracle_ckg_midi_msg_proc_evtdisp.py`, standalone Python, independently
re-derived from the raw disassembly formulas, not copied from the C++):
`GetNoteObjectIndex()`'s in-range/default-1 branches; `NoteOn`'s
always-increment-plus-mark vs `NoteOff`'s ring-gated-defer-then-decrement
shapes, both against the real dword indices; `CCOnByKarma`/`BendOnByKarma`'s
shared-storage net-to-1-within-a-window behavior; `Idle()`'s tick-diff/
checkpoint math across a mid-loop remainder; `CKGMIDIMsgProcessor`'s
status+channel combine, KARMA-on/changeSource 4-way flags-nibble
combinations (independently re-derived truth table, not eyeballed), and
the bend-range `channel-0x20` transform.

Real `make ko-clean && make ko KDIR=/home/build/linux-kronos` Kbuild
build: clean link, `OA.ko` produced, `nm OA.o` confirms every new symbol
(including all 24 inline-stub virtuals, correctly emitted as weak `W`
symbols from their in-class definitions) defined, none `U`.

## Continuation

`CKGMIDIOutMsgHandler`'s own remaining 13 virtual slots + non-virtual
`KillAllDyingNotes()` (621 bytes) and `CKGCCResetHandler`'s remaining 10
virtuals (`HandleNRPNMessage` alone is 381 bytes) are BOTH now real,
substantial, previously-uncatalogued classes of their own -- natural next
targets given they're already partially wired in exactly the same way
this batch's 2 targets were. From the prior batch's still-open list:
`CKGEngine` (74, still just an opaque `ms_poInstance` stand-in with a
growing handful of on-demand methods), `CKGBankManager`'s own remaining
surface, `CKGRTCHandler` (27), `CSKMIDILocalCtrlMsgHandler` (28),
`CSKMIDIPortMsgHandler` (12, needs `CSKMIDIInMsgHandler` as a real base
-- already satisfied), `CKGUIMsgProcessor` (17), `CKGTimerManager` (15).
Full sweep recipe unchanged: group `manifest/oa_functions.csv` pending
rows by class prefix `CKG`/`CSK`, sort by count, but ALSO grep existing
headers for "own class layout out of scope" opaque stand-ins matching
those names first -- lower risk than a cold-start class, though as this
batch shows, "lower risk" can still mean "the ctor alone drags in an
entire unmapped sub-hierarchy," so budget for that possibility rather
than assuming the prior batch's risk framing was exhaustive.
