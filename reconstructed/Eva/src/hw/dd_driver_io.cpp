/*
 * dd_driver_io.cpp  -  see dd_driver_io.h for the full derivation and the
 * deferred-item list.
 */
#include "dd_driver_io.h"
#include "ustg_api_control.h"

unsigned char CDDriverIO::sm_bHasInternalCDRW = 0;
int CDDriverIO::sm_bEnableDevInfoCache = 0;
void *CDDriverIO::sm_poFMBrowse = 0;
unsigned char *CDDriverIO::sm_poDriverApi[10] = {0};
unsigned char CDDriverIO::devstat_tab[10] = {0};
unsigned char CDDriverIO::s_senseKey = 0;
unsigned char CDDriverIO::s_senseAsc = 0;
unsigned char CDDriverIO::s_senseAscq = 0;

typedef void (*ExecCommandFn)(void *self, int opcode, void *buf);

/*
 * Raw indirect vtable dispatch through an unreconstructed CAtaApi/CScsiApi
 * instance -- same treatment as OA.ko's CSTGProgramSlot vtable slot 7
 * (oa_global.h). Ground truth's own call shape:
 *   (**(code **)(*(int *)obj + 8))(obj, opcode, buf)
 */
static void DispatchExecuteCommand(unsigned char *obj, int opcode, void *buf)
{
	unsigned char *vtable = *(unsigned char **)obj;
	ExecCommandFn fn = *(ExecCommandFn *)(vtable + 8);
	fn(obj, opcode, buf);
}

unsigned char CDDriverIO::HasInternalCDRW()
{
	return sm_bHasInternalCDRW;
}

void CDDriverIO::EnableDevInfoCache(int enable)
{
	sm_bEnableDevInfoCache = enable;
}

void CDDriverIO::SetFMBrowseToReport(void *browse)
{
	sm_poFMBrowse = browse;
}

void CDDriverIO::ClearFMBrowseToReport()
{
	sm_poFMBrowse = 0;
}

int CDDriverIO::warmupdrive()
{
	return 0;
}

int CDDriverIO::cooldowndrive()
{
	return 0;
}

unsigned int CDDriverIO::rate2speed(unsigned short rate)
{
	if (rate <= 0xaf)
		return 0;
	if (rate == 0xffff)
		return 0xffffffff;
	return rate / 0xb0;
}

unsigned int CDDriverIO::speed2rate(unsigned char speed)
{
	switch (speed) {
	case 1: return 0xb0;
	case 2: return 0x164;
	case 4: return 0x2c5;
	case 6: return 0x425;
	case 8: return 0x586;
	case 0xa: return 0x6ea;
	case 0x10: return 0xb07;
	case 0x18: return 0x1090;
	case 0xff: return 0xffffffff;
	default: return 0;
	}
}

int CDDriverIO::scsi_mode_sel()
{
	return 1;
}

bool CDDriverIO::prechkdiskchg(int deviceId)
{
	if ((devstat_tab[deviceId] & 8) == 0) {
		unsigned char *sense = scsi_req_sense(deviceId);
		if (sense != 0)
			return sense[0] == 6;
		return (devstat_tab[deviceId] & 4) != 0;
	}
	return true;
}

/* progressOut is a genuinely dead parameter in ground truth -- stored to a
 * local and never read. Kept in the signature for prototype fidelity.
 */
char CDDriverIO::GetProgress(unsigned char deviceId, int *progressOut)
{
	(void)progressOut;
	unsigned char buf[4] = {0};
	DispatchExecuteCommand(sm_poDriverApi[deviceId], 0x26, buf);
	if (buf[0] == 0)
		scsi_req_sense(deviceId);
	return buf[0];
}

char CDDriverIO::ExecuteCommand(int msgType, char *pbuf)
{
	unsigned char deviceId = (unsigned char)pbuf[4];
	pbuf[0] = 0;
	DispatchExecuteCommand(sm_poDriverApi[deviceId], msgType, pbuf);
	char status = pbuf[0];
	if (status == 0 && msgType != 0x25) {
		scsi_req_sense(deviceId);
		status = pbuf[0];
	}
	return status;
}

unsigned char *CDDriverIO::scsi_req_sense(unsigned int deviceId)
{
	unsigned int idx = deviceId & 0xff;
	unsigned char devByte = (unsigned char)deviceId;

	unsigned char reqBuf[5] = {0};
	reqBuf[4] = devByte;
	DispatchExecuteCommand(sm_poDriverApi[idx], 6, reqBuf);
	if (reqBuf[0] == 0) {
		scsi_req_sense(idx);
		if (reqBuf[0] == 0)
			return 0;
	}

	if (s_senseKey == 6) {
		USTGAPIControl::SysLogPrintf("set unit attention(%d)", deviceId);
		devstat_tab[deviceId] |= 8;
	}

	if (s_senseKey == 4) {
		if (s_senseAsc != 9 || s_senseAscq != 2)
			goto tail;

		USTGAPIControl::SysLogPrintf("scsi_reset_spinup(%d)", deviceId);

		{
			unsigned char buf1[6] = {0};
			buf1[4] = devByte;
			buf1[5] = 1;
			DispatchExecuteCommand(sm_poDriverApi[idx], 7, buf1);
			if (buf1[0] == 0)
				scsi_req_sense(idx);
		}

		{
			unsigned char *obj = sm_poDriverApi[idx];
			devstat_tab[deviceId] &= 0xfd;

			unsigned char buf2[6] = {0};
			buf2[4] = devByte;
			DispatchExecuteCommand(obj, 7, buf2);
			if (buf2[0] == 0) {
				scsi_req_sense(idx);
				devstat_tab[deviceId] &= 0xfd;
			} else {
				devstat_tab[deviceId] |= 2;
			}
		}
	}

	if (s_senseKey == 2 && s_senseAsc == 4 && (s_senseAscq == 7 || s_senseAscq == 8)) {
		USTGAPIControl::SysLogPrintf("set busy flag(%d)", deviceId);
		devstat_tab[deviceId] |= 0x10;
		return &s_senseKey;
	}

tail:
	devstat_tab[deviceId] &= 0xef;
	return &s_senseKey;
}
