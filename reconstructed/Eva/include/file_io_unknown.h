/*
 * file_io_unknown.h  -  CFileIoUnknown, the smallest concrete CFileIoBase override
 * (file_io_base.h's own header comment lists this whole family as deliberately
 * out of scope for the CFileIoBase batch; this is the first of that family taken
 * on, 2026-07-27 storage-cluster follow-up).
 *
 * REAL SHAPE: CFileIoUnknown overrides only 6 of CFileIoBase's 49 virtuals
 * (`.text+0x08318d30`..`0x08318e80`, confirmed via `nm -C` -- no other
 * CFileIoUnknown:: symbol exists in the export) plus the compiler-generated
 * dtor pair (`.text+0x08995490` D1 / `0x089954a0` D0). Every other inherited
 * method resolves straight through to CFileIoBase's own stub body -- no vtable
 * slot for them is patched, confirmed by `vtable for CFileIoUnknown`
 * (`.data.rel.ro+0x08f31740`, 0xd4 bytes, same 53-slot layout as `vtable for
 * CFileIoBase` with only the 6 overridden slots differing). No separate
 * `CFileIoUnknown::CFileIoUnknown()` ctor symbol exists either -- construction
 * sites call `CFileIoBase::CFileIoBase()` directly and patch the vtable pointer
 * inline (the trivial-derived-ctor folding GCC does when a subclass adds no
 * members and defines no ctor body of its own); an implicit, compiler-generated
 * default ctor here is faithful.
 *
 * The 6 overrides split into two shapes:
 *
 *   get_iotype()/fmount(EDevice_Id)/funmount(EDevice_Id) -- 3/6/3 bytes,
 *   unconditional immediate-return leaves, no calls. Sentinels: 1 (this class's
 *   own IO-type tag, see file_io_base.h's EFileIOType comment), -1, 0
 *   respectively -- funmount's `return 0` (success) is notably NOT the
 *   CFileIoBase default (-1 + assert); CFileIoUnknown never fails to "unmount"
 *   since it never actually opens anything.
 *
 *   getmediainfo()/format(2-arg)/format(3-arg) -- genuine forwarding bodies,
 *   the interesting part of this override. All 3 reach into out-of-scope
 *   sibling classes (CDDriverIO/CFilesys/CMediaInfo -- file_io_base.h's own
 *   "OUT OF SCOPE" list), modeled as inert EvaVTableStub-style stand-ins (same
 *   convention as panel_ifc_task.cpp's PegMessageQueuePush/
 *   ControlSurfaceGetInstance) rather than real mangled externs, so `make
 *   verify` stays fully linkable:
 *
 *     getmediainfo(EDevice_Id device, CMediaInfo *info): real body queries
 *     `CDDriverIO::read_capacity(device, unsigned short&)` (.text+0x0830e470,
 *     85-method optical-media driver, out of scope) for a 64-bit sector count
 *     (its EDX:EAX return value is fed straight into CMediaInfo::init()'s own
 *     64-bit `size` parameter -- confirmed by direct register tracing, no
 *     intermediate store) plus a per-device write-protect flag byte
 *     (`*(byte*)(device + 0x93b0d54) & 1`, a real global table this
 *     reconstruction doesn't otherwise model), then calls
 *     `CMediaInfo::init(info, "Unformatted" [.rodata+0x8eedd09, confirmed via
 *     objdump -s], type=1, flag, size, extra=0)` (.text+0x08317be0, CMediaInfo
 *     out of scope) and
 *     returns 0 unconditionally. Modeled: read_capacity as an inert stand-in
 *     returning 0/0, the device-flags table read as a real (if unnamed) global
 *     byte array, CMediaInfo::init as an inert stand-in (does nothing to the
 *     passed-in *info -- CMediaInfo has no known members to write faithfully).
 *
 *     format(EDevice_Id device, int arg2) [2-arg]: real body checks the SAME
 *     per-device flags byte's neighbor (`*(byte*)(device + 0x93b0d5e) & 0x30`)
 *     to pick an EFileIOType selector (3 if any of those bits are set, 6
 *     otherwise -- both real immediates, symbolic names not recovered), calls
 *     `CFilesys::get_fileioptr(selector)` (.text+0x083217e0, 87-method VFAT
 *     driver, out of scope) to obtain a CFileIoBase-derived driver instance,
 *     then TAIL-CALLS that instance's own `format(device, arg2)` virtual slot
 *     (confirmed: the real `jmp [edx+0x38]` lands exactly on CFileIoBase's own
 *     format(2-arg) vtable offset, slot 14 -- same layout file_io_base.h
 *     already establishes). Modeled as a real `return`-of-virtual-call
 *     (preserves the tail-call shape faithfully); get_fileioptr() is an inert
 *     stand-in returning a pointer to a local static plain CFileIoBase instance
 *     (itself fully real, file_io_base.h) rather than a raw EvaVTableStub
 *     function-pointer array, since the real return type needs a genuinely
 *     dispatchable object at this exact vtable layout, not a bare function
 *     pointer -- CFileIoBase's own already-real format() bodies (-1 with an
 *     assert-log) are exactly the correct "safe generic driver" behavior for
 *     the stand-in to fall through to.
 *
 *     format(EDevice_Id device, int arg2, EFatType fatType) [3-arg]: same shape
 *     as the 2-arg overload but with the selector FIXED at 6 (no flags-byte
 *     branch), tail-calling the returned driver's format(device, arg2,
 *     fatType) -- vtable slot 15 (`jmp [edx+0x3c]`, CFileIoBase's own
 *     format(3-arg) offset).
 *
 * Dtor (D1 `.text+0x08995490` / D0 `.text+0x089954a0`): SAME shape as
 * CFileIoBase's own dtor pair (file_io_base.h) -- D1 just resets the vtable
 * pointer to CFileIoBase's own vtable+8 (`0x08f31828` == `vtable for
 * CFileIoBase`+8, confirmed by direct symbol-address arithmetic; NOT
 * CFileIoUnknown's own vtable -- standard Itanium construction-order behavior
 * once CFileIoUnknown's own trivial per-class step folds into its base's), D0
 * additionally free()s `this`. An ordinary (compiler-default-shaped) virtual
 * dtor is faithful, no hand-written body needed.
 */

#ifndef FILE_IO_UNKNOWN_H
#define FILE_IO_UNKNOWN_H

#include "file_io_base.h"

class CFileIoUnknown : public CFileIoBase {
public:
	/* .text+0x08318d30, 3 bytes. Real: `return 1;` (this class's own
	 * EFileIOType tag -- file_io_base.h's EFileIOType comment).
	 */
	virtual int get_iotype();

	/* .text+0x08318d40, 6 bytes. Real: `return -1;` (no assert call, unlike
	 * CFileIoBase's own fmount(EDevice_Id) override).
	 */
	virtual int fmount(EDevice_Id device);

	/* .text+0x08318d50, 3 bytes. Real: `return 0;` (success -- notably NOT
	 * CFileIoBase's -1+assert default; see header comment above).
	 */
	virtual int funmount(EDevice_Id device);

	/* .text+0x08318d60, 91 bytes. Real: queries CDDriverIO::read_capacity()
	 * and a per-device flags byte, forwards into CMediaInfo::init(), returns
	 * 0 unconditionally. See header comment for the full breakdown.
	 */
	virtual int getmediainfo(EDevice_Id device, CMediaInfo *info);

	/* .text+0x08318e20, 92 bytes. Real: picks an EFileIOType selector (3 or
	 * 6) off a per-device flags byte, tail-calls
	 * CFilesys::get_fileioptr(selector)->format(device, arg2).
	 */
	virtual int format(EDevice_Id device, int arg2);

	/* .text+0x08318dc0, 89 bytes. Real: same shape as the 2-arg overload but
	 * with the selector fixed at 6, tail-calls
	 * CFilesys::get_fileioptr(6)->format(device, arg2, fatType).
	 */
	virtual int format(EDevice_Id device, int arg2, EFatType fatType);

	/* .text+0x08995490 (D1) / 0x089954a0 (D0). Same shape as CFileIoBase's
	 * own dtor pair -- see header comment above.
	 */
	virtual ~CFileIoUnknown();
};

#endif /* FILE_IO_UNKNOWN_H */
