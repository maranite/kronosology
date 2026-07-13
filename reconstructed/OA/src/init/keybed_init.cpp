// SPDX-License-Identifier: GPL-2.0
/*
 * keybed_init.cpp  -  CSTGKeybedInterface_Startup()/_Cleanup(): init_module
 * step 14 (hard-fail). See oa_keybed_init.h for the ground-truthing
 * details (offsets, storage-model note, INVERTED SUCCESS CONVENTION).
 *
 * Faithful, instruction-level reconstruction from a full objdump
 * disassembly + relocation trace of the real `CSTGKeybedInterface_
 * Startup` (`.text+0x33e5e0`, 319 bytes) and `_Cleanup`
 * (`.text+0x33e720`, 30 bytes) in OA_real.ko.
 *
 * Written with `goto`s that mirror the real disassembly's own labels
 * and branches directly, rather than restructured into "cleaner"
 * nested loops -- the real control flow has several genuinely
 * non-obvious jumps (a special early-trigger path that skips straight
 * into the delay-polling loop; a shared success/failure convergence
 * point reached from two different callers) that are easy to get
 * subtly wrong when reshaped into idiomatic loop constructs. Matching
 * the real branch structure 1:1 is the safer -- if less pretty --
 * choice for a function whose entire value is behavioral fidelity.
 */

#include "oa_keybed_init.h"
#include "oa_internal.h" /* placement operator new(size_t, void*) */

/*
 * `CSTGComPort::OnByteReceived` (vtable slot 0) is now a REAL,
 * confirmed override -- `CSTGKeybedComPort::ReceiveByte` (sec 10.237),
 * placement-constructed here matching the real static constructor's own
 * confirmed action (installing `vtable for CSTGKeybedComPort` at
 * `CSTGKeybedInterface::sInstance+0x0`). An earlier pass in this
 * project modeled this as an empty `KeybedComPortStub` purely to give
 * the vtable-dispatching methods (TriggerInterrupt/HandleInterrupt) a
 * valid vtable pointer -- that placeholder is now replaced by the real
 * class, see oa_comport.h/keybed_receive.cpp.
 */
static unsigned char s_keybedInstance[KEYBED_SINSTANCE_SIZE];

extern "C" unsigned char *CSTGKeybedInterface_sInstance(void)
{
	return s_keybedInstance;
}

static CSTGComPort *ComPort()
{
	return reinterpret_cast<CSTGComPort *>(s_keybedInstance);
}

int CSTGKeybedInterface_Startup(void)
{
	unsigned char *sInstance = s_keybedInstance;
	int comPortId, found;
	/* Named to directly mirror the real disassembly's own `ebx` (the
	 * confirmed real "next port to try after a failure" counter,
	 * confirmed to start at 1, NOT 0 -- `comPortId = nextPortOnFail;
	 * nextPortOnFail++` on every failure, matching `edx=ebx; ebx+=1`
	 * exactly). An earlier draft used a `portsTried` variable starting
	 * at 0 instead, which silently retried port 0 twice before this
	 * was caught by the host KAT -- keeping this 1:1 with the real
	 * register semantics avoids that whole class of off-by-one bug. */
	int nextPortOnFail;
	/* Confirmed real: zeroed ONCE here (before even the debounce filter
	 * init), not reset on each outer retry -- a genuine persistent
	 * counter across the whole function. */
	int outerRetries = 0;

	/* Placement-construct the real CSTGKeybedComPort sub-object's
	 * vtable once -- see this file's own updated comment above. */
	static bool comPortConstructed = false;
	if (!comPortConstructed) {
		new (sInstance) CSTGKeybedComPort();
		comPortConstructed = true;
	}

	/* One-time: the debounce filter sub-object at +0x30. */
	CSTGKeybedKeyDebounceFilter_Initialize(sInstance + KEYBED_OFF_DEBOUNCE_FILTER);

outer_retry:
	nextPortOnFail = 1;
	comPortId = 0;

inner_loop:
	sInstance[KEYBED_OFF_ACK_FLAG] = 0;
	if (!ComPort()->Initialize((CSTGComPort::eComPortId)comPortId,
				    (CSTGComPort::eBaudRateCode)0x18,
				    (CSTGComPort::eReceiveFifoThresholdCode)0))
		goto port_failed;

	ComPort()->txFifo.WriteByte(0xa5);
	sInstance[KEYBED_OFF_STATE] = 1;

	/*
	 * Confirmed real check: if exactly one byte is pending in the TX
	 * fifo (head - tail == 1, i.e. the probe byte we just wrote is the
	 * only one queued), trigger the UART's TX interrupt directly and
	 * skip straight into the delay-polling loop below -- bypassing
	 * nothing else, just entering at a different point.
	 */
	if ((unsigned char)(ComPort()->txFifo.head - ComPort()->txFifo.tail) == 1) {
		ComPort()->TriggerInterrupt();
		goto delay_loop_start;
	}

delay_loop_start:
	__const_udelay(0x20c4ac);
	if (sInstance[KEYBED_OFF_ACK_FLAG])
		goto check_ack_final;
	{
		int retries = 0x32;
		do {
			__const_udelay(0x68dbc);
			if (sInstance[KEYBED_OFF_ACK_FLAG])
				goto check_ack_final;
			retries--;
		} while (retries != 0);
	}

check_ack_final:
	if (sInstance[KEYBED_OFF_ACK_FLAG])
		goto ack_received;

port_failed:
	ComPort()->Cleanup();
	comPortId = nextPortOnFail;
	nextPortOnFail++;
	if (nextPortOnFail != 7)
		goto inner_loop;

	/* All 6 ports (0-5) tried and failed this round -- see this file's
	 * own header comment on why it's 6, not 7. */
	outerRetries++;
	found = 0;
	if (outerRetries == 10)
		goto shared_check;
	goto check_continue;

ack_received:
	outerRetries++;
	found = 1;
	if (outerRetries == 10)
		goto shared_check;
	goto check_continue;

shared_check:
	if (found)
		goto final_success;
	goto final_failure;

check_continue:
	if (!found)
		goto outer_retry;
	goto final_success;

final_failure:
	ComPort()->Cleanup();
	sInstance[KEYBED_OFF_STATE] = 0;
	return 0;

final_success:
	sInstance[KEYBED_OFF_STATE] = 2;
	return 1;
}

void CSTGKeybedInterface_Cleanup(void)
{
	unsigned char *sInstance = s_keybedInstance;
	ComPort()->Cleanup();
	sInstance[KEYBED_OFF_STATE] = 0;
}
