// SPDX-License-Identifier: GPL-2.0
/*
 * oa_keybed_init.h  -  CSTGKeybedInterface_Startup()/_Cleanup(): the
 * real init_module step 14 (hard-fail).
 *
 * Ground-truthed via readelf's symbol table against
 * ARCHIVE/Ignored/DecryptedImages/OA_real.ko (`CSTGKeybedInterface_
 * Startup`, `.text+0x33e5e0`, 319 bytes; `_Cleanup`, `.text+0x33e720`,
 * 30 bytes), then a full objdump disassembly + relocation trace.
 *
 * Confirms and extends what an earlier pass in this project already
 * established (MASTER_REFERENCE.md sec 10.36/10.40): this is OA.ko's
 * own real serial-port keybed handshake, scanning `CSTGComPort::
 * eComPortId` values 0-5 -- CORRECTED from an earlier "0-6" claim: the
 * real loop-exit check computes comPortId=6 but never actually calls
 * Initialize() with it (confirmed via the host KAT catching a real
 * off-by-one in an earlier draft's own C++ translation, then verifying
 * the true bound against the raw disassembly a second time) -- sending
 * a fixed probe byte (0xa5) and busy-waiting for a real keybed board's
 * ACK. INVERTED SUCCESS
 * CONVENTION (matches step 13): nonzero return = success.
 *
 * `CSTGKeybedInterface::sInstance` (confirmed via relocation) is used
 * throughout via DIRECT offset arithmetic on the symbol's own address
 * (`mov eax, sInstance+0x30` etc. -- no bracketed dereference anywhere
 * in this function), NOT loaded as a pointer's VALUE first -- unlike
 * `CSTGGlobal::sInstance`/`CSTGHeapManager::sInstance` elsewhere in
 * this project (real pointer variables, always dereferenced via `[..]`
 * before use). This means `CSTGKeybedInterface`'s singleton is a real
 * STATIC/GLOBAL OBJECT (its own storage IS what the `sInstance` symbol
 * names), not a pointer-to-heap-object -- modeled here as a fixed-size
 * byte blob with named offset constants (this project's established
 * convention for raw offset arithmetic into not-fully-recovered
 * structs), matching that storage model directly.
 *
 * `CSTGComPort` and its nested `TransmitFifo` sub-object are confirmed
 * to live within the same `CSTGKeybedInterface::sInstance` storage
 * (base @+0, `TransmitFifo` sub-object @+4) -- not a separate object.
 * `CSTGComPort::Cleanup`/`TriggerInterrupt`/`TransmitFifo::WriteByte`
 * are now fully reconstructed -- see `oa_comport.h`
 * (MASTER_REFERENCE.md sec 10.53). `Initialize` itself remains a
 * confirmed-real, deliberately deferred extern (that header's own
 * comment explains why).
 *
 * **CORRECTS a real bug in this file's own earlier revision**: these
 * four were originally declared as PLAIN C-linkage functions
 * (`CSTGComPort_Initialize` etc.) -- but the real relocations target
 * genuine MANGLED C++ methods, confirmed via `readelf`. A plain
 * C-linkage symbol of that name would never have matched the real one
 * at Kbuild link time. Fixed by including the real class declaration
 * and calling through it directly (see `keybed_init.cpp`'s own updated
 * call sites).
 */

#ifndef OA_KEYBED_INIT_H
#define OA_KEYBED_INIT_H

#include "oa_comport.h"

/* Fixed-size blob matching CSTGKeybedInterface::sInstance's own
 * storage. Sized to the highest confirmed offset this function
 * touches (+0xb50), rounded up. Only fields this function itself
 * reads/writes are named via offset constants. */
#define KEYBED_SINSTANCE_SIZE            0x0c00
#define KEYBED_OFF_DEBOUNCE_FILTER       0x0030 /* CSTGKeybedKeyDebounceFilter sub-object */
#define KEYBED_OFF_STATE                 0x0b4c /* 0=not started, 1=port open, 2=fully started,
						  * 3=calibration in progress, 4=calibration
						  * ending (confirmed real via
						  * StartCalibration/EndCalibration/
						  * CancelCalibration, batch 64) */
#define KEYBED_OFF_ACK_FLAG              0x0b50 /* cleared before each probe, set by the real ISR */

/*
 * Confirmed real (batch 64, keybed_interface.cpp): a 256-byte raw-message
 * ring buffer (`+0xa44`) with 16-bit write/read cursors (`+0xb44`/`+0xb46`,
 * wrapping mod 256 via `&0xff` on the byte cursor, NOT mod the 16-bit
 * cursor itself -- confirmed via the real disassembly's own `movzbl`
 * truncation on every buffer index). Fed by `WriteMessageToQueue` and
 * `ReceiveMessage`'s own inlined duplicate of the same logic; drained by
 * `ReadMessageFromQueue`. Full messages only -- a write that would not fit
 * in the remaining 0-255 byte window is dropped in its entirety (confirmed
 * real, matches `CSTGComPort::TransmitFifo::WriteBytes`'s own all-or-
 * nothing convention).
 */
#define KEYBED_OFF_MSG_QUEUE_BUF         0x0a44 /* 256-byte raw ring buffer */
#define KEYBED_OFF_MSG_QUEUE_WRITE       0x0b44 /* 16-bit write cursor */
#define KEYBED_OFF_MSG_QUEUE_READ        0x0b46 /* 16-bit read cursor */

/*
 * Confirmed real gate bytes (batch 64, `ReceiveMessage`'s own state==2
 * dispatch): `KEYBED_OFF_ENQUEUE_GATE1` (`+0xb51`) is checked FIRST when
 * `KEYBED_OFF_DISPATCH_GATE2` (`+0xb52`) is clear -- if BOTH are clear the
 * incoming raw message is silently dropped (neither enqueued nor
 * dispatched); if gate2 is set (and `CSTGMessageProcessor::sInstance+0x48`
 * is clear -- an entirely unmodeled external singleton in this project,
 * see keybed_interface.cpp's own comment) OR gate1 is set, the message is
 * pushed onto the raw ring buffer above. Own semantic meaning (plausibly
 * "message-processor owns the port" vs "raw passthrough enabled") not
 * independently confirmed -- see HARDWARE_REVIEW_LOG.md.
 */
#define KEYBED_OFF_ENQUEUE_GATE1         0x0b51
#define KEYBED_OFF_DISPATCH_GATE2        0x0b52

/*
 * Confirmed real "last raw analog value changed" flags (batch 64,
 * `FilterAnalogController`'s own `eSTGAnalogDeviceCode==1`/`==2` branches):
 * set to 1 whenever a non-centered (`!= 0x80`) raw analog byte for that
 * channel arrives; the OLD value gates whether that same call also runs
 * the full calibration-table filter pass. Plausibly joystick X/Y, given
 * `eSTGAnalogDeviceCode==0` (the third, ungated channel) is the aftertouch
 * pressure channel per `ApplyAftertouchTable`'s own confirmed 3-way
 * dispatch on the same `code` values.
 */
#define KEYBED_OFF_ANALOG1_CHANGED_FLAG  0x0b53
#define KEYBED_OFF_ANALOG2_CHANGED_FLAG  0x0b54

extern "C" unsigned char *CSTGKeybedInterface_sInstance(void);

extern "C" {

void CSTGKeybedKeyDebounceFilter_Initialize(unsigned char *filter);

void __const_udelay(unsigned long xloops);

int CSTGKeybedInterface_Startup(void);
void CSTGKeybedInterface_Cleanup(void);

/*
 * Batch 64 (keybed_interface.cpp) -- the remaining ~20 confirmed real
 * `CSTGKeybedInterface` member methods, modeled the SAME way as the pair
 * above: free C-linkage functions taking the `CSTGKeybedInterface::
 * sInstance` blob pointer explicitly (this project's established
 * convention for this singleton, NOT a real C++ class -- see this
 * header's own file comment), rather than real mangled-ABI methods.
 * `CSTGKeybedInterface::SetLED` (oa_setup_global_resources.h/
 * keybed_interface.cpp) is the sole exception, kept as a real class
 * method purely because `CSTGFrontPanel::SetLED`/etc (already
 * reconstructed, front_panel_handlers.cpp) call it by its real mangled
 * name.
 *
 * `CSTGKeybedInterface::Startup()`/`Cleanup()` (`.text+0x33d450`/
 * `0x33d420`) are CONFIRMED REAL, DISTINCT functions from the free
 * `CSTGKeybedInterface_Startup`/`_Cleanup` pair above (different
 * addresses, different bodies) -- not the same code reached two ways.
 * The free pair (init_module's own real boot-path caller) scans
 * `eComPortId` 0-5 each retry round; this member pair is hardcoded to
 * ALWAYS retry `eComPortId` 0 only (confirmed: the real disassembly
 * never reloads its `edx` comPortId argument to `CSTGComPort::
 * Initialize` across any of its 6 per-round retry iterations), 6
 * attempts/round x 10 rounds instead of the free pair's 6 ports x 10
 * rounds. No confirmed caller of this member pair exists anywhere in
 * this project (RE-CHECKED 2026-07-27, fresh survey): the one candidate
 * named here previously, `CSTGControlMsgHandler::TakeOverKeybedComm`, is
 * now reconstructed (`control_msg_handler.cpp`) and confirmed to do
 * something else entirely -- it just writes a one-byte enqueue-gate flag
 * (`KEYBED_OFF_ENQUEUE_GATE1`) on the free pair's own `sInstance` blob,
 * no call to `MemberStartup`/`MemberCleanup`/`TryComPort` anywhere in its
 * body. So this member pair remains genuinely dead code from this
 * project's own reachability analysis, exercised only by
 * `test_keybed_interface.cpp`'s mocks -- a real, still-open gap, not a
 * stale claim. Named `_MemberStartup`/`_MemberCleanup` here to avoid
 * colliding with the free pair's own established names.
 */
int CSTGKeybedInterface_MemberStartup(void);
void CSTGKeybedInterface_MemberCleanup(void);

/* TryComPort(eComPortId) -- single-attempt probe (Initialize + one 0xa5
 * handshake, ~2.6s total worst case: one 0x20c4ac udelay + 50x0x68dbc
 * udelay) on ONE specific port, no outer retry loop. Confirmed real,
 * `.text+0x33d590`, 208 bytes. Returns nonzero on ACK (same INVERTED
 * SUCCESS CONVENTION as the free Startup pair). */
int CSTGKeybedInterface_TryComPort(int comPortId);

/*
 * Calibration control triad -- all three send a fixed 2-byte command
 * (`{0xc0,controller}`/`{0xc1,0}`/`{0xc2,0}`) and gate on
 * `KEYBED_OFF_STATE`: StartCalibration requires state 2 (idle/running)
 * and transitions to state 3; End/CancelCalibration both require state
 * 3 (calibration in progress) and transition to state 4. `eKeybedController`
 * not independently enumerated -- passed as a raw byte, matching this
 * project's own established `CSTGComPort::eComPortId` treatment.
 */
void CSTGKeybedInterface_StartCalibration(unsigned int controller);
void CSTGKeybedInterface_EndCalibration(void);
void CSTGKeybedInterface_CancelCalibration(void);

/* Key-check (diagnostic "which key is this" front-panel mode) -- both
 * require state==2 (exactly, unlike SetLED's `>=2`), send a fixed 2-byte
 * command (`{0x90,0x7f}`/`{0x80,0x7f}`), no state transition. */
void CSTGKeybedInterface_EnterKeyCheckMode(void);
void CSTGKeybedInterface_ExitKeyCheckMode(void);

/* SendByte -- gated (state==2 exactly) single raw byte send, used by
 * `CSTGControlMsgHandler::SendKeybedByte` (not yet reconstructed in this
 * project). Distinct from the two UNGATED `SendCommand` overloads below,
 * which are this class's own internal command-building primitive. */
void CSTGKeybedInterface_SendByte(unsigned char value);

/* SendCommand(unsigned char)/SendCommand(unsigned char const*, unsigned
 * int) -- UNGATED raw byte/buffer send via `TransmitFifo::WriteByte`/
 * `WriteBytes`, triggering the UART TX interrupt only when the FIFO was
 * previously empty (matches every other command-sender in this class).
 * No state check -- this class's own lowest-level send primitive, used
 * internally by the calibration/key-check/LED/USB/rear-LED command
 * builders above (each of which builds its own fixed command bytes
 * inline rather than calling through these, confirmed via each one's own
 * direct `WriteBytes` call -- these two are their own independent,
 * separately-confirmed real methods, not merely a shared helper). */
void CSTGKeybedInterface_SendCommandByte(unsigned char cmd);
void CSTGKeybedInterface_SendCommandBuf(const unsigned char *buf, unsigned int len);

void CSTGKeybedInterface_SetKeyChatterGateTime(unsigned int ms);

/*
 * WriteMessageToQueue/ReadMessageFromQueue -- producer/consumer pair for
 * the raw-message ring buffer (`KEYBED_OFF_MSG_QUEUE_*`). `ReceiveMessage`
 * (keybed_receive.cpp)'s own state==2 dispatch inlines the SAME enqueue
 * logic as `WriteMessageToQueue` (confirmed via disassembly -- byte-for-
 * byte identical body, most likely compiler-inlined from a real call to
 * this same method) -- reused directly rather than duplicated.
 * `ReadMessageFromQueue` returns the number of bytes copied into
 * `outBuf` (0 if the queue was empty, or if the header byte's own
 * message-type lookup came back 0).
 */
void CSTGKeybedInterface_WriteMessageToQueue(const unsigned char *buf, unsigned int len);
unsigned char CSTGKeybedInterface_ReadMessageFromQueue(unsigned char *outBuf);

/* ReceiveStartupMessage -- confirmed real, `.text+0x33dbf0`, 64 bytes:
 * the handshake-ACK-only SUBSET of `ReceiveMessage`'s own state==1
 * branch (same `(buf[0]&0xf0)==0xa0` check, same `STGAPI_OFF_
 * KEYBED_STATUS16`/`KEYBED_OFF_ACK_FLAG` writes), with NO state gate at
 * all -- runs unconditionally regardless of `KEYBED_OFF_STATE`. No
 * confirmed caller in this project yet. */
void CSTGKeybedInterface_ReceiveStartupMessage(const unsigned char *buf);

/* HandleActiveSense -- confirmed real, `.text+0x33e470`, 160 bytes: the
 * STANDALONE version of `ReceiveMessage`'s own inlined `0xE0-0xEF`
 * ("type 6" idle-heartbeat class per [[kronos_keybed_serial_protocol]])
 * dispatch -- byte-for-byte identical body. `ReceiveMessage`'s own
 * heartbeat handling (keybed_receive.cpp) calls THIS function directly
 * rather than re-deriving the same logic a second time. */
void CSTGKeybedInterface_HandleActiveSense(const unsigned char *buf);

/* ApplyAftertouchTable/ApplyCalibrationAndAfterTouchTable -- both
 * dispatch on `STGAPI_OFF_NKS4_PANEL_KIND` (0/1/2+) across the SAME
 * three confirmed real 256-byte `.rodata` lookup tables (`0xa8600`/
 * `0xa8700`/`0xa8800` -- keybed_interface.cpp's own `kAftertouchTable0/
 * 1/2`), most plausibly per-keybed-model aftertouch response curves.
 * `ApplyCalibrationAndAfterTouchTable` additionally pre-filters through
 * `ApplyKeybedCalibration` (fixed `code`=7) and rescales the table
 * result to a wider ~0-1023 range via the same `/255`-magic-number
 * fixed-point divide used throughout this class (see
 * `FilterAnalogController`'s own matching idiom). */
unsigned char CSTGKeybedInterface_ApplyAftertouchTable(unsigned char raw);
short CSTGKeybedInterface_ApplyCalibrationAndAfterTouchTable(short rawAnalog);

/*
 * FilterAnalogController -- confirmed real, `.text+0x33e370`, 256 bytes.
 * `code`==1/2 (plausibly joystick X/Y) each track their own "changed"
 * flag (`KEYBED_OFF_ANALOG1/2_CHANGED_FLAG`) and only run the full
 * calibration filter once a non-centered reading has been seen twice in
 * a row (first non-centered call just arms the flag and reports
 * "unchanged"); `code`==0 (plausibly aftertouch pressure) always
 * filters, gated only on `STGAPI_OFF_NKS4_HW_VERSION==3`. All three
 * paths funnel into the same `ApplyKeybedCalibration(code, scaledRaw)`
 * call, writing the calibrated result back into `*value` UNLESS the
 * real function returns its `0xffff` "no calibration data" sentinel (in
 * which case `*value` is left untouched and `false` is returned).
 * Returns true if `*value` was updated.
 */
bool CSTGKeybedInterface_FilterAnalogController(unsigned int code, unsigned char *value);

/* EnableUSBPort/EnableRearLED -- both gated on state>=2 (`SetLED`'s own
 * `<=1` bail), send a fixed 3-byte command (`{0xb8,port,enable}`/
 * `{0xb9,0,enable}`). EnableUSBPort additionally rejects `port>1`
 * (confirmed 2-port hardware) with no send at all. */
void CSTGKeybedInterface_EnableUSBPort(int port, bool enable);
void CSTGKeybedInterface_EnableRearLED(bool enable);

/*
 * Confirmed real (`.text+0x33edd0`), NOT reconstructed in this pass --
 * deliberately deferred, same "confirmed real, defer the body" treatment
 * as `CSTGComPort::Initialize` before it got one. Reads a pair of
 * `.bss` globals (`SetupKeybedCalibration`/`CleanupKeybedCalibration`,
 * themselves confirmed trivial but NOT members of this class -- plain
 * global-scope C-ish functions, out of this pass's scope) and, if both
 * are set, does a real KERNEL-MODE FPU CONTEXT SWITCH (`mov %cr0,%eax;
 * clts; ...`) before an interpolation pass -- genuinely disproportionate
 * new infrastructure for this pass (an FPU save/restore is a real
 * kernel-hardware concern, not wire-protocol I/O, and needs a
 * calibration-table struct never seen elsewhere in this project). Takes
 * (code, rawValue), returns a calibrated 16-bit value or the confirmed
 * `0xffff` "no data" sentinel.
 */
short ApplyKeybedCalibration(int code, short rawValue) __attribute__((regparm(3)));

} /* extern "C" */

#endif /* OA_KEYBED_INIT_H */
