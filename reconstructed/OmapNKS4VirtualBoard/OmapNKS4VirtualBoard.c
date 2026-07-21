// SPDX-License-Identifier: GPL-2.0
/*
 * OmapNKS4VirtualBoard.c  -  a genuine virtual NKS4 front-panel USB device,
 * built for the same VM/QEMU-TCG boot-testing environment as
 * RTAIVirtualDriver.ko/AT88VirtualChip.ko/KorgUsbAudioVirtualDriver.ko.
 *
 * WHAT THIS IS AND WHY IT'S DIFFERENT FROM EVERY OTHER "VIRTUAL" MODULE IN
 * THIS PROJECT
 * -------------------------------------------------------------------------
 * Every other "VirtualX" module in this project (AT88VirtualChip.ko,
 * KorgUsbAudioVirtualDriver.ko, OmapNKS4VirtualDriver.ko) works by
 * supplying the EXPORT_SYMBOLs a real caller needs, standing in for a real
 * module's own exports. That pattern doesn't apply here: the real
 * OmapNKS4Module.ko is not a library of exported symbols something else
 * calls into for its own device access - it is ITSELF a USB HOST driver
 * (`stg_usb_register_driver`, a thin STGEnabler.ko shim over the real
 * kernel's own `usb_register_driver()`). Its own `OmapNKS4Init()` blocks in
 * `wait_for_completion_timeout(sProbeComplete, 10000)` waiting for the
 * Linux USB core to call its registered `.probe` callback
 * (`OmapNKS4Probe`) - which only happens when a REAL USB DEVICE matching
 * its ID table (vendor 0x0944, product 0x1005, confirmed real values, see
 * usb.cpp's own `OmapNKS4Probe`) actually enumerates. No amount of
 * providing extra exported symbols can substitute for that - the missing
 * piece is a genuine USB DEVICE for the real module's own host-side driver
 * to talk to.
 *
 * This module IS that device - a real Linux USB gadget driver, presenting
 * the exact vendor/product ID and endpoint layout `OmapNKS4Probe`'s own
 * confirmed real logic checks for (usb.cpp: idVendor/idProduct at
 * dev+0xb8/+0xba; endpoint classification by bmAttributes&3==3+IN=interrupt,
 * bmAttributes&3==2+OUT=bulk). Loaded together with `dummy_hcd.ko` (Linux's
 * own loopback USB host+device controller, built from this project's own
 * `/home/build/linux-kronos` tree with `CONFIG_USB_GADGET=m`/
 * `CONFIG_USB_DUMMY_HCD=m` newly enabled - both purely additive kernel
 * config changes, no ABI-relevant option touched) in the SAME kernel
 * instance, `dummy_hcd` loops this gadget's own device-side traffic back to
 * its own virtual USB HOST controller - the real `OmapNKS4Module.ko`,
 * bound to that virtual host controller exactly as it would be to a real
 * PC's own EHCI/OHCI/UHCI controller, sees a real device enumerate and
 * calls `OmapNKS4Probe` for real. No wire-protocol guesswork stands between
 * the two: this is genuine Linux USB core code running the genuine
 * enumeration/probe path on both sides.
 *
 * SCOPE: enumeration + `OmapNKS4Probe`'s own descriptor checks succeed (so
 * `OmapNKS4Init`'s own `wait_for_completion_timeout` observes a real
 * completion instead of timing out) - achieved and boot-tested, see
 * `OmapNKS4DummyHCDFix/README.md`'s "Twelfth pass". Also now implements the
 * 3 specific `COmapNKS4Command` query/response pairs
 * `COmapNKS4Driver_Configure()` itself waits on (CommunicationCheck,
 * ReadPortConfiguration, GetVersion - see the completion handlers' own
 * header comment below for the ground truth), so `Configure()`'s full
 * 9-call sequence can run. Full, general wire-protocol fidelity (every
 * other real `COmapNKS4Command` word, genuine runtime panel events -
 * button/knob/pedal state matching the real NKS4 ARM firmware's own
 * confirmed behavior from this project's `NKS4PanelFirmware/`
 * reconstruction) is still a natural follow-on, not attempted here.
 * Honestly scoped rather than claimed complete.
 *
 * Build: kernel-only Kbuild module, plain C (matches every other virtual
 * driver's own precedent for avoiding C++ against this ancient kernel's
 * headers). Needs `dummy_hcd.ko` loaded first (provides
 * `usb_gadget_register_driver`/`usb_gadget_unregister_driver`, confirmed
 * exported symbols in this kernel's build, not a separate "gadget core"
 * module in this kernel era). See Makefile / README.md.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>

/* BUG FIX (2026-07-19, dummy_hcd full-chain boot test): usb_ep_autoconfig() is not an
 * exported kernel symbol on this kernel version - it lives in epautoconf.c, which every
 * real Linux gadget driver of this vintage (confirmed against this exact kernel tree's
 * own drivers/usb/gadget/zero.c: `#include "usbstring.c"`/`"epautoconf.c"`) compiles
 * directly into its own module via a source-level #include, not a shared/linked symbol -
 * this project's own reconstruction was missing that and failed insmod with "Unknown
 * symbol usb_ep_autoconfig" the first time it was actually boot-tested against real
 * usb_gadget/dummy_hcd core (previous testing only exercised the vm_virtual_probe
 * in-process shortcut, which never needed this). epautoconf.c and its own
 * "gadget_chips.h" dependency are copied verbatim (GPL, unmodified) from
 * /home/build/linux-kronos/drivers/usb/gadget/ into this directory rather than included
 * via a kernel-tree-relative path, so this module keeps building standalone against
 * KDIR without depending on exact kernel-source-tree layout at include time. Only
 * usb_ep_autoconfig() itself is used below (no usb_string()/config-buf helpers), so
 * usbstring.c/config.c were not needed. */
#include "epautoconf.c"

#define NKS4_VENDOR_ID   0x0944	/* confirmed real value, usb.cpp OmapNKS4Probe */
#define NKS4_PRODUCT_ID  0x1005	/* confirmed real value, usb.cpp OmapNKS4Probe */

/* Endpoint addresses are this gadget's own choice (any host-visible
 * IN/OUT pair with the right transfer type satisfies OmapNKS4Probe's own
 * classification logic, which reads bmAttributes/bEndpointAddress from
 * whatever the host's own enumeration returns - it doesn't hardcode a
 * specific endpoint NUMBER, only IN+INTERRUPT and OUT+BULK). Chosen to
 * avoid colliding with control endpoint 0.
 */
#define NKS4_EP_INT_ADDR   0x81	/* EP1 IN, interrupt */
#define NKS4_EP_BULK_ADDR  0x02	/* EP2 OUT, bulk */
#define NKS4_EP_MAXPACKET  64		/* full-speed bulk/interrupt max, real NKS4 wMaxPacketSize not independently confirmed - see README */

struct nks4_dev {
	struct usb_gadget	*gadget;
	struct usb_ep		*ep_int;
	struct usb_ep		*ep_bulk;
	struct usb_request	*req_ep0;
	struct usb_request	*req_bulk_out;
	struct usb_request	*req_int_in;
	u8			config_value;
	struct timer_list	event_timer;	/* periodic synthetic runtime events, see below */
	int			int_in_busy;	/* best-effort guard: req_int_in already queued */
	int			replies_sent;	/* count of the 3 known Configure() command replies actually sent */
};

static struct nks4_dev *nks4;

/* ========================================================================= *
 *  USB descriptors - real, confirmed values where this project has ground
 *  truth (idVendor/idProduct, interface class), reasonable/documented
 *  choices elsewhere (flagged inline, not silently presented as ground
 *  truth). bDeviceClass=0/bDeviceSubClass=0/bDeviceProtocol=0 (class info
 *  lives at the INTERFACE, matching usb.cpp's own probe logic, which never
 *  reads the device descriptor's class fields).
 * ========================================================================= */

static struct usb_device_descriptor nks4_device_desc = {
	.bLength		= USB_DT_DEVICE_SIZE,
	.bDescriptorType	= USB_DT_DEVICE,
	.bcdUSB			= cpu_to_le16(0x0110),	/* USB 1.1 full-speed, matching dummy_hcd's own default speed */
	.bDeviceClass		= 0,
	.bDeviceSubClass	= 0,
	.bDeviceProtocol	= 0,
	.bMaxPacketSize0	= 64,
	.idVendor		= cpu_to_le16(NKS4_VENDOR_ID),
	.idProduct		= cpu_to_le16(NKS4_PRODUCT_ID),
	.bcdDevice		= cpu_to_le16(0x0100),
	.iManufacturer		= 0,
	.iProduct		= 0,
	.iSerialNumber		= 0,
	.bNumConfigurations	= 1,
};

/* Interface class/subclass/protocol: confirmed real values, see
 * OmapNKS4Module/main.cpp's own struct usb_device_id reconstruction
 * (ground truth: read_memory @0x1b040 of the real shipped binary,
 * cross-checked against linux-kronos's real struct usb_device_id layout):
 * match_flags=0x0383 (VENDOR|PRODUCT|INT_CLASS|INT_SUBCLASS|INT_PROTOCOL),
 * bInterfaceClass=0xff, bInterfaceSubClass=0xff, bInterfaceProtocol=0x00.
 *
 * BUG FIX (2026-07-20, OmapNKS4Probe still not firing after config-descriptor
 * fix): bInterfaceSubClass was left at 0 here, but match_flags has
 * USB_DEVICE_ID_MATCH_INT_SUBCLASS (0x0100) set, meaning the real driver's
 * id_table requires an EXACT match on bInterfaceSubClass, not just
 * bInterfaceClass - the real value is 0xff, not 0. With this set to 0, the
 * kernel's own usb_match_one_id() rejects the interface before
 * OmapNKS4Probe() is ever called, regardless of how correctly the device
 * enumerates otherwise - fully explains clean enumeration + probe timeout.
 * See OmapNKS4DummyHCDFix/README.md's "Twelfth pass" section. */
static struct usb_interface_descriptor nks4_intf_desc = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bInterfaceNumber	= 0,
	.bAlternateSetting	= 0,
	.bNumEndpoints		= 2,
	.bInterfaceClass	= 0xff,
	.bInterfaceSubClass	= 0xff,
	.bInterfaceProtocol	= 0,
	.iInterface		= 0,
};

static struct usb_endpoint_descriptor nks4_int_ep_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= NKS4_EP_INT_ADDR,
	.bmAttributes		= USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize		= cpu_to_le16(NKS4_EP_MAXPACKET),
	.bInterval		= 10,	/* 10ms polling, real NKS4 interval not independently confirmed */
};

static struct usb_endpoint_descriptor nks4_bulk_ep_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= NKS4_EP_BULK_ADDR,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize		= cpu_to_le16(NKS4_EP_MAXPACKET),
};

static struct usb_config_descriptor nks4_config_desc = {
	.bLength		= USB_DT_CONFIG_SIZE,
	.bDescriptorType	= USB_DT_CONFIG,
	.wTotalLength		= 0,	/* filled in at bind time */
	.bNumInterfaces		= 1,
	.bConfigurationValue	= 1,
	.iConfiguration		= 0,
	.bmAttributes		= USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower		= 50,	/* 100mA */
};

/* ========================================================================= *
 *  COmapNKS4Command wire protocol - enough to satisfy
 *  COmapNKS4Driver_Configure()'s own query/response pairs (the only calls
 *  in its 9-call sequence that wait for a reply; the other 6 are bulk-OUT-
 *  only setters already satisfied by this file's existing generic re-queue).
 *
 *  Ground truth, independently verified against both sides of the real
 *  protocol (not copied from a single source):
 *  - Request words + expected response tags: OmapNKS4Module/command.cpp
 *    (CommunicationCheck=0x00ee0000/tag 0x0066, ReadPortConfiguration=
 *    0x01f10000/tag 0x0171, GetVersion=0x00f00000/tag 0x0070).
 *
 *  EXTENDED (2026-07-20, TODO.md "wire-protocol depth" item): this gadget's
 *  fixed ReadPortConfiguration reply reports hwVer=1, so
 *  COmapNKS4Driver_Configure() (driver.cpp) always takes its `else` branch
 *  and calls the single 4-out-argument GetVersion() overload (word
 *  0x00f00000) exactly once - the original 3-entry table above was
 *  therefore sufficient for every command word this gadget's own
 *  configuration path can actually produce. But `GetVersion(int index, ...)`
 *  (command.cpp:48), used by the hwVer==2 and hwVer==3 branches
 *  (driver.cpp:792-811, `GetVersion(1..3, ...)`), sends `0x00f00000 |
 *  (index<<8)` - a DIFFERENT command word per index (0x00f00100/0x00f00200/
 *  0x00f00300) that the original table did not recognize at all, so those
 *  calls would silently fall through to the generic bulk-OUT re-queue with
 *  no interrupt-IN reply ever sent, hanging `WaitForNKS4ReadEvent()`
 *  indefinitely and failing Configure() on any hwVer other than 1. Added
 *  entries for all three below (synthetic-but-plausible version/revision
 *  bytes, matching this project's own precedent for the original
 *  GetVersion entry's synthetic 0x00701805 value) so the gadget answers the
 *  full real command surface `command.cpp` defines, not only the specific
 *  subset this gadget's own default hwVer=1 configuration happens to need.
 *  `dev->replies_sent`'s "all replies sent, arm the runtime-event timer"
 *  gate (below) is deliberately NOT widened to include these - it still
 *  counts only the 3 replies the CURRENT hwVer=1 path actually sends; see
 *  `NKS4_CONFIG_REPLY_COUNT` below.
 *  - Wire byte order for the bulk-OUT command word: OmapNKS4Module/
 *    submit.cpp's own header comment on submit_urb_words() - CONFIRMED ON
 *    REAL HARDWARE that the command-word path sends words RAW (no byte
 *    reversal), i.e. plain x86 little-endian, so a straight u32 read of the
 *    first 4 bytes matches the sent word with no byte-swapping needed.
 *  - Interrupt-IN response record format ([dLo][dHi][idx][op], terminated
 *    by a Sync record [0][0][0][0x87]): OmapNKS4Module/driver.cpp's
 *    ReceiveEventBuffer() decode loop, and independently corroborated by
 *    usb.cpp's own already-proven vm_virtual_probe_inject_event() self-test
 *    packet, which uses the identical shape.
 *  - The 3 specific reply records below were derived by hand-tracing
 *    ReceiveEventBuffer()'s own op==0 (analog/CC, idx==0x66/0x70) and op==1
 *    idx==0x71 (ReadPortConfiguration's own special-cased branch, which ORs
 *    in the opcode explicitly rather than using the generic word) paths to
 *    the exact dLo/dHi/idx/op values that produce command.cpp's expected
 *    resp values (0x00660000/0x01710001/0x00701805 - the last two matching
 *    OMAP V01 R08 / PSoC V00 R05 and hwVer=1/is88=0, both confirmed real
 *    values cited in driver.cpp's own comments) - not copied from any single
 *    table, cross-checked against both files' real logic directly.
 * ========================================================================= */

struct nks4_cmd_reply {
	u32 cmd;
	unsigned char record[4];	/* [dLo][dHi][idx][op] */
};

static const unsigned char nks4_sync_record[4] = { 0x00, 0x00, 0x00, 0x87 };

static const struct nks4_cmd_reply nks4_replies[] = {
	{ 0x00ee0000, { 0x00, 0x00, 0x66, 0x00 } },	/* CommunicationCheck -> resp 0x00660000 */
	{ 0x01f10000, { 0x01, 0x00, 0x71, 0x01 } },	/* ReadPortConfiguration -> resp 0x01710001 (hwVer=1, is88=0) */
	{ 0x00f00000, { 0x05, 0x18, 0x70, 0x00 } },	/* GetVersion() 4-out overload -> resp 0x00701805 (OMAP V01 R08 / PSoC V00 R05) */
	/* GetVersion(index, ...) overload, indexed variants - only reachable if
	 * ReadPortConfiguration's hwVer response above is ever changed to 2 or 3
	 * (unreachable, thus untested end-to-end, under this gadget's current
	 * fixed hwVer=1 reply) - see header comment above. */
	{ 0x00f00100, { 0x01, 0x01, 0x70, 0x00 } },	/* GetVersion(1) -> resp 0x00700101 (PanelL/PSoC V01 R01) */
	{ 0x00f00200, { 0x01, 0x01, 0x70, 0x00 } },	/* GetVersion(2) -> resp 0x00700101 (PanelR V01 R01) */
	{ 0x00f00300, { 0x01, 0x01, 0x70, 0x00 } },	/* GetVersion(3) -> resp 0x00700101 (Jack V01 R01) */
};

/* Config-time replies the gadget's CURRENT fixed ReadPortConfiguration
 * response (hwVer=1) actually causes Configure() to wait for: CommunicationCheck,
 * ReadPortConfiguration, and the single 4-out-arg GetVersion() call - see the
 * "EXTENDED" header comment above. Deliberately NOT sizeof(nks4_replies)/... -
 * the indexed GetVersion entries added above are only reachable on a hwVer
 * this gadget never actually reports, and must not block the runtime-event
 * timer from arming. */
#define NKS4_CONFIG_REPLY_COUNT 3

/* ========================================================================= *
 *  Periodic synthetic RUNTIME panel events - beyond the 3 configuration-time
 *  command replies above, exercises ReceiveEventBuffer()'s other opcode
 *  branches (analog/CC, button, aftertouch, rotary encoder, S/PDIF status)
 *  so the real driver's full runtime event-decode path gets real coverage,
 *  not just the config-time handshake.
 *
 *  BUTTON EVENTS ARE NOW REAL, CONFIRMED, NAMED TRAFFIC - not a placeholder.
 *  The wire-idx -> physical-button translation (OA.ko's own
 *  CSTGOmapNKSMsgHandler::ProcessNextNKSEvent(), previously untraced
 *  anywhere in this project) was traced this pass via direct disassembly of
 *  the real OA.ko binary (/tmp/oa_real_disasm.txt, function at 0x2073d0 in
 *  that file's own addressing). Confirmed instruction sequence right before
 *  the real CSTGFrontPanel::HandleSwitchEvent(eSTGButtonCode,bool) call
 *  (regparm(3): EAX=this, EDX=code, CL=pressed - same ABI
 *  KronosScreenRemoteDaemon/nks4_inject_module/nks4_inject.c's own header
 *  independently reconstructed):
 *    movzbl 0x2a(%esp),%eax   ; eax = wire idx byte
 *    and    $0xf0,%eax        ; hi nibble
 *    cmp    $0x30,%eax / cmp $0x40,%eax   ; only these two reach HandleSwitchEvent
 *    cmp    $0x30,%eax; sete %al          ; al = 1 if idx&0xf0==0x30, else 0 (0x40 case)
 *    movzbl 0x29(%esp),%ebx   ; ebx = wire dHi byte
 *    mov    %eax,%esi         ; esi = press/release flag
 *    ... (COmapNKS4Driver_GetTestMode() check, non-test-mode path taken) ...
 *    and    $0x7f,%ebx        ; ebx = dHi & 0x7f  <- this becomes EDX (code)
 *    mov    %esi,%eax ; movzbl %al,%ecx  <- this becomes ECX (pressed)
 *    call   HandleSwitchEvent(this, code=dHi&0x7f, pressed=(idx&0xf0==0x30))
 *  I.e., for a real button press/release: op=0x00, dLo=don't-care (unused
 *  in this path), dHi=the real scan code (0-127, from nks4_inject.c's own
 *  hardware-confirmed btn_table[] - e.g. EXIT=8, HELP=9, ENTER=23),
 *  idx=0x30 for PRESS or 0x40 for RELEASE (low nibble of idx not checked by
 *  this path). This is now a confirmed, disassembly-verified bridge between
 *  the two previously-separate reference layers this project had
 *  (ReceiveEventBuffer's wire decode and nks4_inject.c's real button
 *  names) - see OmapNKS4DummyHCDFix/README.md's "Sixteenth pass".
 *
 *  ROTARY AND ANALOG (incl. AFTERTOUCH/KNOBS) ARE ALSO NOW REAL, CONFIRMED,
 *  NAMED TRAFFIC, traced the same way (direct ProcessNextNKSEvent
 *  disassembly). Two real corrections to this file's own PRIOR comments,
 *  found by disassembly rather than assumed from ReceiveEventBuffer's own
 *  inline labels:
 *  - What ReceiveEventBuffer calls "op==1/idx&0xf0==0x50, button/key" is
 *    NOT a button in ProcessNextNKSEvent - confirmed disassembly shows this
 *    exact shape reaches `CSTGFrontPanel::HandleRotary(this, delta)` with
 *    `delta = dHi<<8 | dLo` (EDX at the call site), matching
 *    nks4_inject.c's own documented HandleRotary ABI exactly.
 *  - What ReceiveEventBuffer calls "op==0x1f, rotary encoder" does NOT reach
 *    HandleRotary at all in ProcessNextNKSEvent - it goes to a diagnostic
 *    PushUnsolicitedMessage path instead (confirmed via the same decompile/
 *    disassembly pass).
 *  - The real analog-controller dispatch (op==3) calls
 *    `HandleAnalogController(this, device_code, param2, param3)` with
 *    **device_code = wire idx & 0x3f** (confirmed disassembly: `and
 *    $0x3f,%edx` on the idx byte immediately before the call) - directly
 *    the SAME device_code space nks4_inject.c's own header already
 *    documents (7=Aftertouch, 8-15=RT Knobs 1-8, 16-23=Sliders 1-8, 27=rear
 *    Pedal jack, 29=Damper, etc., most HARDWARE-CONFIRMED there). `param2`/
 *    `param3` come from `ShortInvertNkS4AnalogValue(dLo, dHi)`, itself now
 *    also disassembled (0x2077f0 in /tmp/oa_real_disasm.txt): it builds a
 *    raw10-style value from the two bytes (bit shape closely matches this
 *    project's own already-established `raw10 = dLo<<2 | dHi>>6` convention
 *    documented for aftertouch calibration elsewhere), then conditionally
 *    inverts it around 0x3ff unless the raw value is exactly the center
 *    point 0x200. This confirms the transform's SHAPE (extract-then-
 *    conditionally-invert), but the exact resulting numeric semantics (what
 *    specific dLo/dHi pair produces what displayed reading) were not fully
 *    derived - the events below confirm the real DISPATCH TARGET (which
 *    device_code fires, disassembly-verified) and the transform's general
 *    shape, but not a claim about a specific meaningful displayed value.
 *
 *  S/PDIF status (op==7) needs no OA.ko-side confirmation - disassembly of
 *  ReceiveEventBuffer itself already shows this is handled entirely inside
 *  OmapNKS4Module.ko (`sInstance.fSpdifClockError = idx & 1`), no dispatch
 *  to OA.ko at all.
 *
 *  See OmapNKS4DummyHCDFix/README.md's "Fourteenth"/"Sixteenth"/
 *  "Seventeenth pass" sections for the full research trail.
 * ========================================================================= */

#define NKS4_EVENT_PERIOD_JIFFIES (2 * HZ)
/* Generous settle time after the 3rd (last) real command reply is sent,
 * before the FIRST synthetic runtime event fires - gives OmapNKS4Init()'s
 * own post-Configure() bring-up (interrupt-URB resubmit, worker-thread
 * startup) a wide margin to finish before this gadget starts sending
 * unsolicited interrupt-IN traffic. See README.md's "Fifteenth pass": a
 * fixed delay measured from SET_CONFIGURATION (not from here) was not a
 * reliable proxy for this and caused a real hang. */
#define NKS4_EVENT_SETTLE_JIFFIES (60 * HZ)

#define NKS4_BTN_EXIT 8		/* nks4_inject.c btn_table[]: real, hardware-confirmed EXIT scan code */
/* nks4_inject.c's own eSTGAnalogDeviceCode reference, real device_code values
 * (device_code = wire idx & 0x3f, confirmed this pass - see file header):
 * 7=Aftertouch (HARDWARE-CONFIRMED), 8=RT Knob 1 (HARDWARE-CONFIRMED). */
#define NKS4_DEV_AFTERTOUCH 7
#define NKS4_DEV_RT_KNOB1   8

static const unsigned char nks4_generic_events[][4] = {
	/* op=1, idx&0xf0==0x50: REAL rotary encoder (HandleRotary, delta=dHi<<8|dLo).
	 * ReceiveEventBuffer's own inline comment calls THIS shape "button/key" -
	 * disassembly of ProcessNextNKSEvent (this pass) shows it actually reaches
	 * HandleRotary, not HandleSwitchEvent. delta=0x0100 (CW), matching
	 * nks4_inject.c's own documented example value. */
	{ 0x00, 0x01, 0x50, 0x01 },
	{ 0x00, NKS4_BTN_EXIT, 0x30, 0x00 },		/* op=0  REAL button PRESS:   dHi=EXIT(8), idx=0x30 - disassembly-confirmed bridge, see file header */
	{ 0x00, NKS4_BTN_EXIT, 0x40, 0x00 },		/* op=0  REAL button RELEASE: dHi=EXIT(8), idx=0x40 - same bridge */
	/* op=3: REAL analog controller (HandleAnalogController, device_code=idx&0x3f).
	 * param2/param3 (derived from dLo/dHi here via ShortInvertNkS4AnalogValue,
	 * now disassembly-confirmed as a raw10-extract-then-conditionally-invert
	 * transform - see file header) go through that transform before reaching
	 * the handler. For device_code=7 (Aftertouch), the DISPATCH target is
	 * confirmed by direct disassembly, but nks4_inject.c itself flags this
	 * specific device group as "NOT hardware-tested" for any value-scaling
	 * formula, so no displayed-value claim is made for this entry. */
	{ 0x80, 0x02, NKS4_DEV_AFTERTOUCH, 0x03 },
	/* op=3  REAL RT Knob 1 (device_code=8). dLo=0x40,dHi=0x00 -> raw10=0x100
	 * -> param2(out_hi)=32 (computed via ShortInvertNkS4AnalogValue, this
	 * pass's own disassembly). nks4_inject.c's own header states this exact
	 * device group (RT Knobs/Sliders/Value Slider) is HARDWARE-CONFIRMED with
	 * a byte0=value*2 scaling formula - applying it here predicts a displayed
	 * value of 16. This prediction itself was not re-verified against real
	 * hardware this pass (would need a live comparison), but it is a
	 * concrete, derived value rather than an arbitrary one, unlike the
	 * Aftertouch entry above. */
	{ 0x40, 0x00, NKS4_DEV_RT_KNOB1, 0x03 },
	{ 0x01, 0x00, 0x00, 0x07 },			/* op=7  S/PDIF status: clock-error flag toggle - real, handled entirely inside OmapNKS4Module.ko, no OA.ko dispatch needed */
};

static void nks4_event_timer_fn(unsigned long data)
{
	struct nks4_dev *dev = (struct nks4_dev *)data;
	static unsigned int next = 0;
	unsigned char *ibuf;
	int qret;

	if (!dev || dev->config_value != 1 || !dev->req_int_in || !dev->req_int_in->buf)
		goto rearm;

	if (dev->int_in_busy)
		goto rearm;	/* best-effort: skip this tick rather than double-queue */

	ibuf = dev->req_int_in->buf;
	memcpy(ibuf, nks4_generic_events[next], 4);
	memcpy(ibuf + 4, nks4_sync_record, 4);
	dev->req_int_in->length = 8;
	dev->int_in_busy = 1;
	qret = usb_ep_queue(dev->ep_int, dev->req_int_in, GFP_ATOMIC);
	printk(KERN_INFO "OmapNKS4VirtualBoard: synthetic runtime event [%02x %02x %02x %02x] qret=%d\n",
	       nks4_generic_events[next][0], nks4_generic_events[next][1],
	       nks4_generic_events[next][2], nks4_generic_events[next][3], qret);
	if (qret)
		dev->int_in_busy = 0;

	next = (next + 1) % ARRAY_SIZE(nks4_generic_events);

rearm:
	mod_timer(&dev->event_timer, jiffies + NKS4_EVENT_PERIOD_JIFFIES);
}

static void nks4_ep0_complete(struct usb_ep *ep, struct usb_request *req)
{
	/* nothing to do: only used for zero-length status-stage acks */
}

static void nks4_bulk_out_complete(struct usb_ep *ep, struct usb_request *req)
{
	if (req->status == 0 && req->actual >= 4) {
		u32 cmd = *(u32 *)req->buf;
		struct nks4_dev *dev = nks4;
		size_t i;

		printk(KERN_INFO "OmapNKS4VirtualBoard: bulk-OUT command word 0x%08x\n", cmd);

		for (i = 0; i < sizeof(nks4_replies) / sizeof(nks4_replies[0]); i++) {
			if (nks4_replies[i].cmd != cmd)
				continue;
			if (dev && dev->req_int_in && dev->req_int_in->buf && !dev->int_in_busy) {
				unsigned char *ibuf = dev->req_int_in->buf;
				int qret;

				memcpy(ibuf, nks4_replies[i].record, 4);
				memcpy(ibuf + 4, nks4_sync_record, 4);
				dev->req_int_in->length = 8;
				dev->int_in_busy = 1;
				qret = usb_ep_queue(dev->ep_int, dev->req_int_in, GFP_ATOMIC);
				if (qret) {
					dev->int_in_busy = 0;
				} else {
					dev->replies_sent++;
					if (dev->replies_sent == NKS4_CONFIG_REPLY_COUNT) {
						printk(KERN_INFO "OmapNKS4VirtualBoard: all %d config-time command "
						       "replies sent - arming synthetic runtime-event timer in %us\n",
						       dev->replies_sent, NKS4_EVENT_SETTLE_JIFFIES / HZ);
						mod_timer(&dev->event_timer, jiffies + NKS4_EVENT_SETTLE_JIFFIES);
					}
				}
				printk(KERN_INFO "OmapNKS4VirtualBoard: command 0x%08x -> interrupt-IN reply qret=%d\n",
				       cmd, qret);
			}
			break;
		}
	} else if (req->status == 0 && req->actual > 0) {
		printk(KERN_INFO "OmapNKS4VirtualBoard: bulk-OUT %d bytes (too short to be a command word)\n",
		       req->actual);
	}

	/* re-queue to keep receiving */
	req->length = NKS4_EP_MAXPACKET;
	if (usb_ep_queue(ep, req, GFP_ATOMIC))
		printk(KERN_ERR "OmapNKS4VirtualBoard: bulk-OUT re-queue failed\n");
}

static void nks4_int_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	/* Nothing to re-queue automatically: each interrupt-IN transfer here is
	 * a one-shot reply (a COmapNKS4Command response, or a synthetic runtime
	 * event from nks4_event_timer_fn()), queued on demand - not a periodic
	 * self-re-arming transfer. Just clear the busy guard so the next
	 * request (from either source) can queue req_int_in again. */
	if (nks4)
		nks4->int_in_busy = 0;
}

/* ========================================================================= *
 *  setup() - control-endpoint request handling. The Linux USB core (via
 *  dummy_hcd, in this substitute) handles the low-level SETUP/DATA/STATUS
 *  transaction sequencing; this callback only needs to build the right
 *  response DATA for each request type. GET_DESCRIPTOR (device/config) and
 *  SET_CONFIGURATION are the only two request types genuinely needed to
 *  complete enumeration and let a host-side probe() fire - every other
 *  standard request (GET_STATUS, SET_ADDRESS, ...) is handled by the
 *  gadget core / dummy_hcd itself before this callback is even invoked, per
 *  the standard Linux gadget API contract.
 * ========================================================================= */

static int nks4_setup(struct usb_gadget *gadget, const struct usb_ctrlrequest *ctrl)
{
	struct nks4_dev *dev = nks4;
	u16 wValue = le16_to_cpu(ctrl->wValue);
	u16 wLength = le16_to_cpu(ctrl->wLength);
	u8 *buf;
	int len = -EOPNOTSUPP;

	/* DIAGNOSTIC (2026-07-19, dummy_timer NULL-deref investigation): temporary,
	 * see README.md's dated section - logs every control request this gadget
	 * sees, to find out whether OUR setup() ever stalls/mishandles a request
	 * during the "device number 2/3/4" re-enumeration churn seen just before
	 * dummy_timer's own crash. Cheap (one printk per control transfer, which
	 * is at most a few dozen per boot), left in rather than ripped out since
	 * it's genuinely useful for any future gadget-side enumeration debugging. */
	printk(KERN_INFO "OmapNKS4VirtualBoard: setup bRequestType=0x%02x bRequest=0x%02x "
	       "wValue=0x%04x wIndex=0x%04x wLength=%u\n",
	       ctrl->bRequestType, ctrl->bRequest, wValue,
	       le16_to_cpu(ctrl->wIndex), wLength);

	if (!dev || !dev->req_ep0)
		return -ENODEV;

	buf = dev->req_ep0->buf;

	switch (ctrl->bRequest) {
	case USB_REQ_GET_DESCRIPTOR:
		if (ctrl->bRequestType != USB_DIR_IN)
			break;
		switch (wValue >> 8) {
		case USB_DT_DEVICE:
			len = sizeof(nks4_device_desc);
			memcpy(buf, &nks4_device_desc, len);
			break;
		case USB_DT_CONFIG: {
			/* config + interface + 2 endpoint descriptors, back to back -
			 * the real wTotalLength every USB host parses to walk the
			 * whole descriptor set in one control transfer.
			 *
			 * BUG FIX (2026-07-20, struct-urb-fix boot test follow-up):
			 * struct usb_endpoint_descriptor (ch9.h) is __attribute__((packed))
			 * but carries 2 trailing audio-only fields (bRefresh,
			 * bSynchAddress) beyond the real 7-byte wire format - the header's
			 * own comment says so explicitly ("use USB_DT_ENDPOINT*_SIZE in
			 * bLength, not sizeof"). Each descriptor's own bLength was already
			 * correctly USB_DT_ENDPOINT_SIZE (7), but this code was using
			 * sizeof(nks4_int_ep_desc)/sizeof(nks4_bulk_ep_desc) (9) to lay out
			 * the buffer and compute wTotalLength - a 2-byte-per-endpoint gap
			 * between what bLength claims and where the next descriptor
			 * actually starts. The real host's config parser walks the buffer
			 * by bLength (7), landing on the 2 leftover zero bytes mid-gap and
			 * reading them as a bogus zero-length descriptor - exactly the
			 * "invalid descriptor of length 0, skipping remainder of the
			 * config" seen live, which also explains why only 1 of the 2
			 * endpoint descriptors was ever found (parsing aborted right there,
			 * before reaching the real second one). Confirmed by boot-testing
			 * this exact fix - see OmapNKS4DummyHCDFix/README.md's "Eleventh
			 * pass" section. */
			int off = 0;

			nks4_config_desc.wTotalLength = cpu_to_le16(
				sizeof(nks4_config_desc) + sizeof(nks4_intf_desc) +
				USB_DT_ENDPOINT_SIZE + USB_DT_ENDPOINT_SIZE);

			memcpy(buf + off, &nks4_config_desc, sizeof(nks4_config_desc));
			off += sizeof(nks4_config_desc);
			memcpy(buf + off, &nks4_intf_desc, sizeof(nks4_intf_desc));
			off += sizeof(nks4_intf_desc);
			memcpy(buf + off, &nks4_int_ep_desc, USB_DT_ENDPOINT_SIZE);
			off += USB_DT_ENDPOINT_SIZE;
			memcpy(buf + off, &nks4_bulk_ep_desc, USB_DT_ENDPOINT_SIZE);
			off += USB_DT_ENDPOINT_SIZE;
			len = off;
			break;
		}
		default:
			break;
		}
		break;

	case USB_REQ_SET_CONFIGURATION:
		if (ctrl->bRequestType != USB_DIR_OUT)
			break;
		dev->config_value = (u8)wValue;
		if (wValue == 1) {
			usb_ep_enable(dev->ep_int, &nks4_int_ep_desc);
			usb_ep_enable(dev->ep_bulk, &nks4_bulk_ep_desc);
			dev->req_bulk_out->length = NKS4_EP_MAXPACKET;
			usb_ep_queue(dev->ep_bulk, dev->req_bulk_out, GFP_ATOMIC);
			printk(KERN_INFO "OmapNKS4VirtualBoard: configured, endpoints enabled\n");
			/* Do NOT start the runtime-event timer here: SET_CONFIGURATION
			 * fires as soon as this gadget enumerates, which can happen well
			 * before OmapNKS4Module.ko even loads (a fixed wall-clock delay
			 * from here is not a reliable proxy for "the real driver has
			 * finished Configure() and its own post-Configure() bring-up" -
			 * confirmed the hard way, see README.md's "Fifteenth pass": a
			 * 10s-from-SET_CONFIGURATION delay caused a real hang, most
			 * likely by delivering synthetic interrupt-IN traffic into
			 * OmapNKS4Init()'s own fragile post-Configure() window. The
			 * timer is instead started from nks4_bulk_out_complete(), only
			 * once all 3 real command replies have actually been sent -
			 * see dev->replies_sent and NKS4_EVENT_SETTLE_JIFFIES below. */
		} else {
			usb_ep_disable(dev->ep_int);
			usb_ep_disable(dev->ep_bulk);
			del_timer_sync(&dev->event_timer);
		}
		len = 0;
		break;

	case USB_REQ_GET_CONFIGURATION:
		if (ctrl->bRequestType != (USB_DIR_IN | USB_RECIP_DEVICE))
			break;
		buf[0] = dev->config_value;
		len = 1;
		break;

	default:
		break;
	}

	if (len >= 0) {
		int qret;

		dev->req_ep0->length = min_t(int, len, wLength);
		dev->req_ep0->zero = (len < wLength);
		dev->req_ep0->complete = nks4_ep0_complete;
		qret = usb_ep_queue(gadget->ep0, dev->req_ep0, GFP_ATOMIC);
		printk(KERN_INFO "OmapNKS4VirtualBoard: setup -> len=%d qret=%d\n", len, qret);
		return qret;
	}
	printk(KERN_INFO "OmapNKS4VirtualBoard: setup -> STALL (len=%d)\n", len);
	return len;
}

/* ========================================================================= *
 *  bind() / unbind() - endpoint allocation and teardown.
 * ========================================================================= */

static int nks4_bind(struct usb_gadget *gadget)
{
	struct nks4_dev *dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->gadget = gadget;
	set_gadget_data(gadget, dev);

	dev->req_ep0 = usb_ep_alloc_request(gadget->ep0, GFP_KERNEL);
	if (!dev->req_ep0)
		goto fail;
	dev->req_ep0->buf = kmalloc(256, GFP_KERNEL);
	if (!dev->req_ep0->buf)
		goto fail;

	dev->ep_int = usb_ep_autoconfig(gadget, &nks4_int_ep_desc);
	if (!dev->ep_int) {
		printk(KERN_ERR "OmapNKS4VirtualBoard: no interrupt-IN endpoint available\n");
		goto fail;
	}
	dev->ep_int->driver_data = dev;

	dev->ep_bulk = usb_ep_autoconfig(gadget, &nks4_bulk_ep_desc);
	if (!dev->ep_bulk) {
		printk(KERN_ERR "OmapNKS4VirtualBoard: no bulk-OUT endpoint available\n");
		goto fail;
	}
	dev->ep_bulk->driver_data = dev;

	dev->req_bulk_out = usb_ep_alloc_request(dev->ep_bulk, GFP_KERNEL);
	if (!dev->req_bulk_out)
		goto fail;
	dev->req_bulk_out->buf = kmalloc(NKS4_EP_MAXPACKET, GFP_KERNEL);
	if (!dev->req_bulk_out->buf)
		goto fail;
	dev->req_bulk_out->complete = nks4_bulk_out_complete;

	dev->req_int_in = usb_ep_alloc_request(dev->ep_int, GFP_KERNEL);
	if (!dev->req_int_in)
		goto fail;
	dev->req_int_in->buf = kmalloc(NKS4_EP_MAXPACKET, GFP_KERNEL);
	if (!dev->req_int_in->buf)
		goto fail;
	dev->req_int_in->complete = nks4_int_in_complete;

	init_timer(&dev->event_timer);
	dev->event_timer.function = nks4_event_timer_fn;
	dev->event_timer.data = (unsigned long)dev;

	nks4 = dev;
	printk(KERN_INFO "OmapNKS4VirtualBoard: bound, ep_int=%s ep_bulk=%s\n",
	       dev->ep_int->name, dev->ep_bulk->name);
	return 0;

fail:
	if (dev->req_int_in) {
		kfree(dev->req_int_in->buf);
		usb_ep_free_request(dev->ep_int, dev->req_int_in);
	}
	if (dev->req_bulk_out) {
		kfree(dev->req_bulk_out->buf);
		usb_ep_free_request(dev->ep_bulk, dev->req_bulk_out);
	}
	if (dev->req_ep0) {
		kfree(dev->req_ep0->buf);
		usb_ep_free_request(gadget->ep0, dev->req_ep0);
	}
	kfree(dev);
	return -ENOMEM;
}

static void nks4_unbind(struct usb_gadget *gadget)
{
	struct nks4_dev *dev = get_gadget_data(gadget);

	if (!dev)
		return;

	del_timer_sync(&dev->event_timer);

	if (dev->req_int_in) {
		kfree(dev->req_int_in->buf);
		usb_ep_free_request(dev->ep_int, dev->req_int_in);
	}
	if (dev->req_bulk_out) {
		kfree(dev->req_bulk_out->buf);
		usb_ep_free_request(dev->ep_bulk, dev->req_bulk_out);
	}
	if (dev->req_ep0) {
		kfree(dev->req_ep0->buf);
		usb_ep_free_request(gadget->ep0, dev->req_ep0);
	}
	kfree(dev);
	set_gadget_data(gadget, NULL);
	nks4 = NULL;
}

static void nks4_disconnect(struct usb_gadget *gadget)
{
	struct nks4_dev *dev = get_gadget_data(gadget);

	if (dev) {
		del_timer_sync(&dev->event_timer);
		usb_ep_disable(dev->ep_int);
		usb_ep_disable(dev->ep_bulk);
		dev->config_value = 0;
	}
}

static struct usb_gadget_driver nks4_driver = {
	.function	= "OmapNKS4VirtualBoard",
	.speed		= USB_SPEED_FULL,
	.bind		= nks4_bind,
	.unbind		= nks4_unbind,
	.setup		= nks4_setup,
	.disconnect	= nks4_disconnect,
	.driver		= {
		.name	= "OmapNKS4VirtualBoard",
		.owner	= THIS_MODULE,
	},
};

/* ========================================================================= *
 *  Module init / exit.
 * ========================================================================= */

static int __init nks4_init(void)
{
	printk(KERN_INFO "OmapNKS4VirtualBoard: loading (genuine USB gadget - "
	       "vendor 0x%04x product 0x%04x, interface class 0xff, "
	       "1 interrupt-IN + 1 bulk-OUT - requires dummy_hcd.ko loaded first)\n",
	       NKS4_VENDOR_ID, NKS4_PRODUCT_ID);
	return usb_gadget_register_driver(&nks4_driver);
}

static void __exit nks4_exit(void)
{
	usb_gadget_unregister_driver(&nks4_driver);
	printk(KERN_INFO "OmapNKS4VirtualBoard: unloaded\n");
}

module_init(nks4_init);
module_exit(nks4_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Genuine USB gadget presenting a virtual NKS4 front-panel "
		    "board (vendor 0x0944/product 0x1005) for VM boot testing "
		    "of the real OmapNKS4Module.ko host driver - see file header");
MODULE_AUTHOR("Korg (reconstructed)");
