/*
 * message_port.h  -  CMessagePort : public CModule, Stage 6 breadth sweep,
 * 2026-07-25 (small-derived-module follow-up batch, see dump_man_mod.h /
 * edit_man.h for the shared "MMainXxx 9-member family" context).
 *
 * GROUND TRUTH: `MMainViewer()` (mains.cpp, already Tier A) builds the base
 * `CModule("ViewBase")` and vtable-swaps in `PTR__CMessagePort_08e88468` --
 * unchanged, same "mains.cpp already produces the correct object" precedent as
 * every other sibling in this batch. NOTE the real name mismatch (ground truth's
 * real class is `CViewBase`/`CReceiveFromModules`-shaped per its own typeinfo
 * name, mains.cpp's `MMainViewer()` builds a `CModule("ViewBase")`, but the
 * INSTALLED vtable and every method below is qualified `CMessagePort::` in
 * `nm -C` -- both names are real, ground truth itself multiply-derives this one
 * module from more than one base; `CMessagePort` is the base this batch's own
 * survey actually found methods under, so that's the name used here, matching
 * the existing `PTR__CMessagePort_08e88468` symbol mains.cpp already declared).
 *
 * UNLIKE every other sibling in this batch, `CMessagePort`'s own `Setup()`/
 * `Config()`/`Start()` (.text+0x0814b6c0/0x0814b6d0/0x0814b6e0, 3 bytes each) are
 * ALL THREE confirmed genuinely empty (`return 0;`) -- no sibling CTask is ever
 * constructed here. This module's real value is its own 6 extra virtual methods
 * (`PTR__CMessagePort_08e88468`'s slots 7-12, beyond CModule's base 7) -- a real,
 * substantial message-routing/view-dispatch interface: `AddView(CTask*)`/
 * `AddView(COutLink*)` (508-643 bytes), `RemoveOutView`/`RemoveInView`
 * (877-1305 bytes), `DisconnectPort` (1305 bytes), `Dispatch` (963 bytes).
 * Genuinely out of scope for this pass (each pulls in further undecoded
 * CTask/COutLink-family internals well beyond this batch's "small derived
 * module" scope, closer in size/depth to `CClientCommServer`'s own dispatch
 * methods than to this batch's other 3 siblings) -- NOT reconstructed; only the
 * ctor and the 3 confirmed-empty lifecycle methods are Tier A here.
 */

#ifndef MESSAGE_PORT_H
#define MESSAGE_PORT_H

#include "module.h"

class CMessagePort : public CModule {
public:
	/* .text+0x0814cea0, 51 bytes. Ground truth's own real boot-path caller
	 * (mains.cpp's MMainViewer()) builds an equivalent object by hand
	 * instead of calling this -- same "provided for structural completeness"
	 * status as CDumpManMod::CDumpManMod(). Real ctor takes no name argument
	 * of its own (always CViewBase::SysName, "ViewBase" -- mains.cpp's own
	 * existing constant).
	 */
	CMessagePort();

	/* .text+0x0814b6c0, 3 bytes. Confirmed genuinely `return 0;`. */
	void Setup();

	/* .text+0x0814b6d0, 3 bytes. Confirmed genuinely `return 0;`. */
	void Config();

	/* .text+0x0814b6e0, 3 bytes. Confirmed genuinely `return 0;`. */
	void Start();

private:
	/* Real: 2 extra fields beyond CModule's own base (mains.cpp's
	 * MMainViewer() zeroes both by hand: a short at +0x2c, an int at +0x30
	 * -- meaning not decoded).
	 */
	short mUnknown2c;
	int   mUnknown30;
};

extern "C" void CMessagePortSetupVSlot(void *obj);
extern "C" void CMessagePortConfigVSlot(void *obj);
extern "C" void CMessagePortStartVSlot(void *obj);

#endif /* MESSAGE_PORT_H */
