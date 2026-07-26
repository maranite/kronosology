/*
 * stg_unsol_msg_handler.cpp  -  see include/stg_unsol_msg_handler.h.
 *
 * Transcribed from Decomp/EVA_Decomp/eva_export/functions/:
 *   CSTGUnsolMsgHandler@0891c090.c        (ctor, 297 bytes)
 *   _CSTGUnsolMsgHandler@089b9e30.c       (ResetVTable, 11 bytes)
 *   _CSTGUnsolMsgHandler@089b9e40.c       (DeletingDtor, 39 bytes)
 *   HandleMessage@089162e0.c              (168 bytes)
 *   EndHandling@0891c290.c                (283 bytes)
 *   SendValueSlider@0891c1f0.c            (71 bytes)
 *   SendValueEncoder@0891c240.c           (66 bytes)
 *   EnterGlobalObjectEdit@0891c3c0.c      (10 bytes)
 *   Initialize@0891c1c0.c / InitializeForSong@0891c1d0.c / BeginHandling@0891c1e0.c
 *     (1 byte each -- real, confirmed-empty)
 *   TestControlMsgHandler@08916230.c / ASKMsgHandler@08916240.c /
 *     CalibrationMsgHandler@08916250.c / FrontPanelMsgHandler@08916260.c /
 *     KLMMsgHandler@08916270.c (1 byte each -- real, confirmed-empty)
 *
 * See stg_unsol_msg_handler.h for the full layout/Tier A/B breakdown.
 */

#include "stg_unsol_msg_handler.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

/* USTGAPIFsck -- not reconstructed elsewhere in this project (a whole not-decoded
 * filesystem-check/mount-helper class, `nm -C` shows further siblings, e.g.
 * `StopCheckMedium()`, out of scope). Declared file-local, same convention as
 * `USTGAPIControl` below -- only the one real static method `SaveRandomSeed()` needs.
 * Real signature/body from GenericMumount@08e27120.c (356 bytes, `cc=__cdecl`,
 * `char **param_1`), cross-checked against direct `objdump -dr` for the two real
 * error-message format strings and the real errno-vs-EINTR (4) retry-loop condition.
 * `sErrorCode`/`sErrorString` are real static data members (`nm -C`-confirmed
 * addresses 0xea2de20/0xea2dda0) -- sized/typed from every real snprintf() call
 * site in this function (`sErrorCode` always literal 0xc; `sErrorString` is the
 * snprintf() destination with a literal 0x80-byte bound, so declared `char[0x80]`).
 */
class USTGAPIFsck {
public:
	/* .text+0x08e27120, 356 bytes. REAL -- fork()+execve("/korg/Eva/mumount",
	 * argv, {NULL})+waitpid() in the child/parent respectively; returns 1 only
	 * if the child both ran and exited with status 0. Every failure path sets
	 * sErrorCode/sErrorString (dead outputs -- nothing reconstructed elsewhere
	 * reads them back) before returning 0.
	 */
	static int GenericMumount(char *const *argv);

private:
	static char sErrorString[0x80];
	static int  sErrorCode;
};

char USTGAPIFsck::sErrorString[0x80];
int  USTGAPIFsck::sErrorCode;

int USTGAPIFsck::GenericMumount(char *const *argv)
{
	pid_t pid = fork();
	if (pid < 0) {
		sErrorCode = 0xc;
		snprintf(sErrorString, 0x80, "can't mount errno %d", errno);
		return 0;
	}

	if (pid == 0) {
		char *const envp[1] = { 0 };
		execve("/korg/Eva/mumount", const_cast<char *const *>(argv), envp);
		/* Real: subroutine does not return. */
		exit(1);
	}

	int status;
	for (;;) {
		pid_t w = waitpid(pid, &status, 0);
		if (w > 0)
			break;
		if (errno != 4 /* EINTR */) {
			sErrorCode = 0xc;
			snprintf(sErrorString, 0x80, "wait fails errno %d", errno);
			return 0;
		}
	}

	if ((status & 0x7f) == 0) {
		unsigned exitCode = (unsigned)(status >> 8) & 0xff;
		if (exitCode != 0) {
			sErrorCode = 0xc;
			snprintf(sErrorString, 0x80, "umount exit %d", exitCode);
			return 0;
		}
		return 1;
	}

	sErrorCode = 0xc;
	snprintf(sErrorString, 0x80, "umount execution errno %d", errno);
	return 0;
}

/* USTGAPIControl -- not reconstructed elsewhere in this project. Declared file-local
 * (same convention as ckernel.cpp's own local CTracer) with only the two real static
 * methods EndHandling() needs. Real signatures confirmed from functions.csv:
 *   ForceErPShutdown(.text+0x08e1cbe0, 60 bytes)  cc=__cdecl, ushort param_1
 *   SaveRandomSeed(.text+0x08e1d090, 78 bytes)    cc=__cdecl, void
 * Both static (no `this`).
 */
class USTGAPIControl {
public:
	static bool SaveRandomSeed();
	static void ForceErPShutdown(unsigned short code);
};

/* .text+0x08e1d090, 78 bytes. REAL -- `USTGAPIFsck::GenericMumount("/korg/Eva/mumount",
 * "sr", NULL)`. The literal `"sr"` (real ground-truth 2nd argv element, confirmed via
 * direct `objdump -dr` at the exact address the ctor loads, 0x08f774c9 -- NOT a guess)
 * turns out to be the compiler's own tail-suffix string-pooling of an unrelated
 * envelope-parameter name table entry ("ahdsr" ends in "...sr\0", reused verbatim as
 * this call's own, unrelated, standalone "sr" literal) -- confirmed via
 * `objdump -s -j .rodata` at that address, not misread. What "sr" itself means to
 * /korg/Eva/mumount (a mode flag? a device name?) is not recovered -- /korg/Eva/mumount
 * itself is a separate on-image binary, out of scope for this project.
 */
bool USTGAPIControl::SaveRandomSeed()
{
	static char argMumount[] = "/korg/Eva/mumount";
	static char argSr[] = "sr";
	char *argv[4] = { argMumount, argSr, 0, 0 };

	int ok = USTGAPIFsck::GenericMumount(argv);
	if (ok == 0)
		puts("failed to save random seed");
	return ok != 0;
}

/* .text+0x08e1cbe0, 60 bytes. REAL -- builds a 16-byte STGMessage (length-prefixed,
 * see ustg_user_api.h's own header comment: byte 0 is the total message length, not
 * a type tag) and forwards it via the already-real `SendSTGMessageWithSource()`.
 * Field shape confirmed from the real disassembly: {u16 length=0x10; u16 subtype=1;
 * u32 field8=0; u32 field12=0x27; u32 payload=code} -- the SAME 4-field prefix
 * `LoadStoredSettings()`'s own STGMessageLocalShape uses (lcd_control.cpp), plus one
 * more explicit trailing payload dword this call site actually sets (that file's own
 * 5th stack slot is present too, just left as uninitialized stack leftover there --
 * see its own comment).
 */
struct ErPShutdownMsgShape {
	unsigned short length;
	unsigned short subtype;
	unsigned int   field8;
	unsigned int   field12;
	unsigned int   payload;
};

void USTGAPIControl::ForceErPShutdown(unsigned short code)
{
	ErPShutdownMsgShape msg;
	msg.length = 0x10;
	msg.subtype = 1;
	msg.field8 = 0;
	msg.field12 = 0x27;
	msg.payload = code;

	USTGUserAPI::SendSTGMessageWithSource(reinterpret_cast<const STGMessage *>(&msg));
}

CSTGUnsolMsgHandler *CSTGUnsolMsgHandler::sInstance = 0;

/* Real values, confirmed by direct raw-byte read of the binary's own .rodata
 * (readelf -l to map VA->file offset, then read 4 bytes): _DAT_08ea8534 = 1023.0f,
 * _DAT_08f29a40 = 127.0f. HandleMessage()/SendValueSlider() scale a 0..127 slider
 * value up to a 0..1023 range with these -- not asserted from the decompile's
 * opaque DAT_ name alone, actually read.
 */
static const float kAnalogScaleNumerator = 1023.0f;
static const float kAnalogScaleDenominator = 127.0f;

/* File-scope statics -- real globals, not CSTGUnsolMsgHandler instance fields
 * (confirmed: symbols.csv lists all 4 as plain "Global" namespace labels, not under
 * "CSTGUnsolMsgHandler"). Nothing reconstructed anywhere in this project writes
 * sNowValueSlider/sLastValueSlider/sEncoderValue to a nonzero value -- the real
 * producer (presumably inside ControlMsgHandler's own Tier-B-stubbed body) isn't
 * traced, so every slider/encoder-forwarding branch below is real but currently dead
 * given this pass's own data, same "faithful but unreached" license used throughout
 * this project (e.g. CScheduler::Exec()'s own bail branches).
 */
static int sNowValueSlider = 0;
static int sLastValueSlider = 0;
static int sEncoderValue = 0;
static int s_bIsInGlobalObjectEdit = 0;

/* --- Stage 6 batch 2 (2026-07-25): PatchMsgHandler/EffectMgrMsgHandler/
 * EffectMsgHandler/HDRTrackMsgHandler/SetListMsgHandler support data ------------
 *
 * CStorage -- a whole not-reconstructed class (symbols.csv shows dozens of
 * methods). Only its three "current selection" statics are needed here, real
 * addresses/sizes confirmed via symbols.csv's own mangled names:
 *   CStorage::sm_ucCurrentProg   0x0af30548 (1 byte)
 *   <unnamed byte>               0x0af30549 (1 byte, immediately adjacent -- real,
 *                                 read by every one of these handlers as a paired
 *                                 "sub-id" alongside sm_ucCurrentProg, but never
 *                                 independently named by any mangled symbol in this
 *                                 export -- kept as a plain DAT_ name rather than
 *                                 guessing a real member name)
 *   CStorage::sm_ucCurrentCombi  0x0af3054a (1 byte)
 *   <unnamed byte>               0x0af3054b (1 byte, same pairing as above)
 *   CStorage::sm_usCurrentSong   0x0af3054c (2 bytes, "us" = unsigned short)
 * All five live in the real binary's own huge bss segment (mutable runtime
 * selection state, not a compile-time constant) -- declared here as genuine
 * zero-initialized globals, same treatment as EditApi/s_eNowRestoreSeqParameters
 * below, not merely `extern` to a symbol this reconstruction doesn't define.
 */
class CStorage {
public:
	static unsigned char sm_ucCurrentProg;
	static unsigned char sm_ucCurrentCombi;
	static unsigned short sm_usCurrentSong;
};
unsigned char CStorage::sm_ucCurrentProg = 0;
unsigned char CStorage::sm_ucCurrentCombi = 0;
unsigned short CStorage::sm_usCurrentSong = 0;
unsigned char DAT_0af30549 = 0; /* paired with sm_ucCurrentProg, see above */
unsigned char DAT_0af3054b = 0; /* paired with sm_ucCurrentCombi, see above */

/* Real global byte flag (0x0af0df1e, bss), gates PatchMsgHandler's entire body on
 * `(DAT_0af0df1e & 7) == 3` -- purpose not traced (a mode/state byte read nowhere
 * else in this reconstruction), kept opaque per this project's own "confirm one
 * field, don't retype the whole struct" convention. Non-static (plain external
 * linkage) so verify/test_stg_unsol_msg_handler.cpp can drive it directly.
 */
unsigned char DAT_0af0df1e = 0;

/* Real global, confirmed via disassembly of every one of these five handlers'
 * `if (s_eNowRestoreSeqParameters != 0) call(EditApi_vtbl+0x3c)` / `+0x38`
 * bracket -- never set nonzero by anything reconstructed in this project (same
 * "faithful but currently dead branch" status as sNowValueSlider et al. above),
 * so kept `static` (no test needs to touch it).
 */
static int s_eNowRestoreSeqParameters = 0;

/* CEditor::lastEditMessage -- real global, `_ZN7CEditor15lastEditMessageE`,
 * 0x0939c1e0, confirmed 2 bytes (every real store is a 16-bit `mov WORD PTR
 * ...,0x500c`). UPDATE (Stage 6 CEditor batch, 2026-07-25): `CEditor` is now a
 * real class (editor.h), which already declares this as a `static` member --
 * this is just that member's qualified out-of-line definition, dropping the
 * former `namespace CEditor { ... }` wrapper (declared directly in the
 * namespace back when CEditor itself wasn't reconstructed as a class yet).
 */
unsigned short CEditor::lastEditMessage = 0;

/* CESSongTask::ms_bShouldDirectStorePMRStatus -- real static, gates a direct-store
 * mode around HDRTrackMsgHandler's own two-track (subtype 0xb/0xc) special case.
 * CESSongTask is a large not-reconstructed class (many real methods per
 * symbols.csv) -- only this one static is declared here, file-local, same
 * Tier-B-adjacent convention as USTGAPIControl above.
 */
class CESSongTask {
public:
	static unsigned char ms_bShouldDirectStorePMRStatus;
};
unsigned char CESSongTask::ms_bShouldDirectStorePMRStatus = 0;

/* Real local `static const` byte tables, each belonging to a different,
 * not-reconstructed free function in the real binary (Ghidra's own
 * `Function(Args)::s_akbyAP`-style qualified names) -- read directly out of the
 * real binary's .rodata (readelf -l VA->file-offset, then a raw byte read), NOT
 * transcribed from the decompile's opaque table reference alone. Each is a
 * `{code, value}` byte-pair table indexed by the message's own subtype/sub-index
 * field. Real addresses/spans confirmed via each mangled `_ZZ...s_akbyAP`
 * symbol's own address up to the next such symbol in symbols.csv -- NOT the
 * CSWTCH_NNN Ghidra-synthesized switch-table names, which are per-decompile
 * artifacts and not reliable global symbols (see CSWTCH_290/CSWTCH_231 note
 * below, confirmed instead via direct disassembly of the real load instruction's
 * immediate operand).
 */
static const unsigned char kHandleEffectLFOParam_s_akbyAP[16] = {
	0x13,0x00, 0x13,0x01, 0x13,0x02, 0x13,0x04, 0x13,0x03, 0x13,0x05, 0x13,0x06, 0x13,0x07,
}; /* HandleEffectLFOParam(STGEffectSlotMsg*)::s_akbyAP, 0x08f1bd3c, 16 bytes */

static const unsigned char kHandleHDRMsg_s_akbyAP[30] = {
	0x58,0x0b, 0x58,0x10, 0x58,0x0f, 0x58,0x12, 0x58,0x11, 0x58,0x13, 0x58,0x14,
	0x58,0x08, 0x58,0x09, 0x58,0x0a, 0x58,0x03, 0x6b,0x00, 0x6b,0x10, 0x58,0x0c, 0x58,0x0d,
}; /* HandleHDRMsg(STGHDRTrackMsg*)::s_akbyAP, 0x08f1bd00, 30 bytes */

static const unsigned char kSetListMsgHandler_s_akbyAPSlot[10] = {
	0x13,0x04, 0x13,0x05, 0x13,0x06, 0x13,0x07, 0x13,0x0b,
}; /* CSTGUnsolMsgHandler::SetListMsgHandler(STGMessage&)::s_akbyAPSlot, 0x08f1bcdc, 10 bytes */

static const unsigned char kSetListMsgHandler_s_akbyAP[26] = {
	0x01,0x00, 0x01,0x01, 0x01,0x02, 0x01,0x03, 0x01,0x04, 0x01,0x05, 0x01,0x06,
	0x01,0x07, 0x01,0x08, 0x01,0x09, 0x02,0x10, 0x02,0x11, 0x01,0x0a,
}; /* CSTGUnsolMsgHandler::SetListMsgHandler(STGMessage&)::s_akbyAP, 0x08f1bce6, 26 bytes */

/* CSWTCH_290 -- SetListMsgHandler's own switch-validity table. Ghidra's decompile
 * expressed this as `CSWTCH_290[iVar3 + 0xf]` (a table it chose to start 15 bytes
 * before the first byte actually referenced); real disassembly
 * (`cmp BYTE PTR [edx+0x8f1c4a0],0x0` with edx holding the message's own raw
 * subtype field, unadjusted) shows the real base is 0x08f1c4a0 indexed directly
 * by the raw subtype -- both describe the same real bytes; this table uses the
 * direct-index form. Real span confirmed to cover indices 0..23 (only 3/4/5/
 * 0x12/0x14 are nonzero, matching the switch's own 5 real cases exactly).
 */
static const unsigned char kCSWTCH_290[24] = {
	1,0,1,1,1,1,0,0, 0,0,0,0,0,0,0,0, 0,0,1,0,1,0,0,0,
}; /* 0x08f1c4a0, real bytes */

/* CSWTCH_231 (EffectSlotMsgHandler's own real int[9] table at 0x08f1c460,
 * `mov ebp,[edi*4+0x8f1c460]`) is NOT the same table as the byte array Ghidra
 * also happens to name "CSWTCH_231" inside GlobalMsgHandler (a different,
 * unrelated table at a different real address -- confirmed by disassembling both
 * sites separately; Ghidra's CSWTCH_NNN names are a per-decompile-run counter,
 * not a real shared symbol). Used by EffectSlotMsgHandler below, keyed by the
 * message's own +2 `eSTGMidiSource` field (0..8, else default flag 1).
 */
static const int kCSWTCH_231[9] = { 4, 1, 2, 1, 1, 3, 1, 4, 2 };

/* HandleEffectSlotMsg(STGEffectSlotMsg*,eSTGMidiSource)::s_akbyAP, 0x08f1bd1e,
 * 30 bytes (15 {code,value} byte pairs) -- real bytes read directly from
 * .rodata, span confirmed bounded by the next mangled symbol in symbols.csv
 * (HandleEffectLFOParam(STGEffectSlotMsg*)::s_akbyAP @ 0x08f1bd3c). Indices 0
 * and 13 are the real 0xff sentinel ("unused sub-index", handler returns).
 */
static const unsigned char kHandleEffectSlotMsg_s_akbyAP[30] = {
	0xff,0xff, 0x01,0x00, 0x01,0x08, 0x01,0x03, 0x01,0x04, 0x01,0x05, 0x01,0x02,
	0x01,0x06, 0x01,0x07, 0x0e,0x02, 0x0d,0x02, 0x0d,0x00, 0x12,0x00, 0xff,0xff, 0x01,0x0a,
};

/* --- GlobalMsgHandler support data (Stage 6 batch 4, 2026-07-26) ------------------
 *
 * Three real local `static const` {code,delta} byte-pair tables, each belonging to a
 * different not-reconstructed free function (Ghidra's own qualified
 * `Function(Args)::s_akbyAP` names below), plus one real byte-flag table -- all read
 * directly out of the real binary's .rodata (`objdump -s -j .rodata`, VA confirmed via
 * the real load instruction's own immediate operand, not guessed from the decompile's
 * opaque table reference), same technique as this file's other s_akbyAP tables above.
 */
static const unsigned char kGlobalParamAP[220] = {
	0x01,0x00, 0x01,0x01, 0x01,0x02, 0x01,0x03, 0x01,0x04, 0x01,0x05, 0x01,0x06, 0x01,0x0b,
	0x01,0x0e, 0x01,0x0f, 0x01,0x10, 0x01,0x11, 0x02,0x00, 0x02,0x06, 0x02,0x0c, 0x02,0x12,
	0x02,0x18, 0x02,0x1e, 0x02,0x24, 0x02,0x36, 0x03,0x00, 0x03,0x01, 0x03,0x02, 0x03,0x03,
	0x03,0x04, 0x03,0x06, 0x03,0x0a, 0x03,0x0b, 0x03,0x0c, 0x03,0x0d, 0x03,0x0e, 0x03,0x0f,
	0x07,0x00, 0x07,0x01, 0x07,0x03, 0x07,0x04, 0x07,0x20, 0x07,0x21, 0x08,0x01, 0x08,0x0d,
	0x06,0x00, 0x06,0x08, 0x06,0x10, 0x06,0x20, 0x06,0x10, 0x06,0x20, 0x06,0x30, 0x06,0x39,
	0x00,0x10, 0x02,0x2a, 0x02,0x30, 0x01,0x0c, 0x03,0x11, 0x03,0x12, 0x01,0x20, 0x03,0x13,
	0x01,0x1f, 0x01,0x21, 0x06,0x5a, 0x06,0x5b, 0x06,0x5c, 0x06,0x5d, 0x06,0x5e, 0x06,0x5f,
	0x06,0x60, 0x06,0x61, 0x06,0x62, 0x06,0x63, 0x06,0x64, 0x06,0x65, 0x06,0x66, 0x06,0x67,
	0x06,0x68, 0x06,0x69, 0x06,0x6a, 0x06,0x72, 0x06,0x7a, 0x06,0x82, 0x06,0x8a, 0x06,0x8b,
	0x06,0x8c, 0x06,0x8d, 0x06,0x8e, 0x06,0x8f, 0x06,0x90, 0x06,0x91, 0x06,0x92, 0x06,0x93,
	0x06,0x94, 0x06,0x95, 0x06,0x96, 0x06,0x97, 0x06,0x98, 0x06,0x99, 0x06,0x9a, 0x06,0x9b,
	0x06,0x9c, 0x06,0x9d, 0x06,0x9e, 0x06,0x9f, 0x06,0xa0, 0x06,0xa1, 0x06,0xa2, 0x06,0xa3,
	0x06,0xa4, 0x06,0xa5, 0x06,0xa6, 0x06,0xa7, 0x07,0x02, 0x01,0x22,
}; /* HandleGlobalMsgGlobalParam(STGGlobalParamMsg_const*)::s_akbyAP, 0x08f1c2e0, 220
    * bytes = 110 pairs, index = global-param code 0..0x6d. NOT sentinel-terminated in
    * this range (no 0xff byte0 anywhere in it) -- GlobalMsgHandler's own "cVar9==-1
    * return" validity check is real but provably dead given this table's own content,
    * same "faithful but unreached" status as this file's other dead guard checks. */

static const unsigned char kDrumkitParamAP[62] = {
	0x0a,0x1f, 0x0a,0x20, 0x0a,0x21, 0x0a,0x0f, 0x0a,0x10, 0x0a,0x0c, 0x0a,0x0d, 0x0a,0x11,
	0x0a,0x12, 0x0a,0x13, 0x0a,0x14, 0x0a,0x15, 0x0a,0x16, 0x0a,0x17, 0x0a,0x18, 0x0a,0x19,
	0x0a,0x1a, 0x0a,0x1b, 0x0a,0x1c, 0x0a,0x1d, 0x0a,0x1e, 0x0a,0x03, 0x0a,0x04, 0x0a,0x07,
	0x0a,0x05, 0x0a,0x06, 0x0a,0x0b, 0x0a,0x0a, 0x0a,0x08, 0x0a,0x09, 0x0a,0x0e,
}; /* HandleGlobalMsgDrumkitParam(STGGlobalDrumKitMsg_const*)::s_akbyAP, 0x08f1c3c0,
    * 62 bytes = 31 pairs, index = drumkit-slot code 0..0x1e. code byte (byte0) is
    * always 0x0a (10) -- only the delta byte varies. */

static const unsigned char kWaveSeqParamAP[68] = {
	0x0b,0x05, 0x0b,0x07, 0x0b,0x06, 0x0b,0x0a, 0x0b,0x0b, 0x0b,0x0e, 0x0b,0x0c, 0x0b,0x0d,
	0x0b,0x11, 0x0b,0x12, 0x0b,0x0f, 0x0b,0x10, 0x0b,0x13, 0x0b,0x14, 0x0b,0x16, 0x0b,0x15,
	0x0b,0x17, 0x0b,0x19, 0x0b,0x1a, 0x0b,0x1b, 0x0b,0x1e, 0x0b,0x1f, 0x0b,0x1d, 0x0b,0x1c,
	0x0b,0x20, 0x0b,0x21, 0x0b,0x22, 0x0b,0x23, 0x0b,0x24, 0x0b,0x25, 0x0b,0x27, 0x0b,0x26,
	0xff,0xff, 0x0b,0x18,
}; /* HandleGlobalMsgWaveSequenceParam(STGGlobalWaveSequenceMsg_const*)::s_akbyAP,
    * 0x08f1c400, 68 bytes = 34 pairs, index = wave-sequence code 0..0x21. Entry 0x20
    * is a real {0xff,0xff} sentinel -- code 0x20 is intercepted by its own dedicated
    * if-branch inside GlobalMsgHandler before this table is ever indexed, so the
    * sentinel is confirmed-dead, not a guess. */

static const unsigned char kGlobalMsgWaveSeqFlag[18] = {
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,
}; /* Real bytes 0x08f1c491..0x08f1c4a2, 18 bytes. Real base address (confirmed via
    * `objdump -dr`: `cmp BYTE PTR [ecx+0x8f1c481],0x0` with ecx holding the message's
    * own raw, unadjusted wave-sequence code) is 0x08f1c481, direct-indexed by the raw
    * code -- NOT Ghidra's own decompile framing, which expresses this same real byte
    * range as `CSWTCH_231[code+0x21]` against an array it declares starting at
    * 0x08f1c460 (the SAME real address as EffectSlotMsgHandler's own, unrelated,
    * already-documented int[9] CSWTCH_231 table -- Ghidra's CSWTCH_NNN names are a
    * per-decompile-run counter, not a real shared symbol, exactly the ambiguity this
    * file's EffectSlotMsgHandler header comment already flags). This array is
    * declared pre-shifted to start at the real 0x08f1c491 (only the range this
    * function ever actually indexes, code in [0x10,0x21]) for a clean 0-based
    * `[code-0x10]` index, same "direct real bytes, index documented against its own
    * real base" convention as this file's own kCSWTCH_290. All 1 except code==0x20
    * (0), i.e. index (0x20-0x10)=0x10=16.
    */

/* SetWithoutUpdatingSTG -- the one out-of-scope free-function dependency
 * GlobalMsgHandler has. Real mangled name (`nm -C`):
 * `_ZL21SetWithoutUpdatingSTGhhhRK6CValue11EEditSource` = internal-linkage
 * `static void SetWithoutUpdatingSTG(unsigned char, unsigned char, unsigned char,
 * CValue const&, EEditSource)` -- a 5-parameter general signature. The real
 * `.clone.1` symbol actually called at both of GlobalMsgHandler's own call sites
 * (`call 8916f00 <..SetWithoutUpdatingSTGhhhRK6CValue11EEditSource.clone.1>`) is a
 * GCC IPA clone: since the function has internal linkage and GCC can see every call
 * site in the whole translation unit, it re-derived a custom argument-to-register
 * assignment for this specific clone and (confirmed by direct register tracing of
 * BOTH real call sites, not assumed) only 4 runtime values are ever actually passed
 * -- scope/code/value/payload-pointer in EAX/EDX/ECX/stack, in that order, at both
 * sites identically. The 5th (EEditSource) parameter is never materialized at either
 * call site, i.e. constant-propagated away by the same clone. Stubbed below matching
 * that real 4-argument runtime shape (not the full mangled signature) -- return value
 * unused at both call sites (each is the last statement in its case block,
 * immediately before an early `return;`), same "genuinely undecoded external call,
 * transcribed anyway" convention as this file's own GetResLength()/
 * CChunkBase_WriteBinary()/TObjArray_SIDEntry_Add() stand-ins.
 */
static void SetWithoutUpdatingSTG(unsigned char scope, unsigned char code, unsigned char value, void *payload)
{
	(void)scope; (void)code; (void)value; (void)payload;
}

/* --- ABI-level helpers to fill the raw {code*, adj} dispatch table -----------------
 *
 * The real ctor stores each handler as a bare code-pointer assignment
 * (`*(code**)(this+off) = ControlMsgHandler;`), which is how Ghidra's decompiler
 * flattens a non-virtual pointer-to-member-function literal once optimized -- on the
 * Itanium C++ ABI (this target), such a literal is a 2-word {ptr, adj} value with
 * adj always 0 and ptr always the function's plain entry address (never the
 * vtable-offset/odd encoding, which only applies to virtual functions). These
 * helpers extract that low word via a union, matching the compiler's own
 * representation directly rather than reproducing it by guesswork -- the same
 * "trust the ABI, do the raw thing" license this project already uses for the
 * CallVSlot1/2-style helpers (ckernel.cpp) and the manual vtable-swap idiom
 * (omega_ptr_array.h et al). Two overloads only, since the 17 real handlers only use
 * two distinct parameter shapes (const STGMessage& and plain STGMessage&).
 */
static inline void *AddrOfConstRefHandler(void (CSTGUnsolMsgHandler::*mfp)(const STGMessage &))
{
	union { void (CSTGUnsolMsgHandler::*m)(const STGMessage &); void *p[2]; } u;
	u.p[1] = 0;
	u.m = mfp;
	return u.p[0];
}

static inline void *AddrOfRefHandler(void (CSTGUnsolMsgHandler::*mfp)(STGMessage &))
{
	union { void (CSTGUnsolMsgHandler::*m)(STGMessage &); void *p[2]; } u;
	u.p[1] = 0;
	u.m = mfp;
	return u.p[0];
}

/* --- ctor --------------------------------------------------------------------- */

CSTGUnsolMsgHandler::CSTGUnsolMsgHandler(CEditor::CPanelIfcTask *owner)
{
	/* Real ctor writes these two bytes first, before the vtable pointer --
	 * preserved in original order even though it has no observable effect
	 * (nothing reads mFlagsA/mForceSaveOnEnd before they're set again, if ever).
	 */
	mFlagsA = 0;
	mForceSaveOnEnd = 0;

	mVtbl = 0; /* real: &PTR__CSTGUnsolMsgHandler_08f75688 -- not reconstructed, see header */
	mOwner = owner;
	sInstance = this;

	mTable[0].pfn  = AddrOfConstRefHandler(&CSTGUnsolMsgHandler::ControlMsgHandler);
	mTable[0].adj  = 0;
	mTable[1].pfn  = AddrOfConstRefHandler(&CSTGUnsolMsgHandler::GlobalMsgHandler);
	mTable[1].adj  = 0;
	mTable[2].pfn  = AddrOfRefHandler(&CSTGUnsolMsgHandler::CombiMsgHandler);
	mTable[2].adj  = 0;
	mTable[3].pfn  = AddrOfRefHandler(&CSTGUnsolMsgHandler::ProgramSlotMsgHandler);
	mTable[3].adj  = 0;
	mTable[4].pfn  = AddrOfRefHandler(&CSTGUnsolMsgHandler::ProgramMsgHandler);
	mTable[4].adj  = 0;
	mTable[5].pfn  = AddrOfRefHandler(&CSTGUnsolMsgHandler::PatchMsgHandler);
	mTable[5].adj  = 0;
	mTable[6].pfn  = AddrOfRefHandler(&CSTGUnsolMsgHandler::VoiceModelMsgHandler);
	mTable[6].adj  = 0;
	mTable[7].pfn  = AddrOfRefHandler(&CSTGUnsolMsgHandler::EffectMgrMsgHandler);
	mTable[7].adj  = 0;
	mTable[8].pfn  = AddrOfRefHandler(&CSTGUnsolMsgHandler::EffectSlotMsgHandler);
	mTable[8].adj  = 0;
	mTable[9].pfn  = AddrOfRefHandler(&CSTGUnsolMsgHandler::EffectMsgHandler);
	mTable[9].adj  = 0;
	/* Slots 10/11/12/13/15 are the 5 confirmed-*static* no-op handlers -- real
	 * plain function-pointer-to-void* casts, no union trick needed.
	 */
	mTable[10].pfn = (void *)&CSTGUnsolMsgHandler::TestControlMsgHandler;
	mTable[10].adj = 0;
	mTable[11].pfn = (void *)&CSTGUnsolMsgHandler::ASKMsgHandler;
	mTable[11].adj = 0;
	mTable[12].pfn = (void *)&CSTGUnsolMsgHandler::CalibrationMsgHandler;
	mTable[12].adj = 0;
	mTable[13].pfn = (void *)&CSTGUnsolMsgHandler::FrontPanelMsgHandler;
	mTable[13].adj = 0;
	mTable[14].pfn = AddrOfRefHandler(&CSTGUnsolMsgHandler::HDRTrackMsgHandler);
	mTable[14].adj = 0;
	mTable[15].pfn = (void *)&CSTGUnsolMsgHandler::KLMMsgHandler;
	mTable[15].adj = 0;
	mTable[16].pfn = AddrOfRefHandler(&CSTGUnsolMsgHandler::SetListMsgHandler);
	mTable[16].adj = 0;

	mSentinel = (int32_t)0xffffffff;
}

/* --- destructor-shaped functions (kept plainly named, not real C++ dtors --
 * see header's mVtbl note) ------------------------------------------------------ */

void CSTGUnsolMsgHandler::ResetVTable()
{
	mVtbl = 0; /* real: &PTR__CSTGUnsolMsgHandler_08f75688 */
}

void CSTGUnsolMsgHandler::DeletingDtor()
{
	ResetVTable();
	/* Real disassembly brackets this free() in HAL_DisableInterrupts()/
	 * HAL_EnableInterrupts() -- the kernel-side critical-section shim already
	 * established as a no-op-and-dropped userspace concern throughout this
	 * project (ckernel.cpp's own header comment); dropped here too, not
	 * declared as an extern call-contract.
	 */
	free(this);
}

/* --- the real dispatcher -------------------------------------------------------
 *
 * Real body reads STGMessage's own offset+4 field as the 0..16 subtype index (a
 * newly confirmed fact about STGMessage's layout -- see header). STGMessage stays
 * opaque otherwise; only that one int field is asserted, via raw byte-offset
 * arithmetic on the reference's address, same as ustg_user_api.cpp's own treatment
 * of this type elsewhere.
 */
void CSTGUnsolMsgHandler::HandleMessage(STGMessage &msg)
{
	int wasSliderPending = sNowValueSlider;
	int subtype = *(int *)((char *)&msg + 4);

	if (subtype < 17) {
		Slot &slot = mTable[subtype];
		void *pcVar4 = slot.pfn;
		void *pCVar3;

		/* Real generic ptr-to-member-function dispatch: low bit of the "ptr"
		 * word selects direct-address (even) vs vtable-offset (odd) encoding.
		 * Every real entry here is the even/direct case (see ctor) -- the odd
		 * branch is faithfully transcribed but dead given this class's own
		 * construction, not exercised by anything in this reconstruction.
		 */
		if (((uintptr_t)pcVar4 & 1) == 0) {
			pCVar3 = (char *)this + slot.adj;
		} else {
			pCVar3 = (char *)this + slot.adj;
			pcVar4 = *(void **)((char *)pCVar3 + (uintptr_t)pcVar4 - 1);
		}

		typedef void (*RawFn)(void *, void *);
		((RawFn)pcVar4)(pCVar3, &msg);
	}

	if (wasSliderPending != 0 && sNowValueSlider == 0) {
		CPanelOut::SAnalogEvt evt;
		evt.type = 0x19;
		evt.value = (int16_t)(int)((sLastValueSlider * kAnalogScaleNumerator) / kAnalogScaleDenominator);
		mOwner->OnAnalogEvent(&evt);
	}
}

/* --- EndHandling ----------------------------------------------------------------
 *
 * EditApi's own class is not reconstructed -- vtable slots +0x28 ("get scope id for
 * a named sub-object", returns a byte) and +0x2c ("query a flag for that id",
 * writes 1 byte through an out-param) are dispatched by hand, matching the original
 * disassembly's own raw vtable-offset calls exactly, same convention already used
 * for COmegaInterface::ExitRequested()'s `*(*sysapi+0x7c)` call (Stage 1) and
 * ckernel.cpp's CTracer/CHostInterfaceBase blobs.
 */
extern "C" void *EditApi; /* real global, defined in mains.cpp */

void CSTGUnsolMsgHandler::EndHandling()
{
	if (sNowValueSlider != 0) {
		CPanelOut::SAnalogEvt evt;
		evt.type = 0x19;
		evt.value = (int16_t)(int)((sLastValueSlider * kAnalogScaleNumerator) / kAnalogScaleDenominator);
		sNowValueSlider = 0;
		mOwner->OnAnalogEvent(&evt);
	}

	if (sEncoderValue != 0) {
		CPanelOut::SEncoderEvt evt;
		evt.value = (uint8_t)((unsigned)sEncoderValue >> 8);
		evt.reserved[0] = evt.reserved[1] = evt.reserved[2] = 0; /* real: uninitialized stack garbage, see header */
		evt.zero = 0;
		sEncoderValue = 0;
		mOwner->OnEncoderEvent(&evt);
	}

	if (mForceSaveOnEnd != 0) {
		typedef unsigned char (*GetScopeIdFn)(void *, const char *);
		typedef void (*QueryFlagFn)(void *, unsigned char, int, int, unsigned char *, int);

		/* EditApi is itself a pointer to the (opaque, not reconstructed)
		 * CEditApiInstance-shaped object -- one level of dereference gets its
		 * vtable pointer (the object's own first field), matching the real
		 * `*EditApi` in the disassembly exactly (not `&EditApi`, which would
		 * just be the global variable's own address).
		 */
		void *vtbl = *(void **)EditApi;
		GetScopeIdFn getScopeId = *(GetScopeIdFn *)((char *)vtbl + 0x28);
		QueryFlagFn queryFlag = *(QueryFlagFn *)((char *)vtbl + 0x2c);

		unsigned char scopeId = getScopeId(EditApi, "ESSong");
		unsigned char flag = 0;
		queryFlag(EditApi, scopeId, 0, 3, &flag, 1);

		if (flag == 0) {
			USTGAPIControl::SaveRandomSeed();
			sync();
			sleep(3);
			USTGAPIControl::ForceErPShutdown(0);
		}
	}
}

/* --- Shared EditApi vtable-dispatch helpers (Stage 6 batch 2, 2026-07-25) --------
 *
 * Every one of the five handlers below repeats the exact same real shape already
 * established (in raw, inlined form) by EndHandling() just above: fetch a scope id
 * via vtbl+0x28, optionally bracket the vtbl+0x30 "set param" call with vtbl+0x3c/
 * +0x38 if a sequencer-parameter restore is in progress, and toggle
 * USTGUserAPI::mNowStopMessaging/CEditor::lastEditMessage around the +0x30 call
 * itself. Factored here rather than re-inlined five times -- still byte-for-byte
 * the same real vtable offsets/argument shapes as each handler's own disassembly,
 * not a behavioral simplification. Every real call site also re-fetches `*EditApi`
 * AFTER the optional +0x3c call (visible in the disassembly as a fresh
 * `iVar = *EditApi;` reload) rather than reusing an earlier cached vtbl pointer --
 * preserved here for faithfulness even though s_eNowRestoreSeqParameters is always
 * 0 in this pass's own data, making the reload currently a no-op.
 *
 * REAL BUG found and fixed alongside this: PTR__CEditApiInstance_08e85da8
 * (mains.cpp) was sized 6 slots -- enough for EndHandling()'s own dead-branch-only
 * +0x28/+0x2c reads above, but not for these five handlers' unconditional +0x28/
 * +0x30 dispatch (+0x30/4 = slot 12). Bumped to 20 slots, see mains.cpp's own
 * WORKAROUND #2 comment.
 */
typedef unsigned char (*EditApiGetScopeIdFn)(void *, const char *);
typedef void (*EditApiVoidSelfFn)(void *);
typedef void (*EditApiSetParamFn)(void *, unsigned char, unsigned char, unsigned char, void *, int, int);

unsigned char CSTGUnsolMsgHandler::EditApiGetScopeId(const char *name)
{
	void *vtbl = *(void **)EditApi;
	EditApiGetScopeIdFn fn = *(EditApiGetScopeIdFn *)((char *)vtbl + 0x28);
	return fn(EditApi, name);
}

void CSTGUnsolMsgHandler::EditApiSendParamMsg(unsigned char scope, unsigned char code, unsigned char value,
                                               void *payload, int len, int flag)
{
	if (s_eNowRestoreSeqParameters != 0) {
		void *vtbl = *(void **)EditApi;
		EditApiVoidSelfFn beginRestore = *(EditApiVoidSelfFn *)((char *)vtbl + 0x3c);
		beginRestore(EditApi);
	}

	void *vtbl = *(void **)EditApi; /* real: fresh reload after the +0x3c call above */
	EditApiSetParamFn setParam = *(EditApiSetParamFn *)((char *)vtbl + 0x30);

	USTGUserAPI::mNowStopMessaging = 1;
	CEditor::lastEditMessage = 0x500c;
	setParam(EditApi, scope, code, value, payload, len, flag);
	USTGUserAPI::mNowStopMessaging = 0;

	if (s_eNowRestoreSeqParameters != 0) {
		void *vtbl2 = *(void **)EditApi;
		EditApiVoidSelfFn endRestore = *(EditApiVoidSelfFn *)((char *)vtbl2 + 0x38);
		endRestore(EditApi);
	}
}

/* --- slider/encoder value senders ------------------------------------------------ */

void CSTGUnsolMsgHandler::SendValueSlider()
{
	CPanelOut::SAnalogEvt evt;
	evt.type = 0x19;
	evt.value = (int16_t)(int)((sLastValueSlider * kAnalogScaleNumerator) / kAnalogScaleDenominator);
	mOwner->OnAnalogEvent(&evt);
}

void CSTGUnsolMsgHandler::SendValueEncoder()
{
	if (sEncoderValue != 0) {
		CPanelOut::SEncoderEvt evt;
		evt.value = (uint8_t)((unsigned)sEncoderValue >> 8);
		evt.reserved[0] = evt.reserved[1] = evt.reserved[2] = 0; /* real: uninitialized stack garbage, see header */
		evt.zero = 0;
		sEncoderValue = 0;
		mOwner->OnEncoderEvent(&evt);
	}
}

void CSTGUnsolMsgHandler::EnterGlobalObjectEdit(int enable)
{
	s_bIsInGlobalObjectEdit = enable;
}

/* --- real, confirmed-empty no-ops (both static and instance shapes) -------------- */

void CSTGUnsolMsgHandler::Initialize(CCombi *, CCombi *) {}
void CSTGUnsolMsgHandler::InitializeForSong(CCombi *, CCombi *) {}
void CSTGUnsolMsgHandler::BeginHandling() {}
void CSTGUnsolMsgHandler::TestControlMsgHandler(STGMessage &) {}
void CSTGUnsolMsgHandler::ASKMsgHandler(STGMessage &) {}
void CSTGUnsolMsgHandler::CalibrationMsgHandler(STGMessage &) {}
void CSTGUnsolMsgHandler::FrontPanelMsgHandler(STGMessage &) {}
void CSTGUnsolMsgHandler::KLMMsgHandler(STGMessage &) {}

/* --- Tier B link-stubs: genuinely deep per-subsystem processing, not implemented -- */

void CSTGUnsolMsgHandler::ControlMsgHandler(const STGMessage &) { /* Tier-B link-stub. .text+0x0891ac70, 4886 bytes. */ }
void CSTGUnsolMsgHandler::CombiMsgHandler(STGMessage &) { /* Tier-B link-stub. .text+0x08919360, 2951 bytes. */ }
void CSTGUnsolMsgHandler::ProgramSlotMsgHandler(STGMessage &) { /* Tier-B link-stub. .text+0x08918410, 1792 bytes. */ }
void CSTGUnsolMsgHandler::ProgramMsgHandler(STGMessage &) { /* Tier-B link-stub. .text+0x08919fd0, 3114 bytes. */ }
void CSTGUnsolMsgHandler::VoiceModelMsgHandler(STGMessage &) { /* Tier-B link-stub. .text+0x08917100, 2487 bytes. */ }

/* --- Tier A, batch 2 (2026-07-25): real bodies -----------------------------------
 *
 * All five share the guard/scope/table/dispatch shape documented at this file's
 * own top (CStorage/DAT_.../kCSWTCH_290/EditApiGetScopeId/EditApiSendParamMsg).
 * STGMessage stays opaque -- every field access below is raw byte-offset pointer
 * arithmetic on `&msg` cast to `unsigned char *`, same convention as
 * HandleMessage()'s own offset+4 read (STGMessage is Ghidra's own effectively
 * 1-byte-element type here, matching the real `param_1 + 0xN` = byte offset N in
 * every one of these functions' own decompile).
 */

/* CSTGUnsolMsgHandler::PatchMsgHandler(STGMessage&), .text+0x08916d90, 340 bytes. */
void CSTGUnsolMsgHandler::PatchMsgHandler(STGMessage &msg)
{
	unsigned char *p = (unsigned char *)&msg;

	if (*(int *)(p + 8) != 0)
		return;

	unsigned int target = *(unsigned int *)(p + 0x10);
	if ((*(unsigned int *)(p + 0xc) == (unsigned int)CStorage::sm_ucCurrentProg && target == (unsigned int)DAT_0af30549)
	    || target == 0xfffe) {
		if (target == 0xfffe && s_bIsInGlobalObjectEdit == 0)
			return;
	} else if (target != 0xffff) {
		return;
	}

	/* real: entire remaining body gated on this opaque mode/state byte, see
	 * this file's own top comment on DAT_0af0df1e.
	 */
	if ((DAT_0af0df1e & 7) != 3)
		return;

	if (*(int *)(p + 0x18) > 0)
		*(int *)(p + 0x18) -= 1;

	unsigned char value = p[0x14];
	unsigned char scope = EditApiGetScopeId("ESProg");

	EditApiSendParamMsg(scope, 0x53, value, p + 0x18, 4, 1);
}

/* CSTGUnsolMsgHandler::EffectMgrMsgHandler(STGMessage&), .text+0x08916600, 541 bytes. */
void CSTGUnsolMsgHandler::EffectMgrMsgHandler(STGMessage &msg)
{
	unsigned char *p = (unsigned char *)&msg;

	if (*(int *)(p + 8) != 0)
		return;

	int kind = *(int *)(p + 0x20);
	unsigned int target = *(unsigned int *)(p + 0x10);
	unsigned int objId, objSub;

	if (kind == 1)      { objId = (unsigned int)CStorage::sm_ucCurrentProg;  objSub = (unsigned int)DAT_0af30549; }
	else if (kind == 2) { objSub = (unsigned int)CStorage::sm_usCurrentSong; objId = 0; }
	else if (kind == 0) { objId = (unsigned int)CStorage::sm_ucCurrentCombi; objSub = (unsigned int)DAT_0af3054b; }
	else                { objId = 0; objSub = 0; }

	if ((*(unsigned int *)(p + 0xc) != objId || target != objSub) && target != 0xfffe && target != 0xffff)
		return;

	int idx = *(int *)(p + 0x18);
	unsigned char code  = kHandleEffectLFOParam_s_akbyAP[idx * 2];
	unsigned char value = kHandleEffectLFOParam_s_akbyAP[idx * 2 + 1];
	unsigned char scope;

	if (kind == 1) {
		if (target == 0xfffe && s_bIsInGlobalObjectEdit == 0) { scope = EditApiGetScopeId("ESSampling"); code += 3; }
		else                                                  { scope = EditApiGetScopeId("ESProg");     code += 2; }
	} else if (kind == 0) {
		scope = EditApiGetScopeId("ESCombi");
	} else {
		if (kind != 2)
			return;
		scope = EditApiGetScopeId("ESSong");
	}

	unsigned char slotVal = p[0x14];
	EditApiSendParamMsg(scope, (unsigned char)(code + slotVal), value, p + 0x1c, 4, 1);
}

/* CSTGUnsolMsgHandler::EffectMsgHandler(STGMessage&), .text+0x08916840, 660 bytes. */
void CSTGUnsolMsgHandler::EffectMsgHandler(STGMessage &msg)
{
	unsigned char *p = (unsigned char *)&msg;

	if (*(int *)(p + 8) != 0)
		return;

	int kind = *(int *)(p + 0x20);
	unsigned int target = *(unsigned int *)(p + 0x10);
	unsigned int objId, objSub;

	if (kind == 1)      { objId = (unsigned int)CStorage::sm_ucCurrentProg;  objSub = (unsigned int)DAT_0af30549; }
	else if (kind == 2) { objSub = (unsigned int)CStorage::sm_usCurrentSong; objId = 0; }
	else if (kind == 0) { objId = (unsigned int)CStorage::sm_ucCurrentCombi; objSub = (unsigned int)DAT_0af3054b; }
	else                { objId = 0; objSub = 0; }

	if ((*(unsigned int *)(p + 0xc) != objId || target != objSub) && target != 0xfffe && target != 0xffff)
		return;

	unsigned char scope = EditApiGetScopeId("ESEffect");
	unsigned char code, value;

	if (*(unsigned int *)(p + 0x18) == 0) {
		int sub = *(int *)(p + 0x14) + (*(int *)(p + 0x14) > 0xb ? 1 : 0);
		int k2 = *(int *)(p + 0x20);

		if (k2 == 1) {
			if (*(int *)(p + 0x10) == 0xfffe && s_bIsInGlobalObjectEdit == 0) { code = (unsigned char)((sub + 4) & 0xff); scope = EditApiGetScopeId("ESSampling"); }
			else                                                              { code = (unsigned char)((sub + 3) & 0xff); scope = EditApiGetScopeId("ESProg"); }
		} else if (k2 == 0) {
			code = (unsigned char)((sub + 1) & 0xff);
			scope = EditApiGetScopeId("ESCombi");
		} else {
			if (k2 != 2)
				return;
			scope = EditApiGetScopeId("ESSong");
			code = (unsigned char)((sub + 1) & 0xff);
		}

		/* real: turns the payload dword into a plain 0/1 boolean in place
		 * before it's sent (as the 4-byte payload) below.
		 */
		*(unsigned int *)(p + 0x1c) = (*(unsigned int *)(p + 0x1c) == 0) ? 1u : 0u;
		value = 1;
	} else {
		value = (unsigned char)(*(unsigned int *)(p + 0x18) & 0xff);
		code = p[0x14];
	}

	EditApiSendParamMsg(scope, code, value, p + 0x1c, 4, 1);
}

/* CSTGUnsolMsgHandler::HDRTrackMsgHandler(STGMessage&), .text+0x08917ad0, 488 bytes. */
void CSTGUnsolMsgHandler::HDRTrackMsgHandler(STGMessage &msg)
{
	unsigned char *p = (unsigned char *)&msg;

	if (*(int *)(p + 8) != 0)
		return;
	unsigned int target = *(unsigned int *)(p + 0xc);
	if (target != (unsigned int)CStorage::sm_usCurrentSong && target != 0xfffe && target != 0xffff)
		return;

	int idx = *(int *)(p + 0x14);
	unsigned char scope = EditApiGetScopeId("ESSong");
	unsigned int field10 = *(unsigned int *)(p + 0x10);
	unsigned char code, value;

	if ((unsigned int)(idx - 0xb) < 2) {
		/* real: brackets the dispatch below in a "direct store PMR status"
		 * mode -- table byte ordering swaps vs. the else branch (see header).
		 */
		CESSongTask::ms_bShouldDirectStorePMRStatus = 1;
		code  = kHandleHDRMsg_s_akbyAP[idx * 2];
		value = (unsigned char)((char)field10 + (char)kHandleHDRMsg_s_akbyAP[idx * 2 + 1]);
		EditApiSendParamMsg(scope, code, value, p + 0x18, 4, 1);
		CESSongTask::ms_bShouldDirectStorePMRStatus = 0;
	} else {
		code  = (unsigned char)((char)field10 + (char)kHandleHDRMsg_s_akbyAP[idx * 2]);
		value = kHandleHDRMsg_s_akbyAP[idx * 2 + 1];
		EditApiSendParamMsg(scope, code, value, p + 0x18, 4, 1);
	}
}

/* CSTGUnsolMsgHandler::SetListMsgHandler(STGMessage&), .text+0x08916b00, 549 bytes. */
void CSTGUnsolMsgHandler::SetListMsgHandler(STGMessage &msg)
{
	unsigned char *p = (unsigned char *)&msg;

	unsigned char scope = EditApiGetScopeId("ESSetList");
	int subtype = *(int *)(p + 0x14);
	unsigned char code, value;

	if ((unsigned int)(subtype - 3) < 0x12 && kCSWTCH_290[subtype] != 0) {
		int idx;
		switch (subtype) {
		case 3:    idx = 1; break;
		case 4:    idx = 2; break;
		case 5:    idx = 3; break;
		case 0x12: idx = 0; break;
		case 0x14: idx = 4; break;
		default:   return;
		}
		code  = (unsigned char)((kSetListMsgHandler_s_akbyAPSlot[idx * 2] + *(int *)(p + 0x10)) & 0xff);
		value = kSetListMsgHandler_s_akbyAPSlot[idx * 2 + 1];
	} else {
		int idx;
		switch (subtype) {
		case 6:    idx = 0;   break;
		case 7:    idx = 1;   break;
		case 8:    idx = 2;   break;
		case 9:    idx = 3;   break;
		case 10:   idx = 4;   break;
		case 0xb:  idx = 5;   break;
		case 0xc:  idx = 6;   break;
		case 0xd:  idx = 7;   break;
		case 0xe:  idx = 8;   break;
		case 0xf:  idx = 9;   break;
		case 0x10: idx = 10;  break;
		case 0x11: idx = 0xb; break;
		case 0x13: idx = 0xc; break;
		default:   return;
		}
		code  = kSetListMsgHandler_s_akbyAP[idx * 2];
		value = kSetListMsgHandler_s_akbyAP[idx * 2 + 1];
	}

	EditApiSendParamMsg(scope, code, value, p + 0x18, 4, 1);
}

/* CSTGUnsolMsgHandler::EffectSlotMsgHandler(STGMessage&), .text+0x08917cd0, real
 * 1856 bytes (0x08917cd0..0x08918410). Promoted from Tier B, see header comment for
 * the full reasoning (switch/jump-table cross-check, local_2c buffer-reuse
 * resolution). idx==2/idx==0xb/idx==3 all match EditApiSendParamMsg's shape exactly
 * (flag always 1, lastEditMessage always 0x500c); only the generic tail (every other
 * idx) diverges and is written out by hand below.
 */
void CSTGUnsolMsgHandler::EffectSlotMsgHandler(STGMessage &msg)
{
	unsigned char *p = (unsigned char *)&msg;

	if (*(int *)(p + 8) != 0)
		return;

	int kind = *(int *)(p + 0x20);
	unsigned int target = *(unsigned int *)(p + 0x10);
	unsigned int objId, objSub;

	if (kind == 1)      { objId = (unsigned int)CStorage::sm_ucCurrentProg;  objSub = (unsigned int)DAT_0af30549; }
	else if (kind == 2) { objSub = (unsigned int)CStorage::sm_usCurrentSong; objId = 0; }
	else if (kind == 0) { objId = (unsigned int)CStorage::sm_ucCurrentCombi; objSub = (unsigned int)DAT_0af3054b; }
	else                { objId = 0; objSub = 0; }

	if ((*(unsigned int *)(p + 0xc) != objId || target != objSub) && target != 0xfffe && target != 0xffff)
		return;

	/* Real field @+2: eSTGMidiSource, per HandleEffectSlotMsg's own mangled name --
	 * only ever used below as a 0..8 index into kCSWTCH_231.
	 */
	unsigned short midiSource = *(unsigned short *)(p + 2);
	int iVar3 = *(int *)(p + 0x14);
	int idx   = *(int *)(p + 0x18);

	unsigned char bVar1 = kHandleEffectSlotMsg_s_akbyAP[idx * 2];
	unsigned char cVar5 = kHandleEffectSlotMsg_s_akbyAP[idx * 2 + 1];
	if (bVar1 == 0xff)
		return;

	unsigned char scope;
	unsigned int code;

	if (kind == 1) {
		if (target == 0xfffe && s_bIsInGlobalObjectEdit == 0) { scope = EditApiGetScopeId("ESSampling"); code = bVar1 + 3; }
		else                                                  { scope = EditApiGetScopeId("ESProg");     code = bVar1 + 2; }
	} else if (kind == 0) {
		scope = EditApiGetScopeId("ESCombi");
		code = bVar1;
	} else {
		if (kind != 2)
			return;
		scope = EditApiGetScopeId("ESSong");
		code = bVar1;
	}

	/* Real switch on idx (0..14, jump table at 0x08f1bb1c), computing a signed
	 * byte adjustment (cVar4) added to `code` below -- except the "default" group
	 * (idx 0/2/3/4/5/6/7/8/13, or idx>14), which instead adds the message's own
	 * `iVar3` (+2) sub-value directly and skips the cVar4 step entirely (real: a
	 * `goto` past it, confirmed in disassembly). idx==2 within that default group
	 * is further special-cased and returns before reaching any of the idx==0xb/
	 * idx==3/generic tail code below.
	 */
	char cVar4 = (char)iVar3;
	bool skipCVar4Add = false;

	switch (idx) {
	default:
		code = code + (unsigned int)iVar3;
		skipCVar4Add = true;
		if (idx == 2) {
			unsigned char payload;
			if (*(int *)(p + 0x1c) == 0x19) {
				payload = 0;
				EditApiSendParamMsg(scope, (unsigned char)code, cVar5, &payload, 1, 1);
			} else {
				payload = 1;
				EditApiSendParamMsg(scope, (unsigned char)code, cVar5, &payload, 1, 1);
				payload = (unsigned char)((char)*(int *)(p + 0x1c) - 1);
				cVar5 = cVar5 + 1;
				EditApiSendParamMsg(scope, (unsigned char)code, cVar5, &payload, 1, 1);
			}
			return;
		}
		break;
	case 1:
		if (iVar3 > 0xb)
			cVar4 = cVar4 + 1;
		break;
	case 9:
		cVar4 = cVar4 - 0xc;
		break;
	case 10:
	case 11:
	case 12:
		cVar4 = 0;
		break;
	case 14:
		if (iVar3 != 0xb) {
			if ((unsigned int)(iVar3 - 0xc) < 2)
				cVar5 = 3;
			else if ((unsigned int)(iVar3 - 0xe) < 2)
				cVar5 = 2;
			if (iVar3 > 0xb)
				cVar4 = cVar4 + 1;
		} else {
			cVar5 = 8;
			cVar4 = 0xb;
		}
		break;
	}

	if (!skipCVar4Add)
		code = (unsigned char)((char)code + cVar4);

	if (idx == 0xb) {
		unsigned char payload;
		if (*(int *)(p + 0x1c) == 0) {
			payload = 0;
			EditApiSendParamMsg(scope, (unsigned char)code, cVar5, &payload, 1, 1);
		} else {
			payload = 1;
			EditApiSendParamMsg(scope, (unsigned char)code, cVar5, &payload, 1, 1);
			payload = (unsigned char)(*(int *)(p + 0x1c) & 0xff);
			cVar5 = cVar5 + 1;
			EditApiSendParamMsg(scope, (unsigned char)code, cVar5, &payload, 1, 1);
		}
	} else if (idx == 3) {
		int32_t val = *(int *)(p + 0x1c);
		if (val > 0xc)
			val -= 0xc;
		EditApiSendParamMsg(scope, (unsigned char)code, cVar5, &val, 4, 1);
	} else {
		/* Generic tail (every idx other than 2/0xb/3): flag comes from
		 * kCSWTCH_231[midiSource] (default 1 if out of range), payload is the
		 * message's own +0x1c field directly (4 bytes, real, no local copy), and
		 * -- the one real divergence from EditApiSendParamMsg's shape --
		 * CEditor::lastEditMessage is `(flag==3) + 0x500c`, not unconditionally
		 * 0x500c. Written out by hand rather than folded into the shared helper.
		 */
		int flag = 1;
		if (midiSource < 9)
			flag = kCSWTCH_231[midiSource];

		if (s_eNowRestoreSeqParameters != 0) {
			void *vtbl0 = *(void **)EditApi;
			EditApiVoidSelfFn beginRestore = *(EditApiVoidSelfFn *)((char *)vtbl0 + 0x3c);
			beginRestore(EditApi);
		}

		void *vtbl = *(void **)EditApi;
		EditApiSetParamFn setParam = *(EditApiSetParamFn *)((char *)vtbl + 0x30);

		USTGUserAPI::mNowStopMessaging = 1;
		CEditor::lastEditMessage = (uint16_t)((flag == 3 ? 1 : 0) + 0x500c);
		setParam(EditApi, scope, (unsigned char)code, cVar5, p + 0x1c, 4, flag);
		USTGUserAPI::mNowStopMessaging = 0;

		if (s_eNowRestoreSeqParameters != 0) {
			void *vtbl2 = *(void **)EditApi;
			EditApiVoidSelfFn endRestore = *(EditApiVoidSelfFn *)((char *)vtbl2 + 0x38);
			endRestore(EditApi);
		}
	}
}

/* CSTGUnsolMsgHandler::GlobalMsgHandler(STGMessage const&), .text+0x08918b50, real
 * 2012 bytes (0x08918b50..0x08919360). Promoted from Tier B (Stage 6 batch 4,
 * 2026-07-26). Every EditApi vtable dispatch below (+0x20/+0x28/+0x2c/+0x30/+0x38/
 * +0x3c) and all five real cases were hand-verified against `objdump -dr -M intel`
 * of the real function body, not just the Ghidra decompile -- the decompile's own
 * control flow turned out accurate everywhere it was cross-checked, EXCEPT its
 * `CSWTCH_231[code+0x21]` table-index framing (a Ghidra-invented base, corrected to
 * the real direct-indexed 0x08f1c481 base above, same ambiguity already flagged for
 * this file's other CSWTCH_NNN tables).
 *
 * Two genuinely asymmetric restore-guard shapes exist here, confirmed by direct
 * register tracing (not assumed to mirror EditApiSendParamMsg()'s uniform "live
 * check right before the call" shape used by every other case below):
 *
 *   - case 0 (subtype 0x26 sub-case): the begin-restore guard immediately before the
 *     shared final `setParam` call uses a SNAPSHOTTED `iVar8` (0 by default; if
 *     s_eNowRestoreSeqParameters was set, `iVar8` is instead re-read fresh
 *     immediately after that sub-case's OWN inner `setParam`+endRestore cycle) --
 *     not a fresh re-read at the point of the check itself. The corresponding
 *     END-restore guard right after the final call, by contrast, DOES use a fresh
 *     live re-read. Confirmed at 0x08918ee7 (`test edi,edi`) vs 0x08918d7f (`mov
 *     edx,ds:0xacadb20` -- fresh) in the real disassembly.
 *   - case 2 (subtype-0x14 field == 0x20 sub-case): real ground truth has a genuine
 *     `goto LAB_089192c8` (real address 0x089192c8, confirmed) from inside the
 *     "non-negative" branch back into the top of the "negative" branch, AFTER that
 *     branch's own inner setParam+endRestore cycle -- meaning the re-entered code's
 *     own begin-restore check can fire a SECOND, independent beginRestore() call
 *     even though one already ran earlier in the same case invocation. Transcribed
 *     below with a real C++ goto/label pair (matching this project's own
 *     edit_server.cpp goto-preservation precedent), not routed around.
 *
 * Both asymmetries are dead code given s_eNowRestoreSeqParameters's own always-0
 * status throughout this reconstruction (same as every other place this flag
 * appears in this file), but are preserved exactly rather than collapsed into the
 * EditApiSendParamMsg() shared helper, which assumes the simpler uniform shape that
 * every OTHER case below (1, 2's other two sub-cases, 3, 4) genuinely has.
 */
void CSTGUnsolMsgHandler::GlobalMsgHandler(const STGMessage &msg)
{
	unsigned char *p = (unsigned char *)&msg;

	/* Local to this function only, same "declared where first needed" convention
	 * as EndHandling()'s own QueryFlagFn -- +0x20 ("get param pointer", returns a
	 * pointer whose own +0x24 field is read) and +0x2c ("query a flag", writes a
	 * dword out-param) are each used by exactly one/two cases below.
	 */
	typedef void *(*EditApiGetParamPtrFn)(void *, unsigned char, unsigned char, unsigned char);
	typedef void (*EditApiQueryFlagFn)(void *, unsigned char, int, int, unsigned char *, int);

	switch (*(int *)(p + 8)) {
	case 0: {
		if (*(unsigned int *)(p + 0x10) > 0x6d)
			return;

		unsigned char scope = EditApiGetScopeId("ESGlobal");
		int paramCode = *(int *)(p + 0x10);
		unsigned int local_2c = *(unsigned int *)(p + 0x14);
		int iVar5 = *(int *)(p + 0xc);
		unsigned char rawCode = kGlobalParamAP[paramCode * 2];
		if (rawCode == 0xff)
			return;

		int iVar8;
		if (paramCode == 0x26) {
			int div12 = iVar5 / 0xc;

			if (s_eNowRestoreSeqParameters != 0) {
				void *vtbl0 = *(void **)EditApi;
				(*(EditApiVoidSelfFn *)((char *)vtbl0 + 0x3c))(EditApi);
			}
			USTGUserAPI::mNowStopMessaging = 1;
			CEditor::lastEditMessage = 0x500c;
			{
				void *vtblInner = *(void **)EditApi;
				EditApiSetParamFn setParamInner = *(EditApiSetParamFn *)((char *)vtblInner + 0x30);
				setParamInner(EditApi, scope, 8, 0, &div12, 4, 1);
			}
			USTGUserAPI::mNowStopMessaging = 0;

			iVar8 = 0;
			if (s_eNowRestoreSeqParameters != 0) {
				void *vtbl2 = *(void **)EditApi;
				(*(EditApiVoidSelfFn *)((char *)vtbl2 + 0x38))(EditApi);
				iVar8 = s_eNowRestoreSeqParameters; /* real: re-read AFTER endRestore, not before */
			}
			iVar5 = iVar5 % 0xc;
		} else {
			iVar8 = s_eNowRestoreSeqParameters;
			if ((unsigned int)(paramCode - 0x2c) < 2) {
				iVar5 = iVar5 + 8;
			} else if ((unsigned int)(paramCode - 4) < 3) {
				local_2c = (local_2c != 0);
			}
		}

		unsigned char value = (unsigned char)((char)iVar5 + (char)kGlobalParamAP[paramCode * 2 + 1]);

		/* Asymmetric begin-guard -- see this function's own header comment. */
		if (iVar8 != 0) {
			void *vtbl3 = *(void **)EditApi;
			(*(EditApiVoidSelfFn *)((char *)vtbl3 + 0x3c))(EditApi);
		}

		USTGUserAPI::mNowStopMessaging = 1;
		CEditor::lastEditMessage = 0x500c;
		{
			void *vtbl = *(void **)EditApi;
			EditApiSetParamFn setParam = *(EditApiSetParamFn *)((char *)vtbl + 0x30);
			setParam(EditApi, scope, rawCode, value, &local_2c, 4, 1);
		}
		USTGUserAPI::mNowStopMessaging = 0;

		/* End-guard is a fresh live re-read -- real, and NOT the same source as
		 * the begin-guard's `iVar8` above (see header comment).
		 */
		if (s_eNowRestoreSeqParameters != 0) {
			void *vtblEnd = *(void **)EditApi;
			(*(EditApiVoidSelfFn *)((char *)vtblEnd + 0x38))(EditApi);
		}
		return;
	}

	case 1: {
		if (*(int *)(p + 0xc) != -1)
			return;
		unsigned int idx = *(unsigned int *)(p + 0x18);
		if (idx > 0x1e)
			return;

		unsigned char scope = EditApiGetScopeId("ESGlobal");
		unsigned char stgCode  = kDrumkitParamAP[idx * 2];
		unsigned int  stgValue = (unsigned int)(unsigned char)kDrumkitParamAP[idx * 2 + 1];

		if (idx < 0x1f && ((1u << (idx & 0x1f)) & 0x401fffffu) != 0)
			stgValue += (unsigned int)(*(int *)(p + 0x14)) * 0x15;

		/* Real inline setParam call (code/value are the LITERAL constants
		 * 0xa/2, not stgCode/stgValue -- those are only used by the
		 * SetWithoutUpdatingSTG() stub call below).
		 */
		EditApiSendParamMsg(scope, 0xa, 2, p + 0x10, 4, 1);

		SetWithoutUpdatingSTG(scope, stgCode, (unsigned char)(stgValue & 0xff), p + 0x1c);
		return;
	}

	case 2: {
		if (*(int *)(p + 0xc) != -1)
			return;

		unsigned char scope = EditApiGetScopeId("ESGlobal");
		unsigned int sub = *(unsigned int *)(p + 0x14);

		if (sub == 0x20) {
			int val = *(int *)(p + 0x1c);

			/* Real goto LAB_089192c8 shape -- see this function's own header
			 * comment for the asymmetric-double-beginRestore nuance.
			 */
			if (val < 0) {
wave_seq_negate:
				val = (int)((unsigned int)(~val) >> 31);
				if (s_eNowRestoreSeqParameters != 0) {
					void *vtbl0 = *(void **)EditApi;
					(*(EditApiVoidSelfFn *)((char *)vtbl0 + 0x3c))(EditApi);
				}
			} else {
				val = val + 1;
				if (s_eNowRestoreSeqParameters != 0) {
					void *vtbl1 = *(void **)EditApi;
					(*(EditApiVoidSelfFn *)((char *)vtbl1 + 0x3c))(EditApi);
				}
				USTGUserAPI::mNowStopMessaging = 1;
				CEditor::lastEditMessage = 0x500c;
				{
					void *vtblInner = *(void **)EditApi;
					EditApiSetParamFn setParamInner = *(EditApiSetParamFn *)((char *)vtblInner + 0x30);
					setParamInner(EditApi, scope, 0xb, 2, &val, 4, 1);
				}
				USTGUserAPI::mNowStopMessaging = 0;
				if (s_eNowRestoreSeqParameters != 0) {
					void *vtbl2 = *(void **)EditApi;
					(*(EditApiVoidSelfFn *)((char *)vtbl2 + 0x38))(EditApi);
					goto wave_seq_negate;
				}
				val = (int)((unsigned int)(~val) >> 31);
			}

			unsigned int local_3c = (unsigned int)val;
			USTGUserAPI::mNowStopMessaging = 1;
			CEditor::lastEditMessage = 0x500c;
			{
				void *vtbl = *(void **)EditApi;
				EditApiSetParamFn setParam = *(EditApiSetParamFn *)((char *)vtbl + 0x30);
				setParam(EditApi, scope, 0xb, 3, &local_3c, 4, 1);
			}
			USTGUserAPI::mNowStopMessaging = 0;
			if (s_eNowRestoreSeqParameters != 0) {
				void *vtblEnd = *(void **)EditApi;
				(*(EditApiVoidSelfFn *)((char *)vtblEnd + 0x38))(EditApi);
			}
			return;
		}

		if (sub > 0x21)
			return;

		unsigned char subCode  = kWaveSeqParamAP[sub * 2];
		unsigned char subValue = kWaveSeqParamAP[sub * 2 + 1];

		if ((sub - 0x10) < 0x12 && kGlobalMsgWaveSeqFlag[sub - 0x10] != 0) {
			unsigned int target = *(unsigned int *)(p + 0x10);

			void *vtblP = *(void **)EditApi;
			EditApiGetParamPtrFn getParamPtr = *(EditApiGetParamPtrFn *)((char *)vtblP + 0x20);
			void *paramObj = getParamPtr(EditApi, scope, 0xb, 4);
			unsigned int clamp = *(unsigned int *)((char *)paramObj + 0x24);

			if ((int)clamp < (int)target) {
				subValue = (unsigned char)(subValue + (char)((char)target - (char)clamp) * 0x11);
				target = clamp;
			}

			EditApiSendParamMsg(scope, 0xb, 4, &target, 4, 1);

			SetWithoutUpdatingSTG(scope, subCode, subValue, p + 0x18);
			return;
		}

		unsigned int local_3c = *(unsigned int *)(p + 0x1c);
		if (sub < 0xe && ((1u << (sub & 0x1f)) & 0x3030u) != 0)
			local_3c += 1;

		EditApiSendParamMsg(scope, subCode, subValue, &local_3c, 4, 1);
		return;
	}

	case 3: {
		unsigned char scope = EditApiGetScopeId("ESGlobal");

		void *vtblQ = *(void **)EditApi;
		EditApiQueryFlagFn queryFlag = *(EditApiQueryFlagFn *)((char *)vtblQ + 0x2c);
		unsigned int flagVal = 0;
		queryFlag(EditApi, scope, 0xa, 1, (unsigned char *)&flagVal, 4);

		unsigned int local_3c = *(unsigned int *)(p + 0xc);
		if (local_3c == flagVal)
			return;

		EditApiSendParamMsg(scope, 0xa, 1, &local_3c, 4, 1);
		return;
	}

	case 4: {
		unsigned char scope = EditApiGetScopeId("ESGlobal");

		void *vtblQ = *(void **)EditApi;
		EditApiQueryFlagFn queryFlag = *(EditApiQueryFlagFn *)((char *)vtblQ + 0x2c);
		unsigned int flagVal = 0;
		queryFlag(EditApi, scope, 0xb, 1, (unsigned char *)&flagVal, 4);

		unsigned int local_2c = *(unsigned int *)(p + 0xc);
		if (local_2c == flagVal)
			return;

		EditApiSendParamMsg(scope, 0xb, 1, &local_2c, 4, 1);
		return;
	}

	default:
		return;
	}
}
