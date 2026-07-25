/*
 * ustg_user_api.cpp  -  see include/ustg_user_api.h.
 *
 * Connect() (.text+0x08e27ea0, 234 bytes) transcribed instruction-by-instruction from
 * the Ghidra decompile (Decomp/EVA_Decomp/eva_export/functions/Connect@08e27ea0.c).
 */

#include "ustg_user_api.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>

void *USTGUserAPI::mFrontPanelStatusAddress = 0;
CSTGHandle *USTGUserAPI::mSharedMem = 0;
int USTGUserAPI::m_rt2userFifo = -1;
int USTGUserAPI::m_user2rtFifo = -1;
int USTGUserAPI::m_userRtDirect = -1;
int USTGUserAPI::m_activeRt2userFD = -1;
int USTGUserAPI::m_activeUser2rtFD = -1;
int USTGUserAPI::m_user2rtPanelFifo = -1;
int USTGUserAPI::m_rtUnsolFifo = -1;
char USTGUserAPI::mNowStopMessaging = 0;
char USTGUserAPI::mForceMessaging = 0;

bool USTGUserAPI::Connect()
{
	CSTGHandle localHandle;
	localHandle.mode = 1;

	mSharedMem = (CSTGHandle *)localHandle.Access();
	mFrontPanelStatusAddress = mSharedMem->Access();

	if (m_rt2userFifo == -1)
		m_rt2userFifo = open("/dev/rtf1", O_RDONLY);
	if (m_user2rtFifo == -1)
		m_user2rtFifo = open("/dev/rtf0", O_WRONLY);
	if (m_userRtDirect == -1)
		m_userRtDirect = open("/dev/dmsg0", O_RDWR);

	m_activeUser2rtFD = m_user2rtFifo;
	m_activeRt2userFD = m_rt2userFifo;

	if (m_rt2userFifo != -1 && m_user2rtFifo != -1)
		return m_userRtDirect != -1;
	return false;
}

bool USTGUserAPI::SendSTGMessageWithSource(const STGMessage *msg)
{
	int fd = m_activeUser2rtFD;

	if (mNowStopMessaging != 0 && mForceMessaging == 0)
		return false;

	if (m_activeUser2rtFD == -1)
		return false;

	const char *buf = (const char *)msg;
	unsigned totalLen = *(const unsigned short *)buf;
	if (totalLen == 0)
		return true;

	unsigned written = 0;
	for (;;) {
		ssize_t n = write(fd, buf + written, totalLen - written);
		if (n < 0)
			break;
		written += (unsigned)n;
		if (written >= totalLen)
			return true;
	}

	int err = errno;
	if (written != 0) {
		syslog(LOG_WARNING,
		       "USTGUserAPI.cpp: WriteFifo() fd %d failed after writing %d of %d bytes, errno %d\n",
		       fd, written, totalLen, err);
	}

	const char *kind = (m_activeUser2rtFD != m_userRtDirect) ? "normal" : "download";
	syslog(LOG_WARNING, "USTGUserAPI::SendSTGMessageWithSource() (%s) failed: (errno %d) %s\n",
	       kind, err, strerror(err));
	return false;
}

bool USTGUserAPI::ConnectPanelFifo()
{
	if (m_user2rtPanelFifo != -1)
		close(m_user2rtPanelFifo);
	m_user2rtPanelFifo = open("/dev/rtf7", O_WRONLY);
	return m_user2rtPanelFifo != -1;
}

/* --- Stage 2 IPC substrate, added 2026-07-25 --- */

void USTGUserAPI::Disconnect()
{
	if (m_rt2userFifo != -1)
		close(m_rt2userFifo);
	if (m_user2rtFifo != -1)
		close(m_user2rtFifo);
	if (m_rtUnsolFifo != -1)
		close(m_rtUnsolFifo);
	if (m_user2rtPanelFifo != -1)
		close(m_user2rtPanelFifo);
	if (m_userRtDirect != -1)
		close(m_userRtDirect);

	m_activeRt2userFD = -1;
	m_activeUser2rtFD = -1;
	m_userRtDirect = -1;
	m_rt2userFifo = -1;
	m_user2rtFifo = -1;
	m_rtUnsolFifo = -1;
	m_user2rtPanelFifo = -1;

	/* Real code calls this completely unconditionally, with no null check on
	 * mSharedMem -- faithfully preserved rather than "fixed": if Disconnect()
	 * is ever called before Connect() has run (never observed on this
	 * project's traced call graph, and presumably never done by any real
	 * caller either, since mSharedMem starts NULL), this dereferences a NULL
	 * `this` inside Release(), same as the real binary would.
	 */
	mSharedMem->Release();

	/* Real code builds a second, synthesized CSTGHandle{mode=1} on the stack
	 * to release mFrontPanelStatusAddress's own attachment -- same pairing
	 * Connect() uses to acquire it (mSharedMem->Access() with mode==1),
	 * faithfully mirrored here rather than tracking a second CSTGHandle*
	 * field this pass hasn't confirmed exists in the real object layout.
	 */
	CSTGHandle unsolHandle;
	unsolHandle.mode = 1;
	mFrontPanelStatusAddress = 0;
	unsolHandle.Release();
}

bool USTGUserAPI::ConnectUnsolicitedFifo()
{
	if (m_rtUnsolFifo != -1)
		close(m_rtUnsolFifo);
	m_rtUnsolFifo = open("/dev/rtf5", O_NONBLOCK);
	return m_rtUnsolFifo != -1;
}

int USTGUserAPI::ReadMessage(char *buf, unsigned bufSize)
{
	if (buf == 0 || m_activeRt2userFD == -1)
		return 0;

	ssize_t n = read(m_activeRt2userFD, buf, 2);
	if (n != 2)
		return 0;

	unsigned msgLen = *(const unsigned short *)buf;
	if (msgLen > bufSize)
		return 0;

	n = read(m_activeRt2userFD, buf + 2, msgLen - 2);
	return (int)n + 2;
}

int USTGUserAPI::ReadMessageWithTimeout(STGMessage *msg, unsigned bufSize, unsigned timeoutMs)
{
	struct timeval now;
	gettimeofday(&now, 0);

	long deadlineSec = now.tv_sec + timeoutMs / 1000;
	long deadlineUsec = now.tv_usec + (timeoutMs % 1000) * 1000;
	if (deadlineUsec > 999999) {
		deadlineSec++;
		deadlineUsec -= 1000000;
	}

	char *buf = (char *)msg;

	if (buf == 0) {
		/* Real quirk: a NULL target just busy-waits out the deadline with no
		 * I/O at all -- transcribed faithfully, not treated as dead code.
		 */
		for (;;) {
			gettimeofday(&now, 0);
			bool reached = (now.tv_sec == deadlineSec)
				? (deadlineUsec <= now.tv_usec)
				: (deadlineSec <= now.tv_sec);
			if (reached)
				return 0;
		}
	}

	for (;;) {
		for (;;) {
			if (m_activeRt2userFD != -1) {
				ssize_t n = read(m_activeRt2userFD, buf, 2);
				if (n == 2 && *(const unsigned short *)buf <= bufSize) {
					n = read(m_activeRt2userFD, buf + 2, *(const unsigned short *)buf - 2);
					if (n + 2 != 0)
						return (int)n + 2;
				}
			}
			gettimeofday(&now, 0);
			if (now.tv_sec != deadlineSec)
				break;
			if (deadlineUsec <= now.tv_usec)
				return 0;
		}
		if (now.tv_sec >= deadlineSec)
			break;
	}
	return 0;
}

int USTGUserAPI::ReadUnsolicitedMessage(char *buf, unsigned bufSize)
{
	if (buf == 0)
		return 0;

	ssize_t n = read(m_rtUnsolFifo, buf, 2);
	if (n != 2)
		return 0;

	unsigned msgLen = *(const unsigned short *)buf;
	if (msgLen > bufSize)
		return 0;

	n = read(m_rtUnsolFifo, buf + 2, msgLen - 2);
	return (int)n + 2;
}

bool USTGUserAPI::SendPanelMessage(const STGMessage *msg)
{
	int fd = m_user2rtPanelFifo;

	if (msg == 0 || m_user2rtPanelFifo == -1)
		return false;

	const char *buf = (const char *)msg;
	unsigned totalLen = *(const unsigned short *)buf;
	if (totalLen == 0)
		return true;

	unsigned written = 0;
	for (;;) {
		ssize_t n = write(fd, buf + written, totalLen - written);
		if (n < 0)
			break;
		written += (unsigned)n;
		if (written >= totalLen)
			return true;
	}

	if (written != 0) {
		int err = errno;
		syslog(LOG_WARNING,
		       "USTGUserAPI.cpp: WriteFifo() fd %d failed after writing %d of %d bytes, errno %d\n",
		       fd, written, totalLen, err);
	}
	return false;
}

int USTGUserAPI::GetProgress()
{
	int value = 0;
	FILE *f = fopen("/proc/OmapNKS4ProgressBar", "r");
	if (f) {
		fscanf(f, "%d", &value);
		fclose(f);
	}
	return value;
}

void USTGUserAPI::IncrementProgress()
{
	FILE *f = fopen("/proc/OmapNKS4ProgressBar", "w");
	if (f) {
		/* Real literal 3-byte payload, confirmed by reading the real binary's
		 * own .rodata at DAT_08fd9367 -- not guessed.
		 */
		fwrite("inc", 1, 3, f);
		fflush(f);
		fclose(f);
	}
}

void USTGUserAPI::SetProgress(int value)
{
	FILE *f = fopen("/proc/OmapNKS4ProgressBar", "w");
	if (f) {
		fprintf(f, "set %d", value);
		fflush(f);
		fclose(f);
	}
}
