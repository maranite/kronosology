// SPDX-License-Identifier: GPL-2.0
/*
 * midi_korgusb_port.cpp -- the KorgUsb MIDI transport (survey candidate
 * 3 of the MIDI-hardware-I/O survey, see agent-memory
 * oa_midi_hardware_io_survey_and_serial_midi.md /
 * oa_midi_out_port_serial_cluster.md): OA.ko's OWN calling/driver code
 * for the 2 MIDI ports carried inside Korg's composite USB-audio
 * interface (as opposed to the physical 5-pin DIN UART, midi_in_port_
 * serial.cpp/midi_out_port_serial.cpp, or the generic-USB-MIDI-class
 * accessory hierarchy, `CSTGMidiInPortUSB` etc, oa_engine.h, still
 * deliberately deferred). NOT the companion `KorgUsbAudioVirtualDriver`
 * project (reconstructed/KorgUsbAudioVirtualDriver/) -- that is the
 * USB-side virtual device/emulator, a separate already-complete
 * project; this file is OA.ko's own client code that calls INTO it (and
 * flags a real ABI mismatch found there, see below).
 *
 * Ground truth, full `objdump -dr -M intel` transcription against
 * `/home/share/Decomp/OA.ko_Decomp/OA.ko` (every address independently
 * re-derived from raw disassembly, NOT the Ghidra decompile export --
 * that export mis-tracks `this`/register arguments throughout this
 * cluster due to the same regparm/thiscall confusion documented
 * elsewhere in this project):
 *   CKorgUsbAudioDriverMidiPorts::CKorgUsbAudioDriverMidiPorts()
 *                                                  .text+0x3400f0  170B
 *   CMidiPortPair::Connect()                       .text+0x3401a0   94B
 *   CMidiPortPair::Disconnect()                     .text+0x340210   66B
 *   CMidiPortPair::InputCallback(void*,USBMidiPacket) (own comdat)     18B
 *   CKorgUsbAudioDriverMidiPorts::ProcessOutput()  .text+0x340260  154B
 *   CSTGMidiInPortKorgUsb::Deactivate()            .text+0x340430   37B
 *   CSTGMidiInPortKorgUsb::Activate(CSTGMidiQueue*) .text+0x340460   35B
 *   CSTGMidiInPortKorgUsb::ShouldActivate() const  (own comdat)        6B
 *   CSTGMidiOutPortKorgUsb::CanSendRealTime() const .text+0x340490   39B
 *   CSTGMidiOutPortKorgUsb::CanSendRegular() const  .text+0x3404c0   39B
 *   CSTGMidiOutPortKorgUsb::SendRealTime(uchar)     .text+0x3404f0   54B
 *   CSTGMidiOutPortKorgUsb::SendSingleByte(uchar)   .text+0x340530   57B
 *   CSTGMidiOutPortKorgUsb::ProcessRegularMessage() .text+0x340570  116B
 *   CSTGMidiOutPortKorgUsb::Deactivate()            .text+0x3405f0   40B
 *   CSTGMidiOutPortKorgUsb::Activate(CSTGMidiQueue*) .text+0x340620   38B
 *   CSTGMidiOutPortKorgUsb::CSTGMidiOutPortKorgUsb(...) .text+0x340650 113B
 *   CSTGMidiOutPortKorgUsb::RealtimeInput(uchar const*,uint) .text+0x3406d0 68B
 *   CSTGMidiOutPortKorgUsb::Input(uchar const*,uint) .text+0x340720   71B
 *   CSTGMidiOutPortKorgUsb::RealtimeOutput()        .text+0x340770  214B
 *   CSTGMidiOutPortKorgUsb::Output()                .text+0x340850  214B
 *   CSTGMidiOutPortKorgUsb::ShouldActivate() const  (own comdat)        6B
 *   STGMidiOutPortKorgUsb_Output                    .text+0x340300  128B
 *   STGMidiOutPortKorgUsb_OutputHandler             .text+0x340930   53B
 *   STGMidiOutPortKorgUsb_OutputThread               .text+0x340970  305B
 *   STGMidiOutPortKorgUsb_Initialize                .text+0x340ab0  111B
 *   STGMidiOutPortKorgUsb_Initialized                .text+0x340b20   11B
 *   STGMidiOutPortKorgUsb_Done                       .text+0x340b30   99B
 *   STGMidiOutPortKorgUsb_ScheduleFromRTAI           .text+0x340ba0   31B
 *   STGMidiOutPortKorgUsb_ScheduleFromLinux          .text+0x340bc0    8B
 *
 * See oa_engine.h (`CSTGMidiInPortKorgUsb`, `USBMidiPacket`,
 * `CSTGMidiInPortUSB`) and oa_engine_init.h (`CSTGMidiOutPortKorgUsb`,
 * `CKorgUsbAudioDriverMidiPorts`) for the full struct-layout derivation
 * -- kept there (not here) matching this project's established
 * class-comment-lives-with-the-class convention.
 *
 * REAL ABI MISMATCH FOUND in the companion `KorgUsbAudioVirtualDriver`
 * project (flagged, NOT fixed here -- separate, already-complete
 * project, out of this task's scope): its existing `korgusbaudio_
 * stub.cpp` declares/implements `KorgUsbMidiInitialize`/
 * `KorgUsbMidiInitialized`/`KorgUsbMidiDone` as taking ZERO arguments,
 * but THIS file's own disassembly-confirmed ground truth shows OA.ko
 * calling all three WITH an `int idx` argument (`Initialize` additionally
 * takes 2 `unsigned int` buffer-size args + a `void*` userdata pointer,
 * regparm(3) + 1 stack arg). The extern declarations below match OA.ko's
 * OWN real calling convention (matching this file's job of reconstructing
 * OA.ko, not the companion module) -- linking against the
 * currently-mismatched stub would misbehave; see HARDWARE_REVIEW_LOG.md.
 */

#include "oa_global.h"
#include "oa_engine.h"
#include "oa_engine_init.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

/* ---------------------------------------------------------------------
 * Pointer <-> packed-u32 helpers (this project's established per-file
 * convention).
 * ------------------------------------------------------------------- */
static inline unsigned int ToU32(const void *p) { return (unsigned int)(unsigned long)p; }
static inline void *FromU32(unsigned int v) { return (void *)(unsigned long)v; }

/* The STGMidiOutPortKorgUsb_* pump functions (declared in
 * oa_engine_init.h, alongside CKorgUsbAudioDriverMidiPorts) are called
 * by CKorgUsbAudioDriverMidiPorts/CSTGMidiOutPortKorgUsb's own methods
 * (defined earlier in this file) but themselves defined later (own
 * section below, matching this file's "singleton/ports first, pump
 * infrastructure last" layout). */
extern "C" void rt_pend_linux_srq(unsigned int);

/* This project's convention: `CSTGMidiInPort` has no modeled `vtable`
 * field, so nothing dispatches through it -- see that class's own
 * comment (oa_engine.h). Declared purely so the ctor below has SOME
 * real address to write at InPort+0x00, matching the real binary's own
 * observable write.
 *
 * SAFETY FIX (2026-07-27, integration-boot regression): this array used
 * to be a literal all-zero placeholder `{0, 0, 0}`. That was safe ONLY
 * as long as `CSTGMidiPortManager::Initialize()`'s port-registration
 * loop (midi_port_manager.cpp) stayed genuinely dead code -- true when
 * this file's ctor ran as an automatic C++ static initializer that the
 * kernel module loader never actually executes (`.init_array` is never
 * run by `do_mod_ctors()`). Once that ordering bug was fixed (see
 * `ConstructKorgUsbMidiPorts()`'s new explicit call as literally the
 * first statement of `init_module()`), `RegisterMidiInPort()` genuinely
 * runs during boot and populates `CSTGMidiPortManager::sMidiInPorts[]`
 * with this pair's two real port objects -- making the previously-dead
 * loop live for the first time. That loop calls `PortQuery(inPort)`
 * (midi_port_manager.cpp), which dereferences `*(void***)inPort` (this
 * placeholder) and calls slot 0 as a function pointer -- with the old
 * all-zero content, that is a call through a NULL pointer, confirmed via
 * a live kronos_vm boot Oops (`EIP is at 0x0`, called from
 * `CSTGMidiPortManager::Initialize()` via `CSTGEngine::Initialize()` via
 * `setup_global_resources()`).
 *
 * This project's own documentation is NOT yet settled on what the real
 * ground-truth semantics of `PortQuery()`'s slot 0 dispatch actually are
 * for this class specifically: midi_port_manager.cpp's own header
 * comment (and oa_engine_init.h's `CSTGMidiOutPort` field-layout note)
 * describe it as "slot 0 bool query, slot 1 hands the port a
 * CSTGMidiQueue region" -- but oa_engine_init.h's own later, more
 * detailed `CSTGMidiOutPort` vtable derivation independently states the
 * REAL vtable order is `[dtor, Activate, Deactivate, BumpTimers, ...]`,
 * i.e. slot 0 would be the (still-pure) destructor, not a bool query.
 * These two comments contradict each other and were not reconciled
 * before this fix -- resolving that ambiguity needs real objdump -dr
 * disassembly at PortQuery()/PortRegister()'s own call site in
 * OA_real.ko, out of scope for this integration-boot pass. Rather than
 * guess at real semantics under that ambiguity, this is a deliberate,
 * clearly-labeled SAFE STUB: slot 0 unconditionally returns false, so
 * `PortQuery()` is satisfied (no NULL call) and the calling loop's own
 * `&&`-short-circuit means `PortRegister()` (slot 1) is never reached
 * through this object -- i.e. this pair's in-ports are treated as "not
 * ready to register" rather than dispatching into not-yet-reconstructed
 * territory. Matches this project's own established convention for
 * exactly this situation (e.g. bar2_stubs.cpp's whole approach). Flagged
 * for a dedicated future pass to determine the TRUE slot semantics and
 * replace this stub with the real dispatch once resolved. */
static bool KorgUsbInPortPortQueryStub(void *)
{
	return false;
}
static const unsigned int _ZTV21CSTGMidiInPortKorgUsb[3] = {
	0, 0, (unsigned int)(unsigned long)&KorgUsbInPortPortQueryStub
};

/*
 * ROOT-CAUSE FIX (2026-07-27, follow-up to the 13fba9f integration-boot
 * regression): unlike the InPort side just above, `CSTGMidiOutPort`
 * (oa_engine_init.h) DOES carry an explicit, CONFIRMED-correct `vtable`
 * field (+0x00) -- but nothing anywhere in this project ever WROTE to
 * it. Confirmed via direct `objdump -dr -M intel` against ground truth
 * OA.ko:
 *
 *   CSTGMidiOutPort::CSTGMidiOutPort(...)   .text+0xf8270:
 *     f827a: c7 00 08 00 00 00   mov DWORD PTR [eax],0x8
 *            f827c: R_386_32  _ZTV15CSTGMidiOutPort
 *
 *   CSTGMidiOutPortKorgUsb::CSTGMidiOutPortKorgUsb(...) .text+0x340650:
 *     340688: c7 03 08 00 00 00  mov DWORD PTR [ebx],0x8
 *             34068a: R_386_32  _ZTV22CSTGMidiOutPortKorgUsb
 *
 * i.e. the real base ctor's FIRST instruction writes `&_ZTV15CSTGMidiOutPort
 * + 8` into `this+0x00` (standard Itanium ABI: +8 skips the 2-word
 * offset-to-top/typeinfo RTTI header, landing on slot 0), then the real
 * derived ctor unconditionally overwrites it with `&_ZTV22CSTGMidiOutPortKorgUsb
 * + 8` right after the base ctor call returns. This project's own
 * reconstructed `CSTGMidiOutPort::CSTGMidiOutPort()` (midi_out_port_serial.cpp)
 * and `CSTGMidiOutPortKorgUsb::CSTGMidiOutPortKorgUsb()` (below) both
 * faithfully transcribe every OTHER instruction in these two ctors but
 * neither ever assigns the `vtable` field -- a genuine missing-field-write
 * gap, not a `.ctors`/`.init_array` issue (this ctor runs for real, via
 * the explicit `Construct()`/placement-new below, already fixed by
 * `5a1b107`). Left unfixed, `outPort->vtable` stays whatever the placement
 * storage happened to contain (0, for `CKorgUsbAudioDriverMidiPorts::
 * sInstance`'s zero-initialized `.bss` storage) -- exactly the live-boot
 * NULL `PortQuery()`/`PortRegister()` crash `13fba9f` stopped with a
 * defensive guard rather than explaining.
 *
 * `.rel.rodata._ZTV22CSTGMidiOutPortKorgUsb` (9 entries, readelf -rW)
 * resolves EVERY one of the 9 slots to a real, non-pure function --
 * unlike the InPort side above, no stub is needed here, every slot is
 * already a genuine reconstructed method on this class (or, for slot 3,
 * the inherited base `CSTGMidiOutPort::BumpTimers()` body -- confirmed
 * by the relocation's own symbol value being 0, i.e. not overridden):
 *   slot0 (+0x08) ShouldActivate() const   -- CSTGMidiOutPortKorgUsb, `return true;`
 *   slot1 (+0x0c) Activate(CSTGMidiQueue*) -- CSTGMidiOutPortKorgUsb
 *   slot2 (+0x10) Deactivate()             -- CSTGMidiOutPortKorgUsb
 *   slot3 (+0x14) BumpTimers()             -- CSTGMidiOutPort (base, not overridden)
 *   slot4 (+0x18) CanSendRealTime() const  -- CSTGMidiOutPortKorgUsb
 *   slot5 (+0x1c) CanSendRegular() const   -- CSTGMidiOutPortKorgUsb
 *   slot6 (+0x20) ProcessRegularMessage()  -- CSTGMidiOutPortKorgUsb
 *   slot7 (+0x24) SendRealTime(uchar)      -- CSTGMidiOutPortKorgUsb
 *   slot8 (+0x28) SendSingleByte(uchar)    -- CSTGMidiOutPortKorgUsb
 * This independently CONFIRMS (rather than guesses) `PortQuery()`/
 * `PortRegister()`'s own existing slot-0/slot-1 semantics
 * (midi_port_manager.cpp): ground truth `CSTGMidiPortManager::Initialize()`
 * (`.text+0xf4f60`) itself does `call DWORD PTR [edx]` (slot 0, gated by
 * `test al,al`) then conditionally `call DWORD PTR [ecx+0x4]` (slot 1,
 * with the region pointer in edx) for every one of the 8 port slots --
 * byte-identical to this project's own `PortQuery()`/`PortRegister()`
 * shape. Also independently resolves the "slot 0 bool query vs dtor"
 * ambiguity this file's own header comment (above) flagged as
 * unreconciled: slot 0 is `ShouldActivate() const` (always returns
 * `true` in ground truth, confirmed by direct disassembly of the
 * 6-byte comdat body), NOT a destructor -- the dtor genuinely doesn't
 * exist as a virtual slot in this hierarchy at all.
 *
 * Non-virtual forwarding trampolines (same technique as
 * `KorgUsbInPortPortQueryStub` above): this project deliberately does
 * NOT use real C++ `virtual` for this class (see oa_engine_init.h's own
 * class comment -- a compiler-generated vtable pointer would silently
 * shift every field offset below +0x40), so there is no `&Class::Method`
 * address directly usable as a `void*`-callable slot; each trampoline
 * just forwards `(void *this, ...)` to the already-real ordinary member
 * call, matching this whole TU's regparm(3) convention so `PortQuery()`/
 * `PortRegister()`'s own `(Fn)vtable[N]` casts dispatch correctly.
 */
static bool OutPortKorgUsb_ShouldActivate(void *p)
{
	return ((CSTGMidiOutPortKorgUsb *)p)->ShouldActivate();
}
static void OutPortKorgUsb_Activate(void *p, void *q3)
{
	((CSTGMidiOutPortKorgUsb *)p)->Activate((CSTGMidiQueue *)q3);
}
static void OutPortKorgUsb_Deactivate(void *p)
{
	((CSTGMidiOutPortKorgUsb *)p)->Deactivate();
}
static void OutPortKorgUsb_BumpTimers(void *p)
{
	((CSTGMidiOutPort *)p)->BumpTimers();
}
static bool OutPortKorgUsb_CanSendRealTime(void *p)
{
	return ((CSTGMidiOutPortKorgUsb *)p)->CanSendRealTime();
}
static bool OutPortKorgUsb_CanSendRegular(void *p)
{
	return ((CSTGMidiOutPortKorgUsb *)p)->CanSendRegular();
}
static bool OutPortKorgUsb_ProcessRegularMessage(void *p)
{
	return ((CSTGMidiOutPortKorgUsb *)p)->ProcessRegularMessage();
}
static void OutPortKorgUsb_SendRealTime(void *p, unsigned char b)
{
	((CSTGMidiOutPortKorgUsb *)p)->SendRealTime(b);
}
static void OutPortKorgUsb_SendSingleByte(void *p, unsigned char b)
{
	((CSTGMidiOutPortKorgUsb *)p)->SendSingleByte(b);
}
static const unsigned int _ZTV22CSTGMidiOutPortKorgUsb[9] = {
	(unsigned int)(unsigned long)&OutPortKorgUsb_ShouldActivate,
	(unsigned int)(unsigned long)&OutPortKorgUsb_Activate,
	(unsigned int)(unsigned long)&OutPortKorgUsb_Deactivate,
	(unsigned int)(unsigned long)&OutPortKorgUsb_BumpTimers,
	(unsigned int)(unsigned long)&OutPortKorgUsb_CanSendRealTime,
	(unsigned int)(unsigned long)&OutPortKorgUsb_CanSendRegular,
	(unsigned int)(unsigned long)&OutPortKorgUsb_ProcessRegularMessage,
	(unsigned int)(unsigned long)&OutPortKorgUsb_SendRealTime,
	(unsigned int)(unsigned long)&OutPortKorgUsb_SendSingleByte,
};

/* ---------------------------------------------------------------------
 * Companion-module (KorgUsbAudioDriver.ko-family) externs. All confirmed
 * `U` in ground truth OA.ko. See the file header comment above for the
 * real ABI vs the currently-mismatched KorgUsbAudioVirtualDriver stub.
 * ------------------------------------------------------------------- */
extern "C" int KorgUsbMidiInitialized(int idx);
extern "C" int KorgUsbMidiInitialize(int idx, unsigned int bufSizeA, unsigned int bufSizeB, void *userdata);
extern "C" int KorgUsbMidiDone(int idx);
extern "C" int KorgUsbRealtimeMidiOutputCanSend(int portId);
extern "C" void KorgUsbRealtimeMidiOutput(int portId, unsigned char *buf, unsigned int count);
extern "C" int KorgUsbMidiOutputCanSend(int portId);
extern "C" void KorgUsbMidiOutput(int portId, unsigned char *buf, unsigned int count);

/* ---------------------------------------------------------------------
 * RTAI/kernel externs. All confirmed `U` in ground truth OA.ko. The
 * first 6 are ALSO declared in src/init/daemon_lifecycle.cpp with
 * identical signatures (same real symbols) -- this project's
 * established "every file declares its own copy" convention (see
 * oa_init.h's own note on init_cpp_support/cleanup_cpp_support), not a
 * conflict: plain extern "C" declarations are freely repeatable across
 * translation units. The remaining ones are new to this file, needed by
 * `STGMidiOutPortKorgUsb_OutputThread()`'s own manual sleep/wake loop
 * (a genuinely different daemon-thread shape than daemon_lifecycle.cpp's
 * completion-pair pattern -- this one uses a real Linux waitqueue).
 * ------------------------------------------------------------------- */
extern "C" int  rt_request_srq(unsigned int label, void (*handler)(void), void *rt_handler);
extern "C" void rt_free_srq(unsigned int srq);
extern "C" long kernel_thread(int (*fn)(void *), void *arg, unsigned long flags);
extern "C" void wait_for_completion(void *completion);
extern "C" long wait_for_completion_timeout(void *completion, unsigned long timeout);
extern "C" void __wake_up(void *q, unsigned int mode, int nr_exclusive, void *key);
extern "C" __attribute__((regparm(0))) void daemonize(const char *name, ...);
extern "C" int  stg_sched_setscheduler(void *task, int policy, void *param);
extern "C" unsigned long stg_cpumask_of_cpu(unsigned int cpu);
extern "C" int  stg_set_cpus_allowed(void *p, unsigned long mask);
extern "C" void prepare_to_wait(void *waitq, void *waitEntry, int state);
extern "C" long schedule_timeout(long jiffies);
extern "C" void finish_wait(void *waitq, void *waitEntry);
extern "C" int  autoremove_wake_function(void *waitEntry, unsigned int mode, int sync, void *key);
extern "C" void complete(void *completion);
extern "C" void complete_and_exit(void *completion, long code);
extern "C" void *stg_get_current_task(void);

/* ---------------------------------------------------------------------
 * CKorgUsbAudioDriverMidiPorts
 * ------------------------------------------------------------------- */

CKorgUsbAudioDriverMidiPorts CKorgUsbAudioDriverMidiPorts::sInstance;

/*
 * CKorgUsbAudioDriverMidiPorts::Construct() -- CONFIRMED real, full
 * transcription of the real ctor's body (`.text+0x3400f0`, 170 bytes).
 * Deliberately NOT a C++ constructor -- called explicitly instead, see
 * the class comment in oa_engine_init.h for why (ctor-array/.init_array
 * vs .ctors toolchain mismatch). Builds both `CMidiPortPair`s at their
 * fixed raw offsets (see class comment, oa_engine_init.h):
 * selfPtr/callbackFnPtr, the base `CSTGMidiInPort` ctor (portType/
 * flagsInit CONFIRMED via register loads at each call site -- pair0:
 * stgPort=0,flagsInit=1; pair1: stgPort=1,flagsInit=1), the derived
 * `CSTGMidiInPortKorgUsb` vtable override + `midiPortIndex` byte
 * (0/1), and the `CSTGMidiOutPortKorgUsb` ctor (korgUsbPort=0/1,
 * stgPort=0/1, flagsInit=0 for both -- CONFIRMED, the 3rd ctor arg is
 * pushed as a literal `0` at both call sites).
 */
void CKorgUsbAudioDriverMidiPorts::Construct()
{
	for (int i = 0; i < 2; i++) {
		unsigned char *pair = storage + i * 0xb48;
		unsigned char *inPort = pair + 0x08;
		unsigned char *outPort = pair + 0x2f4;

		*(unsigned int *)(pair + 0x04) = ToU32((void *)&CKorgUsbAudioDriverMidiPorts::InputCallback);
		*(unsigned int *)(pair + 0x00) = ToU32(pair);

		new (inPort) CSTGMidiInPort(i, 1);
		*(unsigned int *)(inPort + 0x00) = ToU32((unsigned char *)_ZTV21CSTGMidiInPortKorgUsb + 8);
		inPort[0x2e8] = (unsigned char)i;   /* midiPortIndex */

		new (outPort) CSTGMidiOutPortKorgUsb(i, i, 0);
		/* ROOT-CAUSE FIX (2026-07-27): ground truth's own derived ctor
		 * (`.text+0x340650`) overwrites `this+0x00` with
		 * `&_ZTV22CSTGMidiOutPortKorgUsb + 8` as its own last act (see
		 * this array's own header comment above) -- the reconstructed
		 * C++ ctor never performed this write, leaving `outPort->vtable`
		 * at whatever this placement storage already held (0, for this
		 * `.bss`-backed `sInstance`), which is exactly the NULL
		 * `PortQuery()`/`PortRegister()` dispatch `13fba9f`'s defensive
		 * guard was papering over. Explicit post-construction write here
		 * matches this exact file's own established precedent for the
		 * InPort side, 2 lines up. */
		*(unsigned int *)(outPort + 0x00) = ToU32((unsigned char *)_ZTV22CSTGMidiOutPortKorgUsb);
	}
}

/*
 * Plain extern "C" wrapper around Construct() -- see the class comment
 * in oa_engine_init.h and this call site's own comment in
 * init_module.cpp for why this isn't just an automatic C++ static
 * initializer. A free function so init_module()'s own isolated host
 * test can mock it like every other init step.
 */
extern "C" void ConstructKorgUsbMidiPorts(void)
{
	CKorgUsbAudioDriverMidiPorts::sInstance.Construct();
}

/*
 * CMidiPortPair::Connect() -- CONFIRMED real, full transcription. `pair`
 * points at the pair's OWN base (selfPtr field), i.e. InPort - 8.
 */
void CKorgUsbAudioDriverMidiPorts::Connect(unsigned char *pair)
{
	int idx = (signed char)pair[0x2f0];

	if (!KorgUsbMidiInitialized(idx))
		KorgUsbMidiInitialize(idx, 0x400, 0x400, pair);

	if (!STGMidiOutPortKorgUsb_Initialized())
		STGMidiOutPortKorgUsb_Initialize();
}

/* CMidiPortPair::Disconnect() -- CONFIRMED real, full transcription. */
void CKorgUsbAudioDriverMidiPorts::Disconnect(unsigned char *pair)
{
	if (STGMidiOutPortKorgUsb_Initialized())
		STGMidiOutPortKorgUsb_Done();

	int idx = (signed char)pair[0x2f0];
	if (KorgUsbMidiInitialized(idx))
		KorgUsbMidiDone(idx);
}

/*
 * CMidiPortPair::InputCallback(void*, USBMidiPacket) -- CONFIRMED real
 * but genuinely unreachable from anywhere within OA.ko itself (own
 * un-merged comdat, zero xrefs) -- only the companion USB module calls
 * it, asynchronously, with real USB MIDI hardware attached. A pure
 * thunk: `this` (=`ctx`, the pair's own selfPtr) adjusted by +8 to the
 * embedded InPort, tail-forwarded UNTOUCHED into the deliberately-
 * deferred `CSTGMidiInPortUSB::ReceivePacket()` (oa_engine.h).
 */
void CKorgUsbAudioDriverMidiPorts::InputCallback(void *ctx, USBMidiPacket pkt)
{
	((CSTGMidiInPortUSB *)((unsigned char *)ctx + 8))->ReceivePacket(pkt);
}

/*
 * ProcessOutput() -- CONFIRMED real, full transcription. For each of
 * the 2 pairs: if that OutPort's `flags` bit1 (active) is set, drain
 * `RealtimeOutput()`/`Output()` whenever their respective ring has a
 * pending byte (write cursor != read cursor).
 */
void CKorgUsbAudioDriverMidiPorts::ProcessOutput()
{
	for (int i = 0; i < 2; i++) {
		CSTGMidiOutPortKorgUsb *out = OutPort(i);

		if ((out->flags & 0x2) == 0)
			continue;
		if (out->realtimeWriteIdx != out->realtimeReadIdx)
			out->RealtimeOutput();
		if (out->regularWriteIdx != out->regularReadIdx)
			out->Output();
	}
}

/* ---------------------------------------------------------------------
 * CSTGMidiInPortKorgUsb
 * ------------------------------------------------------------------- */

/* Deactivate() -- CONFIRMED real: tears down the companion-module
 * connection FIRST (pair = this - 8), then the base CSTGMidiInPort. */
void CSTGMidiInPortKorgUsb::Deactivate()
{
	CKorgUsbAudioDriverMidiPorts::Disconnect((unsigned char *)this - 8);
	((CSTGMidiInPort *)this)->Deactivate(); /* now real -- see midi_in_port_serial.cpp */
}

/* Activate(CSTGMidiQueue*) -- CONFIRMED real: base Activate() first,
 * THEN the companion-module connection. */
void CSTGMidiInPortKorgUsb::Activate(CSTGMidiQueue *q)
{
	((CSTGMidiInPort *)this)->Activate(q); /* now real -- see midi_in_port_serial.cpp */
	CKorgUsbAudioDriverMidiPorts::Connect((unsigned char *)this - 8);
}

/* ---------------------------------------------------------------------
 * CSTGMidiOutPortKorgUsb
 * ------------------------------------------------------------------- */

CSTGMidiOutPortKorgUsb::CSTGMidiOutPortKorgUsb(int korgUsbPort, int stgPort, unsigned int flagsInit)
	: CSTGMidiOutPort(stgPort, flagsInit)
{
	korgUsbPortIndex = (unsigned char)korgUsbPort;
	realtimeWriteIdx = 0;
	realtimeReadIdx = 0;
	regularWriteIdx = 0;
	regularReadIdx = 0;
	for (int i = 0; i < 0x400; i++) {
		realtimeRing[i] = 0;
		regularRing[i] = 0;
	}
}

/* Deactivate() -- CONFIRMED real: companion-module teardown first
 * (pair = this - 0x2f4), then base Deactivate(). */
void CSTGMidiOutPortKorgUsb::Deactivate()
{
	CKorgUsbAudioDriverMidiPorts::Disconnect((unsigned char *)this - 0x2f4);
	((CSTGMidiOutPort *)this)->Deactivate();
}

/* Activate(CSTGMidiQueue*) -- CONFIRMED real: base Activate() first,
 * then the companion-module connection. */
void CSTGMidiOutPortKorgUsb::Activate(CSTGMidiQueue *q3)
{
	((CSTGMidiOutPort *)this)->Activate(q3);
	CKorgUsbAudioDriverMidiPorts::Connect((unsigned char *)this - 0x2f4);
}

/* CanSendRealTime()/CanSendRegular() -- CONFIRMED real: "free space in
 * the ring exceeds 3 bytes" gate (0x400 minus the wrapped
 * write-minus-read distance), matching the physical-UART ports' own
 * established CanSend* shape. */
bool CSTGMidiOutPortKorgUsb::CanSendRealTime() const
{
	int diff = (int)realtimeWriteIdx - (int)realtimeReadIdx;
	if (diff < 0)
		diff += 0x400;
	return (0x400 - diff) > 3;
}

bool CSTGMidiOutPortKorgUsb::CanSendRegular() const
{
	int diff = (int)regularWriteIdx - (int)regularReadIdx;
	if (diff < 0)
		diff += 0x400;
	return (0x400 - diff) > 3;
}

/* SendRealTime(byte)/SendSingleByte(byte) -- CONFIRMED real: push one
 * byte into the respective ring, wrap the write cursor at 0x400, then
 * kick the RTAI->Linux pump. */
void CSTGMidiOutPortKorgUsb::SendRealTime(unsigned char b)
{
	realtimeRing[realtimeWriteIdx] = b;
	unsigned int next = realtimeWriteIdx + 1;
	realtimeWriteIdx = (next >= 0x400) ? 0 : next;
	STGMidiOutPortKorgUsb_ScheduleFromRTAI();
}

void CSTGMidiOutPortKorgUsb::SendSingleByte(unsigned char b)
{
	regularRing[regularWriteIdx] = b;
	unsigned int next = regularWriteIdx + 1;
	regularWriteIdx = (next >= 0x400) ? 0 : next;
	STGMidiOutPortKorgUsb_ScheduleFromRTAI();
}

/*
 * ProcessRegularMessage() -- CONFIRMED real, full transcription. Polls
 * one message (up to 3 bytes, running-status-free -- this port has no
 * running-status compression, unlike CSTGMidiOutPortSerial) via the
 * base `CSTGMidiOutPort::ReadNextMessage()` (midi_out_port_serial.cpp,
 * shared with the physical-UART port -- see that method's own header
 * comment for why its return value matters here specifically). If a
 * message was returned, copies its bytes into the regular ring
 * (wrapping) and kicks the pump.
 */
bool CSTGMidiOutPortKorgUsb::ProcessRegularMessage()
{
	unsigned char msgBuf[7];
	unsigned int len = ((CSTGMidiOutPort *)this)->ReadNextMessage(msgBuf, sizeof(msgBuf));

	if (len == 0)
		return false;

	unsigned int w = regularWriteIdx;
	for (unsigned int i = 0; i < len; i++) {
		regularRing[w] = msgBuf[i];
		w = (w + 1 >= 0x400) ? 0 : w + 1;
	}
	regularWriteIdx = w;
	STGMidiOutPortKorgUsb_ScheduleFromRTAI();
	return true;
}

/* RealtimeInput(data,len)/Input(data,len) -- CONFIRMED real: append
 * `len` bytes into the respective ring, wrapping each byte's cursor
 * independently (matching the real per-byte `cmovae` loop). No
 * overflow check (mirrors the real code exactly -- callers are expected
 * to have checked `CanSendRealTime()`/`CanSendRegular()` first). */
void CSTGMidiOutPortKorgUsb::RealtimeInput(const unsigned char *data, unsigned int len)
{
	unsigned int w = realtimeWriteIdx;
	for (unsigned int i = 0; i < len; i++) {
		realtimeRing[w] = data[i];
		w = (w + 1 >= 0x400) ? 0 : w + 1;
	}
	realtimeWriteIdx = w;
}

void CSTGMidiOutPortKorgUsb::Input(const unsigned char *data, unsigned int len)
{
	unsigned int w = regularWriteIdx;
	for (unsigned int i = 0; i < len; i++) {
		regularRing[w] = data[i];
		w = (w + 1 >= 0x400) ? 0 : w + 1;
	}
	regularWriteIdx = w;
}

/*
 * Shared drain helper for RealtimeOutput()/Output() -- both real
 * methods have IDENTICAL structure (confirmed via independent full
 * disassembly of both), differing only in which ring/companion-module
 * function pair they use. Computes pending = (write - read) mod 0x400;
 * clamps to whatever the companion module says it can currently accept
 * (`canSend(portId)`), calling `ScheduleFromLinux()` first if clamping
 * was needed (arranges a retry -- see STGMidiOutPortKorgUsb_OutputThread's
 * own header comment for how that retry actually happens); if nothing
 * is pending after clamping, returns; otherwise copies that many bytes
 * out of the ring (wrapping) into a local staging buffer, advances the
 * read cursor, and hands the batch to `output(portId, buf, count)` in
 * one call. A real but PROVABLY UNREACHABLE `jle` branch in both
 * original functions (stale-EFLAGS artifact) that would skip the copy
 * entirely with an uninitialized buffer pointer is confirmed dead and
 * NOT reproduced -- see the class's own header comment (oa_engine_init.h).
 */
extern "C" typedef int (*KorgUsbCanSendFn)(int);
extern "C" typedef void (*KorgUsbOutputFn)(int, unsigned char *, unsigned int);

static void DrainRing(unsigned char *ring, unsigned int *writeIdx, unsigned int *readIdx,
                       int portId, KorgUsbCanSendFn canSend, KorgUsbOutputFn output)
{
	int raw = (int)*writeIdx - (int)*readIdx;
	int avail = (raw >= 0) ? raw : raw + 0x400;

	int cap = canSend(portId);
	if (avail > cap) {
		STGMidiOutPortKorgUsb_ScheduleFromLinux();
		avail = cap;
	}
	if (avail <= 0)
		return;

	unsigned char staging[0x400];
	unsigned int r = *readIdx;
	for (int i = 0; i < avail; i++) {
		staging[i] = ring[r];
		r = (r + 1 >= 0x400) ? 0 : r + 1;
	}
	*readIdx = r;
	output(portId, staging, (unsigned int)avail);
}

void CSTGMidiOutPortKorgUsb::RealtimeOutput()
{
	DrainRing(realtimeRing, &realtimeWriteIdx, &realtimeReadIdx, korgUsbPortIndex,
	          KorgUsbRealtimeMidiOutputCanSend, KorgUsbRealtimeMidiOutput);
}

void CSTGMidiOutPortKorgUsb::Output()
{
	DrainRing(regularRing, &regularWriteIdx, &regularReadIdx, korgUsbPortIndex,
	          KorgUsbMidiOutputCanSend, KorgUsbMidiOutput);
}

/* ---------------------------------------------------------------------
 * STGMidiOutPortKorgUsb_* -- the RTAI-real-time-domain-to-Linux-domain
 * deferred-work pump for CKorgUsbAudioDriverMidiPorts::ProcessOutput().
 * `SendRealTime()`/`SendSingleByte()`/`ProcessRegularMessage()`/
 * `RealtimeOutput()`/`Output()` all run on the RTAI real-time side
 * (regular MIDI-out message dispatch); the actual companion-module
 * `KorgUsb{Realtime}MidiOutput()` calls need full Linux context (they
 * ultimately do USB I/O), so this is the SAME "RTAI SRQ wakes a Linux
 * kernel_thread" bridge pattern already established for the general STG
 * daemon cluster (src/init/daemon_lifecycle.cpp), just with its own
 * dedicated single thread and a real Linux waitqueue instead of a
 * second `struct completion`.
 * ------------------------------------------------------------------- */

static unsigned char sThreadKeepRunning;   /* real .bss+0x26f958 */
static unsigned char sSRQPending;          /* real .bss+0x26f951 -- set by ScheduleFromRTAI */
static unsigned char sLinuxPending;        /* real .bss+0x26f950 -- set by ScheduleFromLinux, NEVER cleared (see below) */
static int sOutputSRQ = -1;                /* real .bss+0x26f954 */

/*
 * wait_queue_head_t stand-in for `sOutputWaitQueueHead` -- CONFIRMED
 * real 12-byte layout AND static initializer via direct `readelf -x
 * .data`/`readelf -rW` against ground truth (`.data+0xa5c4`): a 4-byte
 * spinlock (raw bytes `00000000`, i.e. statically unlocked) followed by
 * an 8-byte `list_head` whose OWN two `R_386_32` relocations (at
 * `+0xa5c8`/`+0xa5cc`) both resolve back to `.data+0xa5c8` -- i.e.
 * `task_list.next`/`task_list.prev` both self-reference the list_head's
 * own address, the exact compile-time shape `DECLARE_WAIT_QUEUE_HEAD()`
 * / `__WAIT_QUEUE_HEAD_INITIALIZER()` produces. Ground truth has NO
 * runtime `init_waitqueue_head()` call anywhere in this cluster -- this
 * head is only ever statically initialized, matching the C++ static
 * initializer below (a real link-time constant, since a static-storage
 * object's own address is a valid constant expression -- no `.ctors`/
 * `.init_array` risk).
 *
 * ROOT-CAUSE FIX (2026-07-27): the prior revision modeled this as a
 * plain zero-initialized `unsigned char[8]` -- undersized by 4 bytes
 * (real size is 12) AND, critically, an all-zero `list_head` is NOT a
 * valid empty-list state (a real empty list_head must self-reference,
 * never point at NULL) -- confirmed via a live kronos_vm boot Oops one
 * layer further into the exact code path the `waitEntry` fix (below,
 * `STGMidiOutPortKorgUsb_OutputThread`) had just unblocked:
 * `prepare_to_wait()`'s own `__add_wait_queue()`/`list_add()` wrote
 * through the bogus NULL `task_list.next`, crashing at `CR2=0x00000004`
 * (`[NULL+4]`, i.e. `next->prev = new` with `next` read back as 0).
 */
struct WaitQueueHeadLayout {
	unsigned int lock;
	void *listNext;
	void *listPrev;
};
static WaitQueueHeadLayout sOutputWaitQueueHeadStorage = {
	0,
	&sOutputWaitQueueHeadStorage.listNext,
	&sOutputWaitQueueHeadStorage.listNext,
};
static void * const sOutputWaitQueueHead = &sOutputWaitQueueHeadStorage;

static unsigned char sExitCompletion[0x10];     /* real .data+0xa5d0, opaque struct completion */

/*
 * STGMidiOutPortKorgUsb_Output -- CONFIRMED real, an independently-
 * compiled duplicate of `CKorgUsbAudioDriverMidiPorts::ProcessOutput()`
 * against the fixed `sInstance` address rather than a generic `this`
 * (typical gcc codegen for a known-address singleton method call) --
 * modeled here as a thin call-through, producing IDENTICAL observable
 * behavior.
 */
void STGMidiOutPortKorgUsb_Output()
{
	CKorgUsbAudioDriverMidiPorts::sInstance.ProcessOutput();
}

/*
 * STGMidiOutPortKorgUsb_OutputHandler -- CONFIRMED real, the RTAI SRQ
 * handler (registered by Initialize() below): if the pump thread is
 * (still) supposed to be running, wake it.
 */
void STGMidiOutPortKorgUsb_OutputHandler()
{
	if (sThreadKeepRunning)
		__wake_up(sOutputWaitQueueHead, 1, 3, 0);
}

/*
 * STGMidiOutPortKorgUsb_OutputThread -- CONFIRMED real, full
 * transcription of the Linux-side pump thread body (`.text+0x340970`,
 * 305 bytes). `arg` is the completion address Initialize() passed to
 * `kernel_thread()`. Sequence: daemonize, pin to CPU 3
 * (`stg_sched_setscheduler` + `stg_set_cpus_allowed(stg_cpumask_of_cpu(3))`),
 * signal readiness via `complete(arg)`, then loop: while
 * `sThreadKeepRunning`, if `sSRQPending` OR `sLinuxPending` is set,
 * dispatch (`STGMidiOutPortKorgUsb_Output()`); CONFIRMED REAL QUIRK:
 * `sSRQPending` IS cleared before dispatch, but `sLinuxPending` is
 * UNCONDITIONALLY RE-SET (never cleared) right before its own dispatch
 * call -- i.e. once `ScheduleFromLinux()` has ever fired even once,
 * this thread dispatches on EVERY subsequent wake for the rest of the
 * module's lifetime (harmless: `Output()`/`RealtimeOutput()` are
 * self-gating no-ops when their own ring diffs are zero). If neither
 * flag is set, sleep on `sOutputWaitQueueHead` for up to 4 jiffies
 * (`prepare_to_wait`/`schedule_timeout`/`finish_wait`) before
 * re-checking. On `!sThreadKeepRunning`, calls `complete_and_exit(
 * &sExitCompletion, 0)`.
 */
extern "C" int STGMidiOutPortKorgUsb_OutputThread(void *arg)
{
	void *self = stg_get_current_task();

	daemonize("STGMidiOutKorgUsb");

	struct { int priority; } schedParam = { 1 };
	stg_sched_setscheduler(self, 1, &schedParam);
	stg_set_cpus_allowed(self, stg_cpumask_of_cpu(3));

	complete(arg);

	while (sThreadKeepRunning) {
		if (!sSRQPending && !sLinuxPending) {
			/*
			 * `wait_queue_t` stand-in -- CONFIRMED real layout+init via
			 * direct `objdump -dr -M intel` (`.text+0x3409ea`-`0x340a06`,
			 * inside this exact `if` branch, re-run fresh every outer-loop
			 * iteration): `struct __wait_queue` is `{flags; private; func;
			 * task_list{next,prev};}`, 20 bytes on -m32. Ground truth writes
			 * all 5 fields EVERY time through this branch, immediately
			 * before `prepare_to_wait()`: flags=0, private=current task
			 * (`self`, captured once at function entry), func=
			 * `&autoremove_wake_function` (declared extern below already),
			 * task_list.next=task_list.prev=&task_list itself (the
			 * standard empty-list self-link `INIT_LIST_HEAD()` produces).
			 *
			 * ROOT-CAUSE FIX (2026-07-27): the prior revision of this
			 * function declared `waitEntry` as a raw, entirely
			 * UNINITIALIZED 24-byte buffer -- none of these 5 fields were
			 * ever written. `prepare_to_wait()`'s own real body checks
			 * `list_empty(&wait->task_list)` before linking it in, and
			 * `finish_wait()` unconditionally calls `list_del_init()` on
			 * it -- both read/write garbage stack contents from a never-
			 * initialized `task_list`, corrupting the real wait queue and
			 * crashing (confirmed via a live kronos_vm boot Oops inside
			 * `finish_wait()`, pid `STGMidiOutKorgU`, CR2 0x7f/EBP 0x7b --
			 * classic leftover-stack-garbage pointer values). This
			 * function was entirely unreachable before the SAME-DAY
			 * `CSTGMidiOutPort` vtable-population fix (see
			 * `_ZTV22CSTGMidiOutPortKorgUsb`'s own comment above) made
			 * `Activate()`/`Connect()`/`STGMidiOutPortKorgUsb_Initialize()`
			 * -- and therefore this thread -- reachable for the first
			 * time; this is a 3rd, independently root-caused bug in the
			 * same integration-boot cascade, not a symptom of the vtable
			 * fix itself.
			 */
			struct {
				unsigned int flags;
				void *priv;
				void *func;
				void *listNext;
				void *listPrev;
			} waitEntry;
			waitEntry.flags = 0;
			waitEntry.priv = self;
			waitEntry.func = (void *)&autoremove_wake_function;
			waitEntry.listNext = &waitEntry.listNext;
			waitEntry.listPrev = &waitEntry.listNext;

			long timeout = 4;
			for (;;) {
				prepare_to_wait(sOutputWaitQueueHead, &waitEntry, 2);
				if (sSRQPending)
					break;
				timeout = schedule_timeout(timeout);
				if (timeout == 0)
					break;
			}
			finish_wait(sOutputWaitQueueHead, &waitEntry);
		}

		if (sSRQPending) {
			sSRQPending = 0;
			STGMidiOutPortKorgUsb_Output();
			continue;
		}
		if (sLinuxPending) {
			sLinuxPending = 1;   /* CONFIRMED real: re-set, not cleared -- see comment above */
			STGMidiOutPortKorgUsb_Output();
		}
	}

	complete_and_exit(sExitCompletion, 0);
	return 0;
}

/*
 * STGMidiOutPortKorgUsb_Initialize -- CONFIRMED real, full
 * transcription. Sets up a local `struct completion` on the stack,
 * marks the thread as wanting to run, spawns
 * `STGMidiOutPortKorgUsb_OutputThread()` via `kernel_thread()` (clearing
 * the keep-running flag again if that fails), blocks on the completion
 * until the new thread signals readiness, then registers the RTAI SRQ
 * (`rt_request_srq(0x4d644f74, OutputHandler, 0)` -- the label is a
 * literal 4-byte ASCII tag, matching this project's established
 * `setup_stg_daemons` convention of loading a 4-char tag per daemon).
 */
void STGMidiOutPortKorgUsb_Initialize()
{
	unsigned char completion[0x10];
	*(unsigned int *)(completion + 0x0) = 0;                 /* done */
	*(unsigned int *)(completion + 0x4) = 0;                 /* wait.lock */
	*(unsigned int *)(completion + 0x8) = ToU32(completion + 0x8); /* wait.task_list.next */
	*(unsigned int *)(completion + 0xc) = ToU32(completion + 0x8); /* wait.task_list.prev */

	sThreadKeepRunning = 1;
	long pid = kernel_thread(STGMidiOutPortKorgUsb_OutputThread, completion, 0);
	if (pid < 0)
		sThreadKeepRunning = 0;

	wait_for_completion(completion);

	sOutputSRQ = rt_request_srq(0x4d644f74u, (void (*)(void))STGMidiOutPortKorgUsb_OutputHandler, 0);
}

/* STGMidiOutPortKorgUsb_Initialized -- CONFIRMED real: `return
 * sThreadKeepRunning;` */
int STGMidiOutPortKorgUsb_Initialized()
{
	return sThreadKeepRunning;
}

/*
 * STGMidiOutPortKorgUsb_Done -- CONFIRMED real, full transcription.
 * Frees the SRQ; if the thread is (still) marked running, clears the
 * flag, wakes it (so it observes the cleared flag and exits via
 * `complete_and_exit`), then waits up to 4 jiffies for that exit to
 * complete.
 */
void STGMidiOutPortKorgUsb_Done()
{
	int oldSrq = sOutputSRQ;
	sOutputSRQ = -1;
	rt_free_srq((unsigned int)oldSrq);

	if (sThreadKeepRunning) {
		sThreadKeepRunning = 0;
		__wake_up(sOutputWaitQueueHead, 1, 3, 0);
		wait_for_completion_timeout(sExitCompletion, 4);
	}
}

/* ScheduleFromRTAI() -- CONFIRMED real: sets sSRQPending, then pends the
 * RTAI SRQ (real code guards `rt_pend_linux_srq` with `srq >= 0`, real
 * disassembly: `test eax,eax; js skip`). */
void STGMidiOutPortKorgUsb_ScheduleFromRTAI()
{
	int srq = sOutputSRQ;
	sSRQPending = 1;
	if (srq >= 0) {
		rt_pend_linux_srq((unsigned int)srq);
	}
}

/* ScheduleFromLinux() -- CONFIRMED real: `sLinuxPending = 1;` (see
 * OutputThread's own comment for why this is never cleared again). */
void STGMidiOutPortKorgUsb_ScheduleFromLinux()
{
	sLinuxPending = 1;
}
