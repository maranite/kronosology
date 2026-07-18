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
 * SCOPE, this pass: get enumeration + `OmapNKS4Probe`'s own descriptor
 * checks to succeed, so `OmapNKS4Init`'s own `wait_for_completion_timeout`
 * observes a real completion instead of timing out. Deep wire-protocol
 * fidelity (responding correctly to every real `COmapNKS4Command` word,
 * matching the real NKS4 ARM firmware's own confirmed behavior from this
 * project's `NKS4PanelFirmware/` reconstruction) is a natural follow-on,
 * not attempted in full here - the interrupt-IN/bulk-OUT completion
 * handlers below are deliberately minimal (log + ACK), not a full protocol
 * implementation. Honestly scoped rather than claimed complete.
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
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>

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

/* Interface class 0xff (vendor-specific): confirmed real value, see
 * CLAUDE.md's own documented ground truth ("one real usb_device_id entry
 * (vendor 0x0944/product 0x1005/vendor-specific interface class 0xff)",
 * cross-confirmed in OmapNKS4Module's own README.md main.cpp section on the
 * real `struct usb_driver` reconstruction). */
static struct usb_interface_descriptor nks4_intf_desc = {
	.bLength		= USB_DT_INTERFACE_SIZE,
	.bDescriptorType	= USB_DT_INTERFACE,
	.bInterfaceNumber	= 0,
	.bAlternateSetting	= 0,
	.bNumEndpoints		= 2,
	.bInterfaceClass	= 0xff,
	.bInterfaceSubClass	= 0,
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
 *  Bulk-OUT / interrupt-IN completion handlers - deliberately minimal, see
 *  file header's own "SCOPE, this pass" note. Real COmapNKS4Command wire
 *  protocol handling (CommunicationCheck ack, version query responses
 *  matching command.cpp's own confirmed encodings) is a natural follow-on
 *  once enumeration itself is confirmed working, not attempted here.
 * ========================================================================= */

static void nks4_ep0_complete(struct usb_ep *ep, struct usb_request *req)
{
	/* nothing to do: only used for zero-length status-stage acks */
}

static void nks4_bulk_out_complete(struct usb_ep *ep, struct usb_request *req)
{
	if (req->status == 0 && req->actual > 0)
		printk(KERN_INFO "OmapNKS4VirtualBoard: bulk-OUT %d bytes (not yet decoded - see file header)\n",
		       req->actual);

	/* re-queue to keep receiving */
	req->length = NKS4_EP_MAXPACKET;
	if (usb_ep_queue(ep, req, GFP_ATOMIC))
		printk(KERN_ERR "OmapNKS4VirtualBoard: bulk-OUT re-queue failed\n");
}

static void nks4_int_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	/* Idle: this substitute doesn't yet generate real panel events
	 * (button/knob/pedal state) - see file header's own scope note. A
	 * real interrupt-IN transfer only needs to be queued when there's
	 * genuine event data to report; nothing re-queues here by design. */
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
			 * whole descriptor set in one control transfer. */
			int off = 0;

			nks4_config_desc.wTotalLength = cpu_to_le16(
				sizeof(nks4_config_desc) + sizeof(nks4_intf_desc) +
				sizeof(nks4_int_ep_desc) + sizeof(nks4_bulk_ep_desc));

			memcpy(buf + off, &nks4_config_desc, sizeof(nks4_config_desc));
			off += sizeof(nks4_config_desc);
			memcpy(buf + off, &nks4_intf_desc, sizeof(nks4_intf_desc));
			off += sizeof(nks4_intf_desc);
			memcpy(buf + off, &nks4_int_ep_desc, sizeof(nks4_int_ep_desc));
			off += sizeof(nks4_int_ep_desc);
			memcpy(buf + off, &nks4_bulk_ep_desc, sizeof(nks4_bulk_ep_desc));
			off += sizeof(nks4_bulk_ep_desc);
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
		} else {
			usb_ep_disable(dev->ep_int);
			usb_ep_disable(dev->ep_bulk);
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
		dev->req_ep0->length = min_t(int, len, wLength);
		dev->req_ep0->zero = (len < wLength);
		dev->req_ep0->complete = nks4_ep0_complete;
		return usb_ep_queue(gadget->ep0, dev->req_ep0, GFP_ATOMIC);
	}
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
