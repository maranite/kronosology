// SPDX-License-Identifier: GPL-2.0
/*
 * calibration_data.cpp  -  SCalibrationData::LoadCalibrationFile() and
 * SCalibrationData::InitAll() (compiled-in calibration defaults).
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

/*
 * SCalibrationData::InitAll() -- compiled-in default calibration
 * constants, used as the fallback when LoadCalibrationFile() above
 * fails (missing/corrupt /korg/rw/Calibration/Calibration.img).
 *
 * Ground truth: `_ZN16SCalibrationData7InitAllEv` (.text+0x3f860,
 * 175 bytes). Writes literal default values into the SAME 252-byte
 * calibration blob LoadCalibrationFile() reads (`panel[0..0xfb]`,
 * i.e. every offset below is < 0xfc) -- confirmed via full
 * `objdump -dr -M intel` transcription, byte for byte.
 *
 * This project independently reconstructed 9 separate one-shot
 * per-item `SCalibrationData::Init<Item>()` methods elsewhere in
 * ground truth (InitDrumPads/InitJoystickX/InitJoystickY/InitRibbon/
 * InitAftertouch/InitDamper/InitVectorJS/InitTouchScreen/
 * InitLCDControl, .text+0x3fb10..0x3fe40) -- confirmed via a full
 * grep of every disassembled call/reloc in this binary that NONE of
 * those 9 has any real caller anywhere in ground truth (dead public
 * API surface, never reconstructed here for that reason). InitAll()
 * does NOT call them either -- it duplicates their field writes
 * itself inline (same literal values at the same offsets, confirmed
 * field-by-field against each sibling), which is why this one
 * function alone is enough to restore every calibratable control's
 * defaults. The two float-valued pairs shared between VectorJS's X
 * and Y axes (offsets 0x64/0x78 and 0x68/0x7c) are written from the
 * same staged register in real code (compiler CSE), reproduced here
 * as plain duplicate literal writes -- semantically identical.
 *
 * Field groups (offsets confirmed against this project's own named
 * fields in calibration_msg_handler.cpp's Start*Calibration methods,
 * where such a mapping exists):
 *   0x00-0x1f  generic default breakpoint/curve table. Byte layout
 *              matches InitDrumPads' own table exactly EXCEPT the
 *              last two bytes (offsets 4,5) -- InitAll's own tail
 *              write (1,2) overrides InitDrumPads' values (0,0x16)
 *              for those two bytes only. Real semantics of this
 *              region are not independently confirmed elsewhere in
 *              this project; treated as an opaque default table.
 *   0x20-0x2c  JoystickX (xMin/lo/hi/xMax + 2 float slopes)
 *   0x34-0x40  JoystickY (yMin/lo/hi/yMax + 2 float slopes)
 *   0x48-0x54,0x98  Ribbon controller (matches the `edx==0` /
 *              default branch of SCalibrationData::InitRibbon(int),
 *              i.e. the non-drum-pad-panel case)
 *   0x5c-0x7c  Vector joystick X/Y (adjusted min/lo/hi/max + slopes)
 *   0x84-0x90  Half-damper pedal
 *   0x9c-0xbc  Touch screen (12 raw fields, exact semantics not
 *              independently confirmed elsewhere in this project)
 *   0xc4-0xe7  LCD control (contrast/brightness gain float = 1.0f
 *              plus several 0xff/0xffff range-clamp-looking words
 *              and 2 trailing enable flags = 1,1)
 *   0xe8-0xf4  Aftertouch
 *   0x04-0x05  final override, written last (byte,byte) = (1,2)
 *
 * No null-check on `panel` inside this function itself -- matches
 * ground truth and this file's own LoadCalibrationFile() convention;
 * guarded at the call site in setup_global_resources.cpp instead.
 */
extern "C" void SCalibrationData_InitAll(unsigned char *panel)
{
	/* 0x00-0x1f: generic default breakpoint/curve table */
	panel[0x00] = 0x03; panel[0x01] = 0x74; panel[0x02] = 0x01; panel[0x03] = 0x29;
	panel[0x06] = 0x04; panel[0x07] = 0x12; panel[0x08] = 0x02; panel[0x09] = 0x4d;
	panel[0x0a] = 0x02; panel[0x0b] = 0x64; panel[0x0c] = 0x02; panel[0x0d] = 0x17;
	panel[0x0e] = 0x01; panel[0x0f] = 0x6d; panel[0x10] = 0x07; panel[0x11] = 0x77;
	panel[0x12] = 0x06; panel[0x13] = 0x19; panel[0x14] = 0x04; panel[0x15] = 0x49;
	panel[0x16] = 0x07; panel[0x17] = 0x78; panel[0x18] = 0x07; panel[0x19] = 0x4c;
	panel[0x1a] = 0x07; panel[0x1b] = 0x37; panel[0x1c] = 0x07; panel[0x1d] = 0x38;
	panel[0x1e] = 0x06; panel[0x1f] = 0x5b;

	/* 0x20-0x2c: JoystickX */
	*(short *)(panel + 0x20) = 0x0075;
	*(short *)(panel + 0x22) = 0x01e0;
	*(short *)(panel + 0x24) = 0x0220;
	*(short *)(panel + 0x26) = 0x0387;
	*(float *)(panel + 0x28) = 1.410468339920044f;
	*(float *)(panel + 0x2c) = 1.4233983755111694f;

	/* 0x34-0x40: JoystickY */
	*(short *)(panel + 0x34) = 0x00a0;
	*(short *)(panel + 0x36) = 0x01d8;
	*(short *)(panel + 0x38) = 0x0228;
	*(short *)(panel + 0x3a) = 0x035f;
	*(float *)(panel + 0x3c) = 1.6410256624221802f;
	*(float *)(panel + 0x40) = 1.6430867910385132f;

	/* 0x48-0x54,0x98: Ribbon (InitRibbon's edx==0 / default branch) */
	*(short *)(panel + 0x48) = 0x0050;
	*(short *)(panel + 0x4a) = 0x01d9;
	*(short *)(panel + 0x4c) = 0x01d9;
	*(short *)(panel + 0x4e) = 0x0370;
	*(float *)(panel + 0x50) = 1.3027989864349365f;
	*(float *)(panel + 0x54) = 1.255528211593628f;
	*(short *)(panel + 0x98) = 0x0190;

	/* 0xe8-0xf4: Aftertouch */
	*(short *)(panel + 0xe8) = 0x00d4;
	*(short *)(panel + 0xea) = 0x0157;
	*(short *)(panel + 0xec) = 0x0159;
	*(short *)(panel + 0xee) = 0x029c;
	*(float *)(panel + 0xf0) = 3.9083969593048096f;
	*(float *)(panel + 0xf4) = 1.5820432901382446f;

	/* 0x84-0x90: Half-damper pedal */
	*(short *)(panel + 0x84) = 0x01fe;
	*(short *)(panel + 0x86) = 0x02a7;
	*(short *)(panel + 0x88) = 0x02a9;
	*(short *)(panel + 0x8a) = 0x0352;
	*(float *)(panel + 0x8c) = 3.029585838317871f;
	*(float *)(panel + 0x90) = 3.0236685276031494f;

	/* 0x5c-0x7c: Vector joystick X/Y */
	*(short *)(panel + 0x5c) = 0x0100;
	*(short *)(panel + 0x5e) = 0x01b0;
	*(short *)(panel + 0x60) = 0x0250;
	*(short *)(panel + 0x62) = 0x02ff;
	*(float *)(panel + 0x64) = 2.909090995788574f;
	*(float *)(panel + 0x68) = 2.9200000762939453f;
	*(short *)(panel + 0x70) = 0x0100;
	*(short *)(panel + 0x72) = 0x01b0;
	*(short *)(panel + 0x74) = 0x0250;
	*(short *)(panel + 0x76) = 0x02ff;
	*(float *)(panel + 0x78) = 2.909090995788574f;
	*(float *)(panel + 0x7c) = 2.9200000762939453f;

	/* 0x9c-0xbc: Touch screen */
	*(short *)(panel + 0x9c) = 0x0008;
	*(short *)(panel + 0xa2) = 0x0007;
	*(short *)(panel + 0x9e) = 0x0000;
	*(short *)(panel + 0xa0) = 0x0000;
	*(int   *)(panel + 0xa4) = 0;
	*(int   *)(panel + 0xa8) = 0;
	*(short *)(panel + 0xb0) = 0x0004;
	*(short *)(panel + 0xb6) = 0x000a;
	*(short *)(panel + 0xb2) = 0x0000;
	*(short *)(panel + 0xb4) = 0x0000;
	*(int   *)(panel + 0xb8) = 0;
	*(int   *)(panel + 0xbc) = 0;

	/* 0xc4-0xe7: LCD control */
	panel[0xc4] = 0x00; panel[0xc5] = 0x00; panel[0xc6] = 0x00; panel[0xc7] = 0x3f;
	*(float *)(panel + 0xc8) = 1.0f;
	*(unsigned short *)(panel + 0xcc) = 0x00ff;
	*(unsigned short *)(panel + 0xce) = 0xffff;
	*(unsigned short *)(panel + 0xd0) = 0x0000;
	*(unsigned short *)(panel + 0xd2) = 0x0000;
	*(unsigned short *)(panel + 0xd4) = 0x00ff;
	*(unsigned short *)(panel + 0xd6) = 0x0000;
	*(unsigned short *)(panel + 0xd8) = 0xffff;
	*(unsigned short *)(panel + 0xda) = 0x0000;
	*(unsigned short *)(panel + 0xdc) = 0x00ff;
	*(int *)(panel + 0xe0) = 0;
	panel[0xe4] = 0x00; panel[0xe5] = 0x00; panel[0xe6] = 0x01; panel[0xe7] = 0x01;

	/* Final override, written last in real code */
	panel[0x04] = 0x01;
	panel[0x05] = 0x02;
}
