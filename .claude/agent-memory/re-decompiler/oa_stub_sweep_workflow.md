---
name: oa_stub_sweep_workflow
description: How the recurring OA.ko bar2_stubs.cpp "smallest-first" decompilation batches are run, verified, and documented -- process gotchas specific to this project, not general RE technique.
type: project
---

Recurring task: reconstruct the next batch of smallest remaining stub
function bodies in `/home/share/kronosology/reconstructed/OA/src/stub/bar2_stubs.cpp`
(a tracking file of deliberately-empty `{}` bodies for OA.ko symbols not
yet reconstructed). Batches so far: batch1 (d47d22d), batch2 (fd06866,
MASTER_REFERENCE sec 10.148), batch3 (8eca6a9, sec 10.149). Each batch
picks ~6-10 of the smallest remaining stubs via
`nm -S -C --size-sort /home/share/Decomp/OA.ko_Decomp/OA.ko` cross-checked
with `objdump -dr`.

**Build/verify host is 192.168.3.92** (root/kronosbuild via SSH — no key
auth from this environment, must use `sshpass -p kronosbuild ssh -o
StrictHostKeyChecking=no root@192.168.3.92 "..."`). Files are edited
directly at `/home/share/kronosology/...` (CIFS-shared with this host) —
no copying needed, just SSH over to run `make`.

**Required verification sequence** (from
`/home/share/kronosology/reconstructed/OA/`, on 192.168.3.92):
```
make clean && make all                          # host-side KATs, all must build+pass
make ko KDIR=/home/build/linux-kronos            # actual .ko build
nm -u OA.ko | wc -l                              # must be exactly 32
objdump -h OA.ko | grep -i linkonce              # .gnu.linkonce.this_module must be 0x148 bytes
readelf -r OA.ko | grep -c R_386_GOTPC           # must be 0
stat -c '%s' OA.ko                                # report exact byte size
```
`make all` only builds the verify/ binaries, it does NOT run them —
must loop over `verify/test_*` executables manually and grep each for
"FAILED" in stdout to get the suite pass/fail count. The project
convention is to do this whole sequence TWICE from a clean state before
trusting the numbers (their own stated "never trust a prior pass's own
confirmation" discipline) — do not skip the second run.

**Recurring gotcha: missed mock promotions in verify/**. Whenever a stub
in `bar2_stubs.cpp` gets promoted to a real body, EVERY verify/test_*.cpp
file that links the real source file directly (not bar2_stubs.cpp) may
carry its own stale flat-counter mock of that same symbol — these
collide at link time with "multiple definition" errors once the real
body is compiled in. `grep -l '<SymbolName>' verify/*.cpp` across the
WHOLE verify/ directory before considering a batch done — do not stop
at the first file a build error surfaces. In batch 3 (sec 10.149),
`test_global.cpp` and `test_engine_startup_bits2.cpp` both had this gap
even though the same symbol (`CSTGVoiceAllocator::EmergencyFreeVoiceList`)
had already been correctly promoted in `test_engine.cpp`/
`test_global_ctor.cpp`/`test_managers.cpp` — inconsistent completeness
across sibling test files is the norm, not the exception, for this
project's history so far.

**Batch 3 specifics** (2026-07-05, commit 8eca6a9): this session started
mid-flight — a PRIOR (interrupted) instance of this exact recurring task
had already written the analysis/implementation for all 6 functions
(uncommitted, in the working tree) but never ran the verify/build/commit/
MASTER_REFERENCE steps. Confirmed via `git diff --name-only` (not
`git status`, which shows stale CIFS "M" flags on ~150 unrelated files —
see CLAUDE.md's own documented gotcha) that only 15 files under
`reconstructed/OA/` were genuinely modified. Re-verified the prior
session's own comments against a real build rather than trusting them,
which is exactly what caught the `test_global.cpp`/
`test_engine_startup_bits2.cpp` gaps above. Lesson: when resuming a
stub-sweep batch that already has uncommitted work in the tree, treat it
as a legitimate continuation (don't redo the disassembly from scratch),
but still independently re-verify via a real rebuild before trusting any
of its "X is real now" claims.

MASTER_REFERENCE.md section numbering: sections are appended just before
the file's own trailing "Preserve obfuscated-but-real symbol names..."
standing-conventions bullet list (grep for that string to find the exact
insertion point — do not just append at EOF).

**Batch 4 specifics** (2026-07-05/06, commit 37e79db, sec 10.150):
picked `CSTGAudioInputMixerBase`'s four setters, `CSTGMidiQueue::
GetNumWritableBytes`, `CSTGSlotVoiceData::Initialize`, `CSTGPlaybackEvent`
ctor — 7 functions, net stub count -3 (112 -> 109, since 4 new
confirmed-real deliberately-deferred stubs got added along the way).

**Recurring gotcha (hit TWICE in this one batch): native pointer fields
in any class reinterpreted onto raw target memory.** This project's own
established convention (`CSTGMidiQueueWriter`, `ToU32`/`FromU32`) stores
pointer-sized fields as packed 32-bit `unsigned int`, NEVER a native
`void*`/`T*` struct member — because a native pointer is 8 bytes on this
64-bit host but 4 bytes on the real 32-bit target. Declaring a native
pointer field either (a) shifts every SUBSEQUENT field's own offset via
8-byte alignment padding that doesn't exist on the real target (hit for
`CSTGAudioInputMixerBase`'s `mixerStateArray`/`busChangeArray`, caught by
a same-day segfault in its own new KAT), or (b) lets two adjacent 8-byte
writes stomp each other's upper 32 bits when the real fields are only 4
bytes apart (hit for `CSTGSlotVoiceData::Initialize`'s own `+0x1480`/
`+0x1484` writes, caught by a real `check_eq` FAIL in `test_global.cpp`).
Both fixed identically: `unsigned int` field + `ToU32`/`FromU32` at every
access site. Rule of thumb going forward: the MOMENT a new class gets a
field meant to represent "a pointer stored at raw offset N" for a class
that's reinterpreted directly onto target memory (not a plain host-side
helper struct), declare it `unsigned int` from the start, never `T*` —
don't wait to be caught by a segfault or failing assertion. The ONE
legitimate exception found so far: a raw vtable-pointer slot that a KAT
needs to dispatch through via real host function pointers (`*(void***)
this`) — that one deliberately stays a native 8-byte read/write in the
test harness (the real 32-bit target naturally reads only 4 bytes there
under `-m32`), matching the pre-existing `test_audio_start.cpp` fake-
vtable convention.

**Splitting one shared-memory subsystem across 3+ TUs is fine and
sometimes necessary**: the ring-buffer subsystem ended up as THREE files
(`midi_queue_writer.cpp` for `Write()`, `midi_queue.cpp` for the new
`GetNumWritableBytes()`, both operating on the same `ringCtl` layout)
specifically because one method's mock was safe to remove from
`test_global.cpp` (tiny footprint, return value never varied) while the
sibling method's mock was NOT (~10-43 load-bearing references) — putting
both real bodies in one file would have forced an all-or-nothing choice
at that file's Makefile link line. When two methods share ground-truth
memory layout but have very different mock footprints in the same test
file, split them into separate files rather than force a single
promote-or-defer decision.

**Before promoting any stub that touches a class already used elsewhere
in `bar2_stubs.cpp`'s own header comments as "confirmed real,
deliberately deferred (own body not reconstructed)"**: re-read that
comment's own stated reason for deferral first (e.g. the ten Model
ctors' shared "needs a new base class + vtables" reasoning,
`WriteSTGMidiOutQueue`'s "~10-43 mock references" reasoning) — if the
reason still holds, it's a legitimate signal to skip that candidate for
THIS batch too rather than re-doing the same analysis from scratch.

**Batch 5 specifics** (2026-07-06, sec 10.151): picked sec 10.150's own
four newly-discovered confirmed-real dependencies (`CSTGBusInfo::
GetSignalSelectionForBusType`, `CBusChangeStateMachine::StartBusChange`,
`CSTGPan::CalculateMonoPanCoeffs`, `CSTGChannelValues::Initialize`) plus
`CTimerManager::ShouldSyncExternalClock` and the sibling `CSTGHDRFileReader`/
`CSTGStreamingFileReader::Initialize()` pair -- 7 functions, stub count
109 -> 103 (net -6, since `CSTGChannelValues::InitializeLongHand()` was
discovered as a new deferred dependency along the way).

**New gotcha, not covered by any prior batch: this project's Kbuild flags
are `-msoft-float -mno-sse -mno-mmx -mno-sse2 -mno-3dnow`.** Any NEW
function using plain C `float`/`double` arithmetic (`*`, `+`, `<`, `<=`,
etc.) silently pulls in libgcc soft-float helpers
(`__mulsf3`/`__addsf3`/`__ltsf2`/`__lesf2`/...) that this freestanding
kernel module can't resolve -- `nm -u OA.ko | wc -l` will jump above 32
(hit 37 in batch 5, from a single new float-heavy function). This
project already has an established fix for exactly this situation:
`global.cpp`'s `MulRoundToFloat()`/`FYL2X()` (sec 10.117) use small x87
inline-asm primitives instead of plain C float ops, since "there is no
libm available in a kernel build" either. When reconstructing ANY
function whose ground truth uses x87 FPU instructions (fld/fmul/fadd/
fcomi/etc in the disassembly), write it via inline asm from the start,
not plain C arithmetic -- don't wait for the `nm -u` count to catch it.
**Sub-gotcha, caught by a real KAT failure in batch 5**: x87 inline-asm
primitives using register-tied constraints (`"=t"`/`"0"`/`"u"`, matching
`MulRoundToFloat`'s own single-call style) are fine for ONE call, but
proved fragile when CHAINED across nested calls (one primitive's result
fed as an input to another). Fix: use ONLY memory (`"m"`) operands for
both inputs AND the output, with the entire x87 push/pop sequence
self-contained inside one `asm volatile` per primitive -- eliminates any
inter-call x87-register-allocation ambiguity. Verified working pattern
(4 primitives: FMul/FAdd/FLess/FLessEq) in
`src/engine/audio_input_mixer.cpp`.

**Recurring reminder, confirmed again in batch 5**: `readelf -r`
relocation "Offset" values are relative to the SPECIFIC named relocation
section (`.rel.rodata` vs `.rel.data` vs `.rel.text` vs
`.rel.rodata.cst4` are all independent numbering spaces) -- grepping
`readelf -r` output for a numeric offset WITHOUT tracking which
`Relocation section '...'` header it falls under will silently attribute
a relocation to the wrong section (hit twice in batch 5: almost
misread `CSTGBusInfo::GetSignalSelectionForBusType`'s real 2-int
`{1,2}` `.rodata` table as relocated pointer data because of a
same-numbered but unrelated `.rel.data` entry; and `CSTGPan::
CalculateMonoPanCoeffs`'s two real float constants live in
`.rodata.cst4`, a SEPARATE section from plain `.rodata` with its own
independent offset numbering, not the main `.rodata`'s file-offset-based
addressing used elsewhere in this project). Always parse/track the
`Relocation section 'NAME'` header line before trusting an offset match.

**Batch 6 specifics** (2026-07-06, sec 10.152): picked `USTGAliasBankTypes`'s
four bank-conversion helpers (`ConvertAliasPgmBankToMidiBank`/
`ConvertCombiBankToMidiBank`/`ConvertMidiBankToCombiBank`/
`ConvertMidiBankToAliasProgramBank`, previously-undisturbed sec
10.98/10.99 deferred externs) and `CSTGWaveSeqGenerator::CSTGWaveSeqGenerator()`/
`Init()` (sec 10.62's own long-deferred pair) -- 6 functions, stub count
103 -> 97, no new deferred dependencies discovered. Before picking,
re-verified (fresh disassembly, not just re-reading old comments) that
the ten Model-class ctors, `WriteSTGMidiOutQueue`, and the six
`ProcessCommands()` file-daemon siblings' own "deliberately deferred"
reasoning all still held -- they did (Model ctors still need a new
`CSTGVoiceModel` base ctor + 10 vtables; `ProcessCommands()` siblings
still dispatch through a shared not-yet-reconstructed ring-buffer/
vtable-callback class, confirmed via fresh disassembly of 4 of the 6
sibling methods, one of which -- `CSTGSamplingDaemon::ProcessCommands()`
-- turned out to enqueue cross-class into `CSTGFileCloser::sInstance`'s
own SEPARATE embedded ring, not just its own; and `SKMain_Initialize`/
`SKMain_Run`, despite being only 85-117 bytes, immediately reach into
an entirely separate "SK subsystem" needing ~10 new external
declarations just to compile).

**New gotcha this batch: a jump table's own REVERSE-direction sibling is
NOT guaranteed to be a value-for-value mirror of the forward table --
always dereference BOTH tables independently via `readelf -r`, never
assume symmetry.** `ConvertCombiBankToMidiBank`'s forward table (bankId
-> midiBank) has a confirmed gap at bankId==7 (skips to midiBank==8).
The first draft of the REVERSE function, `ConvertMidiBankToCombiBank`,
assumed the "obvious" inverse (midiBank==7 -> gap, midiBank==8 ->
bankId==7, i.e. literally reusing the forward array) without actually
walking its own separate `.rodata+0xa570` jump table byte-for-byte --
wrong: the real reverse table's gap sits at a DIFFERENT relative
position (index 7 of ITS OWN 15-entry table, which happens to still
correspond to `midiBankLsb==7`, but every entry past the gap is shifted
down by one relative to what naive "just invert the forward array"
would produce). Caught immediately by a round-trip host KAT
(`test_alias_bank_convert.cpp`'s own `[2]`) checking every `lsb` 0..14
against the ACTUAL disassembled case bodies, not against the forward
table re-used blindly. Lesson: when reconstructing a reverse-direction
sibling of an already-solved forward table, still fully re-derive its
OWN jump table from scratch (dump the raw `.rodata` bytes at its own
relocation offset, map each entry to its own case body) -- do not
shortcut by assuming the inverse relationship "obviously" holds.

**Second new gotcha, same batch: when a real function has two
near-mirror-image branches (e.g. `gm2Mode` vs `mode==0` in
`ConvertMidiBankToAliasProgramBank`), a confirmed "X falls through to
Y" quirk found in ONE branch is not automatically true of the other,
EVEN WHEN they look structurally parallel while hand-tracing the CFG
into prose.** `msb==0`'s own fallthrough target differs completely
between the two branches (one lands on a fixed constant, the other on
a full table lookup) -- an early draft's own explanatory comment
attributed the wrong quirk to the wrong branch purely from working off
a written trace rather than the raw addresses directly, caught only
because the host KAT exercised BOTH branches' `msb==0` case
independently rather than treating the second one as "presumably the
same as the first, just document once." When two branches of the same
function look parallel, write (and test) each one's own quirk
explicitly rather than inferring the second from the first.

**Third, smaller gotcha: a per-instance pointer field written from a
SHARED static "dummy"/placeholder object (address-only, never
dereferenced by the reconstructed function itself) is easy to
undercount by hand from a disassembly listing when the relocations
aren't grouped together.** `CSTGWaveSeqGenerator::Init()`'s own
`sDummyAMS` placeholder is stored into FIVE separate fields
(`+0xc8/+0xcc/+0xd0/+0xf8/+0xfc`), but an initial hand-count (skimming
for the repeated relocation symbol name) found only four -- `+0xcc`'s
own relocation sits between two OTHER unrelated pointer-cache writes
rather than adjacent to its four siblings. Fixed by re-grepping the
full disassembly text specifically for the symbol name (not eyeballing
the listing) before finalizing the field list. Also confirmed (again)
the established convention holds for "address-only, never
dereferenced" globals of unknown size/layout: declare a small opaque
placeholder array (e.g. `static unsigned char sDummyAMS[4];`) with a
comment stating the real size isn't independently determined, rather
than guessing a plausible struct.

**Batch 7 specifics** (2026-07-06, sec 10.153): an unusually
dependency-heavy batch -- evaluated roughly 15 candidates in the
~100-400 byte range and REJECTED most of them for genuinely new-class
requirements only visible after disassembling each one (not predictable
from the stub file's own comments beforehand): `CSTGCCInfo::
sCCInfoTable` (a 1200-byte static table + 7734-byte global ctor,
blocking `OnExtModeKnobAssignChange`/`OnExtModeSliderAssignChange`/
`SetControllerValue`), `CSTGMIDIClockSyncBase`+`CSTGIntMIDIClockSync`
(new base class + vtable, blocking `CSTGMIDIClockSync`'s own ctor
despite being small and float-math-tractable), `CSTGSmootherMapping`,
`CSTGStreamingEvent`+`CSTGHDRCircularBuffer`, `CSTGPianoTypes`+
`CFileStream`+`CSTGPianoModelPatch`+`CPianoOsc`, `CSTGEffectRackVars`+
`CSetListEQ`+`CSTGEffectRack`+`CLoadBalancer::LoadEffectCost` (six new
functions across five new classes, all from one 323-byte function).
Ended up picking 4 functions instead of the usual 6-10: `CSTGAudioBusManager::
LRBusIndivMirror`, `CSTGFrontPanelSmoothers`/`CSTGProgramSlot` ctors
(plus a newly-discovered embedded-sub-object dependency,
`CSTGToneAdjust`'s own ctor), and `CSTGSequence`'s ctor. Lesson for
future batches: when a batch's candidate pool at the "smallest
remaining" tier is dominated by heavy new-class dependencies, it's
better to do fewer, fully-verified functions than to force a 6-10 count
by taking on multiple new subsystems in one pass -- and it's worth
explicitly recording EACH rejected candidate's specific blocking reason
(not just "skipped, too complex") so a future batch doesn't re-do the
same disassembly work from scratch.

**New pattern found: distinguishing safe vtable-pointer INSTALLATION
from unsafe vtable DISPATCH when deciding if a zero-filled placeholder
vtable is acceptable.** This project's established convention already
allows zero-filled placeholder vtables for classes that are never
dispatched through (see bar2_stubs.cpp's own vtable section). This
batch sharpened the actual test to apply: a ctor that WRITES a vtable
pointer into an object (`*(unsigned int*)this = &_ZTVxxx + 8`) is always
safe to leave zero-filled, regardless of the class's own complexity --
the vtable is never READ AS A FUNCTION POINTER by that ctor. The
distinguishing danger is a `call *(*this)`-style DISPATCH within the
SAME function being reconstructed (or one already linked/reachable) --
that's when a real function pointer must exist. Concretely applied
this batch: `CSTGProgramSlot`/`CSTGToneAdjust`/`CSTGSequence`/
`CSTGHDRTrack`/`CSTGMetronomeSettings`'s own vtables were all safe
zero-filled placeholders (their OWN ctors only ever WRITE the pointer);
`CSTGComPort::RTAIInterruptHandler` was rejected specifically because
IT ITSELF dispatches twice through `*(this)`'s vtable (RX/TX byte
callbacks) -- a real crash risk, not deferred out of mere complexity.

**Recurring reminder, confirmed again this batch: changing a class's
own C++ declaration from a flat/opaque struct to a REAL `: public Base`
inheritance relationship can silently change already-passing test
counts, even in test files that never link the new real ctor body.**
`CSTGSequence : public CSTGCombi` means EVERY `CSTGSequence`
construction anywhere -- including a completely unrelated test file's
own trivial mock body for `CSTGSequence::CSTGSequence()` -- now
automatically triggers `CSTGCombi`'s ctor first (compiler-inserted base
construction, unconditional, regardless of what the derived ctor's body
does). This is NOT a bug in the new real ctor; it's the class hierarchy
change itself rippling into any OTHER test file that happens to
construct instances of the (now-more-correctly-related) classes. When
promoting a ctor whose real base class relationship was previously
modeled as a flat/opaque struct, grep every verify/*.cpp file that
constructs either the derived OR base class for existing call-counter
assertions, not just files that will link the new .cpp -- a real
`check_eq` FAIL is the reliable signal (it was here: `got=1993
want=1792`, delta exactly matching the confirmed instance count), but
better to anticipate it before running the suite blind.

**Sub-gotcha on host KAT poison-pattern discipline: 0xcc is a bad
default poison byte for testing an AND-mask/OR-mask read-modify-write
whose specific bit happens to already match 0xcc's own bit pattern
coincidentally.** `CSTGProgramSlot::CSTGProgramSlot()`'s own confirmed
`+0x43 &= 0xfd` (clear bit1) and `+0x45 |= 0x80` (set bit7) checks were
initially written against a blanket `memset(buf, 0xcc, ...)` poison --
`0xcc` = `0b11001100` already has bit1 clear and bit7 set, so both
"masked" checks would have passed even if the real AND/OR instruction
were silently dropped from the C translation (a false-negative-proof
test, not a real one). Fixed by poking those two specific bytes to a
value where the target bit is in the OPPOSITE state before construction
(`0xff` for the AND-clear check, `0x0c` for the OR-set check), so the
instruction's actual effect is what the assertion observes. When
writing a KAT for a real/confirmed bitwise read-modify-write (not a
plain unconditional write), don't rely on a single blanket poison
pattern for every field -- check whether that pattern happens to make
the specific bit(s) under test already look "correct" by coincidence.

**Batch 8 specifics** (2026-07-06, sec 10.154): another dependency-heavy
sweep, on par with batch 7 -- picked only 2 functions
(`CSTGPCMPrecacheManager::Reset`, `CSTGSmoother::CancelAllSmoothers`),
stub count 95 -> 93. Evaluated ~20 candidates in the 55-540 byte range;
most were blocked by genuinely new subsystems (`CSTGCCInfo::sCCInfoTable`,
`CSTGPatchMessageContext`, `CSetListEQ`, `CSTGMIDIClockSync`) already
flagged in prior batches, or by real vtable DISPATCH already reachable
from linked code (see next point). Full rejected-candidate list with
reasons is in sec 10.154 -- read it before re-doing this analysis.

**Sharpened finding on the ten `CSTGVoiceModel`-derived Model ctors**
(`CSTGOffModel`/`CSTGPCMModel`/etc, repeatedly rejected since sec 10.147
as "disproportionate structural cost"): fresh full disassembly this
batch found the ctors THEMSELVES (plus the shared base
`CSTGVoiceModel::CSTGVoiceModel(eSTGVoiceModelType)` and a new tiny
`CSTGVoiceModelManager::Register`) are actually small and fully
mechanical -- vtable install only, no dispatch WITHIN the ctors. The
REAL blocker, found only by tracing forward into the ctors' own callers:
`src/engine/engine_init.cpp`'s ALREADY-REAL `CSTGEngine::Initialize()`
immediately does `CallVtableSlot(new (...) CSTGOffModel(), 2)` (and the
same for all ten) right after constructing each one -- a real,
unconditional vtable slot-2 dispatch in code that's already linked and
reachable. This is the sec 10.153 "vtable install vs. dispatch" rule's
danger case, just ONE LEVEL REMOVED from the ctor being promoted: the
ctor itself never dispatches, but a caller that's already real DOES,
immediately, on the very vtable the ctor would install. Promoting these
ctors with zero-filled placeholder vtables would be a confirmed new
crash (call through a null function pointer) the moment `Initialize()`
runs on real hardware -- strictly worse than the current stub state
(empty ctor leaves whatever pre-existing bank-memory bytes were there,
an already-acknowledged Bar-2 gap, not a newly-introduced deterministic
one). Lesson for future batches: when checking whether a "vtable install
only" ctor is safe to promote, don't just check the ctor's own body --
also check whether any ALREADY-REAL, ALREADY-LINKED caller immediately
dispatches through that same vtable right after constructing the object
(here, `engine_init.cpp`'s `CallVtableSlot` calls sit right next to the
ten `new (...) CSTGXxxModel()` call sites, easy to miss if you only
disassemble the ctor itself and not its construction call site).

**Confirmed-safe pattern for CSTGPCMPrecacheManager::Reset's own
allocator-form quirk**: a function that reads its OWN old state (the
previous element count) BEFORE overwriting it with a new value, then
uses that OLD value to pick between two different (de)allocator forms
(scalar `new`/`delete` for exactly one element vs. array `new[]`/
`delete[]` otherwise) is a real, faithfully-preservable quirk, not a
compiler artifact to simplify away -- confirmed by writing a 4-step KAT
that specifically drives the count sequence 0 -> 3 -> 1 -> 0 to exercise
BOTH delete-form branches (delete[] on the freed count-3 array, then
plain delete on the freed count-1 scalar element), not just the "obvious"
alloc-side paths.

**Return-type correction precedent, reused**: `CSTGPCMPrecacheManager::
Reset()`'s real disassembly ends in `mov eax,0x1` before `ret` (same
shape as `Initialize()`, and matching `AfterProcess()`'s own already-
on-record 2026-07-01 "guessed void, actually bool" correction in
`process_oacmd.cpp`) -- fixed to `bool` in both `oa_setup_global_resources.h`
and `process_oacmd.cpp`'s own separate local redeclaration of the same
class. Not counted as a "newly found bug" since it's the exact same
class of guess this project had already flagged and fixed once for a
sibling method in the same class.

**New reminder: pointer-mangled-type mismatches across this project's
own multiple incompatible declaration ecosystems for the same class name
are sometimes already pre-existing, not something to fix mid-batch.**
`CSTGPCMPrecacheManager::Reset`'s real ground-truth mangled name is
`_ZN22CSTGPCMPrecacheManager5ResetEbbi` (bool,bool,**int**), but this
project's OWN header (`oa_setup_global_resources.h`) and its OWN
separate local redeclaration in `process_oacmd.cpp` both already used
`unsigned long` for the third parameter (`Ebbm`) -- self-consistent
across this project's own ecosystem (so linking still works, since only
this project's own TUs need to agree with each other, never with
OA_real.ko directly) but technically mismatched vs. the real binary.
Functionally identical on `-mregparm=3`/x86-32 (both 4 bytes). Left
as-is (documented, not changed) rather than churning three files for a
zero-behavior-impact cosmetic type difference -- a judgment call, not an
oversight; revisit only if a future pass has an actual reason to touch
all three files anyway.

**Batch 9 specifics** (2026-07-06, sec 10.155): picked 5 functions
(`CSTGControllerRTData`/`CSTGHDRMiniModel` ctors, `CSTGSlotVoiceData`
ctor + its own two newly-discovered embedded-sub-object dependencies,
`CSTGMidiCCFilter::Initialize()`/`CSTGHeldKeyList::CSTGHeldKeyList()`),
stub count 93 -> 88. All five confirmed to have NO vtable at all (`nm -C
OA.ko | grep "vtable for <Class>"`, zero matches for any of the five) --
the easiest possible resolution of the sec 10.153 "install vs. dispatch"
question, since there was no vtable to reason about either way. Full
rejected-candidate list (deeply-hashed per-instrument tables, SSE-math
functions with 6+ unresolved externals, vtable-dispatching
`RunMonitors()`, `CSTGCCInfo::sCCInfoTable`-blocked siblings, a
12+-function dependency cluster, and a brand-new-class blocker for
`CSTGVoiceAllocator::Initialize()`'s own `CSTGVoice::CSTGVoice(short)`)
is in sec 10.155 -- read it before re-doing this analysis.

**New gotcha this batch: naive base-name-only symbol matching when
sizing candidates from `nm -C --size-sort` picks the WRONG overload if
the stub file's own remaining stub is a DIFFERENT overload from an
already-real sibling with the same base name.** A first-draft sizing
script matched `CSTGMidiDispatcher::HandleController` by class+method
name only and picked the smallest of TWO real overloads (75 bytes) --
but that 75-byte one is the ALREADY-real 3-arg pointer version (`const
unsigned char*, eSTGMidiSource, eSTGMidiTargetPerformance`, sec 10.139,
`midi_dispatcher.cpp`); the STILL-stubbed one in `bar2_stubs.cpp` is a
different 5-arg byte-based overload, 5583 bytes -- nowhere near
"smallest remaining." Fixed by re-matching with argument-COUNT
awareness (parse the stub's own declared parameter list, compare count,
not just name) before trusting any nm-derived size ranking. Caught
before wasting a disassembly pass on a huge function mistaken for tiny.

**Second new gotcha, a genuine own-draft bug in a header comment, caught
before commit (not a real-binary quirk): simple hex addition errors are
easy to make when computing an embedded sub-object's own END address by
hand.** Computed `CSTGHeldKeyList`'s embedded end address as `+0x1e80 +
0xa0c` and got `0x2a8c` during initial analysis -- WRONG (the correct
sum is `0x288c`; `1E80+0A0C`: `80+0C=8C`, `E+A=18` write `8` carry `1`,
`1+0+1=2` -> `288C`). This wrong sum then got propagated into a
comment AND into a test's own buffer-size assumption before being
caught by re-deriving the same arithmetic a second time from scratch --
not by a test failure (the test buffer happened to be large enough
regardless, so this would NOT have been caught by the KAT; only
independent re-derivation caught it). Lesson: when computing a
multi-hundred-byte struct's own confirmed end offset via addition,
recompute it a second independent way (or just add the two hex numbers
digit-by-digit twice) before trusting it in a comment or a buffer-size
calculation -- a KAT with a generously-oversized buffer will NOT catch
an arithmetic slip in a boundary comment.

**Third gotcha, a genuine own-draft bug in a KAT (not the reconstruction
itself), caught before commit by re-reading the poison-pattern
discipline note rather than by a test failure:** a per-entry AND/OR-mask
RMW check (`(orig|1)&~2` on a 121-entry array's own `+0xb` byte) was
initially written as `check_eq(..., (e[0xb] & 0x3), 0x1)` -- masking
only the two bits under test. Since the blanket `0xcc` poison already
has bits 0-1 clear, this check would have passed even if the real `|1`
semantics were accidentally dropped and replaced with a plain
unconditional `flags = 1` bug (both produce the same masked-bit result,
`0x1`). Fixed by (a) poking each entry's own `+0xb` byte to `0xff`
first specifically so bit 1's clearing is actually exercised, and (b)
asserting the FULL resulting byte (`0xfd`), not just the masked bits --
the same "assert the whole byte, not just the bit(s) you think you're
testing" discipline as sec 10.153's own poison-pattern note, but this
time the fuller lesson is: even with a good poison BYTE, a WEAK
ASSERTION (masking to just the bits of interest) can still hide the
exact class of bug the poison byte was meant to catch.

**Important, unrelated-to-this-batch finding: two host-KAT suites
(`test_global`, `test_setup_global_resources`) crash on this build host
(192.168.3.92), confirmed via `git stash` to reproduce IDENTICALLY at
HEAD/026242b (sec 10.154's own commit) before any of this batch's work
was applied -- a pre-existing issue, not a regression from batch 9.**
Reproducible even with ASLR disabled (`setarch $(uname -m) -R`), so not
address-randomization flakiness. This directly contradicts sec 10.154's
own concluding claim of "47 host suites pass, 0 FAIL" -- worth
investigating in a future pass (or flagging to the user) since it means
the project's own "must not regress" baseline has quietly already
regressed by 2 before this batch even started. `stdbuf -oL` (line
buffering) is essential for locating the TRUE crash point in any test
binary whose output is redirected to a file/pipe -- plain redirection
fully-buffers stdout, so the crash can appear to happen many lines
"later" than where the last flushed output shows, silently swallowing
several KB of already-executed `ok` lines. Located this pass:
`test_global` crashes in its own "[30] Sec 10.92 batch" section's
`RepeatLastPerformanceChange` test; `test_setup_global_resources`
crashes during its own count-3 `CSTGPCMPrecacheManager::Reset()`
array-allocation case. Left uninvestigated further this batch (out of
scope for a stub-sweep pass) -- when counting "N suites pass" in any
future batch, always run the FULL suite list with real exit-code
checking (not a `grep -i FAILED` on stdout, which has its own false-
positive risk too -- see next paragraph) rather than trusting a
previous batch's own stated count.

**Fourth gotcha: a blind `grep -i FAILED` (or `FAIL\b`) over test stdout
produces false positives when a test's own descriptive label text
happens to contain the word "fail" as part of an intentionally-named
failure-injection SCENARIO, not an actual assertion failure.**
`test_init_module`'s own labels like "Failure at step 5 (InitializeSTGHeap,
the EARLIEST hard-fail gate)" are PASSING check descriptions (the test
deliberately verifies behavior when an injected dependency fails) --
grepping for "fail" flagged this test as broken when it was not. The
reliable signal is the test binary's own EXIT CODE (0 = all checks
passed for every test in this project's own convention) plus, if
wanted, a grep for this project's own literal `"FAILED:"` (with the
trailing colon, this project's own `check_eq` helper's exact failure-
line prefix) rather than a bare substring match on "fail".

**STANDING RULE, promoted to top priority after a real incident (2026-07-06,
sec 10.156): verification must ALWAYS check each test binary's actual
process exit code, NEVER just grep a build log for "FAIL"/"FAILED"
text.** `make all`'s own `verify:` Makefile target
(`@for t in $(TESTS); do echo "== $$t =="; ./$$t; done`) has no `set -e`
and never inspects `$?` -- a test binary that SEGFAULTS mid-run (exit
139) dies silently mid-output with no "FAIL" string ever printed, and
the `for` loop just moves on to the next binary with zero visible
error. This exact gap let `test_global` and `test_setup_global_resources`
segfault on EVERY batch from `8eca6a9` through `026242b` (6 consecutive
batches, sec 10.149-10.154) while every one of those batches' own
concluding reports claimed "all host suites pass, 0 FAIL results" --
each one was telling the truth about what it grepped, but grepping the
wrong signal. Sec 10.155 finally noticed the crashes (by chance, running
a suite directly) but left them uninvestigated; sec 10.156 root-caused
and fixed both. **Going forward, the verification sequence above MUST be
amended**: after `make all`, loop over `verify/test_*` INDIVIDUALLY
(`for t in $(ls verify | grep -v '\.'); do "$t" >/tmp/out 2>&1; echo
"$t: exit=$?"; done`) and require exit 0 from every single one -- a
build-log grep (for "FAIL", "FAILED:", or anything else) is NEVER
sufficient on its own and must not be the sole basis for a "N suites
pass" claim in any future batch report. Also always `stdbuf -oL` (or
loop-redirect per-binary as above) when diagnosing an actual crash --
plain full-buffered redirection silently swallows the last several KB
of already-printed "ok" lines before a crash, making the crash look like
it happened much earlier than it did (rediscovered/reconfirmed in sec
10.156 investigating both bugs).

**Sec 10.156 bug-fix specifics: a segfault's "first bad commit" from an
independent bisection is not automatically the same as "the bug you
were told to look for."** The orchestrator's own bisection for
`test_global` named `8eca6a9` (the `EmergencyFreeVoiceList` mock
rewrite) as the introducing commit, and that commit DOES contain a real,
independent bug (a KAT setup omission: `voice0+0x44`/`+0x50`, the two
list-heads `EmergencyFreeAllVoices()` itself walks, were never wired to
the test's own `node0`, only the OUTER `buf+0x29c9904` locate-list was --
so `FreeVoice()` was silently never exercised). But it was NOT the first
thing gdb hit on a fresh run: fixing that bug alone still crashed,
earlier, in a completely different function (`GetNumWritableBytes()`,
introduced in the LATER commit `37e79db`) via a `CSTGMidiPortManager::
sInstance` dangling-pointer gap in an EARLIER test section (`[30]`, `Sec
10.92 batch`) that never set up its own valid ring-buffer instance and
silently inherited a freed pointer from the section before it. Fixing
THAT then exposed a THIRD, structurally-identical gap in
`EmergencyFreeDyingSlotVoiceData` (`CSTGVoiceAllocator::sInstance`, same
missing-own-setup pattern, this time for the `8eca6a9` commit itself).
Three independent bugs were stacked, each masking the next; a
segfault-exit-code bisection only ever surfaces the FIRST one on the
call stack, so "commit X's bisection is the first bad commit" does not
mean "commit X's own diff is where the fix belongs" -- keep fixing and
re-running until the binary reaches an actual `check_eq` assertion (pass
or fail) instead of a signal, and re-verify the bisection's OWN claimed
introducing commit by literally diffing that commit's own touched
lines, not just trusting the commit message summary.

**Sec 10.156 also found (and fixed) a THIRD class of the same
missing-own-setup bug, latent but not yet crashing on this host**: once
sections `[30]`'s cascade above was fixed, sections `[32]`
(`HandleMidiPerformanceChange`), `[37]`
(`HandleMidiBankAndPerformanceChange`), and `[41]`
(`IncrementPerformance`/`DecrementPerformance`) turned out to have the
EXACT SAME `CSTGMidiPortManager::sInstance`-never-locally-established gap
(all reach `SubmitPerfChangeRequest()`/`GetNumWritableBytes()`, none set
up their own ring buffer, all silently inherit a stale pointer from
whichever section ran last). These did not segfault in this specific
run/build, only because a coincidentally-still-mapped OTHER buffer
happened to occupy the same now-freed virtual address by the time each
one ran -- a genuinely nondeterministic near-miss (`mmap32()`/`MAP_32BIT`
address reuse is layout-dependent, not guaranteed), not a real fix.
**Lesson for any future batch that promotes a mock to a real body with a
global-static `sInstance`-style dependency**: grep EVERY call site of
every function that transitively reaches the newly-real dependency
across the WHOLE test file (not just the one section being edited) and
verify each one establishes its own fresh, valid instance rather than
assuming "it hasn't crashed so far" means the dependency chain is
actually satisfied -- silent address-reuse luck is not a substitute for
an explicit per-section setup, and this project's own established
per-section convention (see every OTHER `midiPortMgr*`/`voiceAlloc*`
variable in `test_global.cpp`) already expects exactly that.

**Batch 10 specifics** (2026-07-06, sec 10.157): picked
`CSTGVoiceAllocator::Initialize()` (719 bytes) + its own newly-discovered
dependency `CSTGVoice::CSTGVoice(unsigned short)` (375 bytes, `CSTGVoice`'s
FIRST real method ever -- previously just an opaque forward decl used as
a pointer target). Stub count -1 (90->89 by this project's own
`grep -cE '^\S.*\{\}$'` convention -- only ONE actual stub body removed,
confirmed via `git diff`; `CSTGVoice` never had its own stub to begin
with since it was a from-scratch new class, not a promoted mock).
Rejected ~7 candidates in the 400-1300 byte tier, each freshly
disassembled and blocked for a genuinely new reason (full detail in sec
10.157): `CSTGSmoother::FinalizeAllSmoothers` (same
`CSTGSmootherMapping::DispatchSmoothedValue` blocker as `FinalizeSmoother`,
sec 10.153 -- and fresh disassembly of `DispatchSmoothedValue` itself,
1343 bytes, confirms it's a 6-way dispatch cluster including
`CSTGPatchMessageContext` and `CSTGCCInfo::sCCInfoTable`, not tractable
either), `CSTGControllerRTData::ResetSendKnobsJumpCatch` (5-sibling
deeply-hashed-table cluster, same shape as `SetAudioInSolo`),
`CSTGAudioBusManager::MixPerformanceOutputs` (6 not-yet-reconstructed
calls across 2 classes), `CSTGMidiDispatcher::ResetAllControllers`
(`CSTGCCInfo::sCCInfoTable` + a dozen-method cluster across 6 classes),
`CSTGMidiPortManager::Initialize` (vtable DISPATCH on 2 external
singletons + 2 new `CSTGMidiQueue` methods + a `CSTGHeapManager`-indexed
lookup pattern), `CSTGProgram::CSTGProgram` (2 brand-new classes,
`CIFXEffectSlot` x10 + `CMFXEffectSlot`).

**Key technique this batch, worth reusing**: when hand-deriving a
complex multi-array function (3 separate intrusive lists here) purely
from raw disassembly with NO working decompiler available (Ghidra MCP
timed out twice on this ~14MB binary, `load_binary` never completed in
600s -- abandoned rather than keep retrying), the single most valuable
cross-check was **re-reading the TARGET class's own already-written
header comment from a PRIOR, unrelated reconstruction pass** (here,
`CSTGVoiceAllocator`'s own ctor, sec 10.147, already documented exactly
which array regions existed at which offsets/strides, `selfRefNodes[50]`/
`ownerBackRefRecords[400]`/`_unrecovered_bigArray[400]`, and for two of
the three even the exact field offsets the ctor itself zeroes). Every
one of this pass's independently-hand-derived node struct offsets landed
EXACTLY on fields the ctor's own comment had already flagged as zeroed
baseline state -- strong, cheap, independent evidence the fresh decode
was right, found by cross-referencing existing project documentation,
not by re-deriving everything from raw bytes alone. When a function
operates on a class some EARLIER, unrelated pass already partially
reverse-engineered, always re-read that class's own header comment FIRST
before hand-tracing a complex loop from scratch -- it may already
independently confirm (or contradict) field offsets you're about to
derive the hard way.

**Sharpened finding on the "host/target pointer-width" gotcha (sec
10.156's own class of bug), a NEW angle**: a KAT for a ctor that only
WRITES self-referential/internal pointers (never reconstitutes and
DEREFERENCES them) is safe with a plain heap `new[]`-backed test object,
even on a 64-bit host where that heap address may exceed 4GB -- truncated
32-bit comparisons against ANOTHER equally-truncated value from the SAME
object still match correctly (this is exactly why `test_managers.cpp`'s
pre-existing `[19]` ctor-only test, using plain `poison_and_construct`,
was already safe). But the MOMENT a function actually WALKS a linked
structure by reconstituting a stored 32-bit pointer back into a real
address and dereferencing it (this batch's `Initialize()`, unlike the
ctor it constructs on top of), the test object itself must ALSO live in
the low 4GB (`MAP_32BIT` `mmap`), not just the individual buffers it
allocates internally -- otherwise a >4GB self-address silently truncates
into a wild pointer on the very first list hop. Distinguish "does this
specific function dereference a pointer it reconstitutes from its own
packed field" (needs `MAP_32BIT` for the WHOLE object) from "does it only
ever write such pointers" (plain heap `new[]` remains fine) on a
per-function basis, not per-class.

**Batch 11 specifics** (2026-07-06, sec 10.158): picked
`CSTGCDWorker::ProcessCommands()` (124 bytes) + its own newly-discovered
dependency `CSTGHDRCircularBuffer` (a complete ~52-byte ring-buffer class,
all NINE real methods 4-88 bytes each, all reconstructed this pass --
embedded inside `CSTGHDRManager` at the already-documented `+0x189c8`
offset, sec 10.148), plus `CSTGStreamingEventManager`'s ctor/`Initialize()`
(156+200 bytes) + its own newly-discovered dependency `CSTGStreamingEvent`
(72 bytes, brand-new class, NOT modeled via real C++ inheritance from
`CSTGAudioEvent` -- same "derived fields overlap the base's own confirmed
tail" reasoning as `CSTGPlaybackEvent`, sec 10.150). Stub count 89 -> 86
(net -3; `CSTGHDRCircularBuffer`/`CSTGStreamingEvent` were from-scratch
classes, not promoted stubs, so no new placeholders introduced).

**New technique, worth repeating: when a "smallest remaining" candidate's
own real external call target is ITSELF tiny (here, `CSTGHDRCircularBuffer::
IncrementAvailableReadBytes`, only 4 bytes), immediately check that
dependency's WHOLE class via `nm -C --size-sort | grep ClassName::` before
assuming it's a one-off extern to stub out.** This found NINE total
methods, all 4-88 bytes, all tractable -- reconstructing the whole class
was barely more work than the one method that was actually needed, and
avoided leaving a half-modeled dependency for a future batch to redo the
same disassembly pass on.

**New gotcha (a genuine own-TEST bug, caught by a real `FAILED` line, not
by re-reading the disassembly a second time): don't assume a ctor zeroes
EVERY scalar field just because most of them are zeroed.**
`CSTGHDRCircularBuffer`'s ctor zeroes ten fields but confirmed (via full
disassembly -- exactly ten `mov [x],0` stores, no more) to never touch
`availableReadBytes`/`availableFillBytes` -- a real, harmless gap (every
real caller runs `Initialize()`/`Reset()`/`SetEffectiveSize()` first,
all three of which DO set both). First KAT draft assumed full-zero and
got `got=0xcccccccc want=0x0` on both fields -- fixed the TEST's own
expectation (poison value, documented as a confirmed gap) rather than
adding fake zeroing to the reconstruction. Lesson: count the ACTUAL number
of zero-stores in the disassembly before writing a ctor KAT's "everything
else should be zero" assertions -- don't extrapolate from "most fields are
zeroed" to "therefore all are."

**New gotcha, a real ripple effect from giving a class a genuine typed
array member of a non-POD class type (distinct from the sec 10.153
inheritance ripple-effect gotcha, but the same family): once
`CSTGStreamingEventManager` got a real `CSTGStreamingEvent events[401]`
member, EVERY construction of `CSTGStreamingEventManager` anywhere --
including a completely unrelated test file's own trivial mock ctor --
automatically invokes `CSTGStreamingEvent`'s own constructor 401 times
(C++ member construction is mandatory, not conditional on what the
enclosing ctor's OWN body does, same principle as base-class construction
sec 10.153 already flagged, but here for a MEMBER not a BASE).**
`verify/test_engine_init.cpp`'s own pre-existing trivial
`CSTGStreamingEventManager::CSTGStreamingEventManager() { sInstance =
this; }` mock suddenly needed a `CSTGStreamingEvent::CSTGStreamingEvent()
{}` mock added too, purely because of the header change, with ZERO edits
to the mock ctor's own body. When adding a real typed array/member of
class type to a struct that ALREADY has mocks elsewhere in the codebase,
grep every file that constructs the ENCLOSING class (not just files that
link the new member's own real .cpp) for a similar gap.

**New gotcha: `sInstance` storage placement matters not just for
"where's the real ctor" but for "which verify/ files link this TU
directly."** `CSTGStreamingEventManager::sInstance`'s storage originally
lived in `engine_init.cpp` (from whichever prior pass first forward-
declared the class) -- `engine_init.cpp` is linked DIRECTLY by
`verify/test_engine_init.cpp` (unlike `managers.cpp`, which that same
test deliberately does NOT link, per its own header comment). Promoting
the real ctor/`Initialize()` into `engine_init.cpp` would have forced
`test_engine_init.cpp`'s carefully-isolated "mock-everything" test to
actually exercise the real body (and transitively `CSTGHDRCircularBuffer`/
`CSTGStreamingEvent`/`CSTGAudioEvent` linkage) -- solved by giving the
class its own brand-new TU (`streaming_event_manager.cpp`) instead,
matching the `CSTGControllerRTData`/`CSTGFrontPanelSmoothers`/
`CSTGHDRMiniModel` precedent (sec 10.150/10.153/10.155) of homing
`sInstance` alongside a class's own real ctor -- but the NEW angle here is
realizing that moving `sInstance` storage OUT of a file also means
checking every verify/ file that used to get that storage "for free" from
the old file now needs its own LOCAL storage definition instead (added to
`test_engine_init.cpp`, one line, matching the `CLoadBalancer`/
`CSTGMetronome`/`CSTGTempoUtils` local-storage precedent already used
there for other classes). Before relocating any class's `sInstance`
storage to a new/different TU, grep every verify/ file for that exact
`ClassName::sInstance` reference AND check its own Makefile link line to
see if it was relying on the OLD file for that storage.

**Recurring reminder, confirmed again: a vtable byte array's REAL storage
living in `bar2_stubs.cpp` (or wherever) does not help a verify/ test that
doesn't link that file -- every isolated test needs its OWN local
`unsigned char _ZTVxxx[N];` definition satisfying the same `extern "C"`
declaration.** `_ZTV18CSTGStreamingEvent`'s real storage went into
`bar2_stubs.cpp` (matching the `_ZTV14CSTGAudioEvent`/
`_ZTV17CSTGPlaybackEvent` precedent, since its own real ctor lives
alongside classes constructed at `CSTGEngine::Initialize()` time, same
"cluster" as those two), but `test_managers.cpp` (extended this batch to
link the new `streaming_event_manager.cpp`) doesn't link `bar2_stubs.cpp`
either -- needed its own local `unsigned char
_ZTV18CSTGStreamingEvent[40];`, exactly matching how `test_engine_init.cpp`
already handles this for the other three vtables in the same family.

**Batch 12 specifics** (2026-07-06, sec 10.159): picked
`CSTGWaveSequence::CSTGWaveSequence()`/`CSetList::CSetList()`,
`CSTGMidiPortManager::WriteSTGMidiOutQueue()`/`NotifyNKS4TestMode()`, and
their newly-discovered dependency `CSTGMidiQueue::Reset()` (36 bytes) --
5 functions, stub count 86 -> 82 (net -4; `CSTGMidiQueue::Reset()` was a
brand-new method on an already-existing class, not a promoted stub, so
no new placeholder was ever introduced for it).

**New technique this batch, worth reusing every time `nm` comes up
empty for a stub's own symbol**: `CSTGWaveSequence::CSTGWaveSequence()`/
`CSetList::CSetList()` have NO standalone symbol anywhere in
`OA_real.ko` at all (confirmed via a whole-symbol-table grep, zero
hits) -- both fully INLINED by the compiler at their one real call
site, `CSTGGlobal::CSTGGlobal()`'s own two array-construction loops
(already partially ground-truthed by an EARLIER pass, `global_ctor.cpp`).
Disassembling that ALREADY-reconstructed caller directly (rather than
searching for a ctor symbol that doesn't exist) found the two
vtable-pointer-install instructions (`movl $0x8,(%ecx) ; reloc
_ZTVxxx`) at the exact loop bodies already documented in
`global_ctor.cpp`'s own header comment -- confirming both ctors are the
sec 10.153 "vtable install only" safe pattern, and giving their
vtables' confirmed real 96-byte sizes (`readelf`) for free. Rule of
thumb: when `nm -C --size-sort` genuinely can't find a stub's own
symbol (not just a size-matching mistake, sec 10.155's own overload
gotcha) -- check whether it's inlined into an ALREADY-reconstructed
caller's disassembly before writing the candidate off as
"unreconstructable this way."

**Second technique, a "re-check an old deferral" win**: two functions
(`WriteSTGMidiOutQueue()`/`NotifyNKS4TestMode()`) had ALREADY been fully
disassembly-confirmed by a much earlier pass (their own header comments
in `oa_engine.h` documented exact real semantics already) but were left
stubbed for two DIFFERENT reasons that both turned out to be resolvable
by techniques this project had ALREADY established elsewhere by the
time this batch ran: (1) `WriteSTGMidiOutQueue`'s stated blocker
("test_global.cpp's existing mocks would need rewiring onto a single
counter") dissolves via the exact same "give it its own dedicated TU,
don't touch existing test mocks" trick that had ALREADY promoted its
own sibling, `CSTGMidiQueueWriter::Write()` (sec 10.83), in the very
same class family -- the older comment just hadn't been re-examined
against that precedent. (2) `NotifyNKS4TestMode`'s stated blocker
("indirect calls through fields of a not-yet-identified structure")
dissolves because the "structure" is `oa_heap_base()`/`oa_heap_region()`
(already in `oa_heap.h`) and the "indirect calls" are direct calls to
`CSTGMidiQueue::Reset()`, a class that plain didn't exist in this
project yet when that comment was written. Lesson: a stub's own
"deliberately deferred" comment is a snapshot of what was true AT THE
TIME it was written -- re-verify its stated reason against everything
this project has ALREADY built since, not just against what's newly
discoverable this pass.

**Recurring ODR-conflict gotcha, hit again (not new, but worth
reinforcing): `oa_heap.h` cannot be `#include`d in the same TU as
`oa_global.h`/`oa_engine.h`.** `oa_heap.h` transitively pulls in
`oa_types.h`'s minimal `struct CSTGGlobal { static char *sInstance; };`,
which conflicts with `oa_global.h`'s fuller `class CSTGGlobal` (a
DIFFERENT type for the same static member) --
`oa_setup_global_resources.h`'s own header comment already documents
this exact hazard. Any NEW file that needs both the real
`CSTGGlobal`/engine classes AND `CSTGHeapManager::sInstance` must
re-derive `oa_heap_base()`/`oa_heap_region()`'s own formulas locally
(private `static` functions) plus declare its own local minimal
`struct CSTGHeapManager { static char *sInstance; };` stand-in
(matching `midi_dispatcher.cpp`'s own established convention -- same
real storage, defined once in `heap_manager.cpp`, never redefined) --
do NOT reach for `#include "oa_heap.h"` as a shortcut the moment
`oa_global.h` is also needed in the same file.

**Batch 13 specifics** (2026-07-06, sec 10.160): picked
`CSTGSamplingInterface::CSTGSamplingInterface()` (910 bytes) and
`CSTGSamplingDaemon::ProcessCommands()` (120 bytes) -- 2 functions, stub
count 82 -> 80. An unusually dependency-heavy sweep on par with sec
10.153/10.154/10.157 -- evaluated ~20 candidates in the 85-540 byte tier;
most were freshly re-confirmed still-blocked by `CSTGCCInfo::
sCCInfoTable`/`CSetListEQ`/`CSTGPatchMessageContext` (all three: `grep
-rn` returns zero hits in this project still), or by a NEWLY-confirmed
vtable-dispatch danger on `CSTGProgramSlot::ChangeProgram` (`call
*0xe0(*this)`, slot 56 of a currently-12-byte/3-slot placeholder vtable
-- a confirmed new crash risk, rejected per the sec 10.153 "install vs
dispatch" rule). Full rejected-candidate list with reasons in sec 10.160
-- read it before re-doing this analysis.

**Key technique this batch, worth reusing: byte SIZE of a candidate ctor
is not a proxy for RISK -- always check for `call`/indirect-dispatch
instructions before assuming "big means complex, means skip."**
`CSTGSamplingInterface::CSTGSamplingInterface()` is 910 bytes (by far the
biggest ctor promoted so far in this sweep) but turned out to be the
SAFEST possible category: a full disassembly dump showed ZERO branches
and ZERO calls of any kind -- confirmed via `readelf -r` showing only
TWO relocations in the whole function (vtable-pointer install, `sInstance
= this`). When triaging a large candidate, don't reject on size alone --
grep its own disassembly for `call` first; a huge but branch-free/call-
free ctor is mechanically trivial (just tedious to transcribe), the
polar opposite of a small-but-vtable-dispatching function like
`ChangeProgram` above.

**Second technique: when a `ProcessCommands()`-family sibling cluster has
already been rejected as "all dispatch through vtables/PTMF tables," still
re-check EACH sibling individually via a full disassembly, not just the
one or two already traced.** `CSTGSamplingDaemon::ProcessCommands()` is
the SAME ring-consumer shape as `CSTGFileCloser`/`CSTGHDRFileReader`/
`CSTGHDRFileWriter`/`CSTGStreamingFileReader::ProcessCommands()` (all
five share sec 10.14's original "file daemon" grouping) -- but it's the
ONLY one of the five with no `call` instruction anywhere in its own body
(its two "dispatch" cases are actually just cross-object field writes and
ring pushes into an ALREADY-typed `TSTGArrayManager<CSTGRecordBuffer>`
and a raw-offset `CSTGFileCloser::sInstance` ring). A blanket "this whole
cluster dispatches through vtables" conclusion from tracing only
`CSTGFileCloser`/`CSTGHDRFileReader` would have wrongly kept this one
blocked too -- confirmed only by independently disassembling and
grepping for `call` in ALL FIVE siblings' own bytes, not generalizing
from a partial sample.

**New GCC-specific gotcha (a genuine own-test pitfall, not a
reconstruction bug): explicit specialization of a TEMPLATE class's
static data member needs an EXPLICIT INITIALIZER to actually emit
storage.** The established "give this file its own local storage" fix
for a class static (sec 10.158's vtable-array precedent) is `template<>
TSTGArrayManager<CSTGRecordBuffer> *TSTGArrayManager<CSTGRecordBuffer>::
sInstance;` -- but GCC 12.2.0 (confirmed via a minimal standalone
reproduction, not just inference from the error) treats this EXACT form,
with no initializer, as a non-defining DECLARATION (shows as undefined
`U` in `nm`, not `B`/bss) -- silently, no warning, no error, just an
`undefined reference` at final link time against whichever OTHER TU
happens to need the real definition. The fix is trivial once found: add
`= 0;`. This is SPECIFIC to explicit specializations of template statics
-- an ordinary (non-template) `T *Class::member;` at namespace scope is
always a full definition either way, which is exactly why this exact
shape of gotcha never surfaced in the twelve prior batches' worth of
local `sInstance`/vtable-array storage additions for plain (non-template)
classes. Whenever a NEW local-storage definition is for a template
class's static member (`TSTGArrayManager<T>`-style, `TListNode<T>`-style,
etc, not a plain class), always write `= 0;` explicitly -- don't copy the
bare-declaration form that works fine for every ordinary class in this
project.

**Reminder, reconfirmed: adding a real function that references an
external symbol requires checking not just files that link its OWN
straight source file, but every OTHER `verify/*.cpp` that links the SAME
already-existing source file the new code lives in.** Promoting
`CSTGSamplingDaemon::ProcessCommands()` inside `managers.cpp` (an
existing, heavily-shared file) meant FIVE separate `verify/` binaries
needed attention: `test_managers.cpp` (new KAT added), `test_engine.cpp`
(had its own stale mock, now removed + its own expected call-order string
updated, exactly the sec-established "recurring gotcha: missed mock
promotions" pattern), and THREE more (`test_global.cpp`/
`test_global_ctor.cpp`/`test_engine_startup_bits2.cpp`) that don't mock
the symbol at all but still link `managers.cpp` directly and therefore
needed the new template-static local-storage line purely to keep
linking. `grep -l managers.cpp` (or the Makefile's own link lines) across
the WHOLE `verify/` directory is the reliable way to enumerate every
affected binary -- checking only the file with the new KAT would have
missed 4 of the 5.

**Batch 14 specifics** (2026-07-06, sec 10.161): resolved
`CSTGCCInfo::sCCInfoTable` -- the recurring blocker explicitly flagged
across sec 10.153/10.154/10.155/10.157/10.160 and this batch's own
assigned priority target -- plus its two now-unblocked consumers,
`CSTGControllerRTData::OnExtModeKnobAssignChange()`/
`OnExtModeSliderAssignChange()`. Stub count 80 -> 78.

**Key technique: a "global constructor" symbol in `nm`/`objdump` is not
automatically unrecoverable just because it's BSS+ctor rather than
`.rodata`.** `sCCInfoTable`'s own populating function (7734 bytes, 2219
instructions) is 100% branch-free/call-free (only `movb`/`mov`/`lea`/
`movzbl`/`ret`) -- the exact same "safe by instruction-class, not by
byte-size" category as sec 10.160's `CSTGSamplingInterface` ctor, just
an order of magnitude bigger. For a function this size, DON'T
hand-transcribe -- write a small Python script that parses
`objdump -dr`'s own instruction+relocation stream and mechanically
REPLAYS the writes (tracking the 1-2 registers actually involved) to
produce the final byte state directly. This is dramatically faster and
more reliable than reading 2219 lines by eye, and gives a free
correctness proof for free (assert zero offset collisions, confirm the
total written-byte count matches the symbol's own confirmed size minus
whatever's provably left at its zero default). Once a large ctor's
entire effect is proven to reduce to a deterministic byte array (every
register value traced back to a literal), reproduce it as a plain
initialized array in the reconstructed source, NOT as a transliterated
sequence of store statements -- both are behaviorally identical, but the
array form is vastly more reviewable and the whole point of doing this
kind of extraction in the first place.

**New gotcha: this project's Makefile has TWO separate, easy-to-conflate
source lists** -- a `SRCS`-shaped list (actually unnamed in this
Makefile, used for the host-side "make all" test-build dependency
tracking) starting around line 170, and `OA-objs` (the ACTUAL kernel
module's own object list that `make ko` links) starting around line 33.
Adding a new source file's `.cpp` to the first list only (which is what
sits visually closest to `src/engine/global.cpp`'s own line, easy to
edit by pattern-matching proximity) lets `make all`/host tests build and
pass completely fine, while `make ko` silently produces a kernel module
still missing that translation unit's linkage --
`_ZN10CSTGCCInfo12sCCInfoTableE` showed up as a genuine new entry in
`nm -u OA.ko` (32 -> 33) even though every host suite passed. This is
exactly why the unresolved-symbol-count invariant check exists as a
SEPARATE, mandatory step from "did the build exit 0" -- caught here on
the very first invariant check after the first `make ko`, before
wasting time debugging test failures that didn't exist. When adding a
brand-new `.cpp` file to the kernel module (not just a `verify/`-only
helper), grep the Makefile for `OA-objs` specifically and confirm the
new `.o` is listed there, in addition to (not instead of) whatever
host-build dependency list also needs it.

**New gotcha, caught by a real host-KAT FAILED line (own-test bug, not a
reconstruction bug): a comment's own field-numbering scheme (`+4`/`+5`/
`+6` as three sequential bytes of one small record) does not guarantee
those are three INDEPENDENT bytes if a SIBLING field elsewhere in the
same function is computed via a different base+stride that happens to
overlap.** `OnExtModeKnobAssignChange`'s own jump-catch record
(`this+0x50+idx*3`, bytes `+4/+5/+6`) has its `+6` byte land on the
EXACT SAME address as an entirely different array 8 lines earlier in the
SAME function (`this+0x56+idx*3`, the "mirror the CC's current value"
store) -- confirmed only by computing both absolute offsets by hand
(`0x50+6 == 0x56`) after a KAT that poked `rec[5]`/`rec[6]` as
independent values got two real `FAILED` results (`got=0x0 want=0x1`/
`want=0x2`), not by re-reading the disassembly's own comment-derived
mental model a second time. The reconstruction itself was already
correct (faithfully preserving the real instruction ORDER, which is what
makes the overlap load-bearing: the mirror-store executes before the
jump-catch code reads the same byte back as "the OLD value to compare
against"). Lesson: whenever a function has TWO OR MORE per-index arrays
at different base offsets with the same per-index stride, explicitly
compute every touched absolute byte offset (base + index*stride +
sub-offset) and check for numeric equality BEFORE writing a KAT that
sets them independently -- this class of bug is invisible from reading
either array's own field list in isolation, only from cross-computing
the numbers.

**Batch 15 specifics** (2026-07-06, sec 10.162): picked
`CSTGChannelValues::SetControllerValue()` (240 bytes -- one of the three
consumers sec 10.153 explicitly named as blocked specifically by
`CSTGCCInfo::sCCInfoTable`, the other two resolved in batch 14) and
`CSTGHDRManager::ProcessRecordCommands()` (303 bytes) + a brand-new
`CSTGRecordTrack` class (`Start`/`Pause`/`Stop` real, `StandbyRec`
deferred). Stub count 78 -> 76. Evaluated ~25 candidates in the 85-560
byte tier; most were freshly re-confirmed still-blocked (full disassembly
each time, not just re-reading old comments) by the same clusters as
before (`CSetListEQ`/`CSTGEffectRack`/`CSTGEffectRackVars`, real vtable
dispatch on unrecovered-vtable classes, `CSTGParamsOwner`'s own abstract
vtable slots) plus one NEW blocker found this batch: `CSetListEQ::
SetBand()` uses real SSE instructions (`unpcklps`/`movlhps`/`movaps
XMMWORD`) for a genuine vectorized biquad-coefficient broadcast on a
`-mno-sse` build -- a new class of "needs SSE reimplementation" problem,
distinct from the already-known "SSE used for pure data movement"
gotcha in CLAUDE.md (this usage is real vector arithmetic, not just a
wide copy, so the fix isn't a simple byte-copy-loop substitution).

**Key technique reused successfully: when a `ProcessCommands()`-family
sibling has ALREADY been blamed for "the whole cluster dispatches through
vtables" by an earlier batch, still re-disassemble it individually before
accepting that verdict.** `CSTGHDRManager::ProcessRecordCommands()` turned
out to be the ONLY one of its own four `ProcessXxxCommands()` siblings
with zero vtable/PMF dispatch (all four calls are direct, non-virtual,
to a brand-new but straightforward `CSTGRecordTrack` class) -- the other
three (`ProcessPlaybackCommands`/`ProcessSamplerCommands`/
`ProcessHDRRecord`) each pull in a DIFFERENT distinct new class
(`CSTGPlaybackBuffer`-adjacent + `signal_daemon`; `CSTGSampler`;
`CSTGSampler`+`CSTGDiskCostManager`+`CSTGMonitorMixerChannel` unrolled
16x) and remain genuinely blocked. Same lesson as sec 10.160's
`CSTGSamplingDaemon` finding, now confirmed a THIRD time on a completely
different manager class -- a cluster-wide "all blocked" verdict from an
earlier pass is a per-SAMPLE finding, not a proof about every sibling.

**New gotcha, a genuine own-test bug caught by an actual segfault (not a
reconstruction bug): a function that unconditionally dereferences a
packed-pointer field with NO null check in the real disassembly will
crash a KAT that leaves that field at its zeroed/default value, even
when the test's OWN intent doesn't care about that field's contents.**
`test_hdr_record_track.cpp`'s first draft of its
`ProcessRecordCommands()` end-to-end test set up a track with
`state=1` to exercise the tag==1 `Start()` dispatch, but never gave it a
valid `meterPtr` (`+0x8`) -- `Start()`'s own real, confirmed,
unconditional `meterPtr->+0x8 = 2` store (no null check either in
ground truth or in this reconstruction, faithfully preserved) then wrote
through a null pointer, crashing with SIGSEGV (`gdb -batch -ex run -ex
bt` pinpointed it immediately -- faster than re-reading either the test
or the reconstruction a second time). Fixed by giving the test its own
real `mmap32()`-backed buffer for every packed-pointer field ANY called
method might dereference, not just the fields the test's own docstring
says it cares about.

**Likely-but-unconfirmed cross-batch layout finding, flagged for a future
pass rather than acted on this batch**: `CSTGHDRManager`'s own long-ago
ctor comment places `CSTGMonitorMixerChannel[16]` at `+0x5a4` (stride
`0xc0`), but this batch's own fresh `ProcessRecordCommands()` disassembly
independently finds the SAME `0xc0`-stride array anchored at `+0x584`
instead, with the ctor's own three zeroed relative offsets in the
"32-byte gap" between them lining up with this batch's own confirmed
`ringBase`/`ringWriteIdx` fields. Likely conclusion: each 192-byte slot
is really one `CSTGRecordTrack` embedding a `CSTGMonitorMixerChannel`
sub-object at ITS OWN `+0x20` -- NOT proven byte-for-byte this pass
(deliberately left the existing `monitorMixerChannelSlots` field
untouched rather than merge/rename it on a partial cross-check). When two
different batches' own confirmed-real offset findings for "the same"
memory region are off by a fixed constant, check whether one is
measuring from an OUTER struct's base and the other from an EMBEDDED
sub-object's own base before assuming either one is wrong.

**Reconfirmed, not new: `SetControllerValue`'s own dedicated-TU
placement decision followed the by-now-standard checklist (grep every
`verify/*.cpp` for the exact symbol name, not just files that will link
the new source file) and found the now-expected pattern -- two trivial
mocks (`test_engine.cpp`/`test_global_ctor.cpp`) plus one LOAD-BEARING
call-counting mock (`test_global.cpp`, its own "SetControllerValue called
twice per channel" assertion) -- all three left completely untouched by
giving the real body its own file, exactly matching the
`WriteSTGMidiOutQueue`/`CSTGStreamingEventManager` precedent (sec
10.145/10.158). No new gotcha here, but worth re-recording since this is
now the THIRD confirmed instance of this exact shape and it's clearly the
right default move whenever `grep -l '<Symbol>' verify/*.cpp` turns up
more than zero hits outside the file being edited.

**Batch 16 specifics** (2026-07-06, sec 10.163): picked up sec 10.162's
own explicitly-flagged priority target, `CSTGControllerRTData::
SetControllerAssignment()` (322 bytes, its 47-entry notify table already
dumped by batch 15), plus its own newly-discovered dependency
`CSTGSlotVoiceData::UpdateAllActiveMIDIFilters()` (624 bytes) and a new
real table, `kControllerCCIdTable` (18 bytes). Stub count 76 -> 75 (net
-1; the two `CSTGSlotVoiceData` methods are brand-new, not promoted
stubs). Also freshly re-disassembled `CSetListEQ::SetBand()` (this
batch's own suggested "fresh look" candidate) and `CSTGFileOpener::
ProcessCommands()` (the one file-daemon-cluster sibling not individually
re-checked in recent batches) -- both re-confirmed still genuinely
blocked, not newly resolvable (see sec 10.163 for full detail on each).

**Key technique this batch: an "inlined helper" is not the same as "a
call to the shared helper function" even when the computed VALUE is
identical -- always check for an actual `call` instruction before
assuming a disassembly site reuses an existing project helper.**
`SetControllerAssignment`'s own `CSTGPerformanceVarsManager::sInstance[8]`-
selector + pointer-array-index sequence is BYTE-FOR-BYTE the same
computation as this project's own already-existing
`ResolveActivePerformanceVarsManagerRaw()` (global.cpp) -- but the real
disassembly has NO `call` at this site, it's fully inlined (matching the
underlying compiler's own inlining choice at the ORIGINAL call site too).
Modeled by literally repeating the 4-line computation in the new file
rather than calling the shared helper -- this also had the practical
benefit of avoiding a dependency on `global.cpp` (a huge, heavily-shared
file) for the new dedicated TU, keeping the new standalone KAT small.
When a disassembly site's computed VALUE matches an existing helper
function's own logic exactly, still check for the actual `call`
instruction before reusing that helper by name in the reconstruction --
if it's inlined in the real binary, inlining it in the reconstruction
too (rather than calling the shared helper) is both more faithful AND
often more convenient for a standalone dedicated-TU KAT.

**Second technique: cross-referencing an EARLIER pass's own
already-documented struct-offset finding caught a subtle "same base,
different sub-field" confusion before it became a bug.** A newly-computed
address (`mgr + channel*0x92c + 0x2410 + ccId*12 + 8`) looked at first
like it might be 8 bytes off from `CSTGMidiDispatcher::
PerfChangeControllerReset()`'s own already-documented `channelValuesObj
= chanBase + 0x2410` (global.cpp) -- turned out to be fully consistent
once recognized as `channelValuesObj + ccId*12 + 8`, i.e. `rawArray[ccId]`'s
own `field8` sub-field (already independently confirmed by
`SetControllerValue`, sec 10.162, to be a real per-CC "live value" mirror)
rather than a different table entirely. Re-deriving a NEW address
independently and then diffing it against an OLDER pass's own confirmed
base offset (rather than either blindly trusting the new derivation or
blindly reusing the old one) is a cheap, high-value cross-check --
worth doing whenever two passes' own confirmed offsets for "the same"
memory region are close but not identical, before concluding either one
is wrong (this project's own sec 10.162 "likely-but-unconfirmed" note
about `CSTGHDRManager`'s two offset findings is the same family of
situation, just left unresolved there instead of reconciled).

**Gotcha (own-test-only, caught by a link error, not a runtime
failure): a caller's own real function and a SIBLING method it calls that's
deliberately given a no-op deferred body CANNOT share a translation unit
if a test needs to MOCK that sibling to observe dispatch behavior.**
First draft put `UpdateAllActiveMIDIFilters()` (real) and
`UpdateMIDIFilterAndResendAllCCs()` (deferred no-op) in one file --
`verify/test_slot_voice_data_midi_filters.cpp` failed to link ("multiple
definition") the moment it tried to provide its own call-recording mock
of the deferred sibling to verify WHICH payloads get dispatched to.
Fixed by splitting the deferred no-op into its own sibling file
(`slot_voice_data_update_midi_filter_resend.cpp`), both still added to
`OA-objs`/host build list for the real kernel module, but the dedicated
test links only the file with the real dispatch logic. Distinguish from
the `CSTGRecordTrack::StandbyRec()` precedent (sec 10.162), where the
deferred sibling's own no-op sat in the SAME file as its caller with NO
conflict -- the difference is whether the test actually needs to OBSERVE
the sibling's dispatch (this batch) vs. merely avoid crashing on it
(sec 10.162).

**Gotcha, a genuine own-test bug caught by `-Warray-bounds`, not a
runtime crash: a stray buffer-size constant left over from a REJECTED
earlier draft can silently write out of bounds into an adjacent, unrelated
allocation.** An early draft of `test_slot_voice_data_midi_filters.cpp`
used a risky "subtract a large offset from a small buffer" pointer trick
to avoid this project's own ~43.6MB `calloc(1, 0x29c9fc0)` convention
(`test_engine.cpp` et al) -- rejected before commit as unverified pointer
arithmetic outside a real allocation's own bounds. Reverted to the
project's own established big-`calloc` pattern, but a `memset(tableBuf,
0, 0x1000)` sized for the REJECTED small-buffer draft was accidentally
left in one test section -- since the real buffer only has ~1.7KB left
past `tableBuf` (`0x29c9fc0 - 0x29c990c == 0x6b4`), this silently wrote
4KB out of bounds. Caught by GCC's own `-Warray-bounds` warning at
compile time, not by any runtime failure (the test suite's own exit code
was 0 regardless, since `calloc`'s own slack absorbed the corruption
harmlessly in practice). Lesson: don't dismiss `-Warray-bounds`/similar
compiler diagnostics on host-side KAT code just because the test binary
exits 0 -- grep the build log for new warnings on every new test file,
not just new errors.

**Re-confirmed rejection, not a new finding: `CSetListEQ::SetBand()`**
(204 bytes) -- this batch's own suggested "fresh look" candidate, to
check whether its SSE usage (`unpcklps`/`movlhps`/`movaps`, sec 10.162)
might be pure data movement (the ALREADY-known, separately-resolvable
"SSE for wide copy" gotcha) rather than genuine vector arithmetic.
Confirmed NOT a data-movement case: the SSE instructions BROADCAST one
computed scalar into all 4 lanes before an aligned store -- real SIMD
arithmetic setup, reimplementable as 4 explicit scalar stores if that
were the ONLY blocker, but `SetBand` also calls `CSTGEQ::
CalculatePeakingCoefficients()` (not reconstructed) and an external
`am_exp2_ess` symbol -- still blocked by a real dependency chain
independent of the SSE question. When a "check if X is actually simpler
than assumed" instruction is given, it's fine (and correct) to conclude
"no, still blocked, but for a DIFFERENT/MORE SPECIFIC reason than
before" -- that's a real, useful finding, not a failure to find
something.

**Batch 17 specifics** (2026-07-06, sec 10.164): picked `CSTGSlotVoiceData::
FreeSlotVoiceData(bool)` (394 bytes, one of sec 10.157's own long-standing
deferred externs) plus its three newly-discovered dependencies
(`CSTGSmoother::CancelAllSlotVoiceDataCCSmoothers`, `CSTGSlotVoiceData::
AreAllKeysAndPedalsReleased`, `CSTGPerformanceVars::
NotifyAllKeysAndPedalsReleased`, all small/tractable), plus separately
resolved `CSTGHeapManager::Alloc(unsigned int)` (this project's own local
"static ecosystem" duplicate of the already-real `Alloc(unsigned long)`).
Stub count -1 by the project's own `grep -cE '^\S.*\{\}$'` convention
(75->74) -- see the new gotcha below on why this doesn't move by -2.

**New gotcha: this project's own stub-counting convention
(`grep -cE '^\S.*\{\}$'`) only counts BARE `{}` stub bodies -- a stub with
ANY content between the braces (`{ return 0; }`, `{ return false; }`,
etc.) is silently excluded from the count, making it easy to overlook as
"not a real stub" when skimming `bar2_stubs.cpp`.** `CSTGHeapManager::
Alloc(unsigned int)`'s own stub body was `{ return 0; }` -- a genuine,
still-unreconstructed deliberate stub (confirmed via its
`oa_setup_global_resources.h` declaration comment: "own body not
reconstructed... NOT independently named in the real binary beyond its
mangled member-function symbol") that the grep-based count had been
silently excluding all along. Found only by cross-referencing that
header comment against the stub file directly, not by trusting the
"N stubs remain" grep output. When scanning for the next batch's
candidates, also check `bar2_stubs.cpp` for non-empty placeholder bodies
(`return 0`/`return false`/etc.), not just bare `{}` lines -- a real,
tractable candidate can hide there and never show up in the "official"
count.

**Key technique this batch, worth reusing: a class's own "local static
ecosystem duplicate" of an already-fully-reconstructed real method can
often be resolved for free by mechanically transliterating the
ALREADY-VERIFIED real algorithm via raw offset arithmetic, rather than
re-disassembling from scratch.** `oa_setup_global_resources.h`'s own
`CSTGHeapManager` stand-in (`static char *sInstance; static unsigned int
Alloc(unsigned int size);`) is a DIFFERENT, differently-mangled symbol
from the real class's own `Alloc(unsigned long)` (Itanium ABI: different
parameter type means a different mangled name, `Ej` vs `Em`) -- but since
`heap_manager.cpp`'s own `Alloc(unsigned long)` was ALREADY fully
ground-truthed (sec 10.63), the static duplicate's own body could be
written as a byte-offset transliteration of that SAME already-proven
algorithm (no new disassembly needed) -- and, as a side benefit, using
raw 4-byte `unsigned int` reads/writes throughout makes THIS version
slightly MORE faithful to the real 32-bit target than the original
class's own `unsigned long`-typed struct fields are on this 64-bit host.
Whenever a "local ecosystem stand-in" duplicates a class whose REAL
counterpart is already reconstructed elsewhere in this project, check
whether the real implementation's own algorithm can just be replayed via
raw offsets before treating the duplicate as a fresh reconstruction task.

**New list-unlink naming discipline, worth reusing whenever a list's own
directional convention isn't independently confirmed**: `FreeSlotVoiceData(bool)`'s
own two embedded intrusive lists (node fields `+0x4`/`+0x8` and
`+0x14`/`+0x18`) use the SAME "identity via `&node+linkOffset`" token
convention already established for `CSTGHeapManager`'s sentinel, but this
batch's own head/tail-update disassembly didn't obviously match either a
"prev" or "next" semantic for either field -- rather than guess and risk
mislabeling (which could mislead a FUTURE pass into "fixing" a
non-existent bug), the fields were transcribed with neutral `link1`/
`link2` names and a comment stating the semantic direction is
unconfirmed. Mechanically verified correct anyway via a host KAT
exercising BOTH a single-entry list (head==tail==self) and a
middle-of-three-node list (checking exact neighbor-propagation values,
not just "no crash").

**Recurring gotcha, hit TWICE in this batch's own new test file (a
genuine own-test bug, not a reconstruction bug): every object whose
address is round-tripped through a packed 32-bit list-link field must be
`mmap32()`/`MAP_32BIT`-backed, never a plain stack array or `calloc()`,
on this 64-bit host.** First hit (caught by `-Warray-bounds` at compile
time, not a runtime crash): `test_slot_voice_data_free.cpp`'s own `[1]`
section used a `buf[0x1800]` too small for the `+0x2888`/`+0x1790`/
`+0x17a8` offsets it needed to poke, and `[2]`'s own `smootherBuf[0x1000]`
was too small for the `+0xf010` list-head offset -- fixed by enlarging
both (and moving `smootherBuf` to `mmap32`). Second, more dangerous hit
(a real SIGSEGV, only found by `stdbuf -oL` per-section output localizing
the crash to section `[2]` -- `gdb -batch -ex run -ex bt` gave no useful
frame info under this project's `-O2`/no-debug-symbols host build):
`[2]`'s own `targetBuf`/`otherBuf`/`node0..2`/`mapping0..2` were plain
STACK arrays whose addresses got stored into 32-bit list-link fields and
read back -- a 64-bit stack address routinely exceeds 32 bits and
truncates to a wild pointer on reconstruction. Fixed by `mmap32()`-backing
all six. Lesson: when a NEW test's own list/node objects have their
OWN address stored into ANY 32-bit field (not just when the test's
top-level "singleton" object does), audit EVERY such object individually
for mmap32 backing -- a `-Warray-bounds` catch on ONE bug in a test file
does not mean there isn't a SECOND, more dangerous pointer-width bug
sitting right next to it.

**Rejected/deferred candidates this batch** (full detail in sec 10.164):
`CLoadBalancer::BalanceStaticLoad()` + its own new `BalanceStaticLoadHelper`
dependency (blocked by a 3-method `CSTGSlotVoiceData` cost-accounting
cluster); `CSTGEffectManager::RunEffects()` (blocked by a 3-deep
`CSTGPerformanceVarsManager::RunEffects()`/`CSTGPerformance::RunEffects()`
chain plus `CSTGMIDIClockSync::GetFilteredTempoBPM()`); `CSTGMidiPortManager::
~CSTGMidiPortManager()` (confirmed real vtable dispatch on up to 8 array
entries); `CSTGPerformanceVars::EnterActivatingState()` (re-confirmed the
EXACT sec 10.153 six-new-class cluster); `CSTGSlotVoiceData::
UpdateGlobalTune(float)` (confirmed real vtable dispatch through
`CSTGPatchMessageContext`, a repeat of the already-known blocker).

**Batch 18 specifics** (2026-07-06, sec 10.165): a repeat of the sec
10.149 "resumed mid-flight" situation, but this time NOT flagged by the
orchestrator -- `git diff --stat`/`git status --short` at session start
showed `bar2_stubs.cpp`/`oa_engine.h`/`oa_global.h`/`Makefile` already
modified plus two fully-written untracked files
(`src/engine/audio_input_use_settings.cpp`, `channel_values_reset.cpp`,
both self-labeled "batch 18") with matching Makefile wiring and
`verify/` KATs already in place. **Standing lesson reinforced: NEVER
trust an orchestrator's/briefing's own claim of "confirmed clean tree"
at face value -- always run `git diff --stat`/`git status --short`
yourself as the very first step of any batch, even when told it's
unnecessary.** Treated the found work as a legitimate continuation (sec
10.149 precedent) rather than redoing the disassembly, but independently
re-verified both files via a real rebuild before trusting them -- both
held up with zero changes needed. Own new work layered on top: resolved
sec 10.164's own explicitly-flagged priority recommendation, the
`CLoadBalancer` cost-accounting cluster -- `CSTGSlotVoiceData::
EnableSlot()`, `CLoadBalancer::BalanceStaticLoad()`, `CLoadBalancer::
BalanceStaticLoadHelper(...)` all reconstructed, plus their own newly-
discovered dependency `CSTGSlotVoiceData::GetPatchStaticCosts(...)`
confirmed STILL genuinely blocked (real vtable dispatch through the
`CIFXEffectSlot`/`CMFXEffectSlot` cluster, sec 10.157) and added as a new
bare-`{}` stub. Stub count 74 -> 72 (net -2: three bare-`{}` removals
from the resumed pair + this batch's own `BalanceStaticLoad`, minus one
newly-added bare-`{}` for `GetPatchStaticCosts`).

**Key technique this batch, worth reusing for any future gnarly
control-flow function with no clean high-level "meaning"**:
`BalanceStaticLoadHelper` (the hardest function reconstructed in this
whole sweep so far) has two interleaved scan/advance re-entry points
whose exact PURPOSE (why two separate output arrays are indexed by two
INDEPENDENTLY-tracked scan positions) is not recoverable from the
disassembly alone. Rather than force a plausible-sounding high-level
narrative, it was transliterated as a literal instruction-level
translation (named locals tracking specific registers/stack slots, e.g.
`v34`/`v38`/`v1c`/`v3c`) -- THEN, critically, independently hand-traced
THREE small `busCount` cases (0, 1, 2) by manually simulating the x86
flag semantics ON PAPER before writing any KAT, and wrote the KAT's own
expected values FROM that independent hand-trace, not from the C
translation being tested. This caught a real transcription slip mid-
derivation (an early draft assumed the "tail settles then busCount-exit"
path reused `chosenPtr`/`distArrayA` the same way the "keep-advancing
then busCount-exit" path does -- re-deriving `busCount==1` by hand a
SECOND time, independently, exposed that BOTH exit paths actually target
`distArrayB` via a saved index, never `distArrayA`) -- caught by the
independent-re-derivation discipline itself, before ever running a KAT,
not by a KAT failure. The `busCount==2` KAT case (deliberately chosen
with monotonically-decreasing `distArrayB`) is what actually PROVES the
translation is right rather than a plausible-but-wrong simplification --
it's the one case where `bestIdx != scanIdx`, and it passed clean on the
first real run. Lesson: for a function this intricate, do TWO
independent things before trusting a mechanical transliteration -- (1)
hand-derive at least 2-3 small cases on paper, not just 1, since a
single traced case can "confirm" a subtly wrong generalization, and (2)
choose at least one KAT case specifically designed to distinguish the
right translation from the most plausible WRONG one (here: two outputs
landing in the same vs. different array slots), not just cases that
exercise the "happy path."

**Second technique, a reused-successfully precedent**: `CSTGSlotVoiceData::
GetTotalStaticCosts()` (already a deferred stub, sec 10.94) was
freshly re-disassembled this batch on the theory it might just call the
newly-reconstructed `GetPatchStaticCosts()` (which would have unblocked
it for free) -- it does NOT; it independently INLINES the identical
vtable-dispatch pattern at its own two call sites instead. Another
confirmed instance of sec 10.163's "an inlined helper is not the same as
a call to the shared helper" finding -- worth re-checking every time a
sibling method LOOKS like it should just call a newly-real helper.

**Batch 18 postscript, a genuine CI/tooling bug found during this batch's
OWN independent re-verification of the already-committed d3dd96c state
(not a reconstruction bug): the entire `Makefile` has ALWAYS used CRLF
line endings (`file Makefile` reports it, ~515/515 lines) -- almost
always harmless (GNU Make silently tolerates a trailing `\r` immediately
after a `\`-continuation), EXCEPT for the LAST token of a `\`-continued
variable when that token sits on the FINAL line of the block (no
trailing `\` after it) -- there, the `\r` becomes a literal trailing byte
of the variable's own last value, silently breaking any exact-match
lookup against that name.** Concretely: appending
`verify/test_load_balancer_static` as the new last line of the `TESTS`
variable made ITS OWN value become `verify/test_load_balancer_static\r`
-- invisible in every editor/pager, but `[ -x "$t" ]` (and, independently
confirmed, `make verify`'s own `for t in $(TESTS); do ./$$t; done` loop)
both silently fail to find/run it, while every OTHER entry in the same
list (each followed by a continuation `\`) works completely normally.
The test binary itself was fully valid and passed cleanly (`exit=0`,
confirmed via a direct, separate invocation) -- this was purely a
list-membership/lookup artifact, not a build or reconstruction failure,
and easy to misdiagnose as "the newest test binary didn't build" when the
real cause is one invisible trailing byte on one line. **Fixed by
stripping CR from the whole file** (`sed -i 's/\r$//' Makefile`,
confirmed via `diff <(tr -d '\r' <backup) Makefile` to be a byte-for-byte
no-op everywhere except CR removal -- zero functional content change).
Lesson for any future batch that adds a NEW final entry to a
`\`-continued Makefile variable (`TESTS`, `OA-objs`, `SRC`, etc): if a
freshly-added test binary mysteriously "doesn't exist" right after a
successful build, check `file Makefile` for CRLF endings and `cat -A` the
specific line before assuming the build itself is broken -- and prefer
appending new entries via a MIDDLE position (with a trailing `\` after
them) rather than as the new last line, until/unless the whole file's
line endings are normalized.

**Batch 19 specifics** (2026-07-06, sec 10.167): session started with a
STALE task briefing -- it claimed "last section was 10.165" and told
this session to write sec 10.166, but `git log --oneline` (checked
BEFORE trusting the briefing, per the standing "always re-derive current
state yourself" rule) showed HEAD had already moved one commit past that
to `ed5574d` (the CRLF bug-fix pass just above, its own now-existing sec
10.166) that landed between the briefing being written and this session
starting. Wrote this batch's own new section as **10.167** instead of
blindly using the number the briefing suggested. Lesson: a numbered
briefing snapshot is exactly as perishable as any other "confirmed clean
tree" claim -- always check `git log`/the actual file's own last `###
10.NNN` heading yourself before picking the next section number, not
just before deciding whether the tree is clean.

Picked `CSTGPerformanceVars::SetIsDying()` (478 bytes) plus 3 newly-
discovered dependencies (`CSTGSlotVoiceData::SetIsDying`,
`CSTGMIDIClockSync::DisableActivePerfClock`, `CSTGPerformance::SetIsDying`)
-- 4 functions fully reconstructed, net stub count **+3** (72 -> 75: -1
promoted, +4 newly-discovered deferred externs added for the four
`OnPerformanceDeactivate`/`ClearUnsolicitedMessages` call targets). A
clean example of "net stub count can legitimately go UP even while doing
solid net-positive reconstruction work" -- don't be alarmed by an
increasing count if the accounting is honestly explained (4 real
functions landed, only 1 of which had a pre-existing bare-`{}` stub to
remove).

**Key technique this batch, a "size is not risk" case going the OTHER
direction from sec 10.160**: the single SMALLEST remaining candidate by
raw byte size, `CSTGAudioThread::AudioTickLoopRoutine()` (141 bytes), was
checked FIRST specifically because of its tiny size -- and REJECTED,
because full disassembly showed FOUR real vtable dispatches (`call
*0xc(%edx)`/`call *0x18(%edx)` through THREE distinct sub-objects' own
vtables: `this`, `this+0x28`, `this+0x4a8`). Sec 10.160 already
established "big size doesn't mean risky" (a 910-byte ctor turned out
branch-free); this batch's own finding is the mirror image -- "tiny size
doesn't mean safe" either. The only reliable signal, in both directions,
is grepping the disassembly for `call`/indirect-dispatch instructions
BEFORE looking at byte count at all.

**Second technique, reused successfully again**: cross-referencing an
EARLIER, unrelated pass's own already-written header comments
(`CSTGSlotVoiceData`'s `+0x40`/`+0x41` fields, already independently
documented across THREE other methods -- `EmergencyFreeDyingSlotVoiceData`,
`StealDyingSlotVoiceDatasForCost`, `UpdateAllActiveMIDIFilters`, `Steal`)
confirmed this batch's own freshly hand-derived field meanings for
`SetIsDying()` before writing a single line of code -- same technique as
sec 10.157's own "re-read the target class's earlier pass" win, now on
its second successful use.

**Third technique: when two sibling call sites within the SAME function
being reconstructed contain byte-for-byte identical instruction
sequences (confirmed via `objdump`, not just "looks similar"), factor
them into ONE shared static helper in the reconstruction rather than
transliterating the block twice.** `SetIsDying()`'s own two branches
(state=3 vs state=4 outcome) both run the exact same "front-panel
active-manager count + maybe PushUnsolicitedMessage" block at two
different `.text` addresses -- confirmed identical opcode-for-opcode, so
a single C helper serves both call sites faithfully with no behavioral
difference, and is far more readable than two copies.

**Fourth technique, a THIRD confirmed instance of a recurring quirk
(first two: `CSTGPerformanceVarsManager::AllocPerformanceVars()`,
`CSTGPerformanceVars::NotifyAllKeysAndPedalsReleased()`, sec 10.164)**: a
shared helper block's own internal guard can be confirmed UNREACHABLE
specifically at ONE caller (not necessarily at every caller of that same
block) when that caller's OWN entry guard already pins the exact byte
the shared block's guard re-reads. `SetIsDying()`'s entry guard requires
`self[0x23d1] == 2`, and nothing between that check and either of its
own two calls into the shared helper writes to that byte -- so the
helper's own `oldState <= 1` check is always false THERE, even though
the exact same helper's guard is genuinely live at its OTHER two call
sites (where the caller's own precondition on that byte is different or
absent). Lesson: "is this guard reachable" is a per-CALL-SITE question,
not a per-shared-helper-function question -- check the specific caller's
own preceding control flow each time, don't assume a block's
reachability transfers across every place it's reused.

**No-short-circuit discipline, reinforced**: `SetIsDying()`'s own
AND-fold over `AreAllKeysAndPedalsReleased()` results must NOT be
written with C++ `&&` (which would short-circuit and skip the call once
the accumulator goes false) -- the real x86 always evaluates every
matching payload's call regardless of the running result. Used a plain
`&` fold instead, and added a dedicated two-payload KAT section (first
payload "not released" then second "released") that specifically fails
if a `&&`-shaped mistranslation were substituted -- confirms the call
count stays at 2, not short-circuited to 1.

**Verification**: 61 verify/ binaries (up from 60), all exit 0, both of
two full clean-rebuild passes; 32 unresolved symbols; `.gnu.linkonce.
this_module` 0x148 bytes; 0 `R_386_GOTPC`; `OA.ko` 151,596 bytes (up from
150,200). Commit `900c8a2`.

**Batch 20 specifics** (2026-07-06, sec 10.168): the user explicitly
RE-authorized the sweep ("continue the decompilation process of OA.ko...
I believe we last started batch 20"), satisfying batch 19's own
stop-until-asked note below. Picked the two TRACTABLE members of the four
`OnPerformanceDeactivate`/`ClearUnsolicitedMessages` externs batch 19
itself created -- a clean "batch N resolves batch N-1's own fallout" pass.
Reconstructed `CSTGAudioInput::OnPerformanceDeactivate()` (39 B, the
bit-clearing counterpart of `UseSettings()`, homed beside it in
`audio_input_use_settings.cpp`), `CSTGMessageProcessor::
ClearUnsolicitedMessages()` (52 B), and the brand-new `CSTGDelayedMsgSender`
class's `Clear()` (131 B, intrusive active->free list recycle) -- the
latter two in a new dedicated TU `message_processor.cpp`. Stub count -2
(75 -> 73). Deferred (documented, real dispatch not mere complexity):
`CSTGControllerInfo::OnPerformanceDeactivate` (its `SetPerfSwitch` callee
has a `call *0x74(%ecx)` vtable dispatch + jump table + 4 more calls) and
`CSTGFrontPanelSmoothers::OnPerformanceDeactivate` (2x `call *0x24(%esp)`
stack-callback dispatch); of the new class's other 5 methods, only
`Clear()` is dispatch-free (`Initialize()` calls its own vtable slot 0,
`AddMessage()` tail-calls unreconstructed `SendMessageNow()`).

**CRITICAL new process gotcha, cost ~3 wasted verification attempts before
being caught: the build host's non-interactive SSH session lands in
`/root`, which has NO Makefile -- NOT the OA dir.** Every `sshpass ...
ssh root@192.168.3.92 "make ..."` MUST begin with `cd
/home/share/kronosology/reconstructed/OA &&`. Worse, the failure is
SILENT and reads as a false PASS: the idiom `make clean >/dev/null 2>&1 &&
make all >/tmp/mk1.log 2>&1; echo "exit=$?"` run from `/root` has `make
clean` fail (no makefile, exit 2), which SHORT-CIRCUITS the `&&` so `make
all` never runs and NEVER WRITES `/tmp/mk1.log` -- leaving a STALE log
from a PRIOR session's successful run, whose "All checks passed" tail looks
exactly like a real pass, while `echo exit=$?` reports `make clean`'s 2 as
if it were `make all`'s. Two independent traps stacked: wrong-cwd + stale
/tmp log. Lessons: (1) always `cd` into the OA dir in the SAME ssh command
(cwd does not persist across ssh invocations); (2) never trust a `/tmp/mk*.log`
you did not just write THIS run -- redirect to a fresh path or verify the
log's own first line matches this run; (3) an `exit=2` with a passing-looking
log tail is the signature of this `&&`-short-circuit-onto-stale-log trap,
not a real partial pass.

**Two divergent repos -- do not confuse them.** The ACTIVE repo is
`/home/share/kronosology` (CIFS, uses "batch N" numbering; batch 19/20
committed 2026-07-06). `/home/build/kronosology` is an OLDER, separately-
rooted divergent line (uses "sec 10.NNN"-only commit subjects, last touched
2026-07-04 at "sec 10.141") -- NOT where this sweep is tracked, despite the
name collision. Verify the batch position via `git -C /home/share/kronosology
log` and MASTER_REFERENCE.md's own last `### 10.NNN` heading, never the
/home/build tree. (The build HOST 192.168.3.92 mounts the same CIFS
/home/share, so files edited here are what it compiles -- no copy step.)

**`||`/`&&` short-circuit IS faithful when the skipped operand is a pure
READ** -- the exact inverse-scoped companion to sec 10.167's no-short-circuit
rule. `OnPerformanceDeactivate`'s `if (g[0x680] || (this[0x77]&1))` matches
the real `cmpb ...; jne` short-circuit (the `this[0x77]` read is genuinely
skipped when `+0x680` is set), and writing it as C++ `||` is BOTH faithful
AND correct SPECIFICALLY because the elided operand is a side-effect-free
field read. Sec 10.167's rule (must use bitwise `&`/`|`, never `&&`/`||`)
applies only when the skipped operand is a CALL whose side effect the real
x86 always performs. Decide per-operand: short-circuit the reads, fold the
calls.

**Batch-19 stop-note (now satisfied)**: batch 19 recorded "the user asked
to stop the stub-sweep after batch 19 -- do not auto-start batch 20 without
being asked again." The user DID ask again (this batch), so batch 20 was
authorized. The standing rule remains: do not auto-continue to batch 21 in
a future session without a fresh explicit request.

**Batch 21 specifics** (2026-07-06, sec 10.169, commit 850a9a3): the user
gave fresh explicit authorization to continue past batch 20, satisfying
the batch-19/20 stop-note above. Re-verified state myself before starting
(HEAD `59c6fd8`, clean tree, stub count 73, last section 10.168 -- all
matched the task briefing this time, no surprises). Picked
`CSTGMIDIClockSync::CSTGMIDIClockSync()` (repeatedly rejected since sec
10.153 as needing a new base class + vtable) -- fresh disassembly found
the whole cluster (ctor + `CSTGMIDIClockSyncBase::Initialize()` + all 8
`CSTGIntMIDIClockSync` vtable-slot methods) is branch/call-light and
fully tractable, matching sec 10.158's "check the whole class once a
tiny dependency turns up" technique. 11 functions reconstructed across a
new file (`src/engine/midi_clock_sync.cpp`) plus 2 more added to the
existing `sk_stg_gate.cpp`. Stub count 73 -> 72 (net -1 -- only the ctor
had a prior bare-`{}` stub; everything else was brand-new).

**IMPORTANT ENVIRONMENT UPDATE, supersedes the batch-20 "must SSH to
192.168.3.92" assumption**: this session's OWN working environment
(hostname `kronosdev`, IP 192.168.3.86) already had direct, local access
to BOTH `/home/share` (no CIFS detour needed to reach it) AND a working
`/home/build/linux-kronos` kernel tree in place -- `make clean && make
all && make ko KDIR=/home/build/linux-kronos` all ran successfully
DIRECTLY, no SSH required. Ran the full verification sequence twice
locally (both clean, both passing identically: 63/63 suites exit 0, all
4 invariants, OA.ko 153,844 bytes both times), then ALSO cross-checked
once on the documented 192.168.3.92 host via
`sshpass -p kronosbuild ssh root@192.168.3.92` (still hitting the
batch-20-documented "non-interactive ssh lands in /root" gotcha --
confirmed STILL TRUE, every remote command still needs its own `cd
/home/share/kronosology/reconstructed/OA &&` prefix) -- got byte-for-byte
IDENTICAL results (63/63, size 153,844) on all three runs. Lesson: don't
assume the CURRENT session's shell is the same restricted environment a
PRIOR batch documented -- check for local `/home/build/linux-kronos` and
try a local `make ko` FIRST (fast, no SSH round-trip, no stale-log risk)
before falling back to the documented 192.168.3.92 SSH workflow; if both
are available, cross-checking on both remains good practice (three
agreeing runs is strictly better evidence than one) but is no longer
strictly required to get a build done.

**Technique reused successfully (3rd confirmed instance): before
hand-rolling x87 inline asm for a confirmed "non-trivial-looking" FPU
control-word rounding sequence (`frndint`+`fisttp` with an explicit
rounding-mode change, here "round toward +infinity"), check whether the
ACTUAL CONFIRMED real-world inputs make the rounding mode provably
immaterial.** `CSTGMIDIClockSyncBase::Initialize()`'s own
`0.104 * 1500.0` was independently verified (Python, double precision)
to equal EXACTLY `156.0` -- zero rounding ambiguity regardless of mode --
so a plain `(int)(...)` C truncation is provably equivalent to the real
ceiling dance for this one confirmed input. Matches the pre-existing
`CSTGDiskCostManager::Initialize()`/`engine_startup_bits2.cpp` "trivial
given" precedent (sec 10.57), now confirmed a further time: don't reach
for inline asm just because the DISASSEMBLY looks intricate -- check the
actual numbers first, they may make the intricacy moot.

**Reconfirmed, not new: the `-mhard-float -msse2 -mfpmath=sse` per-file
Makefile CFLAGS override (four prior sibling files:
engine_startup_bits.cpp/engine_startup_bits2.cpp/scale.cpp/
smoother_init.cpp) is the DEFAULT/majority choice for "genuine but
simple, non-chained-in-a-precision-sensitive-way" float/double
arithmetic under this kernel's own `-msoft-float` build** -- used it
again for `midi_clock_sync.cpp` rather than hand-rolled x87 inline asm
(sec 10.117's `MulRoundToFloat`/`FYL2X` precedent), which remains
reserved for cases with an explicit stated reason to avoid
double-rounding (a downstream exact-boundary clamp comparison, in that
case). When in doubt for a NEW file needing float/double math, default
to the CFLAGS-override + plain-C route first; only reach for inline asm
if there's a SPECIFIC identified precision-sensitive reason not to.

**Offset-collision discipline, applied proactively (no bug found, but
worth recording as a successful catch-before-commit)**: before writing
the ctor's own KAT, explicitly computed ALL absolute byte offsets by
hand for fields that LOOK similar across an outer object and its own
embedded sub-object (here: the ctor's own `+0x78/+0x98/+0xb8` on the
OUTER `this` vs. `CSTGMIDIClockSyncBase::Initialize()`'s own
`fieldAt(0xc)` on the EMBEDDED `this+0x4` sub-object, i.e. absolute
`outerThis+0x10`) -- confirmed these are four genuinely DISTINCT storage
locations despite sharing the same real constant (`48.0f`, reused at a
second confirmed `.rodata.cst4` offset) and the same computation
formula. Matches the sec 10.149/10.155 "recompute hex arithmetic twice,
independently, before trusting it in a comment or KAT" discipline --
applied here BEFORE any test was written, catching what would otherwise
have been an easy field-identity mixup between two independently-styled
comments describing "the same-looking" 48.0-multiply.

No new deferred dependencies discovered this batch -- the whole ctor's
own reachable call graph was fully resolved in one pass, an unusually
clean result for this sweep.

Full analysis, KAT structure, and verification numbers: MASTER_REFERENCE.md
sec 10.169.

**Batch-21 stop note**: no explicit stop instruction was given this
session; the user's own standing rule (do not auto-continue past the
current batch without a fresh request) still applies going into any
future batch 22.

**Batch 22 specifics** (2026-07-06, sec 10.170, commit 78649cc): the user
gave fresh explicit authorization to continue past batch 21. Re-verified
state myself first (HEAD `c6c3fb6`, clean tree, stub count 72, last
section 10.169 -- matched the briefing). Picked `CSTGSmoother::
CSTGSmoother()` (a long-deferred bare-`{}` ctor, sec 10.160 "big but
branch/call-free" category) plus a full `CSTGHDRManager::Initialize()`
cluster: `CSTGPlaybackBuffer::Initialize()` (both overloads),
`CSTGMonitorMixerChannel::Initialize()`, `CSTGRecordTrack::Initialize()`,
`CSTGSampler::Initialize()`, `CSTGCDAudioPlay::Initialize()`,
`CSTGAudioInputMixerBase::Initialize()`, `CBusChangeStateMachine::Reset()`
-- 10 functions total, stub count 72 -> 70. Full writeup:
MASTER_REFERENCE.md sec 10.170.

**Key technique, a new variant of "check the whole reachable cluster
before rejecting a big function":** `CSTGHDRManager::Initialize()` (1284
bytes) had been passed over by several prior batches' cursory looks as
"obviously too big/complex" -- this batch did a FULL disassembly anyway
and found its entire call graph (6 more functions across 4 classes, one
of them -- `CSTGSampler` -- belonging to an otherwise-enormous,
genuinely out-of-scope class) was tractable because each individual
dependency, checked in isolation, turned out to have either zero or one
external call and zero vtable dispatch. Lesson reinforced: a big
function's OWN size/reputation is not evidence about its dependencies'
tractability -- trace the full reachable set before accepting a prior
batch's "too complex" verdict at face value, even when several batches
in a row have already passed on the same candidate.

**Definitively resolved a previously-flagged cross-batch ambiguity**:
sec 10.162 found `CSTGHDRManager`'s own ctor comment and
`ProcessRecordCommands()`'s own comment describing the SAME `+0x584..
+0x11a4` memory region two different ways (`CSTGMonitorMixerChannel[16]`
at `+0x5a4` vs. `CSTGRecordTrack[16]` at `+0x584`) and guessed, without
proof, that they were the same array (`CSTGMonitorMixerChannel` embedded
at `CSTGRecordTrack`'s own `+0x20`). This batch's fresh disassembly of
`CSTGRecordTrack::Initialize()` -- specifically its own `lea
0x20(%ebx),%eax` immediately before calling `CSTGMonitorMixerChannel::
Initialize()`, with no other candidate value assigned to `eax` in
between -- proves it outright. Lesson: when an earlier batch's own
"likely but unconfirmed" hedge names a SPECIFIC follow-up disassembly
that would resolve it (here: "not independently proven byte-for-byte
this pass"), actively look for that resolution when a related function
comes up in a later batch, rather than treating the hedge as permanently
unresolved.

**New gotcha, caught by two real KAT `FAILED` lines, not proactively**:
when a function reads one field early (cached in a register) for one
purpose, and a LATER-called sibling function reads what looks like "the
same numeric field name" for a different purpose, always check whether
they're actually the IDENTICAL absolute byte offset before assuming they
are independent fields deserving independent host-KAT backing buffers.
`CSTGRecordTrack::Initialize()`'s own `this+0x24` (read early, used for
a `+0x68=1.0f` store) and the embedded `CSTGMonitorMixerChannel`'s own
`+0x4` (read inside `CSTGMonitorMixerChannel::Initialize()`, called
moments later on the sub-object at `this+0x20`) are the EXACT SAME
absolute location (`this+0x20+0x4 == this+0x24`) -- an early KAT draft
(in BOTH `test_hdr_record_track.cpp` and `test_hdr_manager_init.cpp`)
provisioned two independent buffers for what it assumed were two
separate fields, and the second buffer's address silently clobbered the
first at that shared offset, producing two real, reproducible `FAILED`
lines (not a crash) that only made sense once the absolute offsets were
recomputed by hand. Fixed by consolidating to one shared buffer per
track in both KATs and correcting the reconstruction's own header
comment (which had independently made the same "these are separate"
assumption). Same family as sec 10.149/10.157's "recompute every touched
absolute offset before trusting a comment" discipline, but this time
surfacing as a genuine field-identity MISTAKE in a first-draft
explanatory comment, not just an arithmetic slip.

**Recurring gotcha, hit again on the READ side this time (previously
only seen on the write/field-declaration side, sec 10.156/10.158)**: a
packed 32-bit "pointer" field on a class reinterpreted directly onto raw
target memory must be read via `FromU32(*(unsigned int*)(base+N))`, NEVER
`*(T**)(base+N)` -- the latter is an 8-byte read on this 64-bit host and
silently pulls in 4 bytes of adjacent poison as the pointer's own upper
half. Caught by a real SIGSEGV (`gdb -batch -ex run -ex bt` pinpointed
the exact line immediately) in a first draft of
`CSTGMonitorMixerChannel::Initialize()`; the identical mistake was found
and fixed pre-emptively in `CSTGRecordTrack::Initialize()`'s own
`this+0x24` read once the audit habit kicked in. When translating ANY
new function that dereferences a packed-pointer field (not just when
declaring one as a struct member), grep the new code for `*(.*\*\*)` /
native double-pointer casts before considering it done -- this project's
own `FromU32`/`ToU32` helpers exist in every file that needs them
specifically to make this mistake impossible to make silently.

**Real, deliberately-NOT-fixed gap found this batch**: `CSTGMonitorMixerChannel::
CSTGMonitorMixerChannel()` and `CSTGPlaybackBuffer::CSTGPlaybackBuffer()`
(both in `managers.cpp`) are STILL no-op `{ }` stubs -- a THIRD confirmed
instance of the sec 10.164 "non-bare stub invisible to the bare-`{}`
count" gotcha (found first for `CSTGHeapManager::Alloc`, sec 10.164).
This means `CSTGMonitorMixerChannel::Initialize()`'s own unconditional
`+0x4` dereference reads a field no ctor in this project populates yet --
a real, pre-existing gap (not introduced by this batch), documented in
both the header comment and both new KATs (which explicitly provision
the field), left for whichever future batch reconstructs those two
ctors. When scanning `bar2_stubs.cpp`/`managers.cpp` for "the next
smallest candidate," keep checking for non-bare stub bodies specifically
-- this is the third time this exact pattern has hidden a real,
still-open dependency from the official count.

**Deferred candidates, fully characterized for a future batch**: `CSTGEQ`'s
five core math functions (`CalculateLowShelfBeta`/`CalculateHighShelfBeta`/
`CalculatePeakingBeta`/`CalculatePeakingCoefficients`/
`CalculateShelvingCoefficients`) block `CSTGHDRMiniModel::Initialize()`
-- all branch/call-free but two use real `fptan` and two do heavy
multi-register x87 stack shuffling across real branches (biquad filter
coefficient math), NOT reducible to this project's existing
single-input/output x87 primitive-wrapper convention (sec 10.117) --
tractable via a "transcribe the whole function as one verbatim inline-asm
block" approach, deliberately not attempted this batch (time budget).
`CSTGDrumKitData::CSTGDrumKitData()` (925 bytes, confirmed fully
branch/call-free, hence zero crash risk to promote) is a genuinely
intricate nested double-loop (273 outer x 129 inner) populating ~8
near-identical UUID-derived sub-records per entry -- safe but warrants
the sec 10.164 "hand-trace 2-3 cases independently" discipline before
committing to a KAT, not attempted this batch either.

**Verification**: 65 verify/ binaries (up from 63), all exit 0, both of
two full clean-rebuild passes (byte-identical both times); 32 unresolved
symbols; `.gnu.linkonce.this_module` 0x148 bytes; 0 `R_386_GOTPC`;
`OA.ko` 156,984 bytes (up from 153,844). Commit `78649cc`.
