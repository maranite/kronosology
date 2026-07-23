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

	/* Set by Connect() (via CSTGHandle::Access, twice); read by
	 * LoadStoredSettings(). Real global, not a class member -- matches the
	 * disassembly (both are file-scope statics in the real binary, not fields
	 * of any USTGUserAPI instance -- the class has no instances, only statics).
	 */
	static void *mFrontPanelStatusAddress;

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
