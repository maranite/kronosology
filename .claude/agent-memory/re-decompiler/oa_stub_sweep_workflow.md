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
nm -u OA.ko | wc -l                              # 36 as of batch 32 (32->33 batch31, ->36 batch32; grows as promoted internals reference more real externals)
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

**Batch 23 specifics** (2026-07-06, sec 10.171, commit `246d1df`): user gave
fresh explicit authorization to continue past batch 22. Re-verified state
myself first (HEAD `64bc607`, clean tree, stub count 70, last section
`10.170` -- matched the briefing). Picked up BOTH of batch 22's own
pre-scouted candidates: `CSTGPlaybackBuffer`/`CSTGMonitorMixerChannel`
ctors (managers.cpp, the sec 10.164 "hidden non-bare stub" category --
`{ }` not `{}`, invisible to the bare-stub grep) plus the same-category
`CSTGSlotState` ctor, and `CSTGDrumKitData::CSTGDrumKitData()` (the
925-byte, confirmed branch/call-free 273x129x8 legacy-UUID table batch 22
explicitly flagged as needing the "hand-trace 2-3 cases independently"
discipline). Stub count 70 -> 69 (net -1, bare-`{}` convention -- only
`CSTGDrumKitData` counted; the other three were already-hidden gaps).

**Key technique this batch, a mechanical scale-up of the sec 10.164
"hand-trace 2-3 cases independently" rule for a loop far too large to
trace safely by eye (35,217 total records)**: wrote a standalone Python
script that replays the EXACT disassembled instruction semantics (same
loop bounds, same per-record field offsets) over a real in-memory
`bytearray`, then dumped first/second/mid/last-record field values BEFORE
writing a single line of the C++ reconstruction. This caught, independent
of any hand arithmetic, that the 8 per-record "velocity zone" sub-records
(25-byte stride, `0x19`) collectively span 4 bytes WIDER than the
record's own 514-byte (`0x202`) stride -- meaning the last sub-record's
own trailing word/mask ALWAYS spills into the next record's own `+0`/`+1`
bytes (confirmed real via the model, not a bug), and for the single LAST
record overall, spills 3 bytes PAST the nominal array end, fixing the
class's real confirmed minimum size at a non-obvious `0x1143529` (not the
naively-expected `vtable+arraySize`). Every one of the model's
predictions matched the real C++ KAT byte-for-byte on the FIRST run.
Lesson for any future batch with a similarly huge nested-loop candidate:
when a loop's total iteration count makes "hand-trace 2-3 cases" itself
error-prone (tens of thousands of records, not tens), write a small
Python replay of the disassembly first and generate ground truth from
that, rather than trying to hand-verify a huge space directly -- this is
the sec 10.161 "replay engine for a huge deterministic byte table"
technique, now confirmed useful for a huge deterministic NESTED-LOOP
effect too, not just a flat populate-a-static-array ctor.

**Real, pre-existing gap RESOLVED this batch (not just found)**:
`CSTGMonitorMixerChannel::Initialize()`'s own batch-22 comment had
explicitly flagged its `+0x4` packed-pointer dereference as depending on
"whatever the kernel handed back as fresh module memory" since its own
ctor was still a no-op. This batch's fresh disassembly of the REAL ctor
found the truth is both simpler and safer than that speculation: `+0x4`
is a SELF-referential 16-byte-aligned scratch pointer the ctor itself
computes (`(this+0x17) & ~0xF`, a manual re-alignment trick needed
because this class sits at a non-16-aligned offset within its own parent
array) and stores back into itself -- never an externally-allocated or
uninitialized buffer at all. Lesson: a "real, pre-existing gap, out of
scope to fix" note attached to an unreconstructed SIBLING ctor is exactly
the kind of thing worth re-checking the moment that ctor itself becomes
tractable -- the speculated danger doesn't always turn out to be real.

**Ripple-effect gotcha (a new angle on the sec 10.153/10.158 family, not
previously seen this way): a source file that has NEVER referenced a
given static array before can start doing so the moment a NEW function is
added to it, silently breaking every verify/ file that links that source
file but not the array's own real-storage-providing file.**
`managers.cpp` had never touched `CSTGAudioBusManager::sGlobalBusSet`
before this batch (its real storage lives in `audio_bus_manager.cpp`) --
both new ctors reference it, requiring the by-now-standard "local storage
definition" fix (sec 10.158) in FIVE separate verify/ files
(`test_managers.cpp`/`test_engine.cpp`/`test_global.cpp`/
`test_global_ctor.cpp`/`test_engine_startup_bits2.cpp`) that all link
`managers.cpp` directly. Found reliably by grepping the Makefile for every
`managers.cpp` link line FIRST (5 targets), not by waiting for 5 separate
link failures one at a time. Whenever a new function lands in an
already-heavily-shared file and references ANY static/global whose real
definition lives in a DIFFERENT file, immediately check every verify/
target that links the shared file (not just the one being edited) for
the same gap -- this is the same discipline sec 10.160's
`TSTGArrayManager<CSTGRecordBuffer>` finding already established, now
confirmed for a plain (non-template) static array too.

**Second ripple-effect gotcha, caught by proactively re-reading existing
test assertions before running the suite (not by a KAT FAIL): promoting a
ctor from empty to real breaks any PRE-EXISTING test that explicitly
asserted "this sub-object's memory is untouched because its own ctor is a
no-op."** `test_managers.cpp`'s own `[18]`/`[19]` sections had exactly
such assertions for all three of this batch's newly-real classes
(`CSTGHDRManager`'s and `CSTGVoiceAllocator`'s own already-real ctors
placement-construct 16 of each). Rewrote all three blocks to check the
real written fields plus the still-genuinely-poisoned confirmed gaps,
instead of blanket "still 0xcc". For `CSTGMonitorMixerChannel`'s
self-aligned pointer specifically, the test re-derives the SAME
`(slot+0x17)&~0xF` formula dynamically rather than assuming a fixed
byte offset (the real offset from the object's own base varies with that
object's own alignment mod 16, which isn't guaranteed constant across
array elements unless the array's own outer base happens to be
16-aligned) -- computing it independently in the test would have been
fragile; re-deriving via the identical formula is not. Lesson, a repeat of
the sec 10.153/10.158 family: before considering a ctor promotion done,
grep every verify/ file for the specific classes it constructs (not just
files that mock the promoted symbol directly) for any assertion whose own
wording implies "because the ctor does nothing" -- that wording is a
reliable signal the assertion needs updating, not just re-running.

**Verification**: 66 verify/ binaries (up from 65, new
`test_drum_kit_data`), all exit 0 by real per-binary process exit code
(never log-grepped), both of two full clean-rebuild passes
(byte-identical both times); 32 unresolved symbols; `.gnu.linkonce.
this_module` 0x148 bytes; 0 `R_386_GOTPC`; `OA.ko` 157,960 bytes (up from
156,984). Commit `246d1df`.

**Deferred for a future batch**: `CSTGEQ`'s five core math functions
(unchanged from sec 10.170 -- not re-evaluated this batch, time budget):
`CalculateLowShelfBeta`/`CalculateHighShelfBeta`/`CalculatePeakingBeta`/
`CalculatePeakingCoefficients`/`CalculateShelvingCoefficients`, blocking
`CSTGHDRMiniModel::Initialize()` -- two use `fptan`, two do heavy x87
stack shuffling across real branches, tractable only via a "transcribe
the whole function as one verbatim inline-asm block" approach (sec
10.117 primitives don't fit). Also worth a fresh look next batch:
`CSTGPlaybackBuffer`/`CSTGMonitorMixerChannel` now being real ctors makes
it worth re-checking whether any of THEIR OWN other methods
(`AddEvent`/`RemoveEvent`/`ProcessSubRate`/`RunMonitor`/`StartRampIn`/
`StartRampOut`/`SetMonitorLevel`/`GetMeterLevel`/etc, still fully
opaque/unreconstructed) are now individually tractable given the class's
own fields are no longer purely speculative.

**Batch 24 specifics** (2026-07-06, sec 10.172, commit `5eba788`): the
user gave fresh explicit authorization to continue past batch 23.
Re-verified state myself first (HEAD `5e28bb4`, clean tree, bare-`{}`
stub count 69, last section `10.171` -- matched the briefing). Ran on
the `kronosdev` host (192.168.3.86, same local `/home/share` +
`/home/build/linux-kronos` access as batch 21 -- no SSH to 192.168.3.92
needed this time either) with **direct access to
`/home/share/Decomp/OA.ko_Decomp/OA.ko`** (the actual OA_real.ko binary,
not just a pre-existing text dump) -- `nm -S -C --size-sort` and
`objdump -dr` against it directly, no Ghidra MCP needed at all this
batch.

Picked up batch 22/23's own explicitly pre-scouted lead: now that
`CSTGPlaybackBuffer`/`CSTGMonitorMixerChannel` have real ctors, checked
their OWN other still-opaque methods. Reconstructed
`CSTGPlaybackBuffer`'s full event-management cluster --
`EventBufferStartLocationUpdated`/`SetCurrentReadEvent`/
`AdvanceToNextFillEvent`/`HandleAdvanceCancelledEvent`/`AddEvent`/
`AdvanceReadPosition` (6 methods, new dedicated file
`playback_buffer_events.cpp`) -- plus, via `AdvanceReadPosition`'s own
one new dependency, ALL SEVEN of `CSTGDiskCostManager`'s remaining small
methods (added directly to `engine_startup_bits2.cpp`, which already
owns that class's real `Initialize()`). 13 functions total, bare-`{}`
stub count UNCHANGED (69 -> 69) -- none of these 13 had a pre-existing
stub placeholder to remove (same accounting shape as sec 10.158's
brand-new-class batches); don't be alarmed by a flat delta when the
batch's own candidates were never bare-`{}` stubs to begin with.

**Key technique, a further confirmed instance of "check the whole class
once a tiny dependency turns up" (sec 10.158/10.169)**: `AdvanceReadPosition`'s
own only unreconstructed dependency, `CSTGDiskCostManager::
UpdateHDRBufferWaterMarks`, was itself a 9-byte one-liner -- checking
that WHOLE class's method table via `nm -S -C --size-sort | grep
CSTGDiskCostManager::` immediately turned up six more equally tiny,
equally self-contained (zero calls, zero vtable dispatch) methods,
reconstructed alongside it for barely more effort than the one method
that was strictly needed.

**Satisfying field-identity confirmation, not a new gotcha**:
`CSTGPlaybackBuffer::EventBufferStartLocationUpdated`/`SetCurrentReadEvent`
both write into `this+0xc` -- which turned out to be the ALREADY-NAMED
`CSTGHDRCircularBuffer::readPos` field (the embedded sub-object at this
class's own offset 0), not a new field needing its own name. It doubles
as a cache of the current read event's own "buffer start location"
pointer. A clean example of the sec 10.157/10.163 "re-read an earlier
pass's own already-confirmed field layout before hand-deriving a new
name" technique paying off immediately, with zero ambiguity.

**Important new gotcha, caught by a real `ld` "multiple definition"
error on the FIRST `make ko` attempt -- a sharper, previously-unstated
angle on the sec 10.160 template-static-storage fix**: that fix (an
explicit full specialization, `template<> TSTGArrayManager<SomeType>
*TSTGArrayManager<SomeType>::sInstance = 0;`) has ORDINARY (strong)
linkage and is a **verify/-only-binary technique** -- safe ONLY in a
standalone host KAT that never links `engine_init.cpp`. A PRODUCTION
`.cpp` file that ends up in `OA-objs` (linked into the real `.ko`
alongside `engine_init.cpp`) must NEVER carry that same explicit
specialization line if `engine_init.cpp` itself already ODR-uses that
exact `T` -- `engine_init.cpp`'s own generic (non-specialized) template
definition line has VAGUE/WEAK linkage and already implicitly
instantiates the same storage there, so the production file's own
strong-linkage explicit specialization collides with it at the real
`.ko` link (`ld: multiple definition of
'TSTGArrayManager<CSTGPlaybackEvent>::sInstance'`), even though the
identical-looking line links perfectly fine in isolation in a verify/-only
binary. Fixed by removing the explicit specialization from the
production file (`playback_buffer_events.cpp`) entirely -- it now relies
on `engine_init.cpp`'s own already-linked instantiation for the real
`.ko` -- and keeping it ONLY in the new file's own dedicated verify/ KAT,
exactly matching the pre-existing (but previously not explicitly
articulated this way) `managers.cpp`/`TSTGArrayManager<CSTGRecordBuffer>`
precedent: `managers.cpp` itself never defines that storage either, only
the verify/ files that skip `engine_init.cpp` do. **Rule going forward:
before writing an explicit-specialization storage line for ANY
`TSTGArrayManager<T>` (or other plain-template-defined) static in a
PRODUCTION file (destined for `OA-objs`), first `grep
'TSTGArrayManager<YourType>' src/engine/engine_init.cpp` -- if
`engine_init.cpp` already ODR-uses that exact `T`, do NOT add the
explicit specialization there; add it only to that file's own dedicated
verify/ test instead.** This is a real, previously-latent trap: sec
10.160's own writeup never distinguished "verify/-only" from
"production/OA-objs" placement, and this is the first batch where a
`TSTGArrayManager<T>` reconstruction landed in a file that's ALSO linked
into the real `.ko` alongside `engine_init.cpp`.

**Smaller gotcha, caught by a real KAT `FAILED` line in the new test's
own first draft (a test bug, not a reconstruction bug)**:
`CSTGHDRCircularBuffer::ReturnUnusedFillBytes(n)` (already-real,
managers.cpp, sec 10.158) decrements `availableReadBytes` (`+0x20`), NOT
`availableFillBytes` (`+0x24`) -- despite the method's own name
suggesting the latter. Fixed the KAT's own expectation, not the
reconstruction (which correctly just calls the already-real method).
Another instance of "don't infer a field's role from a method's English
name alone, check the actual already-committed body" (sec 10.149
family).

**Deliberately NOT promoted, characterized for a future batch**:
`CSTGPlaybackBuffer::RemoveEvent`/`EventFileError` (121B/212B) are BOTH
otherwise fully tractable (their own `TSTGArrayManager<CSTGPlaybackEvent>::
sInstance`-based free-list push, via its already-declared
`bucketArray`/`writeCursor`/`modulus` fields, is itself simple field
arithmetic) but each contains one real `call *0x1c(%edx)` -- a genuine
vtable slot-7 dispatch on `CSTGPlaybackEvent`'s own still-zero-filled
placeholder vtable (`_ZTV17CSTGPlaybackEvent`, sec 10.153's "install
only" category) -- a confirmed new crash risk, not mere complexity.
Tractable the MOMENT that vtable slot gets a real target. `ProcessSubRate()`
(860B) remains rejected too: 2 of its 4 external calls
(`CSTGPlaybackEvent::GetDispositionForReadAttempt`,
`USTGHDRUtils::ConvertWaveToSTGSamples`) are genuinely unreconstructed.

**Verification**: 67 verify/ binaries (up from 66, new
`test_playback_buffer_events`), all exit 0 by real per-binary process
exit code (never log-grepped), both of two full clean-rebuild passes on
`kronosdev` (byte-identical both times); 32 unresolved symbols;
`.gnu.linkonce.this_module` 0x148 bytes; 0 `R_386_GOTPC`; `OA.ko` 159,660
bytes (up from 157,960). Commit `5eba788`.

**Batch 25 specifics** (2026-07-06, sec 10.173, commit `613ebc9`): the
user gave fresh explicit authorization to continue past batch 24.
Re-verified state myself first (HEAD `54a54cf`, clean tree, bare-`{}`
stub count 69, last section `10.172` -- matched the briefing). Ran on
`kronosdev` (192.168.3.86), same local `/home/share` +
`/home/build/linux-kronos` access as batches 21/24 -- no SSH detour.

Picked up sec 10.172's own explicitly pre-scouted lead: `CSTGPlaybackBuffer::
RemoveEvent()`/`EventFileError()`, deferred because both dispatch through
`CSTGPlaybackEvent`'s own vtable slot 7 (`call *0x1c(%edx)`), a
zero-filled placeholder at the time. Checked the WHOLE `CSTGPlaybackEvent`
class (`nm -S -C --size-sort`) rather than just the one needed method --
every remaining method (`Reset()`/`HandleFileOpened()`/`HandleFileClosed()`/
`HandleErrorOpening()`/`HandleErrorReading()`/`GetDispositionForReadAttempt()`/
`IncrementBufferStartLocation()`/`SeekSkipFileBytes()`/destructor) turned
out small and self-contained -- a further confirmed instance of the sec
10.158/10.169/10.172 "check the whole class once a tiny dependency turns
up" technique. 12 functions total across two files
(`playback_event_methods.cpp` new, `playback_buffer_events.cpp` extended).
Bare-`{}` stub count unchanged, 69 -> 69 (none of these had a pre-existing
stub -- same accounting shape as sec 10.158/10.172's brand-new-method
batches).

**Core finding: resolved the vtable slot itself, not just the caller.**
`readelf -r` against `.rodata._ZTV17CSTGPlaybackEvent` shows slot 7 (byte
offset `0x24` of the 40-byte array) is `CSTGPlaybackEvent::Reset()` --
and since the ctor is the ONLY site in the whole real binary that ever
installs this exact vtable pointer (nothing derives from it), there is no
second possible dispatch target to model. `RemoveEvent`/`EventFileError`
reproduce the confirmed `call *0x1c(%edx)` as a DIRECT `event->Reset()`
call rather than populating the placeholder byte-array vtable and reading
through it at runtime -- much simpler than the `g_programSlotVtable`/
`CallVtableSlot7` machinery (sec 10.153/`global.cpp`), which stays
reserved for cases where the real dispatch target genuinely isn't known.
When a "dispatch through a still-placeholder vtable" blocker gets
re-examined, always resolve the SPECIFIC slot via `readelf -r` first --
if it turns out to have exactly one possible real installer anywhere in
the binary, a direct call is both simpler and more honest than rigging a
fake vtable array.

**Confirmed real quirk, found by hand-tracing (not by a KAT failure)**:
`RemoveEvent()`/`EventFileError()` both SAVE the event's own `+0x10`
field (a `CSTGAudioEvent` field that the dispatched `Reset()` call itself
unconditionally zeroes) immediately before the dispatch, and RESTORE the
saved value immediately after -- net effect, that one field alone
survives the `Reset()` call untouched. KAT'd by priming `+0x10` to a
poison value and asserting it survives (sec 10.153's poison-pattern
discipline, applied to "assert what's supposed to survive," not just
what's supposed to change).

**MAJOR new gotcha, found by a real KAT `FAILED` line then confirmed via
`objdump` against an actual `-m32 -mregparm=3` compile: GCC eliminates
memory writes in ANY destructor whose ONLY effect is a write to `this`
with no other calls.** This is a "destructor purity" dead-store
elimination UNIQUE to genuine `~ClassName()` destructors (a plain
function with an IDENTICAL body does NOT get this treatment) -- since
nothing in the C++ abstract machine can legally observe a write to an
object whose lifetime just ended, GCC removes the store entirely,
compiling `~CSTGPlaybackEvent()`'s one-line body down to a bare `ret`.
Verified directly: a plain, non-volatile version of `*(unsigned int
*)this = ToU32(...)` inside a real `~ClassName()` produces ZERO
instructions under `-O2 -m32 -mregparm=3` (checked via a throwaway
host-arch repro AND an actual `-m32` object file compiled with this
project's exact Kbuild flags) -- a real divergence from the confirmed
real 7-byte body. Every OTHER destructor already reconstructed in this
project (`~CSTGVoiceAllocator`/`~CSTGMessageProcessor`/
`~CSTGVoiceModelManager`) happens to call at least one other function in
its own body, which suppresses this optimization -- this is the FIRST
destructor in this project whose only effect is a raw write, hence the
first to expose it. Fixed with a `volatile` write on the store itself
(re-confirmed via the same `-m32 objdump` check to restore the real
instruction) -- doesn't change real-target behavior (nothing on the
real target does C++-abstract-machine lifetime reasoning about kernel
memory; this only defeats GCC's own optimization pass), it's a pure
codegen fix. **Standing rule for all future batches: any destructor whose
ENTIRE body is a memory write with no other calls needs a `volatile`
write, verified via `objdump` -- do NOT trust a host KAT alone to catch
this (the read side, even a `volatile` read, does NOT catch it -- only
making the WRITE volatile fixes the codegen; confirmed both ways via a
throwaway repro before touching production code).** Also worth noting:
a plain FUNCTION (not a real destructor) with the identical body is NOT
affected -- this is genuinely specific to the compiler's C++ object-
lifetime reasoning for `~ClassName()`, not a generic "last write before
return" optimization.

**Second gotcha (minor): a mutual file dependency between two production
`.cpp` files is fine and sometimes unavoidable.** `playback_event_methods.cpp`'s
`HandleFileClosed()` calls `CSTGPlaybackBuffer::RemoveEvent()`
(`playback_buffer_events.cpp`), and `RemoveEvent()`/`EventFileError()`
there call `CSTGPlaybackEvent::Reset()` (`playback_event_methods.cpp`) --
a genuine two-way dependency. Harmless for the real `.ko` build (both
files are always linked together in `OA-objs`), but any verify/ KAT that
exercises EITHER cross-call must link BOTH files. Solved by extending the
existing `test_playback_buffer_events.cpp` (which already has the heavy
fixture -- a real `CSTGPlaybackBuffer::Initialize()`'d instance, a real
`TSTGArrayManager<CSTGPlaybackEvent>` setup) to also link
`playback_event_methods.cpp`, while giving the OTHER class's own simpler
methods a lightweight, SEPARATE `test_playback_event_methods.cpp` that
mocks `CSTGPlaybackBuffer::RemoveEvent()` (to test `HandleFileClosed()`'s
own dispatch logic in isolation, without needing the full heavy fixture).
When two production files have a genuine mutual dependency, split KAT
responsibility this way rather than trying to force one giant combined
test or (worse) picking an arbitrary link order that silently under-tests
one direction.

**Verification**: 68 verify/ binaries (up from 67, two new:
`test_playback_event_methods`, plus `test_playback_buffer_events` extended
with 3 new sections: [7] RemoveEvent, [8] EventFileError, [9]
HandleFileClosed integration), all exit 0 by real per-binary process exit
code (never log-grepped), both of two full clean-rebuild passes on
`kronosdev` (byte-identical both times); 32 unresolved symbols;
`.gnu.linkonce.this_module` 0x148 bytes; 0 `R_386_GOTPC`; `OA.ko` 161,496
bytes (up from 159,660). Commit `613ebc9`.

**Deferred for a future batch**: `CSTGEQ`'s five core math functions
(unchanged, still needs the "transcribe the whole function as one
verbatim inline-asm block" approach). `CSTGPlaybackBuffer::ProcessSubRate()`
(`.text+0xd6660`, 860B) is now down to ONE genuinely unreconstructed
dependency: `USTGHDRUtils::ConvertWaveToSTGSamples(...)` (`.text+0xd37a0`,
245B, a brand-new class) -- its own two prior co-blockers
(`GetDispositionForReadAttempt`/`UpdateHDRBufferWaterMarks`) are both real
now (this batch and sec 10.172 respectively).

**Batch 26 specifics** (2026-07-06, sec 10.174, commit `8ed19b3`): the user gave
fresh explicit authorization to continue past batch 25. Re-verified state
myself first (HEAD `81f7d55`, clean tree, bare-`{}` stub count 69, last
section `10.173` -- matched the briefing). Ran on `kronosdev`
(192.168.3.86), same environment as batches 21/24/25, this time WITH
direct local access to `/home/share/Decomp/OA.ko_Decomp/OA.ko` (unlike
batch 21 which lacked it, matching batch 24's later access).

Picked up sec 10.173's own pre-scouted lead: `USTGHDRUtils::
ConvertWaveToSTGSamples()` (`ProcessSubRate()`'s last remaining
dependency). Fresh disassembly immediately turned up FIVE further plain-
C-linkage (NOT C++ mangled -- confirmed via plain `nm`, not `nm -C`) leaf
functions it dispatches through (`ByteSwapMono2ByteStream`/
`ByteSwapStereo2ByteStream`/`ConvertMono2ByteWaveToSTGSamples`/
`ConvertMono2ByteWaveToSISTGSamples`/`ConvertStereo2ByteWaveToSISTGSamples`)
-- 6 functions total, all in a new `src/engine/wave_sample_convert.cpp`.
Bare-`{}` stub count unchanged 69->69 (none of these 6 had a pre-existing
stub); ONE new non-bare stub added (`Convert44100WaveToSTGSamples`,
`{ return 0; }`, invisible to the bare-`{}` grep, sec 10.164 pattern).

**Key finding: this batch's own 5 leaf functions are REAL SSE2 vector
arithmetic (movaps/pshufd/cvtdq2ps/punpckhwd/punpcklwd/mulps/addps/
pcmpgtw), yet were still tractable -- a DIFFERENT case from the
already-deferred `CSetListEQ::SetBand()` SSE blocker (sec 10.162/10.170/
10.173).** The distinguishing factor isn't "is it real vector arithmetic
vs a wide copy" (both of these ARE genuine arithmetic) -- it's whether the
function has any OTHER unresolved external dependency. `SetBand()` stays
deferred because it ALSO calls unresolved `CSTGEQ::
CalculatePeakingCoefficients()` + an external `am_exp2_ess` symbol; these
5 leaf functions operate on a single self-contained local struct with NO
other external call, so their per-element math (sign-extend int16->int32,
convert to float, scale, optionally duplicate for mono->stereo widen,
mix-add into existing dest) is fully reimplementable as an ordinary scalar
loop with identical per-sample VALUES -- this project has never targeted
byte-identical machine code, only KAT-backed functional correctness.
Lesson for future batches with a similar "real SSE arithmetic" candidate:
check for OTHER unresolved dependencies FIRST, don't reject on "it's real
vector math" alone -- that's necessary but not sufficient grounds to defer.

**Second finding, a NEW angle on the sec 10.117 float/`-msoft-float`
gotcha**: even a plain scalar `float` multiply/add reimplementation (no
x87/SSE asm needed at the source level at all) still pulls in unresolvable
libgcc soft-float helpers (`__mulsf3`/`__addsf3`/`__floatsisf`) under this
kernel build's own `-msoft-float` default -- confirmed via a throwaway
`-m32 -mregparm=3 -msoft-float` compile BEFORE ever wiring the new file
into the Makefile (a cheap, fast check worth doing as a matter of course
for any new file with real float arithmetic, not just ones that "look"
x87/SSE-heavy). Fixed the same established way:
`CFLAGS_wave_sample_convert.o := -mhard-float -msse2 -mfpmath=sse`.

**Third technique, worth reusing: decode `.rodata`/`.rodata.cst4`/
`.rodata.asm` constants via a small Python `struct.unpack('<f', ...)`
one-liner directly from `objdump -s` hex dumps, rather than hand-computing
IEEE-754 bit patterns.** Found the per-format normalization table
(`{0, 1/127, 1/32767, 1/8388607}` for 8/16/24-bit PCM, indexed by the
already-named `CSTGAudioEvent::field1d`) and `allMinus3dB` (`0.70710677 =
1/sqrt(2)`) this way -- fast, mechanical, and independently re-checkable
by anyone re-reading the batch. Also caught (by re-deriving TWICE, sec
10.151-style discipline) that the 24-bit inline path's two separate
`readelf`-visible `.rodata.cst4` relocation SITES resolve to the exact
SAME constant value (`1/8388607`) -- an early draft assumed two different
constants purely from seeing two relocation entries; always dereference
the actual CONTENT at each site, don't infer distinctness from relocation
count alone.

**Real, confirmed quirks faithfully preserved (not simplified away)**:
(1) `ConvertWaveToSTGSamples()`'s return value is `(unsigned char)count`
(the ORIGINAL input truncated to a byte) on every path except `count==0`
and the 44100Hz-forward -- confirmed load-bearing at `ProcessSubRate()`'s
own `movzbl %al,%eax` use as "samples consumed this call," implying every
real caller keeps `count <= 255` (matching the fixed 4096-byte
`sConvertBuffer` scratch area). (2) the 16-bit SSE converters mix-ADD into
the existing dest buffer (confirmed real load-old-value-then-addps-then-
store), while the 24-bit inline path OVERWRITES dest (no old-value load)
-- a real format-dependent asymmetry, independently KAT-exercised both
ways rather than assumed symmetric.

**Toolchain/environment gotcha, none new this batch** -- `kronosdev`
(192.168.3.86) continues to work exactly as documented in batches 21/24/25
(local `/home/share`, local `/home/build/linux-kronos`, no SSH detour
needed either for disassembly OR for the build/verify sequence).

**Verification**: 69 verify/ binaries (up from 68, new
`test_wave_sample_convert`), all exit 0 by real per-binary process exit
code (never log-grepped), both of two full clean-rebuild passes on
`kronosdev` (byte-identical both times); 32 unresolved symbols;
`.gnu.linkonce.this_module` 0x148 bytes; 0 `R_386_GOTPC`; `OA.ko` 163,288
bytes (up from 161,496). Commit `8ed19b3`.

**Deferred for a future batch, unchanged**: `CSTGEQ`'s five core math
functions (still needs the verbatim-inline-asm-transcription approach).
`USTGHDRUtils::Convert44100WaveToSTGSamples()` (`.text+0xd3270`, 1313B) --
newly characterized this batch: a genuine fractional-phase-accumulator
resampler (confirmed real 24.8 fixed-point phase step `0xeb3333` =
44100/48000 exactly, plus a 4-tap history ramp-up gated by `event`'s own
`+0x60`/`+0x61` byte counters), x87-stack-juggling-heavy (5 live FPU-stack
values at once) across real branches -- same deferred category as
`CSTGEQ`, not attempted this batch. `CSTGPlaybackBuffer::ProcessSubRate()`
itself (`.text+0xd6660`, 860B) is now confirmed to have ZERO remaining
EXTERNAL dependency, but its OWN 860-byte body is a substantial real-time
state machine (5-outcome disposition dispatch, `CSTGDiskCostManager`
water-mark update, a 4096-byte `sConvertBuffer` wraparound `rep movsl`
copy, and a `TSTGArrayManager<CSTGPlaybackEvent>` free-list recycle
matching `RemoveEvent()`'s own mechanics) -- deliberately left for its own
dedicated future pass rather than rushed alongside this batch's already-
substantial 6 DSP conversion functions.

**Batch 27 specifics** (2026-07-06, sec 10.175, commit `ab5ac1a`): the user
gave fresh explicit authorization to continue past batch 26. Re-verified
state myself first (HEAD `3835657`, clean tree, bare-`{}` stub count 69,
last section `10.174` -- matched the briefing). Ran on `192.168.3.92`
(root/kronosbuild via `sshpass -p kronosbuild ssh ...` -- no key auth from
this environment), same CIFS-shared `/home/share/kronosology` + local
`/home/build/linux-kronos` as prior batches; always `cd
/home/share/kronosology/reconstructed/OA && ...` in one non-interactive ssh
command (batch 20's own standing gotcha -- a bare login lands in `/root`).

Picked up sec 10.174's own pre-scouted lead: `CSTGPlaybackBuffer::
ProcessSubRate()` (`.text+0xd6660`, 860 bytes), confirmed to have ZERO
remaining external dependency. ONE function reconstructed
(`src/engine/playback_subrate.cpp`), plus its own required new
`CSTGPlaybackBuffer::sConvertBuffer[]` storage + `ProcessSubRate()` method
declaration in `oa_engine.h`. Bare-`{}` stub count unchanged, 69 -> 69 (no
pre-existing stub -- same "brand-new method on an already-real class"
accounting shape as several prior batches).

**Batch 26's own "4096-byte sConvertBuffer" claim was WRONG -- caught by
independently `nm -S`-checking the real symbol before writing any code
against it, not by a KAT failure.** The real confirmed size is `0x100`
(256) bytes, matching the function's own `rep stosb`/`ecx=0x100` zero-fill
exactly. No real behavior was ever at risk (max real per-chunk copy is
`threshold`(<=0x40)*`field1d`(<=3) <= 192 bytes, well under 256) -- but a
future batch inheriting an unverified "4096-byte" claim and sizing a NEW
buffer/test against it would have been building on a wrong premise. Lesson
reinforced (this is at least the second time in this project a PRIOR
batch's own inferred-not-measured claim about a real symbol's size turned
out to be wrong, sec 10.155's own hex-addition slip being the other): when
a future batch's own pre-scouted lead comment states a byte size for a REAL
symbol, re-verify it with `nm -S -C` before trusting it, even if it "sounds
plausible" and even when picking up a lead you didn't originate.

**Full hand-trace technique for a large, no-decompiler-available, tightly
looping state machine (five outcome codes, three different loop-continue
jump targets, one register-carryover-across-loop-iterations dependency)**:
labeled every basic block by its own address first, wrote its literal
register-level effect (never renaming/interpreting until AFTER the full
CFG was mapped), THEN cross-referenced already-established field names from
sibling files (`playback_buffer_events.cpp`'s `windowSize`/`consumed`/
`windowThreshold`/`maxReadBytes`, `playback_event_methods.cpp`'s
`CSTGAudioEvent::fieldC`) to confirm several of ProcessSubRate()'s own
touched offsets were ALREADY-NAMED fields on `CSTGPlaybackEvent`, not new
ones -- a strong independent cross-check (same technique as batch 10's own
"re-read the target class's own earlier header comment first" win),
finding zero contradictions. The one place a register (EAX) genuinely
carries a stale value from a PRIOR loop iteration into a later branch
(the `outcome==3` bookkeeping path) was reproduced via an algebraically
equivalent persistent local (`carry = consumed - convertCount`) rather than
trying to mimic raw register lifetime in structured C -- simpler and
exactly as faithful once the algebra is verified independently (which the
KAT then confirmed).

**Genuinely new gotcha, its own build-flag angle distinct from every prior
`-msoft-float`/float-helper instance**: this function's own wraparound ring
copy has two RUNTIME-length segments (not compile-time constants) --
`__builtin_memcpy` for a runtime length compiles fine under a plain host
`g++` sanity check but, once built against the REAL kernel tree via
`make ko`, lowers to an actual call to library `memcpy` (GCC can't inline a
builtin copy of unknown-at-compile-time length) -- this pushed `nm -u
OA.ko` from 32 to 33, a NEW unresolved symbol no other file in this project
had ever needed. Caught ONLY because the full `make ko`+invariant-check
sequence was run (a host-only `g++` compile of the same file gave zero
warnings and looked completely clean) -- reinforces the standing rule that
a host KAT passing is never sufficient on its own; the real `.ko` build's
own `nm -u`/linkonce/GOTPC checks must ALWAYS run too, even when nothing
about the change "looks" float/SIMD-related. Fixed with a plain manual
per-byte copy loop (`RawCopy()`) instead of `__builtin_memcpy` -- the SAME
file's OWN `__builtin_memset(sConvertBuffer, 0, sizeof(...))` (a genuine
compile-time-constant 256-byte size) was unaffected and stays as-is: the
distinguishing factor for `__builtin_mem*` is compile-time-constant size
(inlines away cleanly) vs. runtime size (silently pulls in a real libcall),
not "is this a builtin at all."

**Confirmed real quirk, found by hand-tracing THEN verified by an actual
KAT `FAILED` line against a first, wrong test expectation (own-test bug,
not a real-binary bug)**: `recycle_event`'s `event->+0x8 = 0` write does
NOT survive to the end of the call when that same event is also
`this->+0x4c` (the current read event) -- recycling never updates
`this->+0x4c`, so the immediately-following `advance_to_next` block
re-examines the SAME event as `curReadEvt`, sees state `0 != 3`, and
unconditionally forces it back to 3. A first KAT draft asserted `+0x8==0`
after recycling and got a real `FAILED: got=0x3 want=0x0` -- fixed the
TEST's own expectation (not the reconstruction) after re-deriving the
interaction a second time from the raw disassembly. Same family as sec
10.158's "don't assume, count the actual zero-stores" discipline, but this
time the trap is a cross-block interaction rather than a single ctor's own
field list -- worth checking for on ANY future "recycle/retire" style
function where the SAME object pointer might still be referenced by an
outer/caller-visible slot after being pushed onto a free list.

**Verification**: 70 verify/ binaries (up from 69, new
`test_playback_subrate`), all exit 0 by real per-binary process exit code
(never log-grepped), both of two full clean-rebuild passes on
`192.168.3.92` (byte-identical both times, `OA.ko` 164,432 bytes both
runs); 32 unresolved symbols; `.gnu.linkonce.this_module` 0x148 bytes; 0
`R_386_GOTPC`. Commit `ab5ac1a`.

**Deferred for a future batch, unchanged**: `CSTGEQ`'s five core math
functions (still needs the verbatim-inline-asm-transcription approach).
`USTGHDRUtils::Convert44100WaveToSTGSamples()` (`.text+0xd3270`, 1313
bytes), fully characterized since batch 26, not attempted. With
`ProcessSubRate()` now real, `CSTGPlaybackBuffer` has no more known
unreconstructed methods of its own -- batch 28 will need a fresh
`nm -S -C --size-sort` sweep rather than a pre-scouted single-class lead.

**Batch 28 specifics** (2026-07-07, sec 10.176, commit `4e411ce`): the user
gave fresh explicit authorization to continue past batch 27. Re-verified
state myself first (HEAD `9ad04ba`, clean tree, bare-`{}` stub count 69,
last section `10.175` -- matched the briefing). Ran on `kronosdev`
(192.168.3.86), same local `/home/share` + `/home/build/linux-kronos` +
local `/home/share/Decomp/OA.ko_Decomp/OA.ko` access as prior batches.

Sec 10.175 left no pre-scouted lead (`CSTGPlaybackBuffer` fully resolved),
so did a fresh sweep of `bar2_stubs.cpp`'s own ~58 remaining bare-`{}`
stubs. Freshly disassembled ~15 candidates (40-1600 bytes); most were
confirmed BLOCKED with concrete new detail (full list in sec 10.176):
the four remaining file-daemon `ProcessCommands()` siblings (all four
individually confirmed real vtable/PTMF dispatch, closing out that
cluster for good), `CSTGMidiPortManager::Initialize()` (16 real indirect
calls), `CSTGEffectManager::RunEffects()`/`CSTGAudioBusManager::
MixPerformanceOutputs()` (both transitively reach `CSetListEQ` via
DIFFERENT chains -- `CSTGPerformance::RunEffects()` and
`CSTGPerformanceVars::Free()` respectively -- strengthening the case
that `CSetListEQ`/`CSTGEffectRack`/`CLoadBalancer::{Load,Unload}EffectCost`
is worth a dedicated future-batch push), `CSTGMonitorMixer::RunMonitors()`
(blocked by a whole separate 7-9-function per-channel-mode DSP kernel
subsystem), `CSTGVoiceAllocator::DoPendingMoveVoices()`/`CSTGSlotVoiceData::
RunVoiceModelFeedback()` (both confirmed real vtable dispatch).

Picked the ONE genuinely tractable find: `CSTGLFOTables::CSTGLFOTables()`
(2433 bytes) -- confirmed zero calls, zero vtable dispatch (no vtable at
all for this class), the "safe by instruction class" category (sec
10.160/10.161) just with ~9 distinct loop/table blocks instead of one
flat table. Populates a `CSTGBankMemory::AllocAligned(0x1830,0x10)`
object with ~15 LFO/step-sequencer waveform tables: phase ramps, a
128-entry sine table (33-entry literal quarter table + mirror + negate,
confirmed exact at all 4 quadrant boundaries vs `sinf(i*pi/64)`), a
128-entry S-curve/tanh ramp reused 4 ways (fwd/rev/even/odd-half-res),
an unidentified 110-entry envelope (no closed form found, reproduced
verbatim), and four staircase quantization tables (3/4/4/6 levels, two
with uneven 22/21/22/21/22/20 segments). Stub count 69 -> 68 (net -1).

**Major new technique, an escalation of the sec 10.161/10.171/10.172
"replay-script" family: when a function is too structurally intricate
to hand-trace safely (not just too big), write a from-scratch x87-stack
+ x86-register MINI-INTERPRETER that mechanically replays the exact
`objdump -dr` instruction+relocation stream over a real virtual buffer,
rather than hand-deriving each loop's semantics.** Implemented every
mnemonic this one function actually used (a small, closed set --
mov/movl/lea/xor/sub/subl/cmp/je/jne/jl/jmp/push/pop plus fld/fld1/
fldz/flds/fst/fsts/fstp/fstps/fadd/fadds/fsub/fsubs/fchs/fxch), NOT a
generic x86 disassembler -- feasible specifically because the mnemonic
set was small and closed. Cross-verified the interpreter's own output
against independent hand-derivation for the first 3-4 blocks (exact
match) before trusting it for the rest. This is a *tool*-level escalation
of the established technique (prior batches wrote replay scripts to
generate ground-truth *data* for an already-understood algorithm; this
one used the replay to discover the algorithm's own structure in the
first place, because the control flow -- not just the iteration count --
was too varied to hand-simulate end to end safely).

**Sharpened finding: resolving a `.rodata`/`.rodata.cst4` relocation by
NAME (`objdump -s -j .rodata.cst4`) silently reads the WRONG bytes when
the binary has thousands of same-named section instances.** This
particular `.ko` (an unlinked-relocatable kernel module, `ld -r`-style)
keeps EVERY input `.o`'s own `.rodata.cst4`/`.rodata` as a SEPARATE
section instance, all sharing the same section NAME -- `readelf -S`
shows 14000+ sections here. The only reliable way to resolve a specific
relocation's target bytes: take `readelf -r --wide`'s packed `Info`
field, shift right 8 bits to get the REAL symtab index (a decimal-vs-hex
column-count trap: don't just read the printed "Sym.Value" or count rows
in `readelf -s`), look that index up in `readelf --syms --wide` to get
the section index (not a name), then `readelf -x <NUMBER>` (numeric
section index, not name) to dump the correct instance's own bytes at
that file offset. Getting this wrong reads plausible-looking but WRONG
floats silently -- worth checking for on every future large-ctor batch
that touches more than one or two `.rodata.cst4` relocations, since one
`.rodata.cst4` name collision proved harmless here (all 7 relocations to
that offset happened to share ONE symbol index, i.e. one section
instance) but is not guaranteed to be harmless in general.

**Major gotcha, a real bug caught ONLY by a full golden-buffer KAT, not
by spot-checks: a multi-array loop where different arrays have
DIFFERENT real lengths sharing the same nominal `k=0..N` loop bound
will silently alias and clobber a sibling array's own first entry.**
`mirror`/`negQuarter`/`negMirror` were all looped over the same `k=0..31`
in an early draft, but `mirror` has only 31 REAL entries
(`+0x690..+0x708`) -- its phantom 32nd write (`k=31`, landing on
`+0x70c`) overwrites `negQuarter[0]`'s own correct `-0.0` with `+0.0`,
since it runs LATER in loop iteration order. `verify/test_lfo_tables.cpp`
embeds a `lfo_tables_golden.h` (1548 raw dwords, generated straight from
the interpreter's own verified buffer, never copy-pasted from the
production file) and diffs the constructed object dword-by-dword against
it -- this caught the bug immediately by exact offset, plus a SECOND
formula bug (`negMirror[k] = -kQuarterSine[32-k]`, not `31-k` as an
early draft assumed by pattern-matching off `mirror`'s own formula) the
same way. **New standing technique for any future batch reconstructing
a ctor with several sibling arrays populated in one pass: compute each
array's own real length/base independently and check for byte-range
overlap BEFORE trusting a shared loop bound -- a full golden-buffer
diff (not just hand-picked spot-check assertions) is the reliable way
to catch this class of bug, since a spot-check list only catches it if
it happens to name the exact aliased field.** Also caught, same
mechanism: a genuine hand-transcription slip in the 110-entry envelope
literal table (112 values pasted, several wrong in the middle) --
caught first as a compile-time array-size mismatch, then confirmed via
an independent regex-extraction-vs-ground-truth diff script before ever
running the KAT.

**Smaller finding, reconfirmed not "cleaned up"**: two of the 6-level
staircase tables' "1/3"-ish segments use TWO DIFFERENT one-ULP-apart
literal constants (`0x3eaaaaac` vs `0x3eaaaaaa`, `0xbeaaaaaa` vs
`0xbeaaaaac`) at different positions in the SAME object -- confirmed
real via the raw disassembly immediates directly, deliberately
preserved exactly rather than unified to one value (same family as sec
10.152's forward/reverse jump-table asymmetry).

**Correction to this project's own "dup"/wraparound quirk family**: all
9 "dup" self-referential fields in this object read a field ALREADY
WRITTEN earlier in the SAME constructor (confirmed for every one, via
the interpreter's own instruction-order tracking) -- NOT reads of
uninitialized/zeroed bank memory as this project's usual fallback
hypothesis (sec 10.156 family) would suggest, and as an initial guess
assumed for two of them here before the true source field became
obvious. Cleaner and fully self-contained; worth checking "is this
really reading an earlier field in the same function" before reaching
for the "assume zero-initialized CSTGBankMemory" explanation on any
future multi-dup-field ctor.

**Verification**: 71 verify/ binaries (up from 70, new
`test_lfo_tables`, including a full 1548-dword byte-for-byte golden
comparison, not just spot checks), all exit 0 by real per-binary process
exit code (never log-grepped), both of two full clean-rebuild passes on
`kronosdev` (byte-identical both times -- `OA.ko` 167,320 bytes both
runs); 32 unresolved symbols; `.gnu.linkonce.this_module` 0x148 bytes; 0
`R_386_GOTPC`. Commit `4e411ce`.

**Deferred for a future batch**: `CSTGEQ`'s five core math functions and
`USTGHDRUtils::Convert44100WaveToSTGSamples()` (both unchanged). NEW:
`CSetListEQ`/`CSTGEffectRack`/`CLoadBalancer::{Load,Unload}EffectCost`
now confirmed reachable from THREE independent call chains
(`SetBand()`, `CSTGPerformance::RunEffects()`,
`CSTGPerformanceVars::Free()`) -- worth a dedicated future batch since
resolving it would likely unblock `CSTGEffectManager::RunEffects()` and
`CSTGAudioBusManager::MixPerformanceOutputs()` at once (both otherwise
fully characterized: 4 of `MixPerformanceOutputs()`'s 6 direct
dependencies are already trivially tractable, the other 2 are the
`CSetListEQ`-reaching ones). `CSTGMonitorMixer::RunMonitors()` needs an
entire separate per-channel-mode DSP kernel subsystem (7-9 new plain-C
functions) -- out of scope for a quick pass. The four file-daemon
`ProcessCommands()` siblings are now ALL individually confirmed blocked
by real dispatch -- fully characterized, can be dropped from future
"re-check individually" sweeps.

**Batch 29 specifics** (2026-07-07, sec 10.177, commit `a207584`): the user gave fresh explicit authorization to
continue past batch 28, with an explicit priority assignment: assess sec
10.176's own flagged `CSetListEQ`/`CSTGEffectRack`/`CLoadBalancer::
{Load,Unload}EffectCost` cluster FIRST, reject-with-reason if it's too
big for one batch, and fall back to a fresh smallest-candidates sweep.
Re-verified state myself first (HEAD `4ad026d`, clean tree, bare-`{}`
stub count 68, last section `10.176` -- matched the briefing exactly).
Ran on `kronosdev` (192.168.3.86), same local `/home/share` +
`/home/build/linux-kronos` + local `/home/share/Decomp/OA.ko_Decomp/
OA.ko` access as batches 21/24/25/26/28.

**Did the assessment properly this time -- disassembled the SPECIFIC
two-hop functions sec 10.176 named as the reach path
(`CSTGPerformance::RunEffects(CSTGPerformanceVars*)`, `CSTGPerformanceVars::
Free()`), not just re-read the prior batch's own summary.** Confirmed the
cluster is genuinely intractable for one batch, with concrete new
evidence: `CSetListEQ::Initialize()` (not just `SetBand()`) ALSO calls
`CSTGEQ::CalculatePeakingBeta()` nine times -- meaning the already-deferred
(since batch 22/23) `CSTGEQ` five-math-function cluster is the TRUE ROOT
blocker of the ENTIRE `CSetListEQ`/`CSTGEffectRack` subsystem, not just
`CSTGHDRMiniModel::Initialize()` as previously scoped. This is the single
most valuable finding this batch -- a future batch attacking this cluster
should start with `CSTGEQ`'s math functions specifically, since that's
the one dependency gating multiple entry points at once. Full call-graph
detail (every function named, byte size, and specific blocker) recorded
in MASTER_REFERENCE.md sec 10.177 -- read it before re-doing this
analysis; it supersedes sec 10.176's own vaguer "worth a dedicated push"
framing with a concrete multi-batch scope estimate.

**Lesson reinforced: when a "worth a dedicated future push" recommendation
is inherited from a prior batch, actually disassemble the SPECIFIC
functions it names as the reach path before deciding whether to attempt
it or reject it again -- a summary-level "reachable via chain X" claim
from a prior pass is not the same as having traced chain X yourself.**
This also surfaced one incidental leaf while investigating the cluster:
`CLoadBalancer::UnloadEffectCost(unsigned long)` (`.text+0x62210`, 117
bytes) turned out to have ZERO calls of any kind (confirmed via full
disassembly) -- its own sibling `LoadEffectCost()` is genuinely blocked
(calls `CCPUCostInfo::GetActualIdleEffectCycles()`, which has a real,
reachable vtable dispatch, sec 10.153 rule), but `UnloadEffectCost()`
itself is safe. Promoted it standalone (own dedicated KAT section [6] in
the EXISTING `verify/test_load_balancer_static.cpp`, since it belongs to
the same `CLoadBalancer` class already homed in
`load_balancer_static.cpp` and had zero pre-existing mocks anywhere to
conflict with). Bare-`{}` stub count unchanged, 68 -> 68 (the symbol was
never declared/stubbed in this project before this batch at all -- a
"brand-new method, zero prior placeholder" accounting shape, same family
as sec 10.170/10.174's flat-delta batches).

**New technique, worth reusing: recognizing that a real x87
`fildll`/`fmuls 0.5f`/`fisttpl` sequence reduces to a PLAIN INTEGER
right-shift avoids the entire `-msoft-float` CFLAGS-override song and
dance before it ever becomes a problem.** Decoded the function's own
single `.rodata.cst4` constant via the sec 10.176-established
symtab-index/section-index method (this `.ko`'s thousands of
same-named `.rodata.cst4` instances make a name-based lookup silently
read the wrong one) -- it resolves to exactly `0.5f`. Truncating
`x*0.5` toward zero, for a non-negative integer `x`, is EXACTLY `x>>1`
-- recognized this before writing any code, so `load_balancer_static.cpp`
needed zero new float/double arithmetic and zero per-file
`-mhard-float -msse2 -mfpmath=sse` Makefile override (sec 10.117/
10.174/10.176's own recurring class of gotcha), unlike every prior batch
that actually needed real float math. When a real disassembly's own
float sequence is "convert an integer, multiply by a power-of-two
constant, truncate," check whether it reduces to a plain integer
shift/multiply BEFORE reaching for the CFLAGS-override fix -- it often
does, and avoiding float entirely is strictly simpler and lower-risk
than adding it correctly.

**Swept ~10 more previously-unassessed "smallest remaining" bare-`{}`
candidates independently of the cluster investigation** (full list with
specific per-candidate blocking reason in MASTER_REFERENCE.md sec
10.177): `CSetList::Activate()` (calls `CSetListEQ::SetBand()` 8x, the
same already-blocked SSE+external-symbol function), `CSTGPianoModel::
RescanPianoTypes()` (re-confirms the batch-7 `CSTGPianoTypes`/
`CFileStream`/`CSTGPianoModelPatch`/`CPianoOsc` cluster, PLUS two real
vtable dispatches not previously noted), `CSTGControllerInfo::
SendUnsolicitedUIParam()` (2 real vtable dispatches),
`CSTGSlotVoiceData::RunVoiceModelStaticFront/Back()` (both heavily
vtable-dispatching, same family as the already-blocked
`RunVoiceModelFeedback`), `CSTGVoiceAllocator::FreeVoice()` (fans into
~9 classes plus 2 vtable dispatches), `CSTGVoiceAllocator::
StealVoiceList()` (depends on the already-blocked
`DoPendingMoveVoices()`), `CSTGControllerRTData::ResetKnobsJumpCatch()`
(confirmed the same "5-sibling deeply-hashed-table cluster" as the
already-blocked `SetAudioInSolo`/`ResetSendKnobsJumpCatch`, batch 10),
`CSTGPerformanceVarsManager::Initialize()` (zero vtable dispatch but
fans into 7 classes, one of which -- `CSetListEQ::Initialize()` -- is
the same newly-identified `CSTGEQ`-blocked root as above).

**Verification**: 71 verify/ binaries (unchanged -- extended the
EXISTING `test_load_balancer_static` with a new KAT section rather than
adding a new binary), all exit 0 by real per-binary process exit code
(never log-grepped), both of two full clean-rebuild passes on
`kronosdev` (byte-identical both times -- `OA.ko` 167,452 bytes both
runs, up from 167,320); 32 unresolved symbols; `.gnu.linkonce.
this_module` 0x148 bytes; 0 `R_386_GOTPC`.

**Deferred for a future batch**: `CSTGEQ`'s five core math functions
(unchanged -- now confirmed the TRUE root blocker of the entire
`CSetListEQ`/`CSTGEffectRack` cluster, not just `CSTGHDRMiniModel::
Initialize()` -- start here for the next dedicated push on this
cluster). `USTGHDRUtils::Convert44100WaveToSTGSamples()` (unchanged).
`CLoadBalancer::LoadEffectCost()` (blocked by a real, reachable vtable
dispatch in its own `GetActualIdleEffectCycles()` callee).
`CSTGMonitorMixer::RunMonitors()` (unchanged). Several NEW
not-yet-individually-assessed dependencies surfaced while scoping the
cluster: `CSTGPerformance::Free()`, `CSTGEffectRackVars::
ResetOnActivation()`/`ApplyDModTickDelay()`, `CSTGMIDIClockSync::
EnableActivePerfClock()`, `CSTGSlotVoiceData::EmergencyFreeAllVoices()`,
`CSTGPan::CalculateStereoPanCoeffs()` -- none individually checked yet
for their own tractability (several look plausible by analogy to
already-real siblings, e.g. `CalculateStereoPanCoeffs()` to
`CalculateMonoPanCoeffs`/sec 10.151, and `EnableActivePerfClock()` to
`DisableActivePerfClock`/batch 19, but this is unconfirmed speculation,
not a checked fact).

**Batch 30 specifics** (2026-07-07, sec 10.178, commit `17b863a`): user
gave fresh explicit authorization with a specific priority
-- resolve the sec 10.177 `CSTGEQ` five-math-function cluster (the
confirmed TRUE root blocker of the whole `CSetListEQ`/`CSTGEffectRack`
subsystem). Re-verified state myself first (HEAD `2b85524`, clean tree,
stub count 68, last section `10.177` -- matched). Ran on `kronosdev`
(192.168.3.86), same local access as batches 21/24/25/26/28/29.
**All five functions done in one pass** (not a partial subset) --
`CalculateLowShelfBeta`/`CalculateHighShelfBeta`/`CalculatePeakingBeta`/
`CalculatePeakingCoefficients`/`CalculateShelvingCoefficients`, new file
`src/engine/eq_coefficients.cpp` + `CSTGEQ`/`STGEQCoefficients`/
`eEQShelvingType` in `oa_global.h` (alongside `CSTGPan`). Stub count
unchanged (68 -> 68 -- brand-new class, never had a `bar2_stubs.cpp`
placeholder). Full derivation/verification in MASTER_REFERENCE.md sec
10.178 -- read it before touching `CSTGEQ` again. Key points worth
repeating here since they generalize beyond this one cluster:

**The "whole-function verbatim x87 inline-asm transcription" technique
(this batch's main methodological contribution) works, and is CHEAPER
than it looks**: write ONE `asm volatile` block per function that copies
the real disassembly's own mnemonics near-verbatim (only `.rodata.cst4`
constants, stack-frame args, and the `(%esp)` scratch slot become named
C memory operands) instead of hand-deriving a clean C expression for
multi-register `fxch`-heavy math. Real branches become GNU-as numeric
local labels (`1:`/`2:`, safely reused across sibling functions/asm
blocks in the same TU -- confirmed: numeric labels resolve to the
nearest `Nf`/`Nb` match in the given direction, no cross-function
collision risk). A real compiler TAIL-MERGE in the original object code
(two branches converging on shared cleanup code, one via its own inlined
copy of the tail's first instruction + a skip-ahead jump, the other via
a jump to the tail's true start) is safe to reproduce as a plain shared
label both branches jump to directly -- the skipped instruction was
idempotent (`fstp %st(0)`/`fstp %st(1)`) regardless of who executes it,
confirmed by tracing both paths' FPU stack state independently rather
than assuming the compiler's own code-layout choice was semantically
load-bearing.

**STANDING GOTCHA, worth its own top-level entry since it will bite any
future x87 whole-function transcription, not just this cluster: the
POPPING two-register x87 forms (`fsubp`/`fsubrp`/`fdivp`/`fdivrp
%st,%st(i)`) have their regular-vs-reverse direction OPPOSITE to the
non-popping `fsub %st(i),%st`/`fsubr %st(i),%st` forms (dest=ST0).**
Non-popping (dest=ST0): `fsub %st(i),%st` = ST0-ST(i), `fsubr
%st(i),%st` = ST(i)-ST0 (matches the "obvious" R-suffix-reverses-order
reading). Popping (dest=ST(i)): `fsubp %st,%st(i)` = ST0-ST(i) (SOURCE
minus DEST -- the "regular" non-R form is the one that reads
backwards!), `fsubrp %st,%st(i)` = ST(i)-ST0 (DEST minus SOURCE). Same
swap for `fdivp`/`fdivrp`. This is exactly backwards from what analogy
with the non-popping forms would suggest, and it is NOT something to
re-derive from memory a second time and trust -- confirmed this batch
only via an actual hardware microtest (`flds`/`flds`/`fsubrp`/`fstps`
with two known distinct constants, comparing both candidate formulas
against the real CPU's own answer). A first-draft Python x87 emulator
(used to independently generate KAT golden values) had this backwards,
producing a sign-flipped-but-correct-MAGNITUDE result -- that specific
symptom (right magnitude, wrong sign, in an all-algebraic no-branch-risk
computation) is the reliable tell that a fsubp/fsubrp/fdivp/fdivrp
direction is swapped, not a transcription slip elsewhere. The actual C++
inline asm was never wrong in this batch (verbatim copy of the real
objdump mnemonics throughout) -- only the INDEPENDENT Python oracle used
to check it had the bug, caught by, then fixed via, a real KAT `FAILED`
line plus a standalone microtest, not by staring at the Intel manual
again.

**Second reusable gotcha: don't round to float32 after every
register-to-register op when writing a from-scratch x87 emulator for
KAT-golden-value generation -- only round at the real hardware rounding
POINTS (`fstps`/`fsts`, explicit single-precision stores).** An early
draft rounded every intermediate result to float32 (an over-cautious
carryover from a much simpler earlier primitive-wrapper style), which
produced 1-ULP-off golden values on ~7 checks relative to the real
compiled+executed asm -- real x87 hardware keeps register-to-register
arithmetic in 80-bit extended precision and only narrows to 32-bit on an
explicit store. Fixed by using Python's native double (53-bit mantissa,
not a perfect stand-in for 80-bit/64-bit-mantissa extended precision,
but far closer than float32) for all intermediate ops, rounding to
float32 ONLY at `fstps`/`fsts`.

**Third reusable point: hardware `fptan`/`fcos` are not bit-reproducible
via a software `tan()`/`cos()` oracle -- classify KAT checks accordingly
BEFORE writing them, not after chasing phantom failures.** Any output
field whose OWN computation chain reads a transcendental result needs an
epsilon-tolerant check; any field that's purely algebraic (confirmed via
full disassembly to contain no `fcos`/`fptan` in its own dependency
chain -- `CalculateShelvingCoefficients` has NONE at all, a fully
algebraic function despite computing filter coefficients) can and should
still be checked bit-exact. Don't blanket-apply epsilon tolerance to a
whole function just because SOME of its fields are transcendental-derived.

**Verification**: 72 verify/ binaries (up from 71, new
`test_eq_coefficients`), all exit 0 by real per-binary process exit code
(never log-grepped), both of two full clean-rebuild passes on
`kronosdev` (byte-identical both times -- `OA.ko` 168,956 bytes both
runs, up from 167,452); 32 unresolved symbols; `.gnu.linkonce.
this_module` 0x148 bytes; 0 `R_386_GOTPC`.

**Deferred for a future batch, unchanged**: `USTGHDRUtils::
Convert44100WaveToSTGSamples()`, `CLoadBalancer::LoadEffectCost()`
(blocked by a real reachable vtable dispatch), `CSTGMonitorMixer::
RunMonitors()` (whole separate DSP subsystem). `CSetListEQ`/
`CSTGEffectRack`/`CSetList` themselves are now UNBLOCKED AT THE ROOT
(`CSTGEQ` is real) but still need their own dedicated future push, plus
sec 10.177's other newly-identified dependencies (`CSTGPerformance::
Free()`, `CSTGEffectRackVars::ResetOnActivation()`/
`ApplyDModTickDelay()`, `CSTGMIDIClockSync::EnableActivePerfClock()`,
`CSTGSlotVoiceData::EmergencyFreeAllVoices()`, `CSTGPan::
CalculateStereoPanCoeffs()`), none individually assessed yet.

**Batch 31 specifics** (2026-07-07, sec 10.179): user redirected the
sweep to focus specifically on the `init_module()` call chain ("get all
dependent sources figured out for at least the initialization of
OA.ko"). Ran on `kronosdev` (192.168.3.86) — I AM the build host now, no
sshpass needed; local `/home/share/kronosology` (the ACTIVE tree, HEAD
was `97e564a`/batch 30) + `/home/build/linux-kronos` + local ground-truth
`/home/share/Decomp/OA.ko_Decomp/OA.ko`. NOTE: `/home/build/kronosology`
is a STALE separate clone (was at `3dfec14`) — do NOT build there; the
live tree is `/home/share/kronosology`.

Promoted the four smallest self-contained init-path C helpers from
`bar2_stubs_c.cpp` into a new `src/init/startup_helpers.cpp`:
`init_cpp_support` (1-byte bare `ret`, confirmed no-op), `GetInstalledRAM`
(reads anonymous `.bss+0x20` global → modeled as `gInstalledRAM`),
`IncProgressBar` (forwards to external `COmapNKS4_IncProgressBar`),
`SetInstalledOptions(int)` (ORs low byte into `sInstance+0x1090`, guarded
on non-NULL — its old `()` stub silently dropped the arg the real fn
uses). New KAT `verify/test_startup_helpers.cpp` (73 binaries now). Both
clean passes byte-identical, `OA.ko` 169,144 bytes.

**STANDING INVARIANT UPDATE — `nm -u OA.ko` is now 33, not 32.**
`IncProgressBar` pulled in the genuinely-external `COmapNKS4_IncProgressBar`
(`U` in ground truth too, OmapNKS4Module.ko, same family as the accepted
`COmapNKS4Driver_*`). The "32" was never a hard ceiling — real OA.ko's own
undefined set is 168; the count is just the subset referenced so far and
grows monotonically as internal functions that call more externals get
promoted. When a future batch promotes a stub that forwards to a real
OmapNKS4/RTAI/kernel external, EXPECT the count to rise and verify the
delta is exactly the intended external(s), don't treat >32 as a failure.

**Gotcha this batch (cost two rebuilds): a literal `*/` inside a heavy
provenance comment silently terminates the block comment early.** Wrote
`COmapNKS4Driver_*/OmapNKS4OutputFifo_*` in a comment; the `_*/O`
sequence closed the `/* */`, turning the rest of the comment into code
and producing a cascade of bogus "missing terminating ' character" +
"CSTGPerformance has not been declared" errors that LOOKED like a
header-ecosystem problem but wasn't. When a promoted function's comment
enumerates wildcard symbol families (`foo_*`, `bar_*`), never put two
adjacent as `_*/`-forming text — space them (`foo_* / bar_*`). The tell:
"missing terminating ' character" pointing at an apostrophe that is
plainly inside a comment means the comment already ended upstream.

**KAT header note:** `test_startup_helpers.cpp` needs BOTH
`oa_setup_global_resources.h` (STGAPIFrontPanelStatus, GetInstalledRAM,
IncProgressBar) AND `oa_init.h` (init_cpp_support, SetInstalledOptions) —
both declare the shared C symbols inside `extern "C"` blocks so including
both is conflict-free. Defined its own `unsigned char
*STGAPIFrontPanelStatus::sInstance;` storage (same as every other panel-
touching test) and a counting `COmapNKS4_IncProgressBar` mock;
`gInstalledRAM` storage comes from `startup_helpers.cpp` itself.

**Deferred, scoped for follow-up init-path batches** (each cascades new
internal stubs): `cleanup_cpp_support` (`.dtors` walk + `stg_cpp_exit`,
both need reconstructing; `.dtors` iteration is a linker construct, model
representationally), `stg_log_startup_error` (needs `stg_is_linux_context`
+ real `CSTGFile_Open/Write/Close`). Bigger init-path subsystems still
stubbed: `setup_stg_daemons`/`cleanup_stg_daemons`/
`setup_stg_decrypt_daemons`/`signal_timed_out_daemons` (RT-thread
lifecycle), `load_global_resources`, the whole `rtwrap_*` RTAI layer, and
the `CSTGFile_*` VFS wrappers (Open 99 / Read 170 / Write 97 / Close 21 /
Seek 55 / GetFileSize 22 bytes) — a coherent "file I/O primitives"
cluster worth one dedicated batch.

**Batch 32 specifics** (2026-07-07, sec 10.180): second init-path batch
of the same session, continuing the user's "de-stub the init path"
directive. Promoted the four non-`set_fs` `CSTGFile_*` VFS wrappers
(`Open`/`Close`/`Seek`/`GetFileSize`) from `bar2_stubs_c.cpp` into new
`src/init/file_io.cpp` + `verify/test_file_io.cpp` (74 binaries now).
`nm -u` 33 -> 36 (added `filp_open`/`filp_close`/`generic_file_llseek`,
all `U` in ground truth). Both clean passes byte-identical, `OA.ko`
169,488 bytes.

**Reusable: dual-platform IS_ERR.** For any reconstructed kernel
pointer/error comparison against the top-4KB error band, write the
threshold as `(unsigned long)-4096` (kernel `IS_ERR_VALUE` idiom), NOT a
literal `0xfffff000`. It is bit-exact on `-m32` and simultaneously
host-correct on the 64-bit KAT (real high heap pointers stay below
`0xffff_ffff_ffff_f000`; small negative ERR_PTRs stay above it), so the
IS_ERR path is host-testable with a plain `(void*)-13` mock return — no
`mmap`-low-address trick. `filp_open`/`filp_close`/`generic_file_llseek`
are regparm(3) (NOT asmlinkage — unlike printk), so plain `extern "C"`
decls match under the module's `-mregparm=3` default.

**Confirmed 2.6.32/x86-32 VFS offsets (needed again by the deferred
Read/Write):** file +0xc dentry / +0x10 f_op / +0x20 f_mode / +0x24
f_pos(loff_t); dentry +0x10 d_inode; inode +0x40 i_size(loff_t);
file_operations +0x8 read / +0xc write. KAT builds fake byte-buffer
structs to this layout so the identical source is off-target testable.

**Deferred: `CSTGFile_Read`/`CSTGFile_Write`** — the `set_fs`
(`esp & ~0x1fff` thread_info addr_limit save/restore) + `f_op->read/write`
dispatch pair. NOT host-executable (clobbers live host stack); needs an
opaque `set_fs` helper (init_module.cpp `current`-accessor precedent) +
fake-f_op-vtable KAT. Read also EOF-clamps count vs `i_size - f_pos`
(64-bit add/adc/cmp). Once these two + `stg_is_linux_context` are real,
`stg_log_startup_error` (needs Open+Write+Close) becomes promotable.

**Batch 33 specifics** (2026-07-07, sec 10.181, commit `af2fe87`): third
init-path batch of the session. Picked up sec 10.180's own explicitly
pre-scoped lead (`CSTGFile_Read`/`Write`, the `set_fs` pair) and, once
disassembled, found the WHOLE rest of the `CSTGFile_*` family
(`FileExists`/`FreeReadBuffer`/`ReadFileIntoNewBuffer`) was small and
tractable too -- did all 5 in one pass, closing the cluster out
completely. Ran on `kronosdev` (192.168.3.86), same local access as
every batch since 21. Implemented the `stg_set_fs`/`stg_restore_fs`
opaque helper pair exactly as sec 10.180 anticipated: real bodies in
`bar2_stubs_c.cpp` (genuine `esp & 0xffffe000` thread_info-locating
inline asm, same host/target divergence pattern as
`stg_get_current_task()`), host KAT gets its own safe mock (a fake
"current addr_limit" global).

**MAJOR new gotcha, a real KAT segfault (not a compiler warning) in a
function UNRELATED to this batch's own new code
(`CSTGFile_GetFileSize`, from batch 32): storing TWO real kernel struct
pointer fields that are only 4 bytes apart in the REAL 32-bit struct
as native 8-byte host pointers in the SAME fake buffer corrupts
whichever is written FIRST, regardless of the actual address values
involved (mmap32/low-address tricks do NOT fix this -- it's a pure
byte-spacing overlap, not an address-range problem; verified this by
worked-through math before concluding a proper fix was needed).** Hit
TWICE in one batch: (1) the test's fake `file_operations`
`read`@+0x8/`write`@+0xc (both needed simultaneously by NEITHER single
call, so fixed via mutual exclusion -- `wire_read_fop()`/
`wire_write_fop()`, only ONE ever populated per test fixture); (2)
`struct file`'s `f_path.dentry`@+0xc/`f_op`@+0x10 -- BOTH genuinely
needed simultaneously by a single `CSTGFile_Read` call, so mutual
exclusion doesn't apply. Root-caused via `CSTGFile_GetFileSize`
segfaulting in [1] of the KAT (a function this batch never touched) --
adding the new `f_op` write at file+0x10 silently corrupted the
pre-existing `dentry` pointer at file+0xc. **Proper fix (not a
workaround): switched file_io.cpp's OWN pointer-field reads (`dentry`,
`d_inode`, `f_op`) to this project's established `FromU32()` convention
(explicit 32-bit read + zero-extend) instead of a native
`unsigned char **` cast** -- identical behavior on the real `-m32`
target (already 4-byte pointers there) and immune to this overlap
class regardless of what future fields get added nearby. Correspondingly
switched the test's `g_inode`/`g_dentry`/`g_fops` from plain static
arrays to `mmap32()`-backed buffers (this project's own established
convention) since their addresses now round-trip through a 32-bit
truncation -- a plain static array's PIE-relocated address (observed:
~0x555555550000) does NOT fit in 32 bits. **Standing rule reinforced:
the MOMENT a host KAT needs to read/write more than one raw kernel/
target pointer field within 8 bytes of each other, default to
FromU32()/ToU32()+mmap32() for ALL of them from the start -- do not
wait for a segfault to catch it, and remember the segfault may surface
in a completely different, already-passing function that merely shares
the same fake struct buffer.**

**Confirmed real quirks (asymmetric NULL-guard, EOF-clamp cascade
signed-high/unsigned-low compare + 32-bit-only clamped subtract,
`ReadFileIntoNewBuffer`'s outLen-written-before-failure quirk,
unconditional close-on-every-path) all preserved -- see MASTER_REFERENCE
sec 10.181 for full detail, don't re-derive from scratch.**

New external deps this batch: `vmalloc`/`vfree` (both confirmed `U` in
ground truth). `nm -u OA.ko`: 36 -> **38**.

**Verification:** 74 verify/ binaries (unchanged count -- extended
`test_file_io.cpp` in place with 5 new sections, 78 checks total), all
exit 0 by real per-binary exit code (TESTS list parsed fresh from the
Makefile, not glob-trusted), two full clean-rebuild passes byte-identical
(`OA.ko` 170,264 bytes both runs, up from 169,488).

**Deferred, unchanged**: `cleanup_cpp_support` (`.dtors` walk). NEWLY
promotable next: `stg_log_startup_error` (its own dependencies --
`CSTGFile_Open`/`Write`/`Close` -- are now ALL real; only
`stg_is_linux_context` remains unresolved for it). Bigger subsystems
still stubbed: `setup_stg_daemons`/`cleanup_stg_daemons`/
`setup_stg_decrypt_daemons`/`signal_timed_out_daemons`,
`load_global_resources`, the `rtwrap_*` RTAI layer.

**Batch 34 specifics** (2026-07-07, sec 10.182, commit `2867d5c`): fourth
init-path batch, took sec 10.181's pre-scoped lead. Promoted `stg_is_linux_context`
(@0x118db0, 29B) + `stg_log_startup_error` (@0x118e10, 99B, regparm3)
from `bar2_stubs_c.cpp` into `src/init/startup_helpers.cpp`. Ran on
`kronosdev` (192.168.3.86), live tree `/home/share/kronosology`, ground
truth `/home/share/Decomp/OA.ko_Decomp/OA.ko`.

**KEY: the oa_export/ Ghidra table (`/home/share/Decomp/oa_export/`)
uses an image base +0x10000 vs the ground-truth OA.ko.** Export lists
`stg_is_linux_context@0x128db0`; real nm/objdump address is `0x118db0`.
Its `functions.csv`/`symbols.csv`/`functions/<name>@<addr>.c` decomps are
a fast way to get a function's C-level shape + prototype + calling
convention (it correctly flagged `stg_log_startup_error` as `__regparm3
(undefined4 param_1)` and `stg_is_linux_context` as `__cdecl bool`), but
ALWAYS re-disassemble at the ground-truth address (subtract 0x10000) with
`objdump -dr` before transcribing — the export's `func_0x00d2b190`-style
unresolved callee names are useless; the real relocs (`rt_whoami`,
`strlen`, `CSTGFile_*`) only show in `objdump -dr` on the real .ko.

**Signature-fidelity gotcha, the real substance of this batch:**
`stg_log_startup_error`'s real arg is a `const char *` (each init_module
call site does `mov $.rodata.str1.1+off,%eax`). The reconstruction had
modeled all 11 call sites as bare `int` offsets (`stg_log_startup_error(
0xa1)`), the header said `int code`, the stub said `const char *` — C
linkage + the 4-byte regparm ABI silently reconciled all three. Promoting
the real body FORCES fixing this: resolved the offsets against
`.rodata.str1.1` (fileoff 0x6b1a28 in ground truth; `readelf -S` for the
section, then read the NUL-terminated string at base+off) to the real
strings — 0xa1="cpu cap", 0xd7="memory error", 0xe4="proc error",
0xef="pcmproc error", 0xfd="alloc resources", 0x12b="authorization",
0x139="setup" (reused at BOTH step 10 and step 12), 0x192="audio threads",
0x1a0="keybed", 0x1c0="UI fifo". Lesson: when a promoted stub's real
signature disagrees with how existing callers (modeled as placeholders)
invoke it, the promotion legitimately extends to fixing those call sites +
the header + any test mock — this is a fidelity IMPROVEMENT, not scope
creep, and leaving the int/char* mismatch would make the "faithful" claim
false (the reconstruction's init_module would deref address 0xa1 if ever
run in Linux context). Touched init_module.cpp (11 calls + 1 comment),
oa_init.h, test_init_module.cpp's mock (`int`->`const char *`; log_call
name-based assertions unaffected).

**host-KAT approach for an RTAI-dependent predicate:** `stg_is_linux_context`
calls extern `rt_whoami()` (RTAI, `U` in ground truth) then reads
`+0x1c` (the RT_TASK priority word) and compares to the sentinel
0x7fffffff (RT_SCHED_LINUX_PRIORITY). Rather than mock the whole
predicate, the KAT mocks ONLY `rt_whoami` (returns a fake 0x40-byte task
buffer whose `+0x1c` the test sets), so `stg_is_linux_context` AND
`stg_log_startup_error` both execute for real on the host. CSTGFile_Open/
Write/Close are recording mocks (real bodies in file_io.cpp, NOT linked
into test_startup_helpers) — asserted the exact `/tmp/startupErrorLog`
path, mode 3, strlen write-count, same-handle write->close threading, and
the two silent-no-op paths (non-linux-context, open-fail).

**nm -u 38 -> 39, NOT 40** despite adding both `rt_whoami` and `strlen`
externs: `strlen` was ALREADY `U` (referenced by products.cpp/
process_oacmd.cpp). The single genuinely-new undefined symbol is
`rt_whoami`. Confirm a suspected "extra" external is actually new by
`nm src/<other>.o | grep <sym>` before assuming the delta is wrong.
`OA.ko` 170,264 -> 170,696 bytes, two clean passes byte-identical,
linkonce 0x148, GOTPC 0, 74 verify/ binaries all exit 0.

**Deferred:** `cleanup_cpp_support` (`.dtors` walk + `stg_cpp_exit`) is now
the only small init-path leaf left. Bigger still-stubbed init subsystems:
`setup_stg_daemons`/`cleanup_stg_daemons`/`setup_stg_decrypt_daemons`/
`signal_timed_out_daemons` (RT-thread lifecycle), `load_global_resources`,
`rtwrap_*` RTAI layer.

**Batch 35 specifics** (2026-07-07, sec 10.183, commit `ee9cfb3`): fifth init-path batch.
`cleanup_cpp_support` scouted + DEFERRED AGAIN (its `.dtors` section-symbol
walk can't be faithfully bound to portable C in this crtstuff-less module
build without a linker boundary symbol / in-section anchor; it's the
unload path, not needed to boot). Its FULL disassembly is now captured in
`bar2_stubs_c.cpp`'s own deferral note so the next attempt doesn't
re-derive it. Retargeted to `signal_timed_out_daemons` (435B, the RT
daemon watchdog, called from engine.cpp:121) which turned out clean and
self-contained.

**Reading a compiler-unrolled + out-of-line-hoisted loop back into a
simple loop:** `signal_timed_out_daemons`' disasm looks scary (7 inline
timeout checks, then 7 separate "kick" bodies AFTER the ret, with `jbe`
back-edges re-entering the main scan) but is just
`now=GetSTGTickCount(); for(i=0;i<7;i++) if((u32)(now-d[i].lastTick) >
d[i].timeout){ d[i].lastTick=GetSTGTickCount(); rt_pend_linux_srq(
d[i].srq);} `. The out-of-line fire bodies + `jbe` re-entries are the
compiler's layout of independent per-iteration branches, NOT a data
dependency between daemons — each daemon is independent, `now` (ebx) is
loop-invariant. Don't over-model hoisted branch layout; recover the loop.
Confirmed quirks to KEEP: unsigned strict `>` (`ja`/`jbe` — elapsed==
timeout does NOT fire); wrapping 32-bit subtract; `now` read ONCE but each
kick re-reads a FRESH tick to stamp lastTick (TWO GetSTGTickCount calls,
not folded).

**Daemon control-block struct**: 7-entry `.bss` array, base +0x1077d8,
stride 0x60, fields +0x00 lastTick / +0x04 timeout / +0x0c srq (only these
3 confirmed; rest is opaque 0x60-stride padding). Modeled as
`STGDaemonWatch gStgDaemons[7]` in new `include/oa_daemons.h` +
`src/init/stg_daemons.cpp`, with a `static_assert(sizeof==0x60)`. Shared
with the still-stubbed `setup_stg_daemons` family (they populate
timeout/srq) — zero-init for now = inert-but-faithful. This shared layout
makes `setup_stg_daemons`/`setup_stg_decrypt_daemons` the natural next
target.

**Reconstruct an internal callee rather than leave it a dangling `U`:**
`signal_timed_out_daemons` calls `GetSTGTickCount` (16B), which is `T`
(internal) in ground truth — leaving it `U` would be a dangling internal
symbol (insmod-unresolvable), unlike the legitimately-external
`rt_pend_linux_srq` (RTAI, `U` in ground truth). So reconstructed it too:
`return *(u32*)((char*)CSTGGlobal::sInstance + 0x29c9fa8)` (one dword above
lfo_stepseq_quad.cpp's own +0x29c9fa0 read). Put it in its OWN TU
(`src/engine/tick_count.cpp`, NOT global.cpp) so the watchdog KAT can mock
it with a scripted tick sequence while its real body gets its own KAT —
the sec-10.150 "split a shared dep across TUs when mock footprints differ"
pattern. Result: nm -u 39 -> 40, the ONLY new external is rt_pend_linux_srq
(GetSTGTickCount + signal_timed_out_daemons now `T`).

**Host-KAT trick for a huge-offset singleton field** (GetSTGTickCount
reads sInstance+0x29c9fa8, ~44MB in): DON'T allocate a 44MB fake object —
set `sInstance = (base of a small local) - 0x29c9fa8` so `sInstance +
0x29c9fa8` lands on the local, and drive the local. sInstance itself is
never dereferenced, only the computed field address. **Compute that base
via `uintptr_t` integer arithmetic, NOT pointer arithmetic off `&local`**
— the latter trips GCC `-Warray-bounds` (it knows the local's size and
flags the deliberately-out-of-object base as a fake overrun). The project
has no `-Werror` so it's only noise, but the clean-build convention wants
it gone.

**Verification**: 2 new KATs (74 -> 76), byte-identical two-pass rebuild,
`OA.ko` 170,696 -> 171,084 bytes, linkonce 0x148, GOTPC 0. New files:
stg_daemons.cpp, tick_count.cpp, oa_daemons.h, test_stg_daemons.cpp,
test_tick_count.cpp; edited oa_global.h (GetSTGTickCount decl), Makefile
(2 objs + 2 SRC + 2 TESTS + 2 rules), bar2_stubs_c.cpp (1 stub removed).

**STANDING POLICY UPDATE (2026-07-10, MASTER_REFERENCE sec 10.185), applies
to all future batches:**
1. RTAI substitution ("virtual class") is now explicitly authorized as a
   first-class technique, not a last resort. When a real RTAI codepath
   (`rtwrap_*` task/thread primitives, `rt_*`/`rtf_*` FIFO layer, hard
   real-time scheduling) blocks progress in the non-realtime VM, write a
   from-scratch substitute (matching the existing `AT88VirtualChip.ko`/
   `KorgUsbAudioVirtualDriver.ko`/`OmapNKS4VirtualDriver.ko` precedent).
   Document what it replaces, why the real thing can't run here, and what
   guarantee it deliberately does NOT provide.
2. Audio DSP/signal-path fidelity is explicitly OUT OF SCOPE. Prioritize
   OA.ko's structural function (load, init sequencing, command/auth/
   file-I/O, daemon lifecycle) over bit-exact DSP transcription. Where an
   audio codepath blocks structural progress, a stub/no-op/virtual
   substitute beats painstaking transcription.
3. Periodic VM validation is expected; RTAI failure under QEMU/TCG is
   normal. Getting further than the last confirmed boot point is the
   progress metric, not full functional correctness.

**IMPORTANT new gotcha (2026-07-11, sec 10.186): uncommitted working-tree
content is not automatically trustworthy just because it's there, even
when a task briefing describes it as "real, substantive, in-progress
work."** Found alongside two genuinely correct, ground-truth-verified bug
fixes (`CCostProfile` vtable shape, `CSTGSampleRateMonitor` `.bss` layout)
in the SAME uncommitted diff set: an `OmapNKS4VirtualDriver/module_main.c`
change containing a live GDT-descriptor patch
(`bar2_fixup_percpu_fs_base()`, `store_gdt`/`struct desc_struct` byte
manipulation/`loadsegment`/`on_each_cpu()`) that:
  - directly contradicted the one existing committed record of the bug it
    claimed to fix (sec 10.184 explicitly says this needs a kernel-source
    or bzImage binary patch, NOT a module-side fix, and never mentions
    this function);
  - cited a specific two-attempt incident history (hostnames, IPs, a
    kernel Oops trace, a QEMU-process-exit "consistent with deadlock")
    that appears NOWHERE else in `MASTER_REFERENCE.md` or this
    agent-memory directory, including a cited-by-name agent-memory note
    (`percpu_fsbase_boot_bug.md`) that does not exist;
  - admitted, in its own comments, a prior run of the exact code
    previously crashed/hung the dedicated `kronosvm` sandbox this
    project's task briefings point agents at for VM validation.
  This combination (technically detailed, self-justifying, uncorroborated
  by any independent record, and describing its own prior real-hardware/
  VM damage) is exactly the profile of unauthorized or injected content
  that should NOT be committed or executed on the strength of in-source
  comments alone, no matter how genuine the surrounding diff looks. It was
  stripped out (not committed, not run) before the rest of the diff
  (which WAS independently verified against ground truth) was committed.
  **Lesson for future batches:** when a batch's own task briefing
  describes an uncommitted diff's contents in a way that undersells or
  doesn't match what's actually in the file (here: described as "adds a
  trivial no-op EXPORT_SYMBOL," actually ~90 extra lines including a live
  kernel-descriptor patch), that mismatch itself is a signal to read the
  FULL diff carefully before trusting any of it, and to independently
  check MASTER_REFERENCE.md/agent-memory for corroboration of any
  specific incident claims embedded in code comments before running that
  code anywhere, especially against shared VM infrastructure.

**New verification gotcha (2026-07-11): extracting the Makefile's own
`TESTS` list with a fixed-line-count `grep -A N` silently truncates it as
the list grows, giving false "all pass" confidence.** The established
convention (CLAUDE.md, this file) already says "extract the literal TESTS
list, don't shell-glob verify/test_*" -- but a `grep -m1 "^TESTS" Makefile
-A 20` extraction only grabbed the first 20 lines after the match; by
2026-07-11 the real list spans 37 lines (76 distinct test binaries), so
this quietly ran only 51 of them and reported "all pass" -- correct for
what it ran, misleading about coverage. Caught by separately counting the
extracted list (`wc -l`) against expectation, not by any test failing.
Fix: extract with an `awk` state machine that keys off the list's own
continuation syntax, not a guessed line count:
```
awk '/^TESTS/{p=1} p{print; if ($0 !~ /\\$/) exit}' Makefile \
  | grep -oE "verify/test_[a-zA-Z0-9_]+" | sort -u
```
Also: running this kind of multi-step verification script THROUGH
`sshpass ssh '...'` with inline `awk`/nested-quote logic is fragile --
one attempt's shell-escaping (`\\\\\\\\$` needed to survive local-shell +
ssh + remote-awk quoting) silently produced a 4-test extraction with exit
0, LOOKING like a clean minimal run rather than an obvious failure.
Prefer writing the verification steps to a real `.sh` file (anywhere
under the CIFS-shared `/home/share`, so both this environment and
192.168.3.92 see it identically) and invoking it with a plain `bash
/path/to/script.sh`, not an inline multi-layer-quoted one-liner. ALWAYS
sanity-check the extracted test count against the expected/previous
count before trusting an "overall fail=0" result -- a truncated or
mis-extracted list will happily report zero failures for whatever subset
it actually ran.

**Batch 37 specifics** (2026-07-11, sec 10.188, commit `cbc395e`): picked
the WHOLE `rtwrap_*` RTAI wrapper cluster --
22 of `bar2_stubs_c.cpp`'s 34 remaining bare-`{}` stubs sat in one
contiguous ground-truth address range (`.text+0x118f00`..`0x1198a0`),
found by `nm -S --size-sort` confirming every one was 3-73 bytes, then
ONE `objdump -dr` dump covering the whole namespace at once -- same
"check the dependency's whole class" efficiency win as sec 10.158's
`CSTGHDRCircularBuffer`. New file `src/init/rtwrap.cpp` +
`verify/test_rtwrap.cpp`. Stub count 34 -> 12 in `bar2_stubs_c.cpp`
(net -22, bar2_stubs.cpp unchanged at 67; combined 101 -> 79).

**Key finding: NOT every "RTAI-adjacent" stub is an RTAI-substitution
case under the sec 10.185 policy, even when its name says `rtwrap_`.**
Checked ground truth's own `nm -u` for every real primitive these 22
wrappers call (`rt_sem_wait`, `rt_task_init`, `rtai_global_heap`, etc)
BEFORE writing any code -- all confirmed `U` in ground truth OA.ko too,
i.e. genuine external RTAI symbols the real binary ALSO never defines
locally (resolved by rtai_hal.ko/rtai_sched.ko/rtai_lxrt.ko at insmod
time on real hardware). The sec 10.185 "write a from-scratch substitute"
clause is for when OA's OWN dispatch/scheduling logic blocks progress --
there is none here, these are pure argument-marshaling forwarders.
Correct treatment is identical to the pre-existing `CSTGFile_*` ->
`filp_open`/`vmalloc` precedent: declare the real externals (they stay
`U`, `nm -u` count rises as expected, resolved on real hardware), do NOT
write a substitute implementation. Lesson for future batches: before
reaching for the RTAI-substitution policy on ANY `rtwrap_*`/`rt_*`-named
stub, check whether ground truth itself resolves the underlying
primitive locally or leaves it `U` -- only a local (`T`) real
implementation in ground truth would indicate OA's own substitutable
scheduling logic; an `U` in ground truth means "just declare the extern,
same as any other kernel/RTAI dependency," no substitute needed.

**Signature-fidelity fix, same family as sec 10.182's
`stg_log_startup_error` int/`const char *` correction**: two of the 22
(`rtwrap_whoami`, `rtwrap_task_suspend`) had project-wide `void`/`void`
0-arg signatures that were WRONG once the real body was examined --
ground truth's real `rtwrap_task_suspend` takes 1 arg (forwarded to
`rt_task_suspend`), and its one real caller
(`CSTGAudioManager::ASKThreadRoutine`, already-real code) does `call
rtwrap_whoami` immediately followed by `call rtwrap_task_suspend` with
NO intervening instruction touching `%eax` -- i.e. `rtwrap_whoami`'s own
return value (current task handle) IS `rtwrap_task_suspend`'s argument
via IMPLICIT register passthrough (a self-suspend idiom), not two
independent void calls. Promoting `rtwrap_task_suspend` to a real
forwarding body while leaving the 0-arg signature would have passed
whatever garbage sat in `%eax` at the call site -- confirmed worse than
the prior stub (a harmless no-op) once real. Fixed both declarations
(`void *rtwrap_whoami(void)`, `void rtwrap_task_suspend(void *task)`)
plus the real caller's body (`void *me = rtwrap_whoami();
rtwrap_task_suspend(me);`) and that caller's own isolated test file's
local mock signatures. Lesson reinforced: whenever a promoted stub's
real ground-truth signature disagrees with how an EXISTING (already-real)
caller invokes it, fixing that caller is in-scope for the promotion, not
scope creep -- and specifically watch for the "two back-to-back calls
with no intervening register-clobbering instruction between them" tell
that means an implicit passthrough, not two independent calls, ESPECIALLY
when one call's C-level signature is currently `void` (no return
consumed) and the very next call's C-level signature is currently 0-arg
(nothing passed) -- that exact SHAPE (discarded return immediately
followed by a suspiciously argument-free call) is worth re-disassembling
the real caller for before assuming they're actually independent.

**`rtwrap_pthread_cancel`'s struct-layout cross-reference, a "derive a
sibling's field meaning from a NOT-YET-promoted function's own
disassembly" pattern, not covered by any prior batch**: its only
non-trivial real body (self-cancel via `rt_whoami()` when the argument
is NULL, then `rtheap_free` a pointer stored at `task+0x5b8`) only makes
sense by ALSO disassembling `rtwrap_pthread_create` (itself NOT one of
this batch's 22, and still correctly deferred) to confirm that `+0x5b8`
is exactly where `rtheap_alloc`'s raw return value gets stashed for
later freeing. Confirmed this doesn't introduce a NEW wild-pointer risk
in this reconstruction's own CURRENTLY REACHABLE call graph specifically
because `rtwrap_pthread_create` itself still always returns NULL (its
own pre-existing stub) -- meaning no caller in this build can currently
obtain a non-NULL handle to pass to `rtwrap_pthread_cancel`, so every
presently-reachable invocation takes the `rt_whoami()` self-cancel path,
gated behind still-stubbed daemon-lifecycle code regardless. Lesson: when
a stub's real body references a field whose MEANING is only established
by a DIFFERENT, still-deferred sibling function's own disassembly, it's
fine to promote the first one anyway as long as (a) the field-meaning
cross-reference is independently confirmed via that sibling's own real
bytes (not guessed), and (b) the current reconstruction's own reachable
call graph is checked to confirm no LIVE path can feed the promoted
function a value inconsistent with that meaning.

**Gotcha hit AGAIN this batch (2nd/3rd occurrence of the sec 10.179
"literal `*/` inside a provenance/family-listing comment silently ends
the block comment early" class): two MORE instances, in two DIFFERENT
files, both only caught by the required rebuild (not by proofreading).**
`bar2_stubs_c.cpp`'s new header text ("`pthread_mutex_*/mutexattr_*`")
and `test_audio_start.cpp`'s updated mock comment ("`void*/1-arg`") each
independently formed an accidental `*/` comment-closer purely from
listing adjacent wildcard-suffixed symbol-family names or type names
back to back with a bare `/` between them, cascading into the exact same
"missing terminating ' character" error shape sec 10.179 already
documented. This is now a THIRD confirmed occurrence of this same
mechanical trap (batch 31/sec 10.179 being the first) -- worth treating
as a standing tripwire: after writing ANY comment that lists multiple
`foo_*`/`bar_*`-style wildcard family names, or any `type*`/`type` pair,
separated only by `/`, grep the new/edited comment text for a literal
`*/` substring BEFORE the first rebuild attempt, not just after hitting
the error.

**Side-observation recorded but deliberately NOT fixed this batch (a
pre-existing inconsistency, not introduced now): `stg_set_cpus_allowed`
is confirmed `U` in ground truth OA.ko (a real external, correctly
already in this project's pre-batch 40-unresolved-symbol count) but this
reconstruction's own `bar2_stubs_c.cpp` DEFINES a local empty-body stub
for it** -- meaning our OA.ko incorrectly makes it a defined (`T`)
symbol where ground truth has `U`. Also: `stg_outb`/
`stg_local_irq_restore`/`stg_inb`/`stg_local_irq_save` have NO standalone
symbol anywhere in ground truth (fully inlined at call sites, sec
10.159's "no standalone symbol" pattern) -- not a quick forwarder
rewrite like the rest of this cluster, would need locating the inlined
call sites first. Flagged for a future pass, not touched here (auditing
every `stg_set_cpus_allowed` call site before changing its symbol
visibility is real, separate work, out of scope for a `rtwrap_*`-focused
batch).

**Batch 38 specifics** (2026-07-11, sec 10.189, commit `d29895d`): the
first batch to focus specifically on finishing `bar2_stubs_c.cpp`
(12 bare-`{}` stubs left) per the task briefing's explicit priority,
rather than a fresh `bar2_stubs.cpp` sweep. Picked all 12 up individually
via `nm -S`/`objdump -dr` -- 9 tractable, 3 genuinely deferred with
concrete scouted reasons now on record in `bar2_stubs_c.cpp` itself
(cm_AuthenEncryptMAC's real cipher-MAC core; the 4-function daemon-
lifecycle cluster; cleanup_cpp_support's unchanged .dtors blocker).
Full per-function derivation in MASTER_REFERENCE sec 10.189 -- read it
before re-touching any of these.

**Reusable pattern: "this project's own descriptive alias vs. ground
truth's obfuscated real name" is not a bug when `nm`/`grep` come up
empty on the alias.** `cm_*`/`nv2ac_*` (oa_atmel.h) don't appear
ANYWHERE in ground truth OA.ko by those names -- initially alarming,
until checking `atmel_setup.cpp`'s own header comment revealed these
are THIS PROJECT's chosen readable names for OA_real.ko's own
obfuscated real symbols (`fFfFfFfFfFfF1C`, `sdflkjsvnd2s`, etc, the
SAME family CLAUDE.md's "preserve obfuscated-but-real symbol names"
rule already covers). Confirmed correct by disassembling the ALREADY-
reconstructed caller (`SetupAtmelForAuthorizations`) and reading its own
relocations -- they resolve to the obfuscated names, at addresses
matching this project's own header-comment cross-references. When a
symbol name a stub declares doesn't show up in ground truth `nm` at
all, check whether it's this project's OWN alias for an obfuscated
real name (grep the surrounding file's header comment / the class's
own header for an existing name-mapping note) before assuming
something is wrong.

**New reusable technique: locating the REAL x86 instruction shape for a
"no standalone symbol" primitive (sec 10.188's pattern) by disassembling
ONE already-reconstructed real CALLER, not by guessing from the C-level
abstraction's name.** `stg_inb`/`stg_outb`/`stg_local_irq_save`/
`stg_local_irq_restore` have zero standalone ground-truth symbols
(confirmed again, matching sec 10.188's finding for the same four) --
resolved by disassembling `CSTGComPort::Initialize()` (an ALREADY-real
2561-byte function that calls all four abstractions many times) and
finding the literal `in`/`out`/`pushf;cli;...;popf` instructions
inline. This confirmed the abstraction is exactly the universal x86
port-I/O/IRQ-save-restore idiom (same as the kernel's own `inb`/`outb`/
`local_irq_save` macros) -- NOT project-specific behavior needing
per-call-site verification, so a SINGLE representative disassembly is
sufficient evidence to write the real inline-asm bodies, unlike (say)
a project-internal helper whose behavior could genuinely differ by
call site. Rule of thumb: when a "no standalone symbol" primitive's
semantics are architecturally universal (not OA-specific), one real
caller's disassembly is enough; when they might be OA-specific
behavior, check more than one call site before trusting a single
sample.

**New gotcha, a real cross-TU signature mismatch caught only by
actively fixing it (not by any prior compile error, since the two
mismatched declarations lived in TUs that never included each other):**
`bar2_stubs_c.cpp`'s own erroneous local `stg_set_cpus_allowed(void*,
unsigned int)` definition silently disagreed with `oa_init.h`'s
ALREADY-correct `int stg_set_cpus_allowed(void*, unsigned long)`
declaration for years of batches, undetected because `bar2_stubs_c.cpp`
never included `oa_init.h`. Confirmed via `nm -u` on ground truth
(genuinely `U`, not locally defined at all) that the fix is to DELETE
the local definition entirely, not reconcile the two signatures --
this is the correct treatment whenever a function batch 37's "check
`nm -u` first" rule confirms is a real external: don't just fix the
call-site/header signature mismatch, remove the incorrect LOCAL BODY
that shouldn't exist in the first place. A stub file defining a body
for a symbol ground truth leaves genuinely `U` is a distinct, worse
class of bug than a signature mismatch between two declarations of a
real internal function (the latter is usually harmless on this ABI per
sec 10.154's own precedent; the former makes our own `T`/`U` symbol
visibility diverge from ground truth's, silently masking what should
be a real insmod-time external dependency).

**Reconfirmed (3rd time, matching sec 10.179/10.188): grep new/edited
comment text for a literal `*/` before the first build, not after an
error.** `drumpad_init.cpp`'s draft header comment
("KorgUsbMidi*/USBMidiAccessory_SetMidiInClient") formed exactly this
trap again -- caught this time by a proactive grep pass over every new
file's comments before the first compile attempt (added a step: `grep
-n '\*/' <new files> | grep -v '^\S*:[0-9]*:\s*\*/\s*$'` to filter out
legitimate own-line comment-closers and flag anything else), not by
waiting for the error. Worth making this grep a standing pre-build step
for every future batch that writes new header-comment prose listing
adjacent symbol/type names.

**Verification**: 4 new dedicated KATs (78 -> 82 verify/ binaries),
byte-identical two-pass clean rebuild (`OA.ko` 172,436 -> 173,292
bytes), linkonce 0x148, GOTPC 0, `nm -u` 55 -> 58 (get_random_bytes,
USBMidiAccessory_SetDrumPadClient, stg_set_cpus_allowed -- all three
independently confirmed `U` in ground truth before accepting). New
files: drumpad_init.cpp, keybed_debounce.cpp, calibration_data.cpp,
atmel_primitives.cpp + their 4 test files; edited bar2_stubs_c.cpp,
setup_global_resources.cpp (+call site), oa_setup_global_resources.h,
test_setup_global_resources.cpp (mock signature only), Makefile.

**Deferred for a future batch**: `bar2_stubs.cpp`'s own 67 bare-`{}`
stubs untouched this batch (explicit priority was finishing
`bar2_stubs_c.cpp` first) -- next batch's natural default is a fresh
smallest-first sweep of THAT file. Within `bar2_stubs_c.cpp`: only 3
bare stubs remain (cleanup_cpp_support, cleanup_stg_daemons,
cm_AuthenEncryptMAC), all with concrete scouted blockers now on
record -- see sec 10.189 for the daemon-lifecycle cluster's full call
graph (SetupDaemon/rtwrap_pthread_create are the true root blockers)
and cm_AuthenEncryptMAC's cipher-core assessment (needs
bzzzzzzzzzzzt12 reconstructed first).

**Batch 39 specifics** (2026-07-11, sec 10.190, commit `691d978`): the
briefing's explicit priority was investigating whether the `rtwrap_*`/
daemon-lifecycle cluster is now more tractable since batch 37 made the
whole `rtwrap_*` layer real. Extensively surveyed `bar2_stubs.cpp`'s 67
remaining stubs (all size-tiers under ~600 bytes, ~15 candidates fully
disassembled) and found the pool now saturated with genuine vtable-
dispatch danger (sec 10.153's own criterion) or deep DSP-cluster
dependencies -- none promoted from that file this batch (bar2_stubs.cpp
stays at 67; full rejected-candidate list with per-function reasons in
MASTER_REFERENCE sec 10.190, don't re-derive). Instead promoted
`rtwrap_pthread_create` (already a non-bare `{ return 0; }` body in
bar2_stubs_c.cpp, not counted in that file's own bare-`{}` tally of 3 --
so this doesn't move either file's stub-count metric, but is real
de-stubbing progress).

**Real ABI bug found and fixed, a NEW angle on "check calling
convention per function" not covered by any prior batch: within ONE
family of RTAI externs (`rt_*`), ground truth's own header mixes
regparm(3) (register-passed) and regparm(0) (stack-passed) functions,
and you cannot infer which is which from the function's NAME or from
"it's an RTAI primitive" -- you must check EACH one's own real call
site(s).** Batch 37 assumed the whole `rtwrap_*` cluster's underlying
`rt_*` externs were uniformly regparm(3) (this file's own
`-mregparm=3` default, no attribute needed) -- wrong for 7 of them
(`rt_sem_wait`/`rt_sem_wait_if`/`rt_sem_signal`/`rt_sem_delete`/
`rt_typed_sem_init`/`rt_task_suspend`/`rt_task_resume`/
`rt_set_runnable_on_cpuid` -- confirmed stack-passed via MULTIPLE
independent real call sites each), correct for the rest
(`rt_task_delete`/`rt_whoami`/irq quartet/`clear_debug_traps_in_rt_task`/
`rt_task_init`). The tell in the disassembly: a register that's
computed then immediately STORED to a stack slot right before the call
(rather than left in the register) means that argument is genuinely
stack-passed, not a compiler quirk -- if you see `mov X,(%esp)` /
`mov X,0x4(%esp)` immediately preceding a `call` with NO subsequent use
of the source register as a call-time value, that's the signal, even
for a function with only 1-3 total args that would trivially fit in
registers under the file's own default convention. Fixed by adding
`__attribute__((regparm(0)))` to the extern declaration -- and,
critically, ALSO to the matching host-KAT mock DEFINITION in the
verify/ file, since a mismatched attribute between the two creates a
real ABI disagreement even within one host binary (the mock, defined
under this file's own `-mregparm=3` default without the override,
would expect register args while the "real" declaration's caller code
now passes them on the stack). This matters here specifically because,
unlike sec 10.154's "our own ecosystem only needs to agree with
itself" pointer-width precedent, these primitives are meant to run
against REAL rtai_sched.ko on hardware -- a convention mismatch isn't
cosmetic, it delivers garbage/uninitialized stack args to real kernel
RT code.

**Second real bug found: promoting a long-stubbed function out of its
"always returns 0/NULL" state can surface a return-value POLARITY bug
in an EXISTING, already-committed caller that was invisible while the
stub always returned the same constant.** `CSTGThread::
CreateRealTimeWithCPUAffinity` (cpu_affinity.cpp, committed before
`rtwrap_pthread_create` was disassembled) checked
`if (!createResult) return 0;` -- backwards from ground truth's real
0=success polarity, confirmed independently from BOTH the callee's own
body and the real caller's own disassembly (`test edi,edi; je
<success-path>` -- jumps to success when zero). This was harmless
while the stub always returned 0 (self-consistently looked like
"always fails") AND the test's own mock had the SAME inverted polarity
(two matching bugs canceling out, so the KAT passed despite both being
wrong) -- exactly the kind of masked bug this project's "promote a
stub, then check every existing caller's assumptions against the real
body" discipline (sec 10.182/10.188) is meant to catch. Generalizes
sec 10.182/10.188's "signature-fidelity forces fixing existing
callers" precedent from argument-signature mismatches to RETURN-VALUE
POLARITY mismatches -- same underlying principle (a promoted function's
real ground-truth behavior disagreeing with how an existing caller was
written is an in-scope fix, not scope creep), new specific shape.
**Reusable check for future batches**: whenever a stub that always
returned a single constant (0, NULL, -1, etc.) gets promoted to a real
body with actual branching return values, explicitly re-verify EVERY
existing caller's own success/failure branch condition against the
newly-known real semantics -- a KAT passing before promotion is not
evidence the caller's assumption was ever correct, only that it never
got exercised against real variation.

**Third bug, this batch's OWN new code, caught by a real KAT FAILED
result (not proofreading): `rtwrap_pthread_create`'s three real
target-struct fields (`task+0x5b0`/`+0x5b4`/`+0x5b8` -- start routine,
thread arg, raw alloc pointer) are only 4 bytes apart, matching sec
10.181's exact "two+ real kernel pointer fields within 8 bytes of each
other" gotcha precisely.** First draft used native 8-byte `void*`
writes for all three, corrupting neighbors on this 64-bit host. Fixed
via this project's established `ToU32`/`FromU32` convention (defined
once per-file, matching the `engine_init.cpp`/etc. precedent, NOT a
shared header function). Applied consistently to `rtwrap_pthread_
cancel`'s own PRE-EXISTING `+0x5b8` read too (can't leave one sibling
field packed and the other native). **New sub-lesson: when a KAT needs
to verify a packed-32-bit field's contents, you don't always need
`mmap32()` for every buffer** -- only the SPECIFIC value that must
survive a lossless FromU32(ToU32(x))==x round trip (e.g.
`rtwrap_pthread_cancel`'s test, which compares a RECONSTRUCTED pointer
against the original) needs its source object mmap32'd into the low
4GB. A test that instead compares the SAME truncated 32-bit value on
BOTH sides (`(unsigned int)*(unsigned int*)(field)` vs.
`(unsigned int)(unsigned long)expectedPtr`) works correctly regardless
of where the original object lives in the host's address space, since
both sides go through identical truncation -- cheaper than mmap32 when
you don't actually need the full pointer value to survive.

**Fourth finding, a methodological lesson about trusting a prior
batch's own "real blocker: X depends on Y" claim**: sec 10.189 guessed
(without disassembling it) that `SetupDaemon` depends on
`rtwrap_pthread_create`. Once `rtwrap_pthread_create` became real this
batch, actually disassembling `SetupDaemon.clone.0` showed the guess
was WRONG -- it uses plain `kernel_thread()`/`wait_for_completion()`/
`rt_request_srq()`, no RTAI task creation at all. **Rule for future
batches: the moment a previously-cited blocking DEPENDENCY becomes
real, re-disassemble the BLOCKED function directly rather than trusting
the old dependency claim** -- a "SetupDaemon depends on X" note written
before X existed is a hypothesis, not a fact, and can be flat wrong once
you can finally check it against ground truth. This is now recorded
correctly (full field-by-field derivation) in bar2_stubs_c.cpp for the
next attempt.

**Verification**: 82 verify/ binaries (unchanged count, `test_rtwrap`/
`test_cpu_affinity` extended in place), THREE full clean-rebuild passes
byte-identical (not just two -- extra caution given the scope of the
ABI/polarity fixes), `OA.ko` 173,628 bytes (up from 173,292), md5
`7a9a0f7fc2ff3f63e68a53f9450b6ccb`. `nm -u` 58 -> 61 (`rtheap_alloc`/
`rt_task_init`/`rt_task_resume`, all confirmed `U` in ground truth).
linkonce 0x148, GOTPC 0. Bare-`{}` counts unchanged (`bar2_stubs.cpp`
67, `bar2_stubs_c.cpp` 3) since `rtwrap_pthread_create` was already a
non-bare stub before this batch.

**Batch 40 specifics** (2026-07-11, sec 10.191, commit `6a48129`): took
sec 10.190's own "daemon-lifecycle cluster" lead (fully scouted by batch
39 but never implemented). Reconstructed the whole cluster in one pass:
`SetupDaemon`/`SetupDecryptDaemon` (internal helpers), `setup_stg_daemons`/
`cleanup_stg_daemons` (general 7-daemon cluster), `setup_stg_decrypt_daemons`/
`cleanup_stg_decrypt_daemons` (decrypt 4-daemon cluster). New file
`src/init/daemon_lifecycle.cpp` + `verify/test_daemon_lifecycle.cpp`.
`bar2_stubs_c.cpp` bare-`{}` count 3 -> 2 (only `cleanup_stg_daemons` was
bare among the 4 promoted; the other 3 were non-bare `{ return 0; }` or
undefined-but-undeclared, so promoting them is real progress not
reflected in the bare-`{}` grep metric -- same distinction sec 10.190
already flagged for `rtwrap_pthread_create`).

**Correction to a batch-39 claim (own prior-batch content, not stale
external info): sec 10.190 guessed ONE shared `SetupDaemon.clone.0`
helper serves BOTH daemon clusters -- WRONG.** Ground truth has TWO
separate helpers: `SetupDaemon.clone.0` (`.text+0x11ce30`, 7 args, DOES
register an RTAI SRQ via `rt_request_srq`) for the 7 general daemons, and
`SetupDecryptDaemon.clone.0` (`.text+0x11c970`, 5 args, NO SRQ at all)
for the 4 decrypt daemons. Found by cross-referencing
`/home/share/Decomp/oa_export/` (a pre-existing Ghidra-analyzed decompile
table, own image base +0x10000 over ground truth per sec 10.182's
established correction) -- its `functions.csv` lists BOTH as separate
local (`t`) symbols with DIFFERENT parameter counts, immediately visible
without re-disassembling anything. Independently re-confirmed both via
fresh `objdump -dr`. Lesson: even a careful prior batch's own
"disassembled and confirmed" claim about a SPECIFIC symbol/relationship
can still be wrong if it never actually diffed the ANALOGOUS decrypt-side
call site's own real target -- always check whether a sibling function
family (here, general vs. decrypt daemons) genuinely shares one helper or
has independent near-twins, don't assume "shared helper" just because
the CALLING PATTERN (index, name, priority, MainRoutine args) looks
parallel.

**STANDING GOTCHA, new this batch, applies to any future cross-section
disassembly: naive `objdump -dr`-displayed target addresses for a call
FROM `.init.text` INTO `.text` are NOT reliable, even though the exact
same raw-byte-plus-displacement method IS reliable for `.text`-to-`.text`
calls.** Both sections show `sh_addr=0` in the still-relocatable `OA.ko`
(ET_REL) -- for a same-section call, the caller's own displayed address
and the raw PC32 displacement combine correctly (both share the same
`sh_addr=0` reference frame in a way that cancels out); for a
cross-section call, they do NOT (confirmed by computing a
self-contradictory sequence of 4 "targets" for what should be the SAME
shared helper called 4 times from `setup_stg_decrypt_daemons`, each
guess landing ~0x27-0x2b bytes apart rather than at one fixed address --
and separately computing a target that landed inside a WRONG, unrelated
function for another such call). **Fix: for any `.init.text` (or other
non-`.text`-section) caller, don't trust the raw-byte guess -- cross-
reference `/home/share/Decomp/oa_export/`'s own Ghidra-analyzed (properly
relocated) decompile for that SPECIFIC caller's real callee names/args
first, then independently re-verify the callee side (which, if it's a
plain `.text`-internal function, IS safely disassemblable directly via
`objdump -dr`).** This generalizes sec 10.182's own "oa_export uses a
different image base, always re-disassemble at the real address" finding
to a NEW, sharper class of danger: it's not just an address-offset
issue, cross-section raw PC32 arithmetic is fundamentally unreliable in
this file, not merely off by a constant.

**Struct-layout sharpening of sec 10.183's `STGDaemonWatch` (batch 35):**
that pass only knew fields at what it called the array's own
`+0x00`/`+0x04`/`+0x0c` (lastTick/timeout/srq) because it only had
`signal_timed_out_daemons`' own READS to go on -- this batch found the
TRUE per-daemon struct starts 0x18 bytes EARLIER (confirmed via
`setup_stg_daemons`' own real indexing base being exactly `gStgDaemons`'
base minus `0x18`, AND via `SetupDaemon.clone.0` itself zeroing
`this[+0x18]` and writing a confirmed-real `0x32` constant at `this[+0x1c]`
that matches `timeout` exactly). **Extending a struct with NEW LEADING
fields (before an existing named field) is always safe and requires zero
changes to any existing caller/test, since C++ field-name access
auto-recomputes the offset** -- distinct from (and safer than) inserting
fields in the MIDDLE of an already-relied-upon byte range. Also corrects
sec 10.190's own speculative guess that `this[+0x1c]=0x32` was a
"priority" constant -- cross-referencing the NOW-KNOWN-to-be-the-same
byte as sec 10.183's own `timeout` field settled it: it's the watchdog
timeout, not a priority, confirmed by BOTH derivations landing on the
identical byte.

**Reused/extended "opaque address constant instead of a new stub"
technique, generalized from ONE trampoline to a whole family:** the two
kernel-thread entry trampolines AND 18 daemon-specific `MainRoutine`/
`SRQHandler` functions (confirmed real `T`/`t` ground-truth symbols, `nm`)
are all "store this address, never call it from code we reconstruct" --
rather than manufacture 18 new placeholder stub bodies (which would have
made bare-`{}` count go UP by 17 net, working against the whole batch's
purpose), all 20 are modeled as `static void (*const Foo)(void) =
(void(*)(void))0x11c...;` raw ground-truth address constants, exactly
matching the pre-existing `RTWRAP_THREAD_TRAMPOLINE` pattern (`rtwrap.cpp`,
batch 39). Rule of thumb: whenever a promoted function's real body only
ever STORES another function's address (never dispatches through it),
check whether that address can be modeled as an opaque constant instead
of writing a new placeholder stub -- especially valuable when there are
MANY such addresses (here, 20) that would otherwise inflate the stub
count for zero behavioral benefit.

**Real build-system gotcha, caught ONLY by the required `make ko` kernel
build, NOT by any passing host `verify/` test:** a new global
(`gStgDaemons`) whose storage ALREADY lives in a sibling TU
(`stg_daemons.cpp`, sec 10.183) was ALSO defined in the new file
(`daemon_lifecycle.cpp`) in the first draft -- compiles clean and every
isolated `verify/` test passes (no single test links BOTH TUs together),
but the real kernel `.ko` link (which combines every `OA-objs` entry)
fails with a genuine `ld` "multiple definition of `gStgDaemons`" error.
**STANDING LESSON, worth its own top-level callout: a clean `make objs
verify` pass is NOT sufficient evidence that a new/shared global's
storage placement is conflict-free across the WHOLE project -- only the
real `make ko` link (combining every OA-objs TU) actually exercises
full-project link consistency.** Do not skip or shortcut the `make ko`
step even when host tests all pass; this is now the SECOND time (after
sec 10.156's per-section mock gaps) that a host-test-clean state hid a
real defect only the fuller build/link step caught. Fixed by making the
new file consume the array via the header's own `extern` declaration
only, and giving the newly-isolated `verify/test_daemon_lifecycle.cpp`
its own LOCAL storage for `gStgDaemons` (matching the sec 10.158 "give an
isolated test its own local storage for an extern it doesn't get for
free" precedent) since that test deliberately links ONLY
`daemon_lifecycle.cpp`, not `stg_daemons.cpp`.

**New host-KAT gotcha (own-test bug, not a real-binary quirk): `(long)`-
casting an `unsigned int` field holding a `-1` sentinel does NOT sign-
extend -- it zero-extends, since the SOURCE type is unsigned.** A check
comparing `(long)someUnsignedField` against literal `-1` silently fails
(`got=4294967295 want=-1`) even though the field genuinely holds the bit
pattern `0xFFFFFFFF`. Fix: cast through `(int)` FIRST (`(long)(int)field`)
so the 32-bit `-1` bit pattern sign-extends correctly during the widen to
a 64-bit `long`. A new variant of the established "packed-field pointer/
integer-width" gotcha family (sec 10.156/10.181/10.190), but for a
signed/unsigned WIDENING comparison rather than a pointer round-trip
truncation -- worth checking for on ANY `unsigned int`/`unsigned long`
field a KAT compares against a negative literal via a `(long)` (or wider
signed-type) cast.

**Freestanding-build gotcha re-hit (already established, easy to forget
mid-batch): no `<cstring>`/libc in the `-ffreestanding -fno-builtin`
TARGET build.** First draft used `#include <cstring>` + `memset()`;
`make objs` failed immediately with "`bits/c++config.h`: No such file"
(no `-m32` multilib libstdc++ headers under `-ffreestanding`). Fixed with
a 4-line local `ZeroBytes()` byte-loop helper, matching
`setup_global_resources.cpp`'s own pre-existing "memset-equivalent, no
libc" convention. Caught immediately by the FIRST `make objs` attempt
(not a subtle failure) -- but worth a standing reminder since this is now
at least the second time a batch has reached for a libc header out of
habit before remembering this project's own freestanding constraint.

**Verification**: 83 verify/ binaries (up from 82, new
`test_daemon_lifecycle`), all exit 0 by real per-binary process exit code
(TESTS list re-extracted via the established `awk` state machine). THREE
full clean-rebuild passes byte-identical (`OA.ko` 176,656 bytes, md5
`e99fb0d706a94b03f43d6c5a7cdc6199`, up from 173,628/
`7a9a0f7fc2ff3f63e68a53f9450b6ccb`). `nm -u` 61 -> 69 (8 new externs:
`kernel_thread`, `wait_for_completion`, `wait_for_completion_timeout`,
`__init_waitqueue_head`, `msecs_to_jiffies`, `rt_request_srq`,
`rt_free_srq`, `__wake_up` -- a `comm -23` diff against ground truth's own
`nm -u` list independently confirms ZERO extra/fake externals). linkonce
0x148, GOTPC 0.

**Remaining candidates, unchanged from sec 10.190**: `cm_AuthenEncryptMAC`
(needs `bzzzzzzzzzzzt12` first), `cleanup_cpp_support` (`.dtors` walk,
still blocked), `CSetList::Activate()` + a stubbed-no-op
`CSetListEQ::SetBand()` (audio-DSP-out-of-scope candidate, viable per sec
10.185 policy, not attempted this batch). `bar2_stubs.cpp`'s own 67
stubs remain untouched and still saturated with genuine vtable-dispatch
blockers per sec 10.190's own extensive survey -- none of THIS batch's
new real code (daemon lifecycle) was found to unblock any of them, but
worth re-checking fresh in a future batch per the "don't trust a stale
blocker claim" rule rather than assuming that finding still holds
without re-checking.

**Batch 41 specifics** (2026-07-11, sec 10.192, commit `f25860e`): took sec
10.190/10.191's own flagged `CSetList::Activate()` + stubbed-`SetBand()`
lead, plus resolved `cleanup_cpp_support` as a documented policy decision
(no code change). `CSetList::Activate()` (266 bytes) reuses the existing
`ResolveActivePerformanceVarsManagerRaw()` helper (same as its immediate
sibling `CSetListSlot::Activate()`), writes a mute-gated gain float + a
raw dword copy into an embedded `CSetListEQ` sub-object, then calls
`SetBand()` nine times -- `SetBand()`'s own body is genuine SSE/x87
EQ-coefficient DSP (same cluster as sec 10.177/10.178's `CSTGEQ`), so it
got a no-op stub in `bar2_stubs.cpp`, identical treatment to
`CSTGControllerInfo::SetPerfSwitch` (sec 10.187): real caller + confirmed-
deferred no-op callee.

**Recurring "missed mock promotion" gotcha, hit exactly as predicted by
this very memory file**: ALL THREE `verify/*.cpp` files linking
`global.cpp` (`test_engine`, `test_global`, `test_global_ctor`) already
had their OWN flat `CSetList::Activate() {}` mock -- not because of
anything in THIS batch, but because `CompletePerformanceActivation()`
(real since sec 10.102) already calls it. `test_global.cpp`'s own mock
was load-bearing (a call counter checked in its own `[40]` section).
Removed all three, rewrote `test_global.cpp`'s `[40]` check to verify the
REAL function's own side effects directly instead of a counter -- a
strictly stronger integration check, in addition to a new dedicated `[53]`
unit KAT. **Reinforced rule: grep the WHOLE verify/ directory for the
exact symbol name before considering ANY promotion done, even when the
CURRENT batch's own work doesn't obviously need a mock there** -- the
mock can predate the promotion by dozens of sections, planted by an
EARLIER caller's own promotion (here, batch/sec 10.102, ~40 sections
before this one).

**New technique: settling a "how do I model this vs. leave it deferred"
question by checking whether the faithful behavior is CURRENTLY vacuous
in THIS OWN project's build, not just in ground truth.** `cleanup_cpp_support`
walks ground truth's `.dtors` array (C++ static-destructor teardown at
module unload under `-fno-use-cxa-atexit`, no crtstuff). Building a
portable-C `.dtors`-walk needs a linker-script boundary symbol GNU ld
does NOT auto-synthesize for a dot-prefixed section name -- a real
infra change to the shared Kbuild. Before assuming that's worth doing,
checked `objdump -h OA.ko | grep dtors` / `readelf -x .dtors OA.ko` on
THIS PROJECT'S OWN build (not ground truth) and found **no `.dtors`
section exists at all** -- zero global C++ objects in this
reconstruction currently have a non-trivial destructor. A walk over an
empty list that falls through to a confirmed no-op (`stg_cpp_exit`) is
bit-for-bit identical to the existing `{}` body, so the current no-op IS
already the faithful behavior for this reconstruction's present state --
not a placeholder. Resolved as a documented sec-10.185-style virtual-
substitute DECISION (comment-only change, explicitly stating the future
revisit condition: if a later batch gives some class a real non-trivial
destructor, THIS build would then emit its own non-empty `.dtors`
section and this no-op would silently skip it -- re-examine then).
**Reusable pattern for future batches facing a similar "faithful
transcription needs infra I don't want to add" call**: check whether the
faithful behavior is CURRENTLY a no-op in this project's OWN build state
(not ground truth's), which can turn "build risky new infra" into "just
document why the existing simple stub is already correct, for now."

**Stub-count metric note, not a discrepancy**: `bar2_stubs.cpp`'s bare-`{}`
grep count stayed at exactly 67 (a real function promoted OUT, a new
confirmed-deferred-DSP no-op stubbed IN, net zero on the metric despite
real forward progress) -- same "doesn't move the metric but is real
progress" shape as sec 10.190's `rtwrap_pthread_create`. `bar2_stubs_c.cpp`
stays at 2 (`cleanup_cpp_support`'s own resolution this batch is a closed
DECISION about an already-`{}` body, not a stub removal).

**Verification**: three full clean-rebuild passes, byte-identical all
three (`OA.ko` 176,824 bytes, up from 176,656; md5
`3880e58e7967971811791a5f9421dd1d`). All 83 verify/ binaries exit 0 (real
process exit code, all three passes). `nm -u` unchanged at 69 (both new
symbols are locally-defined `T` in this build, matching ground truth's
own `T` status for both). linkonce 0x148, GOTPC 0.

**Remaining candidates for a future batch**: `cm_AuthenEncryptMAC` (needs
`bzzzzzzzzzzzt12` first, unchanged). `bar2_stubs.cpp`'s own 67 stubs
remain untouched by this batch's new code -- still saturated with
genuine vtable-dispatch/DSP-cluster blockers per sec 10.190's survey;
re-check fresh next time rather than trusting this note indefinitely.
