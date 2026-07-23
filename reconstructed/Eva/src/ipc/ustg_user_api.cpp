/*
 * ustg_user_api.cpp  -  see include/ustg_user_api.h.
 *
 * Connect() (.text+0x08e27ea0, 234 bytes) transcribed instruction-by-instruction from
 * the Ghidra decompile (Decomp/EVA_Decomp/eva_export/functions/Connect@08e27ea0.c).
 */

#include "ustg_user_api.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
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
