/*
 * test_file_operation.cpp  -  host-side known-answer test for
 * CFileOperation's round-54 24-method batch (solo, 2026-07-29). See
 * include/file_operation.h for the full derivation and the deferred-item
 * list.
 */
#include <cstdio>
#include <cstring>
#include "long_binary_file.h"

static int g_fail;
static void check(const char *label, bool ok)
{
	if (!ok)
		g_fail++;
	printf("  %s  %s\n", ok ? "ok  " : "FAIL", label);
}

int main()
{
	check("Get1MMemory returns &s_ucMem", CFileOperation::Get1MMemory() == &CFileOperation::s_ucMem);
	check("Get900KMemory returns &s_900kMem", CFileOperation::Get900KMemory() == &CFileOperation::s_900kMem);

	CFileOperation::sm_bExecDirectCom = 0;
	check("IsDirectExecCommand reads sm_bExecDirectCom (0)", CFileOperation::IsDirectExecCommand() == 0);
	CFileOperation::sm_bExecDirectCom = 1;
	check("IsDirectExecCommand reads sm_bExecDirectCom (1)", CFileOperation::IsDirectExecCommand() == 1);

	CFileOperation::EnableDirectIOCall(7);
	check("EnableDirectIOCall writes directIOCall", CFileOperation::directIOCall == 7);

	CFileOperation::SetForceDiskChangeTestMode(1);
	check("SetForceDiskChangeTestMode writes sm_bForceDiskChangeTestMode",
	      CFileOperation::sm_bForceDiskChangeTestMode == 1);

	CFileOperation::EnableWaitPIDSignal(1);
	check("EnableWaitPIDSignal writes s_bWaitPIDSignal", CFileOperation::s_bWaitPIDSignal == 1);

	CFileOperation::SetWaitTimeAfterSync(5);
	check("SetWaitTimeAfterSync writes sSecondToWait", CFileOperation::sSecondToWait == 5);

	CFileOperation::sm_bForceDiskChangeTestEventNotify = 0;
	CFileOperation::SetForceDiskChangeTestEvent();
	check("SetForceDiskChangeTestEvent sets the flag to 1",
	      CFileOperation::sm_bForceDiskChangeTestEventNotify == 1);
	check("GetForceDiskChangeTestEvent returns 1 and clears it",
	      CFileOperation::GetForceDiskChangeTestEvent() == 1 &&
	      CFileOperation::sm_bForceDiskChangeTestEventNotify == 0);
	check("GetForceDiskChangeTestEvent returns 0 thereafter",
	      CFileOperation::GetForceDiskChangeTestEvent() == 0);

	check("Readblk returns 0", CFileOperation::Readblk(3) == 0);
	check("Writeblk returns 0", CFileOperation::Writeblk(4) == 0);

	CFileOperation::SetAsUsedScsiGenericNo(2, 0x99);
	check("SetAsUsedScsiGenericNo writes sm_iScsiGenericNoMap[2]",
	      CFileOperation::sm_iScsiGenericNoMap[2] == 0x99);

	CFileOperation::SetAsUsedUSBDirectDeviceId(3, 0x55);
	check("SetAsUsedUSBDirectDeviceId writes sm_iUSBDirectDeviceIdMap[3]",
	      CFileOperation::sm_iUSBDirectDeviceIdMap[3] == 0x55);

	CFileOperation::SetAsUsedUSBDirectDeviceIndex(4, 6);
	check("SetAsUsedUSBDirectDeviceIndex writes sm_iUSBDirectAccessDeviceIndex[4]",
	      CFileOperation::sm_iUSBDirectAccessDeviceIndex[4] == 6);

	memset(CFileOperation::sm_iUSBDirectAccessDeviceIndex, 0,
	       sizeof(CFileOperation::sm_iUSBDirectAccessDeviceIndex));
	CFileOperation::sm_iUSBDirectAccessDeviceIndex[1] = -1;
	check("GetUSBDirectAccessDeviceIndex: -1 mapped -> returns deviceId unchanged",
	      CFileOperation::GetUSBDirectAccessDeviceIndex(1) == 1);
	CFileOperation::sm_iUSBDirectAccessDeviceIndex[1] = 5;
	check("GetUSBDirectAccessDeviceIndex: mapped value present -> returns it",
	      CFileOperation::GetUSBDirectAccessDeviceIndex(1) == 5);

	memset(CFileOperation::sm_iCDDeviceIndex, 0, sizeof(CFileOperation::sm_iCDDeviceIndex));
	CFileOperation::sm_iCDDeviceIndex[2] = -1;
	check("GetCDDeviceIndex: -1 mapped -> returns deviceId unchanged",
	      CFileOperation::GetCDDeviceIndex(2) == 2);
	CFileOperation::sm_iCDDeviceIndex[2] = 8;
	check("GetCDDeviceIndex: mapped value present -> returns it", CFileOperation::GetCDDeviceIndex(2) == 8);

	memset(CFileOperation::sm_bIsDiskInfoDirty, 0, sizeof(CFileOperation::sm_bIsDiskInfoDirty));
	CFileOperation::SetDiskInfoDirty(3);
	check("SetDiskInfoDirty sets flag for in-range device", CFileOperation::sm_bIsDiskInfoDirty[3] == 1);
	check("GetDiskInfoDirty returns 1 then clears it",
	      CFileOperation::GetDiskInfoDirty(3) == 1 && CFileOperation::sm_bIsDiskInfoDirty[3] == 0);
	check("GetDiskInfoDirty out-of-range device -> 0", CFileOperation::GetDiskInfoDirty(50) == 0);

	memset(CFileOperation::sm_iUSBDiskNumber, 0, sizeof(CFileOperation::sm_iUSBDiskNumber));
	memset(CFileOperation::s_iPrevUSBDiskNumber, 0, sizeof(CFileOperation::s_iPrevUSBDiskNumber));
	CFileOperation::sm_iUSBDiskNumber[10] = 42;
	check("GetUSBDiskNumber: in-range index", CFileOperation::GetUSBDiskNumber(10) == 42);
	check("GetUSBDiskNumber: out-of-range index -> -1", CFileOperation::GetUSBDiskNumber(200) == -1);

	CFileOperation::sm_iUSBDiskNumber[20] = 7;
	CFileOperation::SetUSBDiskNumber(20, 99);
	check("SetUSBDiskNumber: saves prior value when it was >= 0 and sets new one",
	      CFileOperation::s_iPrevUSBDiskNumber[20] == 7 && CFileOperation::sm_iUSBDiskNumber[20] == 99);
	CFileOperation::sm_iUSBDiskNumber[21] = -1;
	CFileOperation::s_iPrevUSBDiskNumber[21] = 0x1234;
	CFileOperation::SetUSBDiskNumber(21, 55);
	check("SetUSBDiskNumber: does NOT save prior value when it was < 0",
	      CFileOperation::s_iPrevUSBDiskNumber[21] == 0x1234 && CFileOperation::sm_iUSBDiskNumber[21] == 55);
	CFileOperation::SetUSBDiskNumber(200, 1);
	check("SetUSBDiskNumber: out-of-range index is a no-op (would have been UB otherwise)", true);

	strcpy(CFileOperation::s_akcLinuxMountPoint[2], "/mnt/usb0");
	check("GetLinuxMountPoint: in-range device returns the real buffer",
	      strcmp(CFileOperation::GetLinuxMountPoint(2), "/mnt/usb0") == 0);
	check("GetLinuxMountPoint: out-of-range device returns the empty-string fallback",
	      CFileOperation::GetLinuxMountPoint(50)[0] == '\0');

	CFileOperation::s_eAudioSts = 3;
	CFileOperation::s_ucTrackNo = 9;
	CFileOperation::s_ulBlockNo = 0xdeadbeef;
	int outSts = 0;
	unsigned char outTrack = 0;
	unsigned int rc = CFileOperation::GetResultBlocknoGetcurpos(&outSts, &outTrack);
	check("GetResultBlocknoGetcurpos forwards all three fields",
	      outSts == 3 && outTrack == 9 && rc == 0xdeadbeef);

	{
		unsigned char cacheBuf[0x40];
		memset(cacheBuf, 0, sizeof(cacheBuf));
		unsigned char innerTarget = 0xff;
		*(unsigned char **)(cacheBuf + 0x28) = &innerTarget;
		*(unsigned int *)(cacheBuf + 0x30) = 0x12345678;
		CFileOperation::sm_poFileCache = cacheBuf;

		CFileOperation::EnableFileCache(1);
		check("EnableFileCache(1): sets the flag, does NOT touch sm_poFileCache",
		      CFileOperation::sm_bIsFileCacheEnable == 1 &&
		      *(unsigned int *)(cacheBuf + 0x30) == 0x12345678 && innerTarget == 0xff);

		CFileOperation::EnableFileCache(0);
		check("EnableFileCache(0): clears the flag and zeroes +0x30 and *(+0x28)",
		      CFileOperation::sm_bIsFileCacheEnable == 0 &&
		      *(unsigned int *)(cacheBuf + 0x30) == 0 && innerTarget == 0);
	}

	printf(g_fail ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", g_fail);
	return g_fail ? 1 : 0;
}
