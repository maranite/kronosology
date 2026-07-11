// SPDX-License-Identifier: GPL-2.0
/*
 * module_main.c  -  KorgUsbAudioVirtualDriver.ko module init/exit.
 *
 * Kernel-only (real Linux headers, EXPORT_SYMBOL), deliberately separate
 * from korgusbaudio_stub.cpp (which stays freestanding and host-testable)
 * -- same split AT88VirtualChip uses.
 *
 * Unlike AT88VirtualChip, there is no captured data blob to load here --
 * every exported function operates on purely in-memory stub state (see
 * korgusbaudio_stub.h), so init is trivial: no workqueue, no RTAI-unsafe
 * filp_open concerns.
 *
 * Deliberately plain C, not C++: this ancient kernel's headers use
 * syntax a modern g++ can't parse as C++ (confirmed the hard way -- see
 * MASTER_REFERENCE.md sec 10.43), but parses fine as C via gcc. This
 * file uses no genuine C++ features, so compiling it as C sidesteps the
 * problem entirely (korgusbaudio_stub.h's own extern "C" block is
 * `#ifdef __cplusplus`-guarded to support both this C consumer and the
 * C++ stub implementation/tests).
 */

#include <linux/module.h>
#include <linux/init.h>

#include "korgusbaudio_stub.h"

static int __init KorgUsbAudioVirtualDriverInit(void)
{
	printk(KERN_INFO "KorgUsbAudioVirtualDriver: loading (stand-in for "
	       "KorgUsbAudioDriver.ko's USB audio/MIDI codec access -- see README.md)\n");
	return 0;
}

static void __exit KorgUsbAudioVirtualDriverExit(void)
{
	printk(KERN_INFO "KorgUsbAudioVirtualDriver: unloaded\n");
}

module_init(KorgUsbAudioVirtualDriverInit);
module_exit(KorgUsbAudioVirtualDriverExit);

EXPORT_SYMBOL(KorgUsbAudioInitialize);
EXPORT_SYMBOL(KorgUsbAudioInitialized);
EXPORT_SYMBOL(KorgUsbAudioStart);
EXPORT_SYMBOL(KorgUsbAudioDone);
EXPORT_SYMBOL(KorgUsbAudioOutput);
EXPORT_SYMBOL(KorgUsbAudioInput);
EXPORT_SYMBOL(KorgUsbAudioOutputDone);
EXPORT_SYMBOL(KorgUsbAudioInputDone);
EXPORT_SYMBOL(KorgUsbAudioInputStarving);
EXPORT_SYMBOL(KorgUsbAudioOutputStarving);
EXPORT_SYMBOL(KorgUsbAudioErrorString);
EXPORT_SYMBOL(KorgUsbAudioFormatSize);
EXPORT_SYMBOL(KorgUsbAudioFormatString);
EXPORT_SYMBOL(KorgUsbAudioPrintIndices);
EXPORT_SYMBOL(KorgUsbMidiInitialize);
EXPORT_SYMBOL(KorgUsbMidiInitialized);
EXPORT_SYMBOL(KorgUsbMidiDone);
EXPORT_SYMBOL(KorgUsbMidiOutput);
EXPORT_SYMBOL(KorgUsbMidiOutputCanSend);
EXPORT_SYMBOL(KorgUsbRealtimeMidiOutput);
EXPORT_SYMBOL(KorgUsbRealtimeMidiOutputCanSend);
EXPORT_SYMBOL(USBMidiAccessory_SetDrumPadClient);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("No-op/always-ready stub for KorgUsbAudioDriver.ko's USB audio+MIDI "
		    "codec symbols (VM/foreign-hardware boot testing)");
MODULE_AUTHOR("Korg (reconstructed)");
