// SPDX-License-Identifier: GPL-2.0
/*
 * oa_comport.h  -  CSTGComPort: OA.ko's own real serial-port UART
 * driver, used by CSTGKeybedInterface's keybed handshake (sec 10.49).
 *
 * Ground-truthed via readelf's symbol table against
 * ARCHIVE/Ignored/DecryptedImages/OA_real.ko, then a full objdump
 * disassembly + relocation trace of every method below except
 * `Initialize` itself (see that method's own comment).
 *
 * **Fixes a real, previously-undiscovered bug**: this project's own
 * earlier `CSTGKeybedInterface_Startup` reconstruction (sec 10.49)
 * declared `CSTGComPort_Initialize`/`_Cleanup`/`_TriggerInterrupt`/
 * `_TransmitFifo_WriteByte` as PLAIN C-linkage functions with
 * underscore-separated names -- but the real relocations these calls
 * target are genuine MANGLED C++ methods
 * (`_ZN11CSTGComPort10InitializeE...`, `_ZN11CSTGComPort7CleanupEv`,
 * `_ZN11CSTGComPort16TriggerInterruptEv`,
 * `_ZN11CSTGComPort12TransmitFifo9WriteByteEh`), confirmed via
 * `readelf`. A plain C-linkage symbol named e.g. "CSTGComPort_
 * Initialize" would NEVER match the real mangled symbol at Kbuild
 * link time -- a genuine latent defect (the earlier reconstruction's
 * own SEMANTIC modeling of arguments/behavior was correct, only the
 * symbol names were wrong). Fixed here by declaring the real class
 * with its real methods, and `keybed_init.cpp` updated to call through
 * it properly (see that file's own updated comment).
 *
 * Confirmed real object layout (CSTGComPort's own "this", which is the
 * SAME storage as `CSTGKeybedInterface::sInstance+0` -- confirmed via
 * `CSTGKeybedInterface_Startup`'s own calls passing `sInstance`
 * directly as this class's "this"):
 *   +0x00  vtable pointer (2 confirmed pure-virtual slots: slot 0 is
 *          called `(this, receivedByte)` when the UART has RX'd a
 *          byte -- an "OnByteReceived"-shaped hook a derived/owning
 *          class overrides, not independently named in this pass;
 *          slot 1 is `GetByteToTransmit()`, confirmed BY NAME via its
 *          own real symbol matching this class's real method of the
 *          same name below)
 *   +0x04  TransmitFifo txFifo (a 16-byte circular buffer + capacity +
 *          tail + head, 0x13 bytes total -- see the nested struct)
 *   +0x17  irqEnabled (byte) -- guards SetBaudRate's/TriggerInterrupt's
 *          own cli/sti critical sections, and Cleanup's IRQ teardown
 *   +0x18  comPortId (confirmed: Initialize's own very first
 *          instruction stores its `eComPortId` argument here
 *          unconditionally, even on the >5 out-of-range failure path)
 *   +0x1c  ioBase (the 16550 UART's I/O port base address -- confirmed
 *          real, a genuine PC legacy serial port, via raw `in`/`out`
 *          instructions throughout every method below, NOT a
 *          simulated/abstracted bus)
 *   +0x20  irqNumber (passed to `rtwrap_shutdown_irq`/`rtwrap_release_
 *          irq` by Cleanup)
 *
 * All hardware register access is real raw x86 port I/O (`in`/`out`),
 * confirmed via a standard 16550-compatible register layout relative
 * to `ioBase`: +0 DATA (RBR/THR, or DLL when LCR's DLAB bit is set),
 * +1 IER (or DLM when DLAB set), +2 IIR (read)/FCR (write), +3 LCR
 * (bit 7 = DLAB), +5 LSR (bit 0 = Data Ready, bit 5 = THRE, bit 6 =
 * TEMT). Modeled here via `stg_inb`/`stg_outb` accessors (this
 * project's established host/target-divergence pattern, matching
 * `stg_get_current_task`/`cpu_features_ok` in `init_module.cpp`) since
 * raw port I/O isn't portably host-testable.
 *
 * `Initialize` (`.text+0xaa90`, 2561 bytes -- by far the largest
 * single method in this class) IS now reconstructed, via a full
 * `objdump -d -r` disassembly trace plus a direct `.rodata` extraction
 * of every jump table / per-port lookup table it references (Ghidra's
 * `load_binary` still times out on the full 14MB binary; a plain
 * per-function `decompile_function` call also requires the binary to
 * be loaded first, so this remains objdump-only). Real Super-I/O chip
 * probing/configuration against a SEPARATE class (`CW83627`, confirmed
 * real via relocation -- the Winbond W83627 Super I/O chip, matching
 * this project's own earlier finding, MASTER_REFERENCE.md sec 10.40).
 *
 * **Scope decision, stated plainly rather than silently narrowed**:
 * this function has at least 15 distinct branches with SEVERE register
 * reuse across them (the compiler duplicated the same core detection/
 * LDN-select logic 2-3 times inline for different entry scenarios
 * rather than sharing a helper), making a literal 1:1 instruction
 * trace of every single branch both extremely time-costly and
 * genuinely error-prone. This reconstruction implements the CONFIRMED,
 * HIGH-CONFIDENCE core with full fidelity (see `comport_init.cpp`'s
 * own comment for the exact algorithm traced): bounds check, dual
 * config-port (0x2E then 0x4E fallback) unlock + chip-ID validation,
 * LDN selection via the confirmed per-port mapping table, IRQ
 * discovery, UART hardware reset, `rtwrap_request_irq`/
 * `rtwrap_assign_irq_to_cpu`, and the final baud-rate/LCR/FCR/
 * `rtwrap_startup_irq`/MCR/IER bring-up (this last part reuses the
 * exact same 16550 register semantics already confirmed and
 * implemented in `SetBaudRate` above). Two comPortId values (0 and 3)
 * are confirmed via `.rodata`'s own per-port flag tables to have
 * ADDITIONAL, LDN-specific special-case config blocks beyond this
 * generic path (a GPIO-like register sequence for port 3's LDN, and a
 * distinct extra sequence for port 0's LDN) -- NOT implemented in this
 * pass, a real and deliberately documented gap rather than a silent
 * omission; those two ports fall through this reconstruction's generic
 * path instead. Under a bare QEMU environment with no real Super-I/O
 * chip present, the chip-ID validation step fails first anyway (an
 * unimplemented I/O port read returns 0xFF, which fails the confirmed
 * real `[1, 0xFE]` validity range) -- so this gap is unlikely to be
 * reached in this project's own actual testing environment, though it
 * remains a real fidelity gap worth closing in a future, more focused
 * pass on just those two ports.
 */

#ifndef OA_COMPORT_H
#define OA_COMPORT_H

extern "C" {
unsigned char stg_inb(unsigned int port);
void stg_outb(unsigned int port, unsigned char value);
void rtwrap_shutdown_irq(unsigned int irq);
void rtwrap_release_irq(unsigned int irq);

/*
 * Real raw `pushf`/`cli`/`popf` critical-section instructions,
 * confirmed in SetBaudRate/TriggerInterrupt's own disassembly. These
 * are privileged instructions -- executing them directly would fault
 * in userspace, so (matching this project's established host/target
 * divergence pattern, e.g. `stg_get_current_task`/`cpu_features_ok` in
 * init_module.cpp) they're modeled as accessors here; the real target
 * build executes the instructions directly at these call sites.
 */
unsigned long stg_local_irq_save(void);
void stg_local_irq_restore(unsigned long savedFlags);

/*
 * `stg_inb`/`stg_outb` (declared above) are ALSO what `Initialize`
 * uses for the Winbond Super-I/O config-mode dance (fixed ports
 * 0x2E/0x2F or 0x4E/0x4F, never a variable `ioBase` like the UART's
 * own registers) -- the same real `in`/`out` instructions either way,
 * just a different fixed port range.
 *
 * Confirmed real `rtwrap_request_irq` regparm(3) argument order --
 * NOT the order first assumed (irq, handler, flags, dev): the real
 * disassembly maps irq=eax, handler=edx, dev=ecx (the `this` pointer,
 * passed through so `RTAIInterruptHandler` receives it back), and a
 * flags-shaped 4th argument on the stack (confirmed literal 0 at this
 * call site). Returns 0 on success (confirmed: `test edx,edx; jne
 * fail`), matching typical kernel `request_irq()` convention.
 */
int rtwrap_request_irq(unsigned int irq, void (*handler)(unsigned int, void *),
			void *dev, unsigned int flags);
void rtwrap_assign_irq_to_cpu(unsigned int irq, unsigned int cpu);
void rtwrap_startup_irq(unsigned int irq);
} /* extern "C" */

/*
 * Confirmed real singleton, ground-truthed via relocation
 * (`_ZN7CW836279sInstanceE`) -- `Initialize` caches which of the two
 * possible Super-I/O config ports (0x2E or 0x4E) actually has a chip
 * present, so repeated calls for different comPortIds don't need to
 * re-probe. Own body/methods not reconstructed in this pass; modeled
 * here as a plain data struct matching the confirmed field layout
 * `Initialize` itself reads/writes directly (byte@0 = chip found,
 * dword@4 = the confirmed config port, byte@8 = a per-port
 * "select"/chip-variant byte from `.rodata` Initialize stores here).
 */
struct CW83627 {
	unsigned char found;
	unsigned int configPort;
	unsigned char selectByte;

	static CW83627 sInstance;
};

class CSTGComPort {
public:
	/*
	 * Confirmed real nested enum types (via the mangled signature
	 * `_ZN11CSTGComPort10InitializeENS_10eComPortIdENS_13eBaudRateCodeE
	 * NS_25eReceiveFifoThresholdCodeE` -- each `NS_...E` component names
	 * a nested type within `CSTGComPort`). Declared here purely so
	 * `Initialize`'s own mangled name matches the real one exactly (an
	 * earlier draft of this header used plain `int` parameters, which
	 * mangles differently and would have silently failed to link
	 * against the real symbol even after fixing the free-function-vs-
	 * method naming bug this header's own comment describes -- caught
	 * by comparing the Kbuild-built `.ko`'s own unresolved-symbol list
	 * against the real `Initialize`'s confirmed mangled name). No
	 * enumerator VALUES are confirmed/needed in this pass -- only the
	 * type identity itself affects mangling; the integer values this
	 * project's own callers use (0-5 for port id, 0x18 for baud code, 0
	 * for fifo threshold) are passed via `(eComPortId)0` -- style casts
	 * at the call site instead.
	 */
	enum eComPortId {};
	enum eBaudRateCode {};
	enum eReceiveFifoThresholdCode {};

	/* Confirmed real nested class (`CSTGComPort::TransmitFifo`), a
	 * 16-byte circular buffer. `WriteByte` is the producer side
	 * (called externally, e.g. by the keybed handshake to queue the
	 * probe byte); `GetByteToTransmit` (CSTGComPort's own method,
	 * below) is the consumer side, sharing this exact same storage. */
	struct TransmitFifo {
		unsigned char capacity;	/* +0x0 -- also read by
					 * TriggerInterrupt as the max
					 * burst size per trigger */
		unsigned char buffer[16];	/* +0x1..+0x10 */
		unsigned char tail;		/* +0x11 */
		unsigned char head;		/* +0x12 */

		void WriteByte(unsigned char value);
	};

	TransmitFifo txFifo;		/* +0x04 */
	unsigned char irqEnabled;	/* +0x17 */
	int comPortId;			/* +0x18 */
	unsigned int ioBase;		/* +0x1c */
	unsigned int irqNumber;	/* +0x20 */

	/* Confirmed real vtable slot 0 (`call [esi]`/`call [edx]` with
	 * `(this, receivedByte)` in TriggerInterrupt/HandleInterrupt's own
	 * RX-drain loop): a pure virtual hook this class has no confirmed
	 * body for -- a derived/owning class (plausibly CSTGKeybedInterface
	 * itself, e.g. to set its own ACK flag when a byte arrives; not
	 * independently confirmed in this pass) must override it. No
	 * virtual destructor is declared -- this project's own confirmed
	 * usage never heap-`delete`s a CSTGComPort (CSTGKeybedInterface_
	 * Startup embeds it directly in static/global storage), so slot 0
	 * being the FIRST declared virtual here (not a destructor
	 * competing for that slot under the Itanium ABI) is the correct,
	 * evidence-matching model. */
	virtual void OnByteReceived(unsigned char receivedByte) = 0;

	/* Confirmed real vtable slot 1 (`call [edx+4]`) -- but UNLIKE slot
	 * 0, this class provides a real, confirmed concrete body (below),
	 * dispatched through the vtable so a derived class COULD override
	 * it, but the confirmed keybed usage relies on this base
	 * implementation directly. -1 = no byte available. */
	virtual int GetByteToTransmit();

	/* Confirmed real (via relocation), NOT reconstructed in this pass
	 * -- see this header's own comment above. */
	char Initialize(eComPortId comPortId, eBaudRateCode baudRateCode,
			 eReceiveFifoThresholdCode fifoThresholdCode);

	void Cleanup();
	void TriggerInterrupt();
	void HandleInterrupt();
	void SetBaudRate(int baudRateCode);
	void SetDTR(bool assert);
	bool IsHardwareTransmitterEmpty() const;

	/* Reconstructed for real, batch 48 -- see comport.cpp. Confirmed
	 * real (via relocation, `_ZN11CSTGComPort20RTAIInterruptHandlerEjPv`,
	 * WEAK linkage, its own dedicated 154-byte `.text._ZN...` section)
	 * -- `Initialize` takes its ADDRESS to pass to `rtwrap_request_irq`
	 * as the callback, with `this` forwarded back as `dev` (see the
	 * `rtwrap_request_irq` comment above). Full disassembly shows the
	 * body is byte-for-byte the SAME dummy-IIR-read/poll-drain-transmit
	 * loop already modeled as the shared `ComPortServiceLoop` helper
	 * behind `HandleInterrupt()` -- not merely similar, the same real
	 * logic compiled a second time under RTAI's own IRQ-callback ABI.
	 * `irq` is loaded into a register and never read again in the real
	 * disassembly (confirmed dead here). This was originally deferred
	 * (sec 10.153) purely out of caution over its two real vtable
	 * dispatches (`OnByteReceived`/`GetByteToTransmit`) -- `keybed_
	 * init.cpp`'s own `KeybedComPortStub` placement-construction (added
	 * in a later batch) already makes a real, populated `CSTGComPort`
	 * vtable routinely available and already dispatches through it via
	 * `TriggerInterrupt()`, so forwarding to `HandleInterrupt()` here
	 * carries no additional risk (reuse-existing-hardware-validated-
	 * port technique, not a fresh re-derivation of the loop). */
	static void RTAIInterruptHandler(unsigned int irq, void *dev);
};

#endif /* OA_COMPORT_H */
