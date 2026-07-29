// SPDX-License-Identifier: GPL-2.0
/*
 * filesys.h  -  CFilesys: the CTask-derived virtual-filesystem
 * dispatcher that routes drive-letter-prefixed path operations
 * (fopen/fclose/mkdir/rmdir/rename/chdir/dir/...) to one of a 10-slot
 * per-device driver-pointer table, or to CD-audio/SCSI operations
 * forwarded to the separate (still entirely unreconstructed)
 * `CDDriverIO` static-method family.
 *
 * FOUND 2026-07-29 (round 46, solo). 9/68 clean pending methods landed
 * this round (68/84 total pending methods are decompiler-clean; 16 are
 * `in_stack_`/`unaff_`-flagged and deferred alongside). This class
 * turned out FAR less tractable than its own small average pending-
 * method size (118 bytes) suggested: the overwhelming majority of its
 * "meaty" methods either (a) make a genuine but UNNAMED vtable call
 * through one of the 10 per-device driver pointers (same deferral
 * class already established for CSTGKeyTrack/CSTGPatch in OA.ko), or
 * (b) forward directly into `CDDriverIO::xxx`, a real but entirely
 * separate 85-method pending cluster this round didn't attempt. Only
 * the genuinely self-contained field-only methods below were landed.
 *
 * Confirmed real per-instance layout (all landed methods' own raw
 * offsets, `this`-relative):
 *   +0x00        vptr (real: dtor sets it to `&PTR__CFilesys_08f31d38`,
 *                 opaque placeholder here, not a real vtable)
 *   +0x04/+0x08/+0x0c/+0x10/+0x14/+0x18
 *                 6 individual "file I/O type" driver pointers,
 *                 confirmed via `get_fileioptr(EFileIOType)`'s own
 *                 6-case switch (cases 1..6, default falls through to
 *                 +0x1c) -- real target/content type unrecovered,
 *                 modeled as opaque `void*`.
 *   +0x1c         mDefaultDriver -- the fallback driver pointer used
 *                 whenever a drive-letter/device index doesn't land in
 *                 the 10-slot table below (confirmed by BOTH
 *                 `get_fileioptr(EFileIOType)`'s own default case and
 *                 `get_fileioptr(char const*, EDevice_Id*)`'s own
 *                 `idx>=10` fallback).
 *   +0x20..+0x44  mDeviceDrivers[10] -- the per-device driver-pointer
 *                 table every OTHER deferred method (remove/mkdir/
 *                 rmdir/rename/chmod/chdir/dir/fopen/fclose/...) also
 *                 indexes into, confirmed independently via TWO
 *                 different real index-computation idioms that land on
 *                 the exact same base: `get_fileioptr`'s own
 *                 `this+idx*4+0x20` (idx already 0-9) and e.g.
 *                 `remove()`'s own `this + driveLetterAsciiCode*4 -
 *                 0xe4` (for `driveLetterAsciiCode == 'A' == 0x41`:
 *                 `0x41*4 - 0xe4 == 0x104 - 0xe4 == 0x20`, the SAME
 *                 base).
 *   +0x48         mStickyErrorFlag -- a one-shot "already reported"
 *                 latch, confirmed by `CheckError`'s own body (only
 *                 the FIRST `param==0` call after a real error latches
 *                 it to 3 and forces the shared `buf` scratch's own
 *                 error-code slot to 0; every later call just mirrors
 *                 `param` straight into `buf[0]`).
 *
 * Message-passing convention: this class communicates through 2
 * FILE-SCOPE statics (`msg`/`buf`, set by `setbuf()`), the SAME
 * "shared static scratch buffer" idiom already established for
 * CESDiskTask (es_disk_task.h) -- `buf` modeled as an opaque `int*`
 * (real type `SFileIOPbuf*`, layout not independently confirmed beyond
 * the raw word indices `CheckError`/`setbuf` themselves touch: index 2
 * == a "pending error" flag byte).
 *
 * `~CFilesys()` -- 2 real ground-truth addresses, genuinely DIFFERENT
 * bodies (unlike CSTGKeyTrack/CSTGPatch's byte-identical D0/D2 pairs):
 * the 11-byte one only resets the vptr; the 39-byte one additionally
 * wraps `free(this)` in `HAL_DisableInterrupts()`/
 * `HAL_EnableInterrupts()`. Reconstructed as ONE C++ destructor
 * modeling only the vptr-reset behavior, matching this project's own
 * already-established precedent for exactly this D0-vs-D2 divergence
 * (`CFileIoDos::~CFileIoDos()`, file_io_dos.h's own header comment) --
 * freeing `this` from inside its own destructor isn't meaningfully
 * portable to a host C++ object anyway.
 *
 * `run()` -- landed VERBATIM as a real, genuinely infinite
 * `for (;;) {}` (real ground truth: `do {} while(true)` with a
 * completely empty body, no `in_stack_`/`unaff_` warning, fully
 * concrete) -- almost certainly a default/never-actually-invoked
 * CTask::Run()-family placeholder. Deliberately NOT exercised by
 * verify/test_filesys.cpp (calling it would hang the test process
 * forever); only its address is taken, to prove it's real, callable,
 * correctly-typed code.
 *
 * === Deferred, 3 distinct reasons (75/84 methods) ===
 * (1) 16 methods flagged by the decompiler itself (`in_stack_`/
 *     `unaff_`/"Could not recover jumptable").
 * (2) ~50 methods with a genuine, fully-concrete virtual call through
 *     one of the 10 UNNAMED per-device driver-pointer vtable slots
 *     (`remove`/`mkdir`/`rmdir`/`rename`/`chmod`/`chdir`/`dir`/
 *     `fopen`/`fclose`/`totalfreeclus`/`audio_stop`/`finalize`/
 *     `freebytes`/`getwd`/`getmaxtrk`/`getmaxidx`/... ) -- same
 *     deferral class as CSTGKeyTrack/CSTGPatch in OA.ko: no
 *     independent confirmation of which named method occupies the
 *     target slot.
 * (3) ~9 methods forwarding directly into the separate, entirely
 *     unreconstructed `CDDriverIO` static-method family
 *     (`getdevinfo`/`test_diskchg`/`scsi_removal_lock`/
 *     `closelastses`/`getsectorsize`/...), plus `GetInstance()` (needs
 *     the 631-byte real ctor, itself not landed this round) and
 *     `directexec()` (its own sole callee `decode()` is itself
 *     `in_stack_`-flagged, reason (1)).
 */

#ifndef FILESYS_H
#define FILESYS_H

class CFilesys {
public:
	~CFilesys();

	static void eventhandling();
	static void startup();
	static void run(); /* real: infinite loop, see header comment -- never invoked by the KAT */

	int new_fptr(int deviceIdUnused, int value);

	void CheckError(int errCode);

	void *get_fileioptr(unsigned int fileIoType) const;
	void *get_fileioptr(const char *path, int *outDeviceId) const;

	static void setbuf(unsigned int msgType, int *pbuf);

private:
	unsigned char mVptrPlaceholder[4]; /* +0x00 */
	void *mFileIoPtr1;                 /* +0x04, get_fileioptr case 1 */
	void *mFileIoPtr5;                 /* +0x08, get_fileioptr case 5 */
	void *mFileIoPtr4;                 /* +0x0c, get_fileioptr case 4 */
	void *mFileIoPtr2;                 /* +0x10, get_fileioptr case 2 */
	void *mFileIoPtr3;                 /* +0x14, get_fileioptr case 3 */
	void *mFileIoPtr6;                 /* +0x18, get_fileioptr case 6 */
	void *mDefaultDriver;              /* +0x1c */
	void *mDeviceDrivers[10];          /* +0x20..+0x44 */
	int mStickyErrorFlag;              /* +0x48 */
};

#endif /* FILESYS_H */
