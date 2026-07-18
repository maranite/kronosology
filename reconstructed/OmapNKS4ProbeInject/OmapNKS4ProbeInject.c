// SPDX-License-Identifier: GPL-2.0
/*
 * OmapNKS4ProbeInject.c - direct-probe-injection virtual NKS4 "board", VM-only
 *
 * Companion to OmapNKS4VirtualBoard.ko (see that module's own README for the full
 * story). dummy_hcd.ko's root-hub enumeration hangs under this project's specific
 * QEMU-TCG environment (confirmed via two independent isolation tests, documented
 * there) - the same class of stock-subsystem/TCG incompatibility that motivated
 * RTAIVirtualDriver.ko's own existence. Rather than debug dummy_hcd blind (no
 * symbol-resolved vmlinux available for GDB), this module sidesteps the real USB
 * bus/hub/HCD machinery entirely.
 *
 * OmapNKS4Module.ko's own reconstructed OmapNKS4Probe() (usb.cpp) - the callback
 * the USB core would normally invoke on real enumeration - never dereferences a
 * real kernel struct usb_interface/usb_device through their real field accessors.
 * It reinterprets the interface pointer as a raw int array and walks fixed byte
 * offsets recovered from the shipping binary's own disassembly:
 *
 *   intf[0]                     = altsetting pointer
 *   intf[7]  (offset 0x1c)      = usb_device* ("dev")
 *   dev + 0xb8 / 0xba  (u16)    = idVendor / idProduct
 *   dev - 100                   = this driver's own "sDeviceInstance" private
 *                                 struct; [0] = device address, [7] (0x1c) = speed
 *                                 (3 = USB_SPEED_HIGH) - read by the URB-configure
 *                                 helpers, not by OmapNKS4Probe itself
 *   altsetting + 4  (u8)        = bNumEndpoints
 *   altsetting + 0xc (ptr)      = endpoint descriptor array, 0x2c bytes/entry
 *   entry + 2 (u8) / +3 (u8) / +4 (u16) / +6 (u8) = bEndpointAddress / bmAttributes /
 *                                 wMaxPacketSize / bInterval
 *
 * None of this requires a real, live struct usb_device/usb_bus/usb_hcd - just
 * correctly-shaped memory. This module hand-builds that memory (matching
 * OmapNKS4VirtualBoard.c's own vendor 0x0944 / product 0x1005 / one interrupt-IN
 * (0x81) + one bulk-OUT (0x02), both wMaxPacketSize 64) and calls the real,
 * exported OmapNKS4Probe() directly - exercising OmapNKS4Module.ko's genuine
 * probe/bring-up logic (endpoint classification, URB pool allocation, subsystem
 * init) and, on success, its real complete(&sProbeComplete), which wakes
 * OmapNKS4Init()'s own already-running wait_for_completion_timeout() for real.
 *
 * What this deliberately does NOT do: submit any URBs, or make wire-protocol
 * traffic flow. OmapNKS4Probe() itself only allocates/configures URBs (confirmed
 * by reading its full body) - actual transfers happen later, from file-op paths
 * this module doesn't exercise. Scope here is exactly what the /goal directive
 * asks for: get the real driver to recognize and bring up a virtual board.
 *
 * Load order: RTAIVirtualDriver.ko -> STGEnabler.ko -> STGGmp.ko ->
 * OmapNKS4Module.ko (starts its 10s probe-wait) -> OmapNKS4ProbeInject.ko (fires
 * the injected probe before the wait times out). No dummy_hcd, no
 * OmapNKS4VirtualBoard.ko needed for this path.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

struct usb_interface;
extern int OmapNKS4Probe(struct usb_interface *intf);

#define NKS4_VENDOR_ID   0x0944
#define NKS4_PRODUCT_ID  0x1005
#define NKS4_EP_INT_ADDR  0x81
#define NKS4_EP_BULK_ADDR 0x02
#define NKS4_EP_MAXPACKET 64

/* dev must sit >= 100 bytes into its own allocation, and have room past +0xba,
 * for the "dev - 100" sDeviceInstance region below it and the idVendor/idProduct
 * fields above it to both land inside the same buffer. */
#define FAKE_DEV_BUF_SIZE 0x200
#define FAKE_DEV_OFFSET   100

static void *fake_dev_buf;
static void *fake_intf_buf;
static void *fake_altsetting_buf;
static void *fake_ep_array;

static void nks4_inject_free_all(void)
{
	kfree(fake_ep_array);
	kfree(fake_altsetting_buf);
	kfree(fake_intf_buf);
	kfree(fake_dev_buf);
	fake_ep_array = fake_altsetting_buf = fake_intf_buf = fake_dev_buf = NULL;
}

static void nks4_inject_write_ep(unsigned char *ep, unsigned char addr,
				  unsigned char attrs, unsigned short maxpkt,
				  unsigned char interval)
{
	ep[2] = addr;
	ep[3] = attrs;
	*(unsigned short *)(ep + 4) = maxpkt;
	ep[6] = interval;
}

static int __init nks4_probe_inject_init(void)
{
	unsigned char *dev;
	unsigned long dev_addr;
	int *intf;
	unsigned char *altsetting;
	int rc;

	fake_dev_buf = kzalloc(FAKE_DEV_BUF_SIZE, GFP_KERNEL);
	fake_intf_buf = kzalloc(0x20, GFP_KERNEL);
	fake_altsetting_buf = kzalloc(0x10, GFP_KERNEL);
	fake_ep_array = kzalloc(2 * 0x2c, GFP_KERNEL);
	if (!fake_dev_buf || !fake_intf_buf || !fake_altsetting_buf || !fake_ep_array) {
		printk(KERN_ERR "OmapNKS4ProbeInject: kzalloc failed\n");
		nks4_inject_free_all();
		return -ENOMEM;
	}

	/* fake "usb_device": dev = fake_dev_buf + 100, so (dev - 100) == fake_dev_buf
	 * is the sDeviceInstance region the URB-configure helpers read. */
	dev = (unsigned char *)fake_dev_buf + FAKE_DEV_OFFSET;
	dev_addr = (unsigned long)dev;

	*(int *)((char *)fake_dev_buf + 0x00) = 1;   /* sDeviceInstance[0]: device address */
	*(int *)((char *)fake_dev_buf + 0x1c) = 3;   /* sDeviceInstance[7]: USB_SPEED_HIGH */
	*(unsigned short *)(dev + 0xb8) = NKS4_VENDOR_ID;
	*(unsigned short *)(dev + 0xba) = NKS4_PRODUCT_ID;

	/* fake endpoint array: entry 0 = interrupt-IN, entry 1 = bulk-OUT. */
	nks4_inject_write_ep((unsigned char *)fake_ep_array + 0 * 0x2c,
			      NKS4_EP_INT_ADDR, 0x03, NKS4_EP_MAXPACKET, 1);
	nks4_inject_write_ep((unsigned char *)fake_ep_array + 1 * 0x2c,
			      NKS4_EP_BULK_ADDR, 0x02, NKS4_EP_MAXPACKET, 0);

	/* fake "altsetting": bNumEndpoints=2, endpoint array pointer. */
	altsetting = (unsigned char *)fake_altsetting_buf;
	altsetting[4] = 2;
	*(void **)(altsetting + 0xc) = fake_ep_array;

	/* fake "usb_interface": word0=altsetting, word7 (0x1c)=dev. */
	intf = (int *)fake_intf_buf;
	intf[0] = (int)(unsigned long)altsetting;
	intf[7] = (int)dev_addr;

	printk(KERN_INFO "OmapNKS4ProbeInject: calling OmapNKS4Probe() with a synthetic "
			  "vendor=%04x product=%04x, 1 int-IN + 1 bulk-OUT ep\n",
	       NKS4_VENDOR_ID, NKS4_PRODUCT_ID);

	rc = OmapNKS4Probe((struct usb_interface *)fake_intf_buf);

	if (rc == 0) {
		printk(KERN_INFO "OmapNKS4ProbeInject: OmapNKS4Probe() returned 0 (SUCCESS) - "
				  "the virtual NKS4 board is up\n");
	} else {
		printk(KERN_ERR "OmapNKS4ProbeInject: OmapNKS4Probe() returned %d (FAILED)\n", rc);
	}

	/* Deliberately not freed on success: OmapNKS4Probe() stashed pointers derived
	 * from these buffers (sDeviceInstance, the URBs' +0x28 "dev" field) into
	 * OmapNKS4Module.ko's own live state - freeing them out from under it would
	 * leave dangling pointers a later disconnect/cleanup path could touch. Freed
	 * only on the failure path, where OmapNKS4Probe() itself has already torn
	 * down anything it allocated. */
	if (rc != 0)
		nks4_inject_free_all();

	/* Return 0 (module stays loaded) regardless of rc, so dmesg is inspectable
	 * after the fact and, on success, the buffers above stay alive for as long
	 * as OmapNKS4Module.ko itself is using them. */
	return 0;
}

static void __exit nks4_probe_inject_exit(void)
{
	printk(KERN_INFO "OmapNKS4ProbeInject: unloading (leaking any still-in-use fake "
			  "device buffers on purpose - see init's own comment)\n");
}

module_init(nks4_probe_inject_init);
module_exit(nks4_probe_inject_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Direct-probe-injection virtual NKS4 board (dummy_hcd TCG-hang workaround)");
MODULE_AUTHOR("kronosology");
