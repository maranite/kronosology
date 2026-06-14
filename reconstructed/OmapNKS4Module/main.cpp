// SPDX-License-Identifier: GPL-2.0
/*
 * main.c  -  module init/exit and the two service threads.
 *
 *   OmapNKS4Init : bring up the C++ runtime, grab an RTAI SRQ, register the USB driver,
 *                  wait for probe, then start /proc, the interrupt transfer, configure
 *                  the panel, and launch the worker threads + active-sense RT thread.
 *   OmapNKS4Exit : reverse of the above.
 *   ProcessMsgRoutine   : video message-processor thread (drains the LCD event ring).
 *   ShutdownSSDRoutine  : on a panel power-button event, flush/stop the internal SSD
 *                         (SCSI sync) then tell the panel to shut down.
 *
 * The C++ runtime (init_cpp_support / cleanup_cpp_support, operator new/delete over
 * stg_kmalloc, __cxa_pure_virtual) is provided by the shared STG cpp-support shim.
 */

#include "omapnks4_internal.h"

extern "C" {
void init_cpp_support(void);
void cleanup_cpp_support(void);
int  create_thread(int (*fn)(void *), const char *name);

int  COmapNKS4Driver_Configure(void);
void COmapNKS4_SetMaxBulkOutMsgSize(void);
}

static int sDriverState;
static int sSTG2NKS4SrqNumber;
static void *sInterruptURB;

/* thread state / wake flags */
int sProcessMsgThreadRunning, sVideoMsgSignalled;
int sShutdownSSDThreadRunning, sShutdownSSDSignaled;
static int sIsSSDReadyToShutdown;

/* ===================================================================== */

static int __init OmapNKS4Init(void)
{
	init_cpp_support();		/* run C++ global constructors */
	printk("<6>OmapNKS4: OmapNKS4Init: enter\n");
	sDriverState = 0;

	sSTG2NKS4SrqNumber = rt_request_srq();
	if (sSTG2NKS4SrqNumber < 1) {
		printk("<6>OmapNKS4: could not get srq!\n");
		goto fail;
	}
	if (stg_usb_register_driver() != 0) {
		printk("<6>OmapNKS4: Cannot register nks4 usb driver!\n");
		goto cleanup;
	}

	/* probe() runs from the USB core and completes this; wait for it */
	wait_for_completion_timeout();
	printk("<6>OmapNKS4: Waited for OmapNKS4Probe(). driver state is %d\n", sDriverState);
	if (sDriverState != 1) {
		printk("<6>OmapNKS4: probe failed\n");
		goto cleanup;
	}

	if (OmapNKS4ProcInitialize() != 0) { goto cleanup; }

	if (stg_usb_submit_urb(sInterruptURB, 0) != 0) {	/* start the interrupt-IN xfer */
		printk("<6>OmapNKS4: error submitting interrupt xfer\n");
		goto cleanup;
	}
	if (COmapNKS4Driver_Configure() != 0) {
		printk("<6>OmapNKS4: Problem configuring OmapNKS4 in Init\n");
		goto cleanup;
	}

	sProcessMsgThreadRunning = 1;
	create_thread(ProcessMsgRoutine, "kOmapNKS4MsgRoutine");
	sShutdownSSDThreadRunning = 1;
	create_thread(ShutdownSSDRoutine, "kShutdownSSDRoutine");
	CActiveSenseThread::Setup();
	return 0;

cleanup:
	CleanupOmapNKS4Driver();
fail:
	cleanup_cpp_support();
	return -1;
}

static void __exit OmapNKS4Exit(void)
{
	CActiveSenseThread::Cleanup();

	sProcessMsgThreadRunning = 0;
	__wake_up(0);
	wait_for_completion_timeout();		/* join the msg thread */

	sShutdownSSDThreadRunning = 0;
	__wake_up(0);
	wait_for_completion_timeout();		/* join the ssd thread */

	CleanupOmapNKS4Driver();
	cleanup_cpp_support();
}

/* ===================================================================== */

/* Video message-processor: wake on SignalVideoMessageProcessor(), drain the ring. */
int ProcessMsgRoutine(void *arg)
{
	daemonize("kOmapNKS4MsgRoutine");
	stg_sched_setscheduler(2 /* SCHED_RR */);
	block_all_signals();
	complete();				/* tell Init we're up */

	while (sProcessMsgThreadRunning) {
		wait_event(sVideoMsgSignalled);
		sVideoMsgSignalled = 0;
		if (!sProcessMsgThreadRunning)
			break;
		OmapNKS4VideoAPIProcessEvents();
	}
	sProcessMsgThreadRunning = 0;
	complete_and_exit();
	return 0;
}

/*
 * SSD-shutdown thread: when the panel signals a power-off (SignalShutdownSSD), make
 * the internal SSD safe by walking the SCSI hosts, flushing/quiescing each device
 * (scsi_device_set_state + the device's ->shutdown), then sleeping and telling the
 * panel firmware to power down.
 */
int ShutdownSSDRoutine(void *arg)
{
	daemonize("kShutdownSSDRoutine");
	stg_sched_setscheduler(2 /* SCHED_RR */);
	block_all_signals();
	complete();

	for (;;) {
		wait_event_timeout(sShutdownSSDSignaled, 10000);
		if (!sShutdownSSDThreadRunning) {
			sShutdownSSDThreadRunning = 0;
			complete_and_exit();
			return 0;
		}
		if (sIsSSDReadyToShutdown || !sShutdownSSDSignaled)
			continue;

		/* flush + stop every SCSI device on every host (the internal SSD) */
		for (int host = 0; ; host++) {
			void *shost = scsi_host_lookup(host);
			if (!shost)
				break;
			void *sdev = scsi_device_lookup(shost);
			if (sdev) {
				mutex_lock();
				scsi_device_set_state(sdev /* SDEV_QUIESCE */);
				dev_shutdown(sdev);	/* sdev->host->hostt->... / driver shutdown */
				scsi_device_set_state(sdev /* SDEV_OFFLINE */);
				bus_shutdown(sdev);
				mutex_unlock();
				scsi_device_put(sdev);
			}
			scsi_host_put(shost);
		}

		msleep(500);
		sShutdownSSDSignaled = 0;
		COmapNKS4Driver_ShutDown();
		sIsSSDReadyToShutdown = 1;
	}
}

module_init(OmapNKS4Init);
module_exit(OmapNKS4Exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Korg Kronos OMAP NKS4 front-panel USB driver");
MODULE_AUTHOR("Korg (reconstructed)");
