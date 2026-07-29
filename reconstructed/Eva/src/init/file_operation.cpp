/*
 * file_operation.cpp  -  CFileOperation's round-54 23-method real batch.
 * See long_binary_file.h's own header comment (CFileOperation section) for
 * the full derivation and the deferred-item list.
 *
 * `g_fileOpDeviceId` is ground truth's own `DAT_098efd6c` -- a plain
 * (non-class-scoped) global shared between these landed methods and the
 * much larger deferred `Execute()`-calling family (which also writes it,
 * e.g. `SortDir`'s `DAT_098efd6c = param_1; DAT_098efd70 = param_1;
 * Execute();`) -- declared here since `Readblk`/`Writeblk` already write
 * it for real; a future round reconstructing the `Execute()` family
 * reuses this same symbol.
 */
#include "long_binary_file.h"

unsigned char CFileOperation::s_ucMem;
unsigned char CFileOperation::s_900kMem;
int CFileOperation::sm_bExecDirectCom;
int CFileOperation::directIOCall;
int CFileOperation::sm_bForceDiskChangeTestMode;
int CFileOperation::s_bWaitPIDSignal;
int CFileOperation::sSecondToWait;
int CFileOperation::sm_bForceDiskChangeTestEventNotify;
int CFileOperation::sm_iScsiGenericNoMap[10];
int CFileOperation::sm_iUSBDirectDeviceIdMap[10];
int CFileOperation::sm_iUSBDirectAccessDeviceIndex[10];
int CFileOperation::sm_iCDDeviceIndex[10];
int CFileOperation::sm_bIsDiskInfoDirty[10];
int CFileOperation::sm_iUSBDiskNumber[127];
int CFileOperation::s_iPrevUSBDiskNumber[127];
char CFileOperation::s_akcLinuxMountPoint[10][15];
int CFileOperation::s_eAudioSts;
unsigned char CFileOperation::s_ucTrackNo;
unsigned int CFileOperation::s_ulBlockNo;
unsigned char *CFileOperation::sm_poFileCache;
int CFileOperation::sm_bIsFileCacheEnable;

extern "C" unsigned int g_fileOpDeviceId; /* DAT_098efd6c */
unsigned int g_fileOpDeviceId;

static const char kEmptyString[1] = {0}; /* DAT_08e7fbae */

unsigned char *CFileOperation::Get1MMemory()
{
	return &s_ucMem;
}

unsigned char *CFileOperation::Get900KMemory()
{
	return &s_900kMem;
}

int CFileOperation::IsDirectExecCommand()
{
	return sm_bExecDirectCom;
}

void CFileOperation::EnableDirectIOCall(int enable)
{
	directIOCall = enable;
}

void CFileOperation::SetForceDiskChangeTestMode(int enable)
{
	sm_bForceDiskChangeTestMode = enable;
}

void CFileOperation::EnableWaitPIDSignal(int enable)
{
	s_bWaitPIDSignal = enable;
}

void CFileOperation::SetWaitTimeAfterSync(int seconds)
{
	sSecondToWait = seconds;
}

void CFileOperation::SetForceDiskChangeTestEvent()
{
	sm_bForceDiskChangeTestEventNotify = 1;
}

int CFileOperation::GetForceDiskChangeTestEvent()
{
	int v = sm_bForceDiskChangeTestEventNotify;
	sm_bForceDiskChangeTestEventNotify = 0;
	return v;
}

int CFileOperation::Readblk(int deviceId)
{
	g_fileOpDeviceId = deviceId;
	return 0;
}

int CFileOperation::Writeblk(int deviceId)
{
	g_fileOpDeviceId = deviceId;
	return 0;
}

void CFileOperation::SetAsUsedScsiGenericNo(int deviceId, int value)
{
	sm_iScsiGenericNoMap[deviceId] = value;
}

void CFileOperation::SetAsUsedUSBDirectDeviceId(int deviceId, int value)
{
	sm_iUSBDirectDeviceIdMap[deviceId] = value;
}

void CFileOperation::SetAsUsedUSBDirectDeviceIndex(int deviceId, int value)
{
	sm_iUSBDirectAccessDeviceIndex[deviceId] = value;
}

int CFileOperation::GetUSBDirectAccessDeviceIndex(int deviceId)
{
	if (sm_iUSBDirectAccessDeviceIndex[deviceId] > -1)
		return sm_iUSBDirectAccessDeviceIndex[deviceId];
	return deviceId;
}

int CFileOperation::GetCDDeviceIndex(int deviceId)
{
	if (sm_iCDDeviceIndex[deviceId] > -1)
		return sm_iCDDeviceIndex[deviceId];
	return deviceId;
}

void CFileOperation::SetDiskInfoDirty(int deviceId)
{
	if (deviceId < 10)
		sm_bIsDiskInfoDirty[deviceId] = 1;
}

int CFileOperation::GetDiskInfoDirty(int deviceId)
{
	if (deviceId < 10) {
		int v = sm_bIsDiskInfoDirty[deviceId];
		sm_bIsDiskInfoDirty[deviceId] = 0;
		return v;
	}
	return 0;
}

int CFileOperation::GetUSBDiskNumber(int index)
{
	if ((unsigned int)index < 0x7f)
		return sm_iUSBDiskNumber[index];
	return -1;
}

void CFileOperation::SetUSBDiskNumber(int index, int value)
{
	if ((unsigned int)index < 0x7f) {
		if (sm_iUSBDiskNumber[index] > -1)
			s_iPrevUSBDiskNumber[index] = sm_iUSBDiskNumber[index];
		sm_iUSBDiskNumber[index] = value;
	}
}

const char *CFileOperation::GetLinuxMountPoint(int deviceId)
{
	if (deviceId < 10)
		return s_akcLinuxMountPoint[deviceId];
	return kEmptyString;
}

unsigned int CFileOperation::GetResultBlocknoGetcurpos(int *audioStatus, unsigned char *trackNo)
{
	*audioStatus = s_eAudioSts;
	*trackNo = s_ucTrackNo;
	return s_ulBlockNo;
}

void CFileOperation::EnableFileCache(int enable)
{
	sm_bIsFileCacheEnable = enable;
	if (enable == 0) {
		unsigned char *inner = *(unsigned char **)(sm_poFileCache + 0x28);
		*(unsigned int *)(sm_poFileCache + 0x30) = 0;
		*inner = 0;
	}
}
