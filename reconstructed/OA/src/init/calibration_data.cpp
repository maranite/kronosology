// SPDX-License-Identifier: GPL-2.0
/*
 * calibration_data.cpp  -  SCalibrationData::LoadCalibrationFile().
 *
 * Ground truth: `_ZN16SCalibrationData19LoadCalibrationFileEv`
 * (.text+0x3f7c0, 156 bytes). This project's earlier stub modeled it
 * as a no-arg plain-C wrapper (`SCalibrationData_LoadCalibrationFile
 * (void)`) -- WRONG: the real function is a genuine C++ method taking
 * `this` in EAX (regparm(3)), and there is no separate standalone
 * wrapper anywhere in ground truth under either name. Its ONE real
 * caller, `setup_global_resources()` (.text+0x116c40, offset +0x19ae =
 * .text+0x1185eb), does `mov eax,ebx; call ...` where `ebx` is the SAME
 * `panel` pointer (`STGAPIFrontPanelStatus::sInstance`) already used
 * for every neighboring panel field write in that function -- i.e.
 * `SCalibrationData` is not a separate object at all; the 252-byte
 * calibration blob is read directly into the FIRST 0xfc bytes of the
 * front-panel status object. SIGNATURE FIXED accordingly (batch 38):
 * takes that pointer explicitly instead of guessing a hidden global.
 *
 * Confirmed real algorithm (full objdump -dr trace):
 *   1. CSTGFile_Open("/korg/rw/Calibration/Calibration.img", 0 /
 *      O_RDONLY) (path string confirmed via .rodata.str1.4+0x4a0).
 *      NULL -> return false immediately (edi, the return value, is
 *      zeroed at function entry and never touched on this path).
 *   2. CSTGFile_Read(handle, panel, 0xfc) -- read 252 bytes directly
 *      into panel[0..0xfb]. Size mismatch -> close + return false.
 *   3. CSTGFile_Read(handle, &storedChecksum, 4) -- read a trailing
 *      4-byte stored checksum into a local. Size mismatch -> close +
 *      return false (same landing pad as step 2's failure).
 *   4. Sum panel[0..0xfb] as unsigned bytes into a 32-bit accumulator
 *      (plain byte-sum, no CRC/polynomial -- confirmed via the loop's
 *      own `movzx eax,byte ptr [esi]; add edi,eax`).
 *   5. CSTGFile_Close(handle) unconditionally on this path.
 *   6. Return (storedChecksum == computedSum) as a bool (`sete al`).
 *
 * No null-check on `panel` inside this function itself -- matches
 * ground truth (the real caller doesn't check either, right at this
 * specific call; see setup_global_resources.cpp's own call-site
 * comment for why THIS project still guards it defensively there).
 */

extern "C" void *CSTGFile_Open(const char *path, int mode);
extern "C" int CSTGFile_Read(void *handle, void *buf, unsigned int size);
extern "C" int CSTGFile_Close(void *handle);

extern "C" char SCalibrationData_LoadCalibrationFile(unsigned char *panel)
{
	void *fh = CSTGFile_Open("/korg/rw/Calibration/Calibration.img", 0 /* O_RDONLY */);
	if (fh == 0)
		return 0;

	if (CSTGFile_Read(fh, panel, 0xfc) != 0xfc) {
		CSTGFile_Close(fh);
		return 0;
	}

	unsigned int storedChecksum = 0;
	if (CSTGFile_Read(fh, &storedChecksum, 4) != 4) {
		CSTGFile_Close(fh);
		return 0;
	}

	unsigned int sum = 0;
	for (unsigned int i = 0; i < 0xfc; i++)
		sum += panel[i];

	CSTGFile_Close(fh);
	return (char)(storedChecksum == sum);
}
