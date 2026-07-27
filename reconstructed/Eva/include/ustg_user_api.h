/*
 * ustg_user_api.h  -  Eva's IPC client to OA.ko (Stage 1 boot path / Stage 2 substrate).
 *
 * USTGUserAPI is Eva's counterpart to OA.ko's own /proc/.oacmd + RTAI-FIFO + stg_direct
 * plumbing (see reconstructed/OA/README.md's "/proc/.oacmd plumbing" section and
 * [[eva_oa_ghidra_coordination]] for the shared IPC picture). Connect() is on Eva's real
 * boot path (called directly from main()); the rest of the class (message send/receive,
 * STGMessage wire format) is Stage 2, declared here only as far as Connect() and
 * LoadStoredSettings() need.
 */

#ifndef USTG_USER_API_H
#define USTG_USER_API_H

#include "eva_types.h"

/* STGMessage -- wire format Eva sends to OA.ko over the rt2user/user2rt FIFOs (Stage 2,
 * not fully reconstructed). SendSTGMessageWithSource() (see below) resolves one real
 * fact about this layout: the first field is a **u16 total message byte length**, not
 * a "message type" tag as this project's Stage-1 pass originally guessed (the field
 * LoadStoredSettings() sets to 0x10/16 turns out to be exactly the byte size of the
 * 4-field local shape LoadStoredSettings() builds -- a length prefix, corroborated
 * independently by SendSTGMessageWithSource() reading it as the write() byte count).
 * Declared opaque here (full struct still not recovered) -- SendSTGMessageWithSource()
 * casts through `const char*` for the actual byte-level write() loop rather than doing
 * pointer arithmetic on the incomplete type.
 */
struct STGMessage;

class USTGUserAPI {
public:
	/* Real boot-path call (main()). Opens the three IPC device nodes OA.ko's own
	 * init_module creates (rtai_fifos.ko / stg_rtfifo_init step -- see
	 * reconstructed/OA/README.md's init_module step 11): /dev/rtf1 (read, rt->user),
	 * /dev/rtf0 (write, user->rt), /dev/dmsg0 (read/write, stg_direct). Returns true
	 * only if all three opened successfully. Real callers (main()) do not check the
	 * return value -- preserved faithfully, not "fixed".
	 */
	static bool Connect();

	/* Real boot-path call (main(), via LoadStoredSettings). .text+0x08e280f0, 275
	 * bytes -- reconstructed (see ustg_user_api.cpp). Writes the message's own
	 * length-prefixed byte range to m_activeUser2rtFD, retrying on short writes,
	 * logging (syslog) and returning false on a hard failure.
	 */
	static bool SendSTGMessageWithSource(const STGMessage *msg);

	/* .text+0x08e280b0, 58 bytes -- reconstructed. Opens /dev/rtf7 (the "panel"
	 * FIFO), closing any previously-open fd first. Called by CLinuxPanelDriver's own
	 * ctor (mains.cpp) -- not on the traced main() boot path itself.
	 */
	static bool ConnectPanelFifo();

	/* --- Stage 2 IPC substrate, added 2026-07-25 (Eva/README.md's "Stage 2" gap) ---
	 * All 8 of the following are Tier A, transcribed from
	 * Decomp/EVA_Decomp/eva_export/functions/. Together with Connect()/
	 * SendSTGMessageWithSource()/ConnectPanelFifo() above, this closes out
	 * USTGUserAPI's own real send/receive/teardown surface -- the only remaining
	 * USTGUserAPI members not reconstructed are the ~150 per-subsystem
	 * `USTGAPIXxx::UpdateYyy()` wrapper classes (Stage 5/6 breadth, not IPC
	 * plumbing itself).
	 */

	/* .text+0x08e27f90, 213 bytes. Tears down every fd Connect()/
	 * ConnectPanelFifo()/ConnectUnsolicitedFifo() may have opened and releases
	 * both CSTGHandle attachments Connect() made (mSharedMem, and a second,
	 * synthesized mode=1 handle standing in for mFrontPanelStatusAddress --
	 * same double-Access()/double-Release() pairing Connect() itself uses).
	 * CORRECTED 2026-07-27 (re-check for the CBDApiInstance grep-methodology
	 * bug class -- see HARDWARE_REVIEW_LOG.md): the original "no caller found
	 * in the export at all" claim was WRONG, a full `objdump -dr -M intel`
	 * sweep of the real ground-truth binary (not a re-check of the earlier
	 * search's own scope) found 2 real callers: `CEditor::CMainTask::Exec()`
	 * (.text+0x0824aa95) and `CEditor::CMainTask::EnterCheckHardware(int)`
	 * (.text+0x0824b0e0) -- both already-named in editor.h's own header
	 * comment as touching USTGUserAPI, just not down to this specific method
	 * before now. Not called anywhere on THIS RECONSTRUCTION's own traced
	 * boot/shutdown path (COmegaInterface::Close() just sets s_bRunning=0),
	 * because both real ground-truth callers are themselves still Tier-B
	 * stubs (editor.cpp) whose bodies were never filled in -- so no functional
	 * behavior actually changes here, but "no caller found in the export" was
	 * a false claim about ground truth, not just an honest "not on our own
	 * wired path yet" scoping.
	 */
	static void Disconnect();

	/* .text+0x08e28070, 58 bytes. Opens /dev/rtf5 (O_NONBLOCK, 0x800) -- a
	 * fifth real device node, for the "unsolicited" message channel
	 * ReadUnsolicitedMessage() reads from. CORRECTED 2026-07-27 (same re-check
	 * as Disconnect() above): has exactly 1 real ground-truth caller,
	 * `CStorage::Initialize(CStaticLabel*, PegRect&)` (.text+0x08a57ee1) --
	 * itself deep, unavoidably-Peg-coupled and confirmed out of scope
	 * (HARDWARE_REVIEW_LOG.md's CStorage entry, same day). Real, not currently
	 * exercised by anything this reconstruction has built -- "no caller found
	 * in the export" was still wrong as a ground-truth claim, just with no
	 * practical consequence here since the one real caller stays out of scope
	 * either way.
	 */
	static bool ConnectUnsolicitedFifo();

	/* .text+0x08e282d0, 113 bytes. Blocking length-prefixed read off the
	 * active rt->user fd: first reads exactly 2 bytes (the u16 length prefix,
	 * same field SendSTGMessageWithSource() writes), then -- only if that
	 * length fits the caller's buffer (param_2) -- reads the remaining
	 * (length-2) bytes. Returns total bytes read (>=2) on success, 0 on any
	 * short-read/oversize-message/no-fd case. CORRECTED 2026-07-27: "no caller
	 * found in the export" was wrong -- a full-binary `objdump -dr` sweep
	 * found 200+ real call sites, essentially all inside the ~150 per-subsystem
	 * `USTGAPIXxx::UpdateYyy()`/`ReceiveXxxMessage()` wrapper family this same
	 * header already names above as "Stage 5/6 breadth, not IPC plumbing
	 * itself" (e.g. `USTGAPICombi::SharedMemCombiDump`, `USTGAPIProgram::
	 * SharedMemProgramDump`, `USTGAPICalibration::ReceiveSimpleMessage`,
	 * `USTGAPIFrontPanel::SetLED`, plus `CSTGUnsolMsgProcessor::Process()` and
	 * `CPerformanceDownloader::STGSharedMemProgramDump`) -- confirms this IPC
	 * substrate genuinely IS the foundation that entire un-reconstructed layer
	 * depends on, not dead completeness work. None of those callers are
	 * reconstructed in this project, so ReadMessage() is still not reachable
	 * on this project's OWN traced call graph -- only the "no caller anywhere
	 * in the export" framing was false.
	 */
	static int ReadMessage(char *buf, unsigned bufSize);

	/* .text+0x08e28350, 282 bytes. Same length-prefixed read as ReadMessage(),
	 * but polls in a loop against a deadline computed from gettimeofday() +
	 * timeoutMs, busy-looping (no sleep) until either a full message arrives
	 * or the deadline passes. Real quirk preserved: if msg==NULL, it does
	 * nothing but spin until the deadline with no I/O at all -- a real "just
	 * wait" mode, not a bug. CORRECTED 2026-07-27: has 7 real callers, all
	 * inside `USTGAPICalibration`'s own methods (e.g. `ReceiveSimpleMessage`,
	 * `EndJSXCalibration`) -- same out-of-scope USTGAPIXxx family as
	 * ReadMessage() above, "no caller found" was equally wrong here.
	 */
	static int ReadMessageWithTimeout(STGMessage *msg, unsigned bufSize, unsigned timeoutMs);

	/* .text+0x08e28470, 122 bytes. Same length-prefixed read shape as
	 * ReadMessage(), but off m_rtUnsolFifo (the ConnectUnsolicitedFifo() fd)
	 * instead of the active rt2user fd. CORRECTED 2026-07-27: has 9 real
	 * callers, all inside `CSTGUnsolMsgProcessor::Process()` (.text+0x0891c3d0
	 * region) -- the real unsolicited-message dispatch loop, itself part of
	 * the already-documented-out-of-scope `CSTGUnsolMsgHandler` ~20-method
	 * dispatch family. Same correction as the two methods above.
	 */
	static int ReadUnsolicitedMessage(char *buf, unsigned bufSize);

	/* .text+0x08e28220, 166 bytes. Length-prefixed write to
	 * m_user2rtPanelFifo (the ConnectPanelFifo() fd) -- same retry-on-partial-
	 * write + syslog-on-hard-failure shape as SendSTGMessageWithSource(), just
	 * against a different fd and without the mNowStopMessaging gate. Real
	 * quirk preserved: a zero-length message (first u16 == 0) returns true
	 * (1) without writing anything -- same "empty send is a no-op success" as
	 * SendSTGMessageWithSource(). CORRECTED 2026-07-27: has 5 real callers, all
	 * inside `USTGAPIFrontPanel`'s own methods (`SetLEDBlinking`/`ResetLED`/
	 * `SetLED16Bit`/`Beep`/`SetLED`) -- same out-of-scope USTGAPIXxx family as
	 * ReadMessage() above.
	 */
	static bool SendPanelMessage(const STGMessage *msg);

	/* .text+0x08e285c0/0x08e28560/0x08e284f0, 81/85/104 bytes. A real, tiny
	 * out-of-band progress-reporting channel via a /proc file
	 * (`/proc/OmapNKS4ProgressBar`) -- NOT the rtf/dmsg0 FIFO substrate at
	 * all. GetProgress() fscanf()s "%d"; SetProgress() fprintf()s "set %d";
	 * IncrementProgress() writes the literal 3-byte string "inc" (confirmed
	 * by reading the real binary's own .rodata at DAT_08fd9367, not guessed).
	 * All three silently no-op (return 0 / do nothing) if the /proc file
	 * doesn't exist -- matches this project's OmapNKS4-virtual-driver scope
	 * ([[omapnks4_probe_success_milestone]]), though that driver doesn't
	 * currently expose this specific /proc file. CORRECTED 2026-07-27 (see
	 * HARDWARE_REVIEW_LOG.md's progress-channel entry): "no caller found in
	 * the export for any of the three" was wrong -- real callers exist for
	 * all three: `IncrementProgress()` has 72 real call sites (the KSF/KMP/
	 * KSC sample-format loader family -- `CLoadKsfManager::Load`, `CFileKMP::
	 * load`, `CFileKSC::load` -- plus `CDesktop`'s own ctor and
	 * `OnStartup()`); `SetProgress()` has 1 real caller (`CDesktop::
	 * OnStartup()`); `GetProgress()` has 2 real callers (both inside
	 * `CStorage::Initialize(CStaticLabel*, PegRect&)`). All real
	 * callers are in genuinely out-of-scope subsystems (sample/disk loading,
	 * the Peg-coupled `CDesktop`/`CStorage`) that this project has never
	 * touched -- so this progress channel is confirmed to be real,
	 * actively-used firmware instrumentation on real hardware, not vestigial,
	 * even though nothing in this reconstruction currently drives it.
	 */
	static int GetProgress();
	static void IncrementProgress();
	static void SetProgress(int value);

	/* Set by Connect() (via CSTGHandle::Access, twice); read by
	 * LoadStoredSettings(). Real global, not a class member -- matches the
	 * disassembly (both are file-scope statics in the real binary, not fields
	 * of any USTGUserAPI instance -- the class has no instances, only statics).
	 */
	static void *mFrontPanelStatusAddress;

	/* Test-only seam (verify/test_ustg_user_api.cpp): the real class has no
	 * public setters for its fd caches (Connect()/ConnectPanelFifo()/
	 * ConnectUnsolicitedFifo() always open real device nodes that don't exist
	 * on a host build). This lets the KAT point the length-prefix read/write
	 * logic at real host pipes instead, to genuinely exercise
	 * ReadMessage()/ReadMessageWithTimeout()/ReadUnsolicitedMessage()/
	 * SendPanelMessage()/Disconnect() rather than relying on decompile
	 * cross-check alone -- same spirit as level_manager_array.h's own
	 * extraction for verify/test_level_manager_array.cpp, just via a friend
	 * instead of a header split (these fields are file-local statics in the
	 * real binary, not worth their own header).
	 */
	friend struct UstgUserApiTestHooks;

	/* CSTGUnsolMsgHandler::EditApiSendParamMsg() (stg_unsol_msg_handler.cpp,
	 * Stage 6 batch 2, 2026-07-25) toggles mNowStopMessaging around every real
	 * EditApi "set param" dispatch, matching SendSTGMessageWithSource()'s own
	 * real gate above -- friended rather than adding a public setter, same
	 * "real class has no public setter" shape as the fd-cache test hook above.
	 */
	friend class CSTGUnsolMsgHandler;

private:
	static CSTGHandle *mSharedMem;

	/* fd caches. -1 == "not yet opened" is the real sentinel Connect() checks
	 * against before calling open() again -- so Connect() is safe to call more
	 * than once (only actually opens missing fds).
	 */
	static int m_rt2userFifo;
	static int m_user2rtFifo;
	static int m_userRtDirect;
	static int m_activeRt2userFD;
	static int m_activeUser2rtFD;
	static int m_user2rtPanelFifo;

	/* /dev/rtf5 fd cache -- ConnectUnsolicitedFifo()'s own fd, read by
	 * ReadUnsolicitedMessage() and closed by Disconnect(). Added 2026-07-25.
	 */
	static int m_rtUnsolFifo;

	/* Real globals SendSTGMessageWithSource() checks before doing any work: if
	 * mNowStopMessaging is set (and mForceMessaging isn't also set, which
	 * overrides it), the call is a silent no-op returning false. Real setters not
	 * traced -- both start false/0 here, so this pass's own boot path always takes
	 * the "send for real" branch.
	 */
	static char mNowStopMessaging;
	static char mForceMessaging;
};

#endif /* USTG_USER_API_H */
