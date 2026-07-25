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
	 * Real finding: not called anywhere on this project's traced boot/shutdown
	 * path (COmegaInterface::Close() just sets s_bRunning=0) -- no caller found
	 * in the export at all; reconstructed for IPC-surface completeness, not
	 * because it's currently reachable.
	 */
	static void Disconnect();

	/* .text+0x08e28070, 58 bytes. Opens /dev/rtf5 (O_NONBLOCK, 0x800) -- a
	 * fifth real device node, for the "unsolicited" message channel
	 * ReadUnsolicitedMessage() reads from. No caller found in the export
	 * either (same as Disconnect()) -- real, but not currently exercised.
	 */
	static bool ConnectUnsolicitedFifo();

	/* .text+0x08e282d0, 113 bytes. Blocking length-prefixed read off the
	 * active rt->user fd: first reads exactly 2 bytes (the u16 length prefix,
	 * same field SendSTGMessageWithSource() writes), then -- only if that
	 * length fits the caller's buffer (param_2) -- reads the remaining
	 * (length-2) bytes. Returns total bytes read (>=2) on success, 0 on any
	 * short-read/oversize-message/no-fd case. No caller found in the export.
	 */
	static int ReadMessage(char *buf, unsigned bufSize);

	/* .text+0x08e28350, 282 bytes. Same length-prefixed read as ReadMessage(),
	 * but polls in a loop against a deadline computed from gettimeofday() +
	 * timeoutMs, busy-looping (no sleep) until either a full message arrives
	 * or the deadline passes. Real quirk preserved: if msg==NULL, it does
	 * nothing but spin until the deadline with no I/O at all -- a real "just
	 * wait" mode, not a bug. No caller found in the export.
	 */
	static int ReadMessageWithTimeout(STGMessage *msg, unsigned bufSize, unsigned timeoutMs);

	/* .text+0x08e28470, 122 bytes. Same length-prefixed read shape as
	 * ReadMessage(), but off m_rtUnsolFifo (the ConnectUnsolicitedFifo() fd)
	 * instead of the active rt2user fd. No caller found in the export.
	 */
	static int ReadUnsolicitedMessage(char *buf, unsigned bufSize);

	/* .text+0x08e28220, 166 bytes. Length-prefixed write to
	 * m_user2rtPanelFifo (the ConnectPanelFifo() fd) -- same retry-on-partial-
	 * write + syslog-on-hard-failure shape as SendSTGMessageWithSource(), just
	 * against a different fd and without the mNowStopMessaging gate. Real
	 * quirk preserved: a zero-length message (first u16 == 0) returns true
	 * (1) without writing anything -- same "empty send is a no-op success" as
	 * SendSTGMessageWithSource(). No caller found in the export.
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
	 * currently expose this specific /proc file. No caller found in the
	 * export for any of the three.
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
