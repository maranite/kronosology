/*
 * dd_driver_io.h  -  CDDriverIO, the static (no-instance) optical/ATA-media
 * driver-dispatch class beneath CScsiDriverBase (scsi_driver_base.h's own
 * "the deeper optical-media driver cluster" note). 85 `nm -C` methods total;
 * this pass lands the 13 that are genuinely unambiguous. Reconstructed
 * 2026-07-29, CDDriverIO/CScsiDriverBase follow-up round.
 *
 * REAL SHAPE: CDDriverIO has no instances anywhere in the binary -- every
 * method is `__cdecl` (not `__thiscall`) and operates purely on static class
 * members. `Initialize()` (0x0830d0e0, not reconstructed this pass -- see
 * below) fills `sm_poDriverApi[0..9]` with `new CAtaApi(...)`/
 * `new CScsiApi(...)` instances for however many real SATA/optical devices
 * `CDiskUtil::GetNumActiveSATADrives()` reports, confirming the array is
 * exactly 10 entries (indices 0..9, `EDevice_Id`'s real range).
 *
 * `sm_poDriverApi[i]` is a pointer to a POLYMORPHIC `CAtaApi`/`CScsiApi`
 * instance -- neither class is reconstructed. Every dispatching method here
 * reads the object's own +0x0 vtable pointer, indexes a FIXED byte offset
 * into that vtable, and calls through it -- same raw-indirect-dispatch
 * treatment already established for `CSTGProgramSlot`'s own vtable slot 7 in
 * OA.ko (oa_global.h) and reused here since `CAtaApi`/`CScsiApi`'s real
 * vtable layout/RTTI isn't independently reconstructed either. See
 * `DispatchExecuteCommand()` in dd_driver_io.cpp.
 *
 * `devstat_tab[10]` is a real `.bss` byte array (one status-flag byte per
 * device, same 10-entry indexing as `sm_poDriverApi`) -- bit meanings
 * recovered empirically from each method's own read/write (bit3=unit-
 * attention-pending, bit2=asleep, bit0=write-protected, bit4=busy).
 *
 * `s_senseKey`/`s_senseAsc`/`s_senseAscq` are 3 contiguous `.bss` bytes
 * (`scsi_req_sense(EDevice_Id)::senseinfo`, `DAT_093b0e21`, `DAT_093b0e22`)
 * -- a real static SCSI REQUEST SENSE result (key/ASC/ASCQ), shared across
 * all devices. Ground truth wraps first-use zero-init in an
 * `__cxa_guard_acquire`/`_release` pair; modeled here as ordinary zero-
 * initialized statics instead (observably identical -- BSS is already zero
 * on first use, so the guard's own zero-fill is a no-op in practice; not
 * worth hand-modeling the guard machinery itself).
 *
 * DEFERRED, 3 reasons (not implemented this pass):
 *
 * 1. Ghidra itself flags the call site as unrecovered ("Could not recover
 *    jumptable... Too many branches", "Treating indirect jump as call") --
 *    the visible decompiled body (esp. a trailing indirect call with NO
 *    visible arguments, which cannot be a real virtual call taking `this`)
 *    is very likely not a faithful recovery. Landing this as literal ground
 *    truth risks silently wrong behavior:
 *      GetTypeOfDevice, GetTargetId, SetTargetId, SetTimeout,
 *      EnableAsyncMode, IsAsyncCommandCompleted
 *
 * 2. The response buffer passed to the vtable dispatch has a Ghidra-split
 *    local (e.g. test_wp's `local_27`) that is READ after the call but never
 *    WRITTEN anywhere in the visible body -- meaning the real callee
 *    (unreconstructed CAtaApi/CScsiApi) must write further into the buffer
 *    than this call site alone can confirm. Every method landed this pass
 *    was individually checked for this exact pattern and has none (see
 *    dd_driver_io.cpp per-method comments):
 *      scsi_removal_lock, test_wp, scsi_sleep, WakeupDevice
 *
 * 3. Depends on constructing an unreconstructed `CDeviceInfo` object
 *    (`CDeviceInfo::init(...)`), whose own result is then never read in the
 *    visible body -- landing it would mean linking a whole new class for a
 *    call whose only purpose here appears to be a vestigial/dead
 *    construction:
 *      test_devicechg
 *
 * ALSO DEFERRED: InitProcessBar, UpdateMsg, EnableProgress, DoneProgress,
 * SetProcessBar, EnableCancelBtn -- all forward straight to an
 * unreconstructed `CFMBrowseForm` (205 pending methods of its own, a UI
 * form class entirely out of scope this pass).
 *
 * ALSO NOT ATTEMPTED: `Initialize()` itself (935B, real `new CAtaApi`/
 * `new CScsiApi` construction loop -- needs both classes reconstructed
 * first) and the remaining ~50 `scsi_*`/`ata_*` low-level SCSI/ATA command
 * builders (same family as CScsiDriverBase's own 35 `SetParamXxx`, likely
 * similarly tractable in a dedicated future pass).
 */
#ifndef DD_DRIVER_IO_H
#define DD_DRIVER_IO_H

class CDDriverIO {
public:
	static unsigned char HasInternalCDRW();
	static void EnableDevInfoCache(int enable);
	static void SetFMBrowseToReport(void *browse);
	static void ClearFMBrowseToReport();

	/* Both real bodies are unconditional `return 0;` -- no device access at
	 * all despite taking an EDevice_Id parameter in the mangled prototype.
	 */
	static int warmupdrive();
	static int cooldowndrive();

	static unsigned int rate2speed(unsigned short rate);
	static unsigned int speed2rate(unsigned char speed);

	/* Real body ignores both mangled parameters (EDevice_Id, unsigned
	 * short), unconditionally `return 1;`.
	 */
	static int scsi_mode_sel();

	static bool prechkdiskchg(int deviceId);
	static char GetProgress(unsigned char deviceId, int *progressOut);
	static char ExecuteCommand(int msgType, char *pbuf);
	static unsigned char *scsi_req_sense(unsigned int deviceId);

	/* Public (not private) for the same reason CScsiDriverBase::sm_oDataBuf
	 * is public: host KAT tests need to mock the target CAtaApi/CScsiApi
	 * object and its response buffers directly.
	 */
	static unsigned char *sm_poDriverApi[10];
	static unsigned char devstat_tab[10];
	static unsigned char s_senseKey;
	static unsigned char s_senseAsc;
	static unsigned char s_senseAscq;

private:
	static unsigned char sm_bHasInternalCDRW;
	static int sm_bEnableDevInfoCache;
	static void *sm_poFMBrowse;
};

#endif /* DD_DRIVER_IO_H */
