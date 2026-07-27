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
#include "system_api.h"

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
struct CStorageSingleton; /* forward declaration -- defined near CombiMsgHandler's own
                           * CStorage::GetInstance() support code below, the only
                           * caller that needs it. */

class CStorage {
public:
	static unsigned char sm_ucCurrentProg;
	static unsigned char sm_ucCurrentCombi;
	static unsigned short sm_usCurrentSong;

	/* .text+0x08a5f000, 92 bytes, __cdecl (functions.csv) -- see
	 * CombiMsgHandler's own section (this file, further down) for the real
	 * call-site evidence and the CStorageSingleton definition.
	 */
	static CStorageSingleton *GetInstance();
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

/* --- Tier B link-stub: not implemented -- see this file's own header comment (the
 * "Tier B" section) for the full 2026-07-26 from-scratch re-trace findings and exact
 * evidence. Summary:
 *   - ControlMsgHandler: RE-CONFIRMED genuinely deep (18 distinct out-of-scope call
 *     targets spanning CMMI/CControlSurface/CHelpManager/CModeManager/CDiskUtil/
 *     CSmplModeMgr/real Peg CForm dialogs/raw HAL interrupt-mask control). Real size
 *     5152 bytes (0x0891ac70..0x0891c090), corrected from the previously-documented
 *     4886B (an undercount from trusting Ghidra's own size label over the real
 *     next-symbol gap).
 * (VoiceModelMsgHandler was also RE-CHARACTERIZED alongside ControlMsgHandler on
 * 2026-07-26, but has since been promoted to Tier A -- see the "Tier A, batch 8" section
 * further down this file for its real implementation.)
 */

void CSTGUnsolMsgHandler::ControlMsgHandler(const STGMessage &) { /* Tier-B link-stub. .text+0x0891ac70, 5152 bytes. */ }

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

/* --- Tier A, batch 5 (2026-07-26): ProgramSlotMsgHandler --------------------------
 *
 * Promoted from Tier B (follow-up to the 2026-07-26 5-handler recheck, which had
 * traced ~80% of this by hand but deliberately stopped short of a verified
 * reconstruction -- see eva_stg_unsol_5handlers_recheck_2026-07-26.md). Every branch,
 * table read, and vtable dispatch below is hand-verified against `objdump -dr -M
 * intel` of the real .text+0x08918410..0x08918b49 body (NOT just the Ghidra
 * decompile, whose own SSA variable-name reuse across reloads made several fields
 * look like distinct values when they are in fact the same message field read twice
 * -- e.g. the decompile's `iVar3` and `uVar11` are BOTH just `*(int*)(msg+0x1c)`).
 *
 * Real size is 1849 bytes (0x08918410..0x08918b49), not Ghidra's own "size=1792"
 * label -- same "decompiler undercounts trailing out-of-line branch targets" pattern
 * already documented for EffectSlotMsgHandler; confirmed by disassembling straight
 * through to the next function's own entry with no gap.
 *
 * Shape: the same CStorage-guard / kind-switch (0=Combi/1=Prog/2=Song against
 * sm_ucCurrentCombi+DAT_0af3054b / sm_ucCurrentProg+DAT_0af30549 / sm_usCurrentSong,
 * 0xfffe/0xffff wildcard-target carve-out) already established by every other
 * handler in this file. Two real dependencies new to this file:
 * `CMMI::GetInstance()`/`CModeManager::IsOnTimbreProgramEditInContext(int) const`/
 * `CModeManager::ChangeToTopPage(CDesktop*, int)` and `CKGMsgProcessor::GetInstance()`
 * -- all stubbed just above (file-local, real signatures from functions.csv,
 * "real-signature call-contract extern, opaque-but-safe-to-call body" convention,
 * same spirit as SetWithoutUpdatingSTG() in the GlobalMsgHandler section above).
 *
 * Real dispatch is on `idx = *(int*)(msg+0x1c)` -- this is BOTH the value the real
 * code branches on AND the direct *2-stride index into the 77-entry {code,value}
 * byte-pair table `kProgramSlot_s_akbyAP` (real .rodata 0x08f1c140, confirmed by
 * `lea ebp,[esi+esi*1+0x8f1c140]` with esi holding this exact field, computed
 * UNCONDITIONALLY before any range check -- see the out-of-range note below):
 *   idx == 0x4a       : special case A -- extra field msg[0x28] feeds `value`
 *                       directly, single EditApiSendParamMsg call (len=4, using
 *                       &msg[0x20] as payload), return.
 *   idx == 9          : special case B -- packs (msg[0x28]<<7)|msg[0x20] into a
 *                       2-byte payload. If NOT in timbre-program-edit context
 *                       (IsOnTimbreProgramEditInContext(msg[0x14])==false), sends it
 *                       straight through (code/value from the table, unmodified). If
 *                       IN that context, queries EditApi's own "ESProg" flag (code=0,
 *                       value=0) and compares against the packed payload -- if
 *                       different, navigates via ChangeToTopPage() and sends a fixed
 *                       code=0/value=0 message instead, THEN stamps
 *                       CKGMsgProcessor::GetInstance()'s own +0x28/+0x29 byte flags
 *                       (purpose not traced, opaque, real) before returning; if EQUAL,
 *                       falls through to the same unmodified-table send as the
 *                       not-in-context case.
 *   idx == 0xb        : if kind==0 (Combi), decrements msg[0x20]'s own byte (real
 *                       `cmp cl,2; adc cl,0xff` idiom -- proven equivalent below to a
 *                       plain "decrement, floored so 0/1 stay unchanged"), then falls
 *                       into the generic tail with SVar6=0 (same as the plain-default
 *                       group).
 *   idx == 8          : special case C -- same overall shape as idx==9, but the
 *                       "ESCombi/ESSong-scope query" happens UNCONDITIONALLY up front
 *                       (its own "value" argument is the table's own value byte, not
 *                       a literal 0) to build the payload word, THEN the
 *                       in-context/ChangeToTopPage tail is the same shape as idx==9's.
 *   every other idx in [0, 0x49] (i.e. not 0x4a/9/0xb/8) : SVar6 = 0, generic tail.
 *   idx in [0x4b, 0x4d)                                   : SVar6 = msg[0x18] (byte).
 *   idx > 0x4c (out of the real table's own 0..0x4c bound) : SVar6 = 0, generic tail
 *                       -- real ground truth computes the table address (`ebp`)
 *                       BEFORE this bounds check and would read past the table's own
 *                       154 bytes if actually dereferenced; the bounds check gates
 *                       every real *use* of that address, so the OOB address itself
 *                       is dead, not dereferenced, in the shipped binary. This
 *                       reconstruction gates the table read on the same bound
 *                       (idx <= 0x4c) to avoid true C++ UB, which does not change any
 *                       observable outcome (SVar6=0 either way; the code/value
 *                       computed below become moot at the same idx-only-affects-
 *                       table-index-not-branch-outcome point).
 *
 * The generic tail (idx==0xb after its own decrement, the plain-default group, and
 * both the msg[0x18]-as-SVar6 and out-of-range cases) is the one hand-written case
 * that does NOT match EditApiSendParamMsg()'s uniform shape -- same real divergence
 * already established for EffectSlotMsgHandler's own generic tail: `flag` comes from
 * `kCSWTCH_231[midiSource]` (the SAME shared table/address already declared above,
 * confirmed by direct disassembly of this function too, not a coincidence) and
 * `CEditor::lastEditMessage` is `(flag==3)+0x500c`, not fixed. Every other call site
 * below (special cases A/B/C, both their simple and ChangeToTopPage-guarded tails)
 * genuinely has flag=1/lastEditMessage=0x500c fixed at every one of its own real call
 * sites -- confirmed by direct disassembly, not assumed -- so those five call sites
 * legitimately reuse the shared EditApiSendParamMsg() helper.
 */

/* HandleProgramSlot(STGProgramSlotMsg*,eSTGMidiSource)::s_akbyAP -- the small 2-entry
 * "direct store PMR status" table (index = msg[0x1c]-0xe, 0 or 1), real bytes at
 * 0x08f1c13c, 4 bytes, immediately followed in .rodata by the main table below (same
 * "compiler pools per-function local statics back to back" layout already seen
 * elsewhere in this file). Ghidra's own decompile did not recognize this as the same
 * named local static as the main table (it expressed the access as raw
 * `uVar11 + 0x8f1c12f + uVar2` arithmetic instead, which does not correspond to any
 * real single instruction) -- confirmed instead by direct `objdump -dr`: the real
 * instructions are `add bl,[ebp+ebp*1+0x8f1c13d]` / `movzx esi,[ebp+ebp*1+0x8f1c13c]`
 * with ebp=(msg[0x1c]-0xe), i.e. a direct idx*2-stride pair table exactly like every
 * other s_akbyAP table in this file, not the Ghidra-invented offset expression.
 */
static const unsigned char kProgramSlot_s_akbyAPSong[4] = {
	0x6a, 0x00, 0x6a, 0x10,
}; /* 0x08f1c13c, real bytes */

/* HandleProgramSlot(STGProgramSlotMsg*,eSTGMidiSource)::s_akbyAP -- the main 77-entry
 * {code,value} table, real bytes at 0x08f1c140, 154 bytes, indexed directly by
 * msg[0x1c] (0..0x4c) with no further scaling beyond the usual *2 stride --
 * confirmed by `lea ebp,[esi+esi*1+0x8f1c140]` with esi holding the raw field value
 * (not a separately-normalized index). Every even byte (the "code" component)
 * happens to be the constant 0x48 across all 77 entries -- confirmed by direct byte
 * read, not assumed -- but transcribed here in full rather than special-cased, same
 * "keep the real table even when a pattern is visible" convention as
 * kCSWTCH_290/kCSWTCH_231 above.
 */
static const unsigned char kProgramSlot_s_akbyAP[154] = {
	0x48,0x1c, 0x48,0x0d, 0x48,0x05, 0x48,0x0e, 0x48,0x0f, 0x48,0x1e, 0x48,0x1d, 0x48,0x1f,
	0x48,0x00, 0x48,0x00, 0x48,0x01, 0x48,0x02, 0x48,0x03, 0x48,0x04, 0x48,0x38, 0x48,0x54,
	0x48,0x39, 0x48,0x3a, 0x48,0x3b, 0x48,0x3c, 0x48,0x3d, 0x48,0x06, 0x48,0x07, 0x48,0x08,
	0x48,0x0b, 0x48,0x09, 0x48,0x0a, 0x48,0x0c, 0x48,0x36, 0x48,0x3e, 0x48,0x40, 0x48,0x3f,
	0x48,0x41, 0x48,0x42, 0x48,0x44, 0x48,0x43, 0x48,0x45, 0x48,0x20, 0x48,0x21, 0x48,0x22,
	0x48,0x23, 0x48,0x24, 0x48,0x25, 0x48,0x26, 0x48,0x27, 0x48,0x28, 0x48,0x29, 0x48,0x2a,
	0x48,0x2b, 0x48,0x2c, 0x48,0x2d, 0x48,0x2e, 0x48,0x2f, 0x48,0x30, 0x48,0x31, 0x48,0x33,
	0x48,0x32, 0x48,0x34, 0x48,0x35, 0x48,0x48, 0x48,0x47, 0x48,0x46, 0x48,0x4f, 0x48,0x50,
	0x48,0x51, 0x48,0x53, 0x48,0x52, 0x48,0x37, 0x48,0x4a, 0x48,0x49, 0x48,0x4c, 0x48,0x4b,
	0x48,0x4d, 0x48,0x4e, 0x48,0x10, 0x48,0x56, 0x48,0x58,
}; /* 0x08f1c140, real bytes, indices 0..0x4c */

/* CDesktop/CModeManager/CMMI -- not reconstructed elsewhere in this project. Declared
 * file-local (same convention as CESSongTask/USTGAPIControl above) with only the real
 * methods this handler needs. Real signatures confirmed via functions.csv:
 *   CMMI::GetInstance()                                       .text+0x08964540, __cdecl
 *   CModeManager::IsOnTimbreProgramEditInContext(int) const   .text+0x08966580, __thiscall
 *   CModeManager::ChangeToTopPage(CDesktop*, int)              .text+0x08965410, __thiscall
 * CMMI::GetInstance()'s returned singleton has two real fields read directly by every
 * caller in this file (+0x00 CDesktop*, +0x04 CModeManager*) -- modeled here as a
 * genuine static instance (not a null-returning stub) so the two pointers it yields
 * are always valid to pass as a real (if behaviorally inert) `this`, same
 * "safe-to-call opaque stub" spirit as SetWithoutUpdatingSTG() above, applied to a
 * pointer-returning singleton rather than a plain free function.
 */
class CDesktop {};
class CModeManager {
public:
	bool IsOnTimbreProgramEditInContext(int ctx) const { (void)ctx; return false; }
	void ChangeToTopPage(CDesktop *desktop, int flag) { (void)desktop; (void)flag; }

	/* ProgramMsgHandler's own case 5 reads a real int field at raw offset +0x30
	 * directly off a CModeManager* (`*(int*)(*(int*)(CMMI::GetInstance()+4)+0x30)`,
	 * purpose not traced) to choose between "ESCombi"/"ESSong" scope -- backing
	 * storage added (this class has no other data members, so +0x30 lands safely
	 * inside mRaw) so that raw pointer-offset read stays defined. Always 0 (neither
	 * 1 nor 2), so that whole branch is a confirmed-dead path in this reconstruction,
	 * same "real but currently unreachable from an opaque stub" status as
	 * IsOnTimbreProgramEditInContext()'s hardcoded false above.
	 */
	unsigned char mRaw[0x40];
	CModeManager() { for (int i = 0; i < 0x40; ++i) mRaw[i] = 0; }
};
class CMMI {
	struct Singleton { CDesktop *desktop; CModeManager *modeManager; };
public:
	static void *GetInstance()
	{
		static CDesktop sDesktop;
		static CModeManager sModeManager;
		static Singleton sInstance = { &sDesktop, &sModeManager };
		return &sInstance;
	}
};

/* CKGMsgProcessor -- not reconstructed elsewhere (a whole class with many real
 * methods per symbols.csv -- ctor/dtor/SetGEMax/GetKarmaNotes/Process/etc, all out of
 * scope). Only the real GetInstance() (.text+0x089138f0, __cdecl) this handler needs;
 * its singleton's own +0x28/+0x29 byte fields are set directly here (purpose not
 * traced, opaque, real) -- backed by a real static buffer (not a null stub) for the
 * same reason as CMMI's above.
 */
class CKGMsgProcessor {
public:
	static void *GetInstance()
	{
		static unsigned char sInstance[0x40] = { 0 };
		return sInstance;
	}
};

void CSTGUnsolMsgHandler::ProgramSlotMsgHandler(STGMessage &msg)
{
	typedef void (*EditApiQueryFlagFn)(void *, unsigned char, int, int, unsigned char *, int);

	unsigned char *p = (unsigned char *)&msg;

	if (*(int *)(p + 8) != 1)
		return;

	int kind = *(int *)(p + 0x24);
	unsigned int target = *(unsigned int *)(p + 0x10);
	unsigned int objId, objSub;

	if (kind == 1)      { objId = (unsigned int)CStorage::sm_ucCurrentProg;  objSub = (unsigned int)DAT_0af30549; }
	else if (kind == 2) { objSub = (unsigned int)CStorage::sm_usCurrentSong; objId = 0; }
	else if (kind == 0) { objId = (unsigned int)CStorage::sm_ucCurrentCombi; objSub = (unsigned int)DAT_0af3054b; }
	else                { objId = 0; objSub = 0; }

	if ((*(unsigned int *)(p + 0xc) != objId || target != objSub) && target != 0xfffe && target != 0xffff)
		return;

	unsigned short midiSource = *(unsigned short *)(p + 2);
	int modeCtx = *(int *)(p + 0x14);
	int idx = *(int *)(p + 0x1c);

	void *mmi = CMMI::GetInstance();
	CModeManager *modeMgr = *(CModeManager **)((char *)mmi + 4);
	bool inTimbreEdit = modeMgr->IsOnTimbreProgramEditInContext(modeCtx);

	unsigned char scope;
	if (kind == 0) {
		scope = EditApiGetScopeId("ESCombi");
	} else {
		scope = EditApiGetScopeId("ESSong");
		int songIdx = idx - 0xe;
		if ((unsigned int)songIdx < 2) {
			unsigned char payload = p[0x20];
			unsigned char code  = kProgramSlot_s_akbyAPSong[songIdx * 2];
			unsigned char value = (unsigned char)(p[0x14] + kProgramSlot_s_akbyAPSong[songIdx * 2 + 1]);

			CESSongTask::ms_bShouldDirectStorePMRStatus = 1;
			EditApiSendParamMsg(scope, code, value, &payload, 1, 1);
			CESSongTask::ms_bShouldDirectStorePMRStatus = 0;
			return;
		}
	}

	/* idx == 0x4a: special case A. */
	if (idx == 0x4a) {
		unsigned char code  = (unsigned char)(p[0x14] + kProgramSlot_s_akbyAP[idx * 2]);
		unsigned char value = (unsigned char)(p[0x28] + kProgramSlot_s_akbyAP[idx * 2 + 1]);
		EditApiSendParamMsg(scope, code, value, p + 0x20, 4, 1);
		return;
	}

	/* idx == 9: special case B. */
	if (idx == 9) {
		uint16_t payload16 = (uint16_t)(((unsigned int)*(int *)(p + 0x28) << 7) | *(unsigned short *)(p + 0x20));

		if (inTimbreEdit) {
			unsigned char scopeProg = EditApiGetScopeId("ESProg");
			unsigned int queried = 0;
			{
				void *vtbl = *(void **)EditApi;
				EditApiQueryFlagFn queryFlag = *(EditApiQueryFlagFn *)((char *)vtbl + 0x2c);
				queryFlag(EditApi, scopeProg, 0, 0, (unsigned char *)&queried, 4);
			}
			if (payload16 != (uint16_t)queried) {
				void *mmiB = CMMI::GetInstance();
				(*(CModeManager **)((char *)mmiB + 4))->ChangeToTopPage(*(CDesktop **)mmiB, 0);
				EditApiSendParamMsg(scopeProg, 0, 0, &payload16, 2, 1);
				void *kgmp = CKGMsgProcessor::GetInstance();
				((unsigned char *)kgmp)[0x28] = 1;
				((unsigned char *)kgmp)[0x29] = 1;
				return;
			}
		}

		unsigned char code  = (unsigned char)(p[0x14] + kProgramSlot_s_akbyAP[idx * 2]);
		unsigned char value = kProgramSlot_s_akbyAP[idx * 2 + 1];
		EditApiSendParamMsg(scope, code, value, &payload16, 2, 1);
		return;
	}

	int svar6 = 0;

	if (idx == 0xb) {
		if (kind == 0) {
			/* real: `cmp cl,2; adc cl,0xff` -- proven-equivalent plain
			 * decrement floored at value<2 (0 or 1 left unchanged).
			 */
			unsigned char v = p[0x20];
			if (v >= 2)
				v -= 1;
			*(uint32_t *)(p + 0x20) = v;
		}
		/* svar6 stays 0; falls into the generic tail below. */
	} else if (idx == 8) {
		/* idx == 8: special case C -- unconditional query up front (using the
		 * table's own value byte, not a literal), then the same in-context/
		 * ChangeToTopPage shape as idx==9.
		 */
		uint16_t payload16;
		{
			unsigned char code   = (unsigned char)(p[0x14] + kProgramSlot_s_akbyAP[idx * 2]);
			unsigned char qvalue = kProgramSlot_s_akbyAP[idx * 2 + 1];
			unsigned int queried = 0;
			void *vtbl = *(void **)EditApi;
			EditApiQueryFlagFn queryFlag = *(EditApiQueryFlagFn *)((char *)vtbl + 0x2c);
			queryFlag(EditApi, scope, code, qvalue, (unsigned char *)&queried, 2);
			payload16 = (uint16_t)((queried & 0x7f) | ((unsigned int)(*(int *)(p + 0x20)) << 7));
		}

		if (inTimbreEdit) {
			unsigned char scopeProg = EditApiGetScopeId("ESProg");
			unsigned int queried2 = 0;
			{
				void *vtbl = *(void **)EditApi;
				EditApiQueryFlagFn queryFlag = *(EditApiQueryFlagFn *)((char *)vtbl + 0x2c);
				queryFlag(EditApi, scopeProg, 0, 0, (unsigned char *)&queried2, 4);
			}
			if (payload16 != (uint16_t)queried2) {
				void *mmiC = CMMI::GetInstance();
				(*(CModeManager **)((char *)mmiC + 4))->ChangeToTopPage(*(CDesktop **)mmiC, 0);
				EditApiSendParamMsg(scopeProg, 0, 0, &payload16, 2, 1);
				void *kgmp = CKGMsgProcessor::GetInstance();
				((unsigned char *)kgmp)[0x28] = 1;
				((unsigned char *)kgmp)[0x29] = 1;
				return;
			}
		}

		unsigned char code  = (unsigned char)(p[0x14] + kProgramSlot_s_akbyAP[idx * 2]);
		unsigned char value = kProgramSlot_s_akbyAP[idx * 2 + 1];
		EditApiSendParamMsg(scope, code, value, &payload16, 2, 1);
		return;
	} else if ((unsigned int)(idx - 0x4b) < 2) {
		svar6 = p[0x18];
	}
	/* else: plain-default group or out-of-range (idx > 0x4c) -- svar6 stays 0. */

	/* Generic tail -- shared by idx==0xb (after its own decrement), the
	 * plain-default group, the out-of-range case, and the msg[0x18]-as-svar6 case.
	 * Diverges from EditApiSendParamMsg()'s shape: `flag` comes from
	 * kCSWTCH_231[midiSource] and CEditor::lastEditMessage is `(flag==3)+0x500c`,
	 * not fixed -- same real divergence already established for
	 * EffectSlotMsgHandler's own generic tail.
	 */
	int flag = 1;
	if (midiSource < 9)
		flag = kCSWTCH_231[midiSource];

	/* Real ground truth reads kProgramSlot_s_akbyAP[idx*2] unconditionally, even
	 * when idx > 0x4c (dead/OOB address, never actually reached with an
	 * out-of-bounds idx by any real caller) -- gated here on the table's own real
	 * bound to avoid true C++ UB; see this function's own header comment.
	 */
	unsigned int ti = ((unsigned int)idx <= 0x4c) ? (unsigned int)idx : 0;
	unsigned char code  = (unsigned char)(p[0x14] + kProgramSlot_s_akbyAP[ti * 2]);
	unsigned char value = (unsigned char)(svar6 + kProgramSlot_s_akbyAP[ti * 2 + 1]);

	if (s_eNowRestoreSeqParameters != 0) {
		void *vtbl0 = *(void **)EditApi;
		(*(EditApiVoidSelfFn *)((char *)vtbl0 + 0x3c))(EditApi);
	}

	void *vtbl = *(void **)EditApi;
	EditApiSetParamFn setParam = *(EditApiSetParamFn *)((char *)vtbl + 0x30);

	USTGUserAPI::mNowStopMessaging = 1;
	CEditor::lastEditMessage = (uint16_t)((flag == 3 ? 1 : 0) + 0x500c);
	setParam(EditApi, scope, code, value, p + 0x20, 4, flag);
	USTGUserAPI::mNowStopMessaging = 0;

	if (s_eNowRestoreSeqParameters != 0) {
		void *vtbl2 = *(void **)EditApi;
		(*(EditApiVoidSelfFn *)((char *)vtbl2 + 0x38))(EditApi);
	}
}

/* --- Tier A, batch 6 (2026-07-26): ProgramMsgHandler ------------------------------
 *
 * Promoted from Tier B this session (the recheck's own scaffolding: "9-way jump
 * table on msg+8, case 0 calls the shared file-local HandleProgToneAdjustParam(),
 * fewer distinct externs than CombiMsgHandler despite being the largest of the
 * three"). Real size 3114 bytes (0x08919fd0..0x891ac70), a real 9-way jump table on
 * `*(int*)(msg+8)` (.rodata 0x08f1bb88, index 0 and >8 both alias the same "return"
 * epilogue -- confirmed via objdump, not guessed) selecting cases 1..8 (no real case
 * 0). Every case re-checks the SAME Prog-only object guard (msg[0xc]==
 * CStorage::sm_ucCurrentProg && msg[0x10]==DAT_0af30549, 0xfffe/0xffff wildcard) --
 * GCC folded the repeated guard fragments across cases into shared jump trampolines
 * (confirmed real, not an artifact: several cases' guard-fail paths physically jump
 * into a DIFFERENT case's own guard code and back out again), which does not change
 * the source-level behavior, only its physical encoding. Every one of the 9 real
 * EditApi dispatch call sites uses flag=1/CEditor::lastEditMessage=0x500c fixed
 * EXCEPT case 5's own CMMI-gated sub-branch (flag=kCSWTCH_231[midiSource],
 * lastEditMessage=(flag==3)+0x500c, same divergence already established for
 * EffectSlotMsgHandler/ProgramSlotMsgHandler's own generic tails) -- so every other
 * case legitimately reuses the shared EditApiSendParamMsg() helper.
 *
 * Real dependencies new to this handler: `HandleProgToneAdjustParam` (case 6) -- a
 * real, internal-linkage (`static`) free function per its own mangled name
 * (`_ZL25HandleProgToneAdjustParamP30STGProgramPatchIndexedParamMsgb`), confirmed
 * regparm(3) with only 2 real runtime args (STGProgramPatchIndexedParamMsg*, bool),
 * same shared dependency the 2026-07-26 recheck flagged for CombiMsgHandler -- stubbed
 * file-local below (opaque, real-signature call-contract extern, same convention as
 * SetWithoutUpdatingSTG()). `CMMI`/`CModeManager` (case 5's own CModeManager+0x30
 * raw-offset read, gating an ESCombi/ESSong scope choice) reuse the classes already
 * declared above for ProgramSlotMsgHandler.
 *
 * Nine real per-subtype {code,value} byte-pair tables, all confirmed by direct
 * `objdump -dr`/raw .rodata byte reads (not the decompile's own table-name framing
 * alone -- several sit close enough together that a naive "just trust the name"
 * transcription would have silently spliced two tables' bytes, most notably case 5's
 * own Sampling/normal pair, whose real bases are exactly 32 bytes apart and share one
 * coincidentally-identical byte pair at the seam: table1's own last entry
 * (0x08f1c0e0/e1 = {0x21,0x01}) reads identically to table2's own first entry at the
 * same address -- two genuinely separate compile-time arrays, confirmed by their
 * distinct real base addresses in the two call sites' own `lea`/`movzx` immediates,
 * not a single shared array):
 *   kProgramMsg_HandleProgramParamForSampling  0x08f1bf40, 48 entries (case 1, Sampling)
 *   kProgramMsg_HandleProgramParam             0x08f1bfa0, 58 entries (case 1, ESProg)
 *   kProgramMsg_HandleVectorMotionParam        0x08f1c060, 24 entries (case 2)
 *   kProgramMsg_HandleCommonLFOParam           0x08f1c020, 17 entries (case 3)
 *   kProgramMsg_HandleCommonStepSeqParam       0x08f1c090, 13 entries (case 4)
 *   kProgramMsg_HandleProgControllerParamForSampling 0x08f1c0c0, 17 entries (case 5, Sampling)
 *   kProgramMsg_HandleProgControllerParam      0x08f1c0e0, 17 entries (case 5, normal)
 *   kProgramMsg_HandleProgAudioInputParamForSampling 0x08f1c10e, 10 entries (case 7, Sampling)
 *   kProgramMsg_HandleProgAudioInputParam      0x08f1c122, 10 entries (case 7, normal)
 *   kProgramMsg_HandleProgEffectBalanceParam   0x08f1c136, 3 entries (case 8, already
 *     declared above for ProgramSlotMsgHandler's own neighbor-table note -- reused).
 * Five real scope-name string literals confirmed via `objdump -s -j .rodata`:
 * "ESProg" (0x8e79800), "ESCombi" (0x8e79831), "ESSong" (0x8e79896), "ESSampling"
 * (0x8e7987c), "ESCommon" (0x8e798c4).
 *
 * Case-by-case shape (msg+8 == subtype):
 *   1: two branches (target==0xfffe && !s_bIsInGlobalObjectEdit -> Sampling, else ->
 *      ESProg). ESProg branch has 6 sub-shapes keyed on msg[0x14] (0x25/0x26 do an
 *      EditApi QueryFlag + bit-merge into a 2-byte payload before the final
 *      2-byte-payload dispatch; 0x31/0x24 dispatch a 4-byte payload directly with a
 *      literal code; msg[0x14] in {0xe,0xf} zeroes msg[0x18] after reading it;
 *      msg[0x14]==0 rewrites msg[0x1c] in place first; every other value is the
 *      plain table lookup) -- all real, hand-verified against objdump.
 *   2: single ESProg table, msg[0x14] in {0x10,0x11} doubles the msg[0x18] adjustment.
 *   3: single ESProg table, no special sub-case.
 *   4: single ESProg table, msg[0x18] adjustment.
 *   5: Sampling branch (bound msg[0x14]<=0x10, msg[0x14]==0xf reassigns scope to
 *      "ESCommon", ==10 adds 8 to the adjustment) vs. normal branch (same 0xf/10
 *      special-casing, PLUS a msg[0x14]==6 CMMI-gated ESCombi/ESSong reselection
 *      sub-branch with a literal code and a different flag/lastEditMessage shape --
 *      the one hand-written call site in this handler).
 *   6: HandleProgToneAdjustParam(msg+0xc, false), return (no EditApi dispatch here
 *      at all -- real, matches decompile exactly).
 *   7: Sampling vs. normal branch, each with its own {code,value} table AND its own
 *      -1-sentinel-gated early return (`if (cVar6==-1) return;`) -- both branches
 *      otherwise identical shape.
 *   8: bound msg[0x14]<=2, single ESProg table, no adjustment.
 */

/* HandleProgToneAdjustParam -- shared file-local dependency (case 6 here, and
 * CombiMsgHandler's own case 0, per the 2026-07-26 recheck). Real internal-linkage
 * (`static`) free function, .text+0x08916390, 585 bytes, confirmed regparm(3) with
 * only 2 real runtime arguments (STGProgramPatchIndexedParamMsg*, bool) --> EAX/EDX.
 * Stubbed opaque (genuinely deep tone-adjustment-curve processing, out of scope for
 * this pass), same "real-signature call-contract extern" convention as
 * SetWithoutUpdatingSTG() above.
 */
static void HandleProgToneAdjustParam(void *msg, bool flag) __attribute__((regparm(3)));
static void HandleProgToneAdjustParam(void *msg, bool flag)
{
	(void)msg; (void)flag;
}

static const unsigned char kProgramMsg_HandleProgramParamForSampling[96] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00,
	0xff, 0xff, 0x01, 0x01, 0x01, 0x02, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
}; /* 0x08f1bf40, 48 entries */

static const unsigned char kProgramMsg_HandleProgramParam[116] = {
	0x23, 0x03, 0x23, 0x06, 0x23, 0x07, 0x23, 0x0a, 0x23, 0x09, 0x23, 0x05, 0x23, 0x12, 0x23, 0x11,
	0x23, 0x0b, 0x33, 0x4e, 0x23, 0x17, 0x23, 0x18, 0x23, 0x19, 0xff, 0xff, 0x23, 0x28, 0x23, 0x29,
	0x23, 0x0e, 0x23, 0x0d, 0x23, 0x0f, 0x23, 0x10, 0x23, 0x08, 0x23, 0x1a, 0x23, 0x1c, 0x23, 0x13,
	0x23, 0x15, 0x23, 0x14, 0x23, 0x16, 0x23, 0x1b, 0x4d, 0x0a, 0x4d, 0x0b, 0x23, 0x0c, 0x23, 0x2c,
	0x23, 0x2d, 0x23, 0x2e, 0x23, 0x2f, 0xff, 0xff, 0x53, 0x06, 0x23, 0x30, 0x23, 0x30, 0x23, 0x32,
	0x23, 0x31, 0x23, 0x33, 0x23, 0x34, 0x23, 0x35, 0x23, 0x36, 0x23, 0x37, 0x23, 0x38, 0x23, 0x39,
	0x23, 0x3a, 0x23, 0x41, 0x23, 0x3b, 0x23, 0x3c, 0x23, 0x3d, 0x23, 0x3e, 0x23, 0x3f, 0x23, 0x40,
	0x23, 0x4d, 0xff, 0xff,
}; /* 0x08f1bfa0, 58 entries */

static const unsigned char kProgramMsg_HandleVectorMotionParam[48] = {
	0x19, 0x06, 0x19, 0x07, 0x19, 0x04, 0x19, 0x00, 0x19, 0x01, 0x19, 0x05, 0x19, 0x02, 0x19, 0x03,
	0x19, 0x08, 0x19, 0x09, 0x4d, 0x00, 0x4e, 0x00, 0x19, 0x0b, 0x19, 0x0c, 0x19, 0x32, 0x19, 0x33,
	0x19, 0x0d, 0x19, 0x0e, 0x19, 0x17, 0x19, 0x1b, 0x19, 0x20, 0x19, 0x24, 0x19, 0x28, 0x19, 0x2d,
}; /* 0x08f1c060, 24 entries */

static const unsigned char kProgramMsg_HandleCommonLFOParam[34] = {
	0x30, 0x00, 0x30, 0x04, 0x30, 0x05, 0x30, 0x06, 0x30, 0x07, 0x30, 0x09, 0x30, 0x02, 0x30, 0x03,
	0x30, 0x01, 0x30, 0x08, 0x30, 0x0a, 0x30, 0x0b, 0x30, 0x0c, 0x30, 0x0d, 0x30, 0x0e, 0x30, 0x0f,
	0x30, 0x10,
}; /* 0x08f1c020, 17 entries */

static const unsigned char kProgramMsg_HandleCommonStepSeqParam[26] = {
	0x54, 0x00, 0x54, 0x01, 0x54, 0x02, 0x54, 0x03, 0x54, 0x04, 0x54, 0x05, 0x54, 0x07, 0x54, 0x08,
	0x54, 0x09, 0x54, 0x0a, 0x54, 0x2a, 0x54, 0x4a, 0x54, 0x06,
}; /* 0x08f1c090, 13 entries */

static const unsigned char kProgramMsg_HandleProgControllerParamForSampling[34] = {
	0x26, 0x01, 0x26, 0x04, 0x25, 0x06, 0x26, 0x00, 0x25, 0x00, 0x25, 0x02, 0x25, 0x04, 0x26, 0x02,
	0xff, 0xff, 0x30, 0x08, 0x30, 0x08, 0x26, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
	0x21, 0x01,
}; /* 0x08f1c0c0, 17 entries */

static const unsigned char kProgramMsg_HandleProgControllerParam[34] = {
	0x21, 0x01, 0x21, 0x04, 0x23, 0x23, 0x21, 0x00, 0x23, 0x1d, 0x23, 0x1f, 0x23, 0x21, 0x21, 0x02,
	0xff, 0xff, 0x2d, 0x08, 0x2d, 0x08, 0x21, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
	0x2e, 0x40,
}; /* 0x08f1c0e0, 17 entries */

static const unsigned char kProgramMsg_HandleProgAudioInputParamForSampling[20] = {
	0xff, 0xff, 0x1c, 0x00, 0x1c, 0x06, 0x1c, 0x0c, 0x1c, 0x12, 0x1c, 0x18, 0x1c, 0x1e, 0x1c, 0x24,
	0x1c, 0x2a, 0x1c, 0x30,
}; /* 0x08f1c10e, 10 entries */

static const unsigned char kProgramMsg_HandleProgAudioInputParam[20] = {
	0x22, 0x36, 0x22, 0x00, 0x22, 0x06, 0x22, 0x0c, 0x22, 0x12, 0x22, 0x18, 0x22, 0x1e, 0x22, 0x24,
	0x22, 0x2a, 0x22, 0x30,
}; /* 0x08f1c122, 10 entries */

/* HandleProgEffectBalanceParam(STGProgramIndexedParamMsg*)::s_akbyAP -- real bytes at
 * 0x08f1c136, 6 bytes, 3 entries (case 8's own idx<=2 bound). Real address
 * confirmed by the SAME instruction that ends kProgramMsg_HandleProgAudioInputParam's
 * own 10-entry span immediately above it in .rodata (0x08f1c122+20 = 0x08f1c136,
 * confirmed exact, no gap).
 */
static const unsigned char kProgramMsg_HandleProgEffectBalanceParam[6] = {
	0x18, 0x00, 0x18, 0x01, 0x18, 0x02,
}; /* 0x08f1c136, 3 entries */

/* CSTGUnsolMsgHandler::ProgramMsgHandler(STGMessage&), .text+0x08919fd0, 3114 bytes.
 * See this section's own header comment above for the full case-by-case shape.
 */
void CSTGUnsolMsgHandler::ProgramMsgHandler(STGMessage &msg)
{
	typedef void (*EditApiQueryFlagFn)(void *, unsigned char, int, int, unsigned char *, int);

	unsigned char *p = (unsigned char *)&msg;
	int subtype = *(int *)(p + 8);

	/* Every real case below targets a "Prog" object only -- the same guard,
	 * repeated per case in the real ground truth (GCC folded the repeated
	 * fragments into shared jump trampolines physically, not a behavioral
	 * difference; see header comment). Factored once here since every case
	 * needs exactly the same check.
	 */
	unsigned int target = *(unsigned int *)(p + 0x10);
	if ((*(unsigned int *)(p + 0xc) != (unsigned int)CStorage::sm_ucCurrentProg || target != (unsigned int)DAT_0af30549)
	    && target != 0xfffe && target != 0xffff)
		return;
	bool isSampling = (target == 0xfffe && s_bIsInGlobalObjectEdit == 0);

	switch (subtype) {
	case 1: {
		int idx = *(int *)(p + 0x14);

		if (isSampling) {
			unsigned char scope = EditApiGetScopeId("ESSampling");
			if (idx == 0) {
				int v = (*(int *)(p + 0x1c) != 3) ? *(int *)(p + 0x1c) : 5;
				*(int *)(p + 0x1c) = v;
			}
			unsigned char code  = (unsigned char)(p[0x18] + kProgramMsg_HandleProgramParamForSampling[idx * 2]);
			unsigned char value = kProgramMsg_HandleProgramParamForSampling[idx * 2 + 1];
			EditApiSendParamMsg(scope, code, value, p + 0x1c, 4, 1);
			return;
		}

		unsigned char scope = EditApiGetScopeId("ESProg");
		unsigned char code, value;

		if (idx == 0x25 || idx == 0x26) {
			unsigned char tblValue = kProgramMsg_HandleProgramParam[idx * 2 + 1];
			uint16_t local1e = 0;
			{
				void *vtbl = *(void **)EditApi;
				EditApiQueryFlagFn queryFlag = *(EditApiQueryFlagFn *)((char *)vtbl + 0x2c);
				queryFlag(EditApi, scope, 0x23, tblValue, (unsigned char *)&local1e, 2);
			}
			if (idx == 0x25)
				local1e = (uint16_t)((local1e & 0x7f) | (unsigned int)(*(int *)(p + 0x1c) << 7));
			else
				local1e = (uint16_t)((local1e & 0xff80) | *(uint16_t *)(p + 0x1c));
			EditApiSendParamMsg(scope, 0x23, tblValue, &local1e, 2, 1);
			return;
		}

		if (idx == 0x31) {
			value = (unsigned char)(p[0x18] + kProgramMsg_HandleProgramParam[idx * 2 + 1]);
			EditApiSendParamMsg(scope, 0x23, value, p + 0x1c, 4, 1);
			return;
		}

		if (idx == 0x24) {
			value = (unsigned char)(7 - (*(int *)(p + 0x18) == 0 ? 1 : 0));
			EditApiSendParamMsg(scope, 0x53, value, p + 0x1c, 4, 1);
			return;
		}

		int svar10;
		int iVar9;
		if ((unsigned int)(idx - 0xe) < 2) {
			iVar9 = *(int *)(p + 0x18);
			svar10 = 0;
			*(int *)(p + 0x18) = 0;
			iVar9 = iVar9 * 2;
		} else {
			if (idx == 0) {
				int v = (*(int *)(p + 0x1c) != 3) ? *(int *)(p + 0x1c) : 5;
				*(int *)(p + 0x1c) = v;
			}
			svar10 = p[0x18];
			iVar9 = 0;
		}
		code  = (unsigned char)(svar10 + kProgramMsg_HandleProgramParam[idx * 2]);
		value = (unsigned char)((iVar9 + kProgramMsg_HandleProgramParam[idx * 2 + 1]) & 0xff);
		EditApiSendParamMsg(scope, code, value, p + 0x1c, 4, 1);
		return;
	}

	case 2: {
		if (isSampling)
			return;
		unsigned char scope = EditApiGetScopeId("ESProg");
		int idx = *(int *)(p + 0x14);
		signed char adj = (signed char)(*(int *)(p + 0x18));
		if ((unsigned int)(idx - 0x10) < 2)
			adj = (signed char)(adj * 2);
		unsigned char code  = kProgramMsg_HandleVectorMotionParam[idx * 2];
		unsigned char value = (unsigned char)(adj + kProgramMsg_HandleVectorMotionParam[idx * 2 + 1]);
		EditApiSendParamMsg(scope, code, value, p + 0x1c, 4, 1);
		return;
	}

	case 3: {
		if (isSampling)
			return;
		unsigned char scope = EditApiGetScopeId("ESProg");
		int idx = *(int *)(p + 0x14);
		unsigned char code  = kProgramMsg_HandleCommonLFOParam[idx * 2];
		unsigned char value = kProgramMsg_HandleCommonLFOParam[idx * 2 + 1];
		EditApiSendParamMsg(scope, code, value, p + 0x1c, 4, 1);
		return;
	}

	case 4: {
		if (isSampling)
			return;
		int idx = *(int *)(p + 0x14);
		unsigned char value = (unsigned char)(p[0x18] + kProgramMsg_HandleCommonStepSeqParam[idx * 2 + 1]);
		unsigned char code  = kProgramMsg_HandleCommonStepSeqParam[idx * 2];
		unsigned char scope = EditApiGetScopeId("ESProg");
		EditApiSendParamMsg(scope, code, value, p + 0x1c, 4, 1);
		return;
	}

	case 5: {
		if (*(unsigned int *)(p + 0x14) > 0x10)
			return;

		if (isSampling) {
			unsigned char scope = EditApiGetScopeId("ESSampling");
			int idx = *(int *)(p + 0x14);
			signed char adj = (signed char)(p[0x18]);
			if (idx == 0xf) {
				scope = EditApiGetScopeId("ESCommon");
			} else if (idx == 10) {
				adj = (signed char)(adj + 8);
			}
			unsigned char code  = kProgramMsg_HandleProgControllerParamForSampling[idx * 2];
			unsigned char value = (unsigned char)(adj + kProgramMsg_HandleProgControllerParamForSampling[idx * 2 + 1]);
			EditApiSendParamMsg(scope, code, value, p + 0x1c, 4, 1);
			return;
		}

		unsigned short midiSource = *(unsigned short *)(p + 2);
		unsigned char scope = EditApiGetScopeId("ESProg");
		int idx = *(int *)(p + 0x14);
		int iVar9 = *(int *)(p + 0x18);
		signed char adj = (signed char)iVar9;

		/* Real: idx==0xf reassigns scope AND falls into the same "msg[0x14]==6"
		 * check below (harmless -- idx can't be both 0xf and 6 at once); idx==10
		 * skips the check entirely via its own real `goto` around it (see header
		 * comment); every other idx reaches the check with a real chance of
		 * being 6.
		 */
		if (idx == 0xf) {
			scope = EditApiGetScopeId("ESCommon");
		} else if (idx == 10) {
			adj = (signed char)(adj + 8);
			goto skipCmmiCheck;
		}

		if (idx == 6) {
			void *mmi = CMMI::GetInstance();
			CModeManager *modeMgr = *(CModeManager **)((char *)mmi + 4);
			int field30 = *(int *)((char *)modeMgr + 0x30);
			if ((unsigned int)(field30 - 1) < 2) {
				void *mmi2 = CMMI::GetInstance();
				CModeManager *modeMgr2 = *(CModeManager **)((char *)mmi2 + 4);
				int field30b = *(int *)((char *)modeMgr2 + 0x30);
				scope = (field30b == 1) ? EditApiGetScopeId("ESCombi") : EditApiGetScopeId("ESSong");

				int flag = 1;
				if (midiSource < 9)
					flag = kCSWTCH_231[midiSource];
				unsigned char value = (unsigned char)((iVar9 + 0xb) & 0xff);

				if (s_eNowRestoreSeqParameters != 0) {
					void *vtbl0 = *(void **)EditApi;
					(*(EditApiVoidSelfFn *)((char *)vtbl0 + 0x3c))(EditApi);
				}
				void *vtbl = *(void **)EditApi;
				EditApiSetParamFn setParam = *(EditApiSetParamFn *)((char *)vtbl + 0x30);
				USTGUserAPI::mNowStopMessaging = 1;
				CEditor::lastEditMessage = (uint16_t)((flag == 3 ? 1 : 0) + 0x500c);
				setParam(EditApi, scope, 0x2e, value, p + 0x1c, 4, flag);
				USTGUserAPI::mNowStopMessaging = 0;
				if (s_eNowRestoreSeqParameters != 0) {
					void *vtbl2 = *(void **)EditApi;
					(*(EditApiVoidSelfFn *)((char *)vtbl2 + 0x38))(EditApi);
				}
				return;
			}
		}

skipCmmiCheck:
		int flag = 1;
		if (midiSource < 9)
			flag = kCSWTCH_231[midiSource];
		unsigned char code  = kProgramMsg_HandleProgControllerParam[idx * 2];
		unsigned char value = (unsigned char)(adj + kProgramMsg_HandleProgControllerParam[idx * 2 + 1]);

		if (s_eNowRestoreSeqParameters != 0) {
			void *vtbl0 = *(void **)EditApi;
			(*(EditApiVoidSelfFn *)((char *)vtbl0 + 0x3c))(EditApi);
		}
		void *vtbl = *(void **)EditApi;
		EditApiSetParamFn setParam = *(EditApiSetParamFn *)((char *)vtbl + 0x30);
		USTGUserAPI::mNowStopMessaging = 1;
		CEditor::lastEditMessage = (uint16_t)((flag == 3 ? 1 : 0) + 0x500c);
		setParam(EditApi, scope, code, value, p + 0x1c, 4, flag);
		USTGUserAPI::mNowStopMessaging = 0;
		if (s_eNowRestoreSeqParameters != 0) {
			void *vtbl2 = *(void **)EditApi;
			(*(EditApiVoidSelfFn *)((char *)vtbl2 + 0x38))(EditApi);
		}
		return;
	}

	case 6:
		if (isSampling)
			return;
		HandleProgToneAdjustParam(p + 0xc, false);
		return;

	case 7: {
		int idx = *(int *)(p + 0x14);
		int fieldVal = *(int *)(p + 0x18);
		unsigned char scope;
		unsigned char code, value;

		if (isSampling) {
			scope = EditApiGetScopeId("ESSampling");
			code = kProgramMsg_HandleProgAudioInputParamForSampling[idx * 2];
			if (code == 0xff)
				return;
			value = kProgramMsg_HandleProgAudioInputParamForSampling[idx * 2 + 1];
		} else {
			scope = EditApiGetScopeId("ESProg");
			code = kProgramMsg_HandleProgAudioInputParam[idx * 2];
			if (code == 0xff)
				return;
			value = kProgramMsg_HandleProgAudioInputParam[idx * 2 + 1];
		}

		unsigned char sum = (unsigned char)((fieldVal + value) & 0xff);
		EditApiSendParamMsg(scope, code, sum, p + 0x1c, 4, 1);
		return;
	}

	case 8: {
		if (isSampling)
			return;
		if (*(unsigned int *)(p + 0x14) > 2)
			return;
		unsigned char scope = EditApiGetScopeId("ESProg");
		int idx = *(int *)(p + 0x14);
		unsigned char code  = kProgramMsg_HandleProgEffectBalanceParam[idx * 2];
		unsigned char value = kProgramMsg_HandleProgEffectBalanceParam[idx * 2 + 1];
		EditApiSendParamMsg(scope, code, value, p + 0x1c, 4, 1);
		return;
	}

	default:
		return;
	}
}

/* --- Tier A, batch 7 (2026-07-26): CombiMsgHandler --------------------------------
 *
 * Promoted from Tier B (deferred earlier this same day -- see
 * eva_stg_programslot_programmsg_reconstructed_combimsg_deferred_2026-07-26.md).
 * Real size 3184 bytes (0x08919360..0x08919fd0, not the "2951" functions.csv label --
 * same "decompiler undercounts trailing out-of-line branch targets" pattern already
 * flagged for EffectSlotMsgHandler/ProgramSlotMsgHandler; confirmed by disassembling
 * straight through to ProgramMsgHandler's own next entry with no gap).
 *
 * The deferred note's own concern was real: this function's physical .text layout
 * groups guard/table code BY SCOPE STRING rather than by case number (every case's
 * "kind==0" jump-out stub is a bare 2-instruction trampoline living in one clustered
 * region near the end of the function, physically far from that case's own main
 * body). The Ghidra decompile's own pseudocode, however, already reassembles this
 * correctly into a clean per-case switch -- confirmed NOT by trusting that
 * pseudocode blindly, but by independently re-deriving every case's own table
 * address from its own real `GetScopeId` call site via `objdump -dr -M intel`
 * (exactly the method the deferred note recommended) and cross-checking each one
 * against the decompile's own claim. Every one of the ~20 "kind==0" trampolines and
 * every jump-out/jump-back pair was traced by hand; the decompile's case-to-table
 * attribution turned out correct in every instance checked.
 *
 * Real 7-way jump table at 0x08f1bb6c (subtype = msg+8, 0..6):
 *   case 0: 0x89195d8   case 1: 0x8919678   case 2: 0x8919758   case 3: 0x8919880
 *   case 4: 0x8919390 (physically first)   case 5: 0x89194a8   case 6: 0x8919520
 * Kind field offset differs PER CASE (confirmed via direct disassembly of each
 * case's own entry, not guessed): cases 0/1/2/4/5 read kind from msg+0x20; case 3
 * from msg+0x28; case 6 from msg+0x1c -- same "each case re-derives its own kind
 * field" pattern already established for ProgramSlotMsgHandler/ProgramMsgHandler.
 * The objId/objSub resolution and the final wildcard-gated guard compare are
 * byte-for-byte identical across all 7 cases -- factored once into
 * CombiMsgHandlerGuardPass() below rather than repeated seven times.
 *
 * Eleven real per-case/per-scope {code,value} byte tables, every base address
 * independently re-derived this session (several disagree with the prior session's
 * own unverified proximity-based guesses -- confirmed by direct call-site tracing,
 * not assumed):
 *   case 0 ESSong  0x08f1c1e4  5 entries   case 0 ESCombi 0x08f1c1ee  5 entries
 *     (byte-identical to ESSong's own 5 entries -- confirmed by direct read, not a
 *     transcription error)
 *   case 1 (single table, both scopes) 0x08f1c200  24 entries
 *   case 2 ESSong  0x08f1c240  17 entries  case 2 ESCombi 0x08f1c260  17 entries --
 *     BOTH really are 17 entries (matching the real `idx<=0x10` bound check) even
 *     though each one's own compile-time array is only 16 entries long: the 17th
 *     ("idx==0x10") real read spills one {code,value} pair into the NEXT table in
 *     .rodata (ESSong's own idx16 reads ESCombi's idx0 bytes; ESCombi's own idx16
 *     reads case 3's HandleCombiToneAdjustParam table's own idx0 bytes) -- a real,
 *     confirmed-by-direct-byte-read coincidental adjacency, same class of gotcha as
 *     ProgramMsgHandler's own documented case-5 Sampling/normal 32-byte overlap.
 *     Transcribed here as genuine 17-entry arrays (the 17th entry's value IS
 *     whatever the adjacent table's own first bytes are) rather than modeled as a
 *     separate "spillover" special case, since that is exactly what the real
 *     hardware does.
 *   case 3 (single table, indexed by kind 0/2 only) 0x08f1c280  7 entries (only
 *     indices 0/2 are ever real-reachable; entries 1/3..6 are real bytes, unused by
 *     any call site, kept per this file's own "transcribe the whole real table"
 *     convention)
 *   case 4 ESSong  0x08f1c28e  10 entries  case 4 ESCombi 0x08f1c2a2  10 entries
 *     (byte-identical to ESSong's own 10 entries)
 *   case 5 ESSong  0x08f1c2b6  3 entries   case 5 ESCombi 0x08f1c2bc  3 entries
 *     (byte-identical to ESSong's own 3 entries)
 *   case 6 (single table, "ESSong" scope unconditionally regardless of kind) 0x08f1c2c2
 *     2 real entries, then real zero-padding -- real ground truth has no visible
 *     bound check on the index here either; modeled with a defensive 3rd "catch-all"
 *     entry equal to that real zero padding, same convention as cases 0/1 below.
 * Cases 0 and 1 ALSO have no visible bound check in the real ground truth (unlike
 * cases 2/4/5, which do) -- gated to their own real table extent here (idx<=4,
 * idx<=23) to avoid true C++ UB, same "no behavioral change for any real caller"
 * convention as ProgramSlotMsgHandler's own idx-bound gate.
 *
 * Case-by-case shape:
 *   0: kind 0/2 select ESCombi/ESSong + matching table, indexed by msg[0x18].
 *   1: kind 0/2 select scope only -- BOTH branches share the SAME single table
 *      (HandleCombiVectorMotionParam), indexed by msg[0x18]; adj = msg[0x14]*2
 *      UNLESS msg[0x18] is 0x10/0x11 (then adj = msg[0x14], undoubled) -- real,
 *      transcribed exactly as ground truth computes it.
 *   2: kind 0/2 select ESCombi/ESSong + matching table, indexed by msg[0x18]
 *      (bound <=0x10, matches each table's real 17-entry extent). msg[0x18]==0xf
 *      reassigns scope to "ESCommon"; ==10 adds 8 to the adjustment. Own
 *      CMMI-independent CSWTCH_231[midiSource]-gated flag/lastEditMessage dispatch
 *      (same divergence already established for ProgramMsgHandler's own case 5) --
 *      does NOT reuse EditApiSendParamMsg().
 *   3: kind 0/2 (from msg+0x28) select ESCombi/ESSong + a large not-reconstructed
 *      per-slot edit-buffer array (real base 0xaf0fb42 for Combi, 0xaf119e2 for
 *      Song -- both backed here by a generously-sized, modulo-clamped placeholder
 *      buffer, see CombiMsgHandlerReadBuf()'s own comment). If
 *      CModeManager::IsOnTimbreProgramEditInContext(msg[0x14]) is true, builds a
 *      24-byte on-stack STGProgramPatchIndexedParamMsg-shaped buffer and calls the
 *      SAME shared HandleProgToneAdjustParam() stub ProgramMsgHandler's own case 6
 *      already declared, then returns -- no EditApi dispatch at all in that branch.
 *      Otherwise: msg[0x20]<3 is a "fast" ConvertParamToLinear path whose RETURN
 *      VALUE is written directly into the outgoing param value (a genuine
 *      value-dependency, not just a gate -- see ConvertParamToLinear's own comment
 *      below); msg[0x20]>=3 is an IsCopyableBank-gated path with a second
 *      ConvertParamToLinear call whose result, compared against two literal
 *      sentinels (0x3d/0x28), gates a further EditApi vtbl+0x20 "get descriptor"
 *      dispatch -- dead in this reconstruction (ConvertParamToLinear's own stub
 *      never returns those sentinels) but transcribed faithfully anyway, per this
 *      file's own "keep real code even when currently unreachable" convention.
 *   4: kind 0/2 select ESCombi/ESSong + matching table, indexed by msg[0x18];
 *      table's own code byte doubling as a real -1 sentinel (`if(code==0xff)
 *      return;`), never actually hit by any of the 10 real entries in either table.
 *   5: kind 0/2 select ESCombi/ESSong + matching table, indexed by msg[0x18]
 *      (bound <=2).
 *   6: kind only gates the object guard (from msg+0x1c) -- scope is unconditionally
 *      "ESSong" regardless of which kind actually passed the guard, a real,
 *      confirmed-by-direct-disassembly quirk (no kind-based branch exists in the
 *      real code past the guard). Indexed by msg[0x14]; payload pointer is
 *      msg+0x18 (not msg+0x1c, unlike every other case here).
 * Every case above (except case 2, which has its own inline dispatch) reuses the
 * shared EditApiSendParamMsg() helper (flag=1, lastEditMessage=0x500c fixed),
 * confirmed real at every one of their own call sites.
 */

/* Shared guard-resolution helper for all 7 cases above -- kind's own source field
 * offset differs per case (passed in via kindOffset), but the objId/objSub
 * resolution and the final wildcard-gated compare are byte-for-byte identical in
 * every case, confirmed via direct disassembly of all 7 -- factored once here
 * rather than repeated seven times inline.
 */
static bool CombiMsgHandlerGuardPass(unsigned char *p, int kindOffset, int &kindOut)
{
	int kind = *(int *)(p + kindOffset);
	unsigned int target = *(unsigned int *)(p + 0x10);
	unsigned int objId, objSub;

	if (kind == 1)      { objId = (unsigned int)CStorage::sm_ucCurrentProg;  objSub = (unsigned int)DAT_0af30549; }
	else if (kind == 2) { objSub = (unsigned int)CStorage::sm_usCurrentSong; objId = 0; }
	else if (kind == 0) { objId = (unsigned int)CStorage::sm_ucCurrentCombi; objSub = (unsigned int)DAT_0af3054b; }
	else                { objId = 0; objSub = 0; }

	kindOut = kind;
	if ((*(unsigned int *)(p + 0xc) != objId || target != objSub) && target != 0xfffe && target != 0xffff)
		return false;
	return true;
}

/* CPrograms/CProgAncestor/EProgParamBankID/CProgCommon::EOscType -- new real
 * dependencies for case 3 only, not reconstructed elsewhere in this project.
 * CPrograms::GetProgramPointer/IsCopyableBank real signatures (functions.csv,
 * confirmed non-virtual plain `call <addr>` at both real call sites, not a vtable
 * dispatch):
 *   GetProgramPointer(EProgParamBankID, int, int)  .text+0x08a2ed60, 230B, __thiscall
 *   IsCopyableBank(EProgParamBankID, CProgCommon::EOscType)  .text+0x08a31270, 228B,
 *     __thiscall
 * Both stubbed opaque (never dereference `this`), backed by a REAL non-null static
 * CPrograms instance (not a null-returning stub) so calling a plain instance method
 * on it stays well-defined C++ -- same "real, non-null singleton" precedent as
 * CMMI::GetInstance() above, not the "null this" pattern a bare stub pointer would
 * invite. CProgAncestor stays a pure forward declaration (pointer-only use, never
 * dereferenced by anything reconstructed here).
 */
enum EProgParamBankID { eProgParamBankIDReserved = 0 };
class CProgCommon { public: enum EOscType { eOscTypeReserved = 0 }; };
class CProgAncestor; /* opaque, pointer-only */

class CPrograms {
public:
	CProgAncestor *GetProgramPointer(EProgParamBankID bank, int a, int b)
	{ (void)bank; (void)a; (void)b; return 0; }
	int IsCopyableBank(EProgParamBankID bank, CProgCommon::EOscType osc)
	{ (void)bank; (void)osc; return 0; }
};

/* CStorage::GetInstance() -- .text+0x08a5f000, 92 bytes, __cdecl (functions.csv).
 * Real ground truth returns a singleton whose first field is a CPrograms* (per
 * case 3's own two call sites: `puVar8 = (undefined4*)CStorage::GetInstance();`
 * immediately fed as `this` to GetProgramPointer/IsCopyableBank). Exposed here via
 * a real, host-pointer-sized named field rather than a raw 4-byte reinterpret of an
 * opaque pointer -- a literal `*(int*)` read of a real pointer field is only
 * correct on the real 32-bit target and would silently misread on a 64-bit
 * reconstruction host, the exact hazard class this project's own vtable-slot
 * lessons warn about applied to a plain data field instead of a vtable.
 */
struct CStorageSingleton { CPrograms *progs; };

CStorageSingleton *CStorage::GetInstance()
{
	static CPrograms sProgs;
	static CStorageSingleton s = { &sProgs };
	return &s;
}

/* CToneAdjustTool::ConvertParamToLinear -- .text+0x08a6b4d0, 294 bytes, __cdecl
 * (functions.csv). Unlike every other out-of-scope dependency in this file, its
 * RETURN VALUE is genuinely consumed (not merely gated) by case 3 above: written
 * directly into the outgoing message's own param-value field on the "fast" path,
 * and compared against two literal sentinels (0x3d/0x28) on the IsCopyableBank
 * path. Stubbed to a fixed sentinel (0) -- genuinely deep tone-adjustment-curve
 * algorithm, out of scope for this pass. This makes the 0x3d/0x28 comparison always
 * false, so the deeper EditApi vtbl+0x20 dispatch in case 3's own IsCopyableBank
 * branch never fires in this reconstruction -- same status as
 * CModeManager::IsOnTimbreProgramEditInContext()'s hardcoded false above.
 */
class CToneAdjustTool {
public:
	static int ConvertParamToLinear(const CProgAncestor *prog, int param)
	{ (void)prog; (void)param; return 0; }
};

/* CStorage::sm_oCombiEditBuffer / the analogous per-Song edit buffer (real address
 * 0xaf119e2 -- decompile's own literal, confirmed via direct disassembly; the Combi
 * variant's real combined base is 0xaf0fb42, a single constant at the real `add`
 * instruction, NOT a separate "+0x12c2" addend the way the decompile's own
 * two-piece symbolic expression implies -- Ghidra split a single real address into
 * a named symbol plus a constant offset, but the CPU only ever computes the one
 * combined value). Both are large, not-reconstructed live edit-buffer arrays
 * indexed at `base + msg[0x14]*0xbc` (per-slot base) then further at
 * `slotBase + slotIdx*4 + 0x36`/`+0x96` -- real, unguarded byte reads in the ground
 * truth (msg[0x14]/msg[0x1c] have no visible bound check there either). Backed here
 * by a generously-sized flat placeholder buffer (the real total array size is
 * unknown from this export) with a defensive modulo-clamp on the final computed
 * byte offset in CombiMsgHandlerReadBuf() below -- same "avoid true C++ UB, no
 * behavioral change for any real, legitimate caller" convention as
 * ProgramSlotMsgHandler's own idx-bound gate.
 */
static unsigned char DAT_0af0fb42_CombiEditBuffer[0x20000];
static unsigned char DAT_0af119e2_SongEditBuffer[0x20000];

static inline unsigned char CombiMsgHandlerReadBuf(unsigned char *buf, size_t bufSize, int slot, int extra)
{
	size_t off = ((size_t)(unsigned int)slot * 0xbc + (size_t)(unsigned int)extra) % bufSize;
	return buf[off];
}

static const unsigned char kHandleCombiMsgParam_Song[10] = {
	0x19, 0x00, 0x2e, 0x04, 0x2e, 0x06, 0x2e, 0x05, 0x2e, 0x11,
}; /* 0x08f1c1e4, 5 entries */

static const unsigned char kHandleCombiMsgParam_Combi[10] = {
	0x19, 0x00, 0x2e, 0x04, 0x2e, 0x06, 0x2e, 0x05, 0x2e, 0x11,
}; /* 0x08f1c1ee, 5 entries -- byte-identical to Song's own table, confirmed real */

static const unsigned char kHandleCombiVectorMotionParam[48] = {
	0x17, 0x06, 0x17, 0x07, 0x17, 0x04, 0x17, 0x00, 0x17, 0x01, 0x17, 0x05, 0x17, 0x02, 0x17, 0x03,
	0x17, 0x08, 0x17, 0x09, 0xff, 0xff, 0xff, 0xff, 0x17, 0x0b, 0x17, 0x0c, 0x17, 0x32, 0x17, 0x33,
	0x17, 0x0d, 0x17, 0x0e, 0x17, 0x17, 0x17, 0x1b, 0x17, 0x20, 0x17, 0x24, 0x17, 0x28, 0x17, 0x2d,
}; /* 0x08f1c200, 24 entries, shared by both ESCombi/ESSong branches of case 1 */

static const unsigned char kHandleCombiControllerParam_Song[34] = {
	0x2c, 0x01, 0x2c, 0x04, 0x2e, 0x0d, 0x2c, 0x00, 0x2e, 0x07, 0x2e, 0x09, 0x2e, 0x0b, 0x2c, 0x02,
	0xff, 0xff, 0x37, 0x08, 0x37, 0x08, 0x2c, 0x03, 0xff, 0xff, 0xff, 0xff, 0x2c, 0x0d, 0x00, 0x00,
	0x2c, 0x01,
}; /* 0x08f1c240, 17 entries -- idx16 is ESCombi's own idx0, see header comment */

static const unsigned char kHandleCombiControllerParam_Combi[34] = {
	0x2c, 0x01, 0x2c, 0x04, 0x2e, 0x0d, 0x2c, 0x00, 0x2e, 0x07, 0x2e, 0x09, 0x2e, 0x0b, 0x2c, 0x02,
	0xff, 0xff, 0x37, 0x08, 0x37, 0x08, 0x2c, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
	0x38, 0x40,
}; /* 0x08f1c260, 17 entries -- idx16 is case 3's HandleCombiToneAdjustParam idx0 */

static const unsigned char kHandleCombiToneAdjustParam[14] = {
	0x38, 0x40, 0x38, 0x00, 0x38, 0x10, 0x38, 0x30, 0x38, 0x49, 0x38, 0x08, 0x38, 0x20,
}; /* 0x08f1c280, 7 entries, only indices 0/2 ever real-reachable (kind is 0/2) */

static const unsigned char kHandleCombiAudioInputParam_Song[20] = {
	0x2d, 0x36, 0x2d, 0x00, 0x2d, 0x06, 0x2d, 0x0c, 0x2d, 0x12, 0x2d, 0x18, 0x2d, 0x1e, 0x2d, 0x24,
	0x2d, 0x2a, 0x2d, 0x30,
}; /* 0x08f1c28e, 10 entries */

static const unsigned char kHandleCombiAudioInputParam_Combi[20] = {
	0x2d, 0x36, 0x2d, 0x00, 0x2d, 0x06, 0x2d, 0x0c, 0x2d, 0x12, 0x2d, 0x18, 0x2d, 0x1e, 0x2d, 0x24,
	0x2d, 0x2a, 0x2d, 0x30,
}; /* 0x08f1c2a2, 10 entries -- byte-identical to Song's own table, confirmed real */

static const unsigned char kHandleCombiEffectBalanceParam_Song[6] = {
	0x16, 0x00, 0x16, 0x01, 0x16, 0x02,
}; /* 0x08f1c2b6, 3 entries */

static const unsigned char kHandleCombiEffectBalanceParam_Combi[6] = {
	0x16, 0x00, 0x16, 0x01, 0x16, 0x02,
}; /* 0x08f1c2bc, 3 entries -- byte-identical to Song's own table, confirmed real */

static const unsigned char kHandleCombiMetronomeParam[6] = {
	0x68, 0x0d, 0x68, 0x0e, 0x00, 0x00,
}; /* 0x08f1c2c2, 2 real entries + 1 catch-all matching real zero-padding, see header */

/* CSTGUnsolMsgHandler::CombiMsgHandler(STGMessage&), .text+0x08919360, 3184 bytes.
 * See this section's own header comment above for the full case-by-case shape.
 */
void CSTGUnsolMsgHandler::CombiMsgHandler(STGMessage &msg)
{
	typedef void *(*EditApiGetParamDescFn)(void *, unsigned char, unsigned char, unsigned int);

	unsigned char *p = (unsigned char *)&msg;
	int subtype = *(int *)(p + 8);

	switch (subtype) {
	case 0: {
		int kind;
		if (!CombiMsgHandlerGuardPass(p, 0x20, kind))
			return;
		unsigned char scope;
		const unsigned char *table;
		if (kind == 0)      { scope = EditApiGetScopeId("ESCombi"); table = kHandleCombiMsgParam_Combi; }
		else if (kind == 2) { scope = EditApiGetScopeId("ESSong");  table = kHandleCombiMsgParam_Song; }
		else return;

		int idx = *(int *)(p + 0x18);
		unsigned int ti = ((unsigned int)idx <= 4) ? (unsigned int)idx : 0;
		unsigned char code  = table[ti * 2];
		unsigned char value = table[ti * 2 + 1];
		EditApiSendParamMsg(scope, code, value, p + 0x1c, 4, 1);
		return;
	}

	case 1: {
		int kind;
		if (!CombiMsgHandlerGuardPass(p, 0x20, kind))
			return;
		unsigned char scope;
		if (kind == 0)      scope = EditApiGetScopeId("ESCombi");
		else if (kind == 2) scope = EditApiGetScopeId("ESSong");
		else return;

		int idx = *(int *)(p + 0x18);
		int field14 = *(int *)(p + 0x14);
		/* Real: adj = field14*2 by default, UNLESS idx is 0x10 or 0x11 (then adj
		 * stays field14, undoubled) -- transcribed exactly as ground truth
		 * computes it.
		 */
		signed char adj = ((unsigned int)(idx - 0x10) <= 1) ? (signed char)(field14 * 2)
		                                                     : (signed char)field14;
		unsigned int ti = ((unsigned int)idx <= 23) ? (unsigned int)idx : 0;
		unsigned char code  = kHandleCombiVectorMotionParam[ti * 2];
		unsigned char value = (unsigned char)(adj + kHandleCombiVectorMotionParam[ti * 2 + 1]);
		EditApiSendParamMsg(scope, code, value, p + 0x1c, 4, 1);
		return;
	}

	case 2: {
		int kind;
		if (!CombiMsgHandlerGuardPass(p, 0x20, kind))
			return;
		unsigned short midiSource = *(unsigned short *)(p + 2);
		if (*(unsigned int *)(p + 0x18) > 0x10)
			return;

		int field14 = *(int *)(p + 0x14);
		int idx = *(int *)(p + 0x18);
		unsigned char scope;
		const unsigned char *table;
		if (kind == 0)      { scope = EditApiGetScopeId("ESCombi"); table = kHandleCombiControllerParam_Combi; }
		else if (kind == 2) { scope = EditApiGetScopeId("ESSong");  table = kHandleCombiControllerParam_Song; }
		else return;

		int adj = field14;
		if (idx == 0xf)
			scope = EditApiGetScopeId("ESCommon");
		else if (idx == 10)
			adj = adj + 8;

		int flag = 1;
		if (midiSource < 9)
			flag = kCSWTCH_231[midiSource];
		unsigned char code  = table[idx * 2];
		unsigned char value = (unsigned char)(adj + table[idx * 2 + 1]);

		if (s_eNowRestoreSeqParameters != 0) {
			void *vtbl0 = *(void **)EditApi;
			(*(EditApiVoidSelfFn *)((char *)vtbl0 + 0x3c))(EditApi);
		}
		void *vtbl = *(void **)EditApi;
		EditApiSetParamFn setParam = *(EditApiSetParamFn *)((char *)vtbl + 0x30);
		USTGUserAPI::mNowStopMessaging = 1;
		CEditor::lastEditMessage = (uint16_t)((flag == 3 ? 1 : 0) + 0x500c);
		setParam(EditApi, scope, code, value, p + 0x1c, 4, flag);
		USTGUserAPI::mNowStopMessaging = 0;
		if (s_eNowRestoreSeqParameters != 0) {
			void *vtbl2 = *(void **)EditApi;
			(*(EditApiVoidSelfFn *)((char *)vtbl2 + 0x38))(EditApi);
		}
		return;
	}

	case 3: {
		int kind;
		if (!CombiMsgHandlerGuardPass(p, 0x28, kind))
			return;

		int modeCtx = *(int *)(p + 0x14);
		void *mmi = CMMI::GetInstance();
		CModeManager *modeMgr = *(CModeManager **)((char *)mmi + 4);
		bool inTimbreEdit = modeMgr->IsOnTimbreProgramEditInContext(modeCtx);

		if (inTimbreEdit) {
			EditApiGetScopeId("ESProg");
			/* Real stack layout (6 dwords, 24 bytes): local_30=0xffff,
			 * local_24=msg[0x1c], local_28=msg[0x20], local_20=msg[0x24] --
			 * exact field offsets preserved for documentation fidelity only;
			 * HandleProgToneAdjustParam is itself an opaque stub that never
			 * dereferences its argument, so the values don't affect behavior.
			 */
			int32_t local[6] = { 0, 0, 0, 0, 0, 0 };
			local[1] = 0xffff;
			local[2] = *(int32_t *)(p + 0x1c);
			local[3] = *(int32_t *)(p + 0x20);
			local[4] = *(int32_t *)(p + 0x24);
			HandleProgToneAdjustParam(local, true);
			return;
		}

		int bankSel = *(int *)(p + 0x20);
		int slotIdx = *(int *)(p + 0x1c);
		int field14 = *(int *)(p + 0x14);
		unsigned char scope;
		unsigned char *editBuf;
		size_t editBufSize;

		if (*(int *)(p + 0x28) == 0) {
			scope = EditApiGetScopeId("ESCombi");
			editBuf = DAT_0af0fb42_CombiEditBuffer; editBufSize = sizeof(DAT_0af0fb42_CombiEditBuffer);
		} else if (*(int *)(p + 0x28) == 2) {
			scope = EditApiGetScopeId("ESSong");
			editBuf = DAT_0af119e2_SongEditBuffer; editBufSize = sizeof(DAT_0af119e2_SongEditBuffer);
		} else {
			return;
		}

		if ((unsigned int)bankSel < 3) {
			int payloadVal = (*(int *)(p + 0x18) == 0) ? *(int *)(p + 0x24) : (*(int *)(p + 0x24) | 0x80);
			unsigned char bankByte0 = CombiMsgHandlerReadBuf(editBuf, editBufSize, field14, 0);
			unsigned char bankByte1 = CombiMsgHandlerReadBuf(editBuf, editBufSize, field14, 1);
			CPrograms *progs = CStorage::GetInstance()->progs;
			CProgAncestor *prog = progs->GetProgramPointer((EProgParamBankID)bankByte1, bankByte0, 0);
			*(int32_t *)(p + 0x24) = CToneAdjustTool::ConvertParamToLinear(prog, payloadVal);
		} else {
			unsigned char bankByte1 = CombiMsgHandlerReadBuf(editBuf, editBufSize, field14, 1);
			CPrograms *progs = CStorage::GetInstance()->progs;
			int copyable = progs->IsCopyableBank((EProgParamBankID)bankByte1, (CProgCommon::EOscType)0);
			if (copyable != 0) {
				int paramVal;
				if (bankSel == 5) {
					paramVal = CombiMsgHandlerReadBuf(editBuf, editBufSize, field14, slotIdx * 4 + 0x36);
				} else {
					paramVal = -1;
					if (bankSel == 4)
						paramVal = CombiMsgHandlerReadBuf(editBuf, editBufSize, field14, slotIdx * 4 + 0x96);
				}
				unsigned char bankByte0b = CombiMsgHandlerReadBuf(editBuf, editBufSize, field14, 0);
				unsigned char bankByte1b = CombiMsgHandlerReadBuf(editBuf, editBufSize, field14, 1);
				CProgAncestor *prog = progs->GetProgramPointer((EProgParamBankID)bankByte1b, bankByte0b, 0);
				int result = CToneAdjustTool::ConvertParamToLinear(prog, paramVal);
				if ((result == 0x3d || result == 0x28) && *(int32_t *)(p + 0x24) < 0) {
					/* Dead in this reconstruction (ConvertParamToLinear's stub
					 * never returns 0x3d/0x28) -- transcribed faithfully anyway,
					 * see this section's own header comment.
					 */
					unsigned char code  = (unsigned char)(field14 + kHandleCombiToneAdjustParam[kind * 2]);
					unsigned char value = (unsigned char)((kHandleCombiToneAdjustParam[kind * 2 + 1] + slotIdx) & 0xff);
					void *vtbl = *(void **)EditApi;
					EditApiGetParamDescFn getDesc = *(EditApiGetParamDescFn *)((char *)vtbl + 0x20);
					void *desc = getDesc(EditApi, scope, code, value);
					*(int32_t *)(p + 0x24) = *(int32_t *)((char *)desc + 0x20);
				}
			}
		}

		unsigned char code  = (unsigned char)(field14 + kHandleCombiToneAdjustParam[kind * 2]);
		unsigned char value = (unsigned char)((kHandleCombiToneAdjustParam[kind * 2 + 1] + slotIdx) & 0xff);
		EditApiSendParamMsg(scope, code, value, p + 0x24, 4, 1);
		return;
	}

	case 4: {
		int kind;
		if (!CombiMsgHandlerGuardPass(p, 0x20, kind))
			return;
		int field14 = *(int *)(p + 0x14);
		unsigned char scope;
		const unsigned char *table;
		if (kind == 0)      { scope = EditApiGetScopeId("ESCombi"); table = kHandleCombiAudioInputParam_Combi; }
		else if (kind == 2) { scope = EditApiGetScopeId("ESSong");  table = kHandleCombiAudioInputParam_Song; }
		else return;

		int idx = *(int *)(p + 0x18);
		unsigned int ti = ((unsigned int)idx <= 9) ? (unsigned int)idx : 0;
		unsigned char code = table[ti * 2];
		if (code == 0xff)
			return;
		unsigned char value = (unsigned char)((field14 + table[ti * 2 + 1]) & 0xff);
		EditApiSendParamMsg(scope, code, value, p + 0x1c, 4, 1);
		return;
	}

	case 5: {
		int kind;
		if (!CombiMsgHandlerGuardPass(p, 0x20, kind))
			return;
		if (*(unsigned int *)(p + 0x18) > 2)
			return;
		unsigned char scope;
		const unsigned char *table;
		if (kind == 0)      { scope = EditApiGetScopeId("ESCombi"); table = kHandleCombiEffectBalanceParam_Combi; }
		else if (kind == 2) { scope = EditApiGetScopeId("ESSong");  table = kHandleCombiEffectBalanceParam_Song; }
		else return;

		int idx = *(int *)(p + 0x18);
		unsigned char code  = table[idx * 2];
		unsigned char value = table[idx * 2 + 1];
		EditApiSendParamMsg(scope, code, value, p + 0x1c, 4, 1);
		return;
	}

	case 6: {
		int kind;
		if (!CombiMsgHandlerGuardPass(p, 0x1c, kind))
			return;
		/* Real, confirmed-by-disassembly quirk: scope is unconditionally
		 * "ESSong" here regardless of which kind actually passed the guard --
		 * no kind-based branch exists in the real code past this point.
		 */
		unsigned char scope = EditApiGetScopeId("ESSong");
		int idx = *(int *)(p + 0x14);
		unsigned int ti = (idx == 0 || idx == 1) ? (unsigned int)idx : 2;
		unsigned char code  = kHandleCombiMetronomeParam[ti * 2];
		unsigned char value = kHandleCombiMetronomeParam[ti * 2 + 1];
		/* Real payload pointer is msg+0x18 here, not msg+0x1c like every other
		 * case in this function.
		 */
		EditApiSendParamMsg(scope, code, value, p + 0x18, 4, 1);
		return;
	}

	default:
		return;
	}
}

/* --- Tier A, batch 8 (2026-07-27): VoiceModelMsgHandler --------------------------
 *
 * Promoted from Tier B (deferred 2026-07-26 as "genuinely more mechanical than
 * previously documented, but not simple enough to reconstruct in this same pass" --
 * see this file's own header comment for the from-scratch re-trace that established
 * the two real jump tables/guard shape/dependency inventory). Real size 2512 bytes
 * (0x08917100..0x08917ad0, corrected from the previously-documented 2487B).
 *
 * Full case-by-case tracing this session (`objdump -dr -M intel` against the real
 * ground-truth binary, every jump target followed by hand, every table read directly
 * out of .rodata via readelf VA->file-offset + raw byte read -- same discipline as
 * this file's other s_akbyAP tables) confirms the header's own "~90% mechanical, one
 * genuine deep leaf" verdict exactly. Real control flow (real addresses in parens):
 *
 *   - Soft assert (0x08917131): if (int16_t)(msg+0x14) > 1, fires the real Api+0x94
 *     assert-report idiom already established elsewhere in this project (tempo.cpp/
 *     mains.cpp/chunk_man.cpp/chunk_server.cpp/edit_server.cpp/config_manager.cpp)
 *     with file string "MsgProcessor/STGUnsolMsgProcessor/CSTGUnsolMsgHandler.cpp",
 *     line 0x1074 -- never fatal, always falls through and continues.
 *   - Guard (0x08917159): real condition is
 *       (msg+0xc == CStorage::sm_ucCurrentProg && target == DAT_0af30549)
 *         || target == 0xffff  -> MAIN dispatch
 *       target == 0xfffe                                -> WILDCARD dispatch
 *       else                                              -> return (no dispatch)
 *     (byte-identical guard SHAPE to PatchMsgHandler's own, confirmed here by direct
 *     disassembly. DAT_0af30549 is a single byte [0,255], so it can never coincide
 *     with the 0xfffe/0xffff sentinels -- the real disassembly's own "id-match, then
 *     re-check target" control flow at 0x08917430/0x08917440 collapses to exactly
 *     this without loss, confirmed by hand-tracing both branches.)
 *   - WILDCARD (0x089173ca): real global `DAT_0acadb30` (purpose untraced, kept
 *     opaque, same convention as DAT_0af0df1e) must be 0 to proceed (nonzero
 *     re-enters MAIN instead, a real `jne` back into the main branch, not a bail);
 *     msg+0x8 must be 0; subindex(msg+0x16) selects one of two bespoke, self-
 *     contained sub-cases, each with its OWN F bound check before its OWN
 *     GetScopeId("ESSampling") call (S==0: 0x089173e8, F<=5, table 0x08f1bd4c;
 *     S==3: 0x08917452, F<=8, table 0x08f1bd58; both share the common tail at
 *     0x0891748a) -- any other S bails. (Corrected from this section's own first
 *     trace pass, which wrongly claimed S==0 falls through into idx1's 0x089174cb
 *     body -- it does not; it is separate, self-contained code that merely uses an
 *     analogous shape and a different table/scope.)
 *   - MAIN (0x08917189): branches on `(DAT_0af0df1e & 7) == 3` (the same opaque mode
 *     byte PatchMsgHandler's own guard reads):
 *       != 3: msg+0x8 must be 0; subindex(msg+0x16)+1 (wrapping 0xffff->0) indexes
 *         the real 17-entry jump table at 0x08f1bac0 (bound <=16).
 *       == 3: msg+0x8 must be 0; reads a real, not-yet-modeled "algorithm
 *         descriptor" byte from a static array based at 0x0af0e049, indexed by
 *         msg+0x14 (raw "value" field) * 0x41c (1052) -- confirmed real via direct
 *         disassembly, backed here by a zero-initialized bounded placeholder (see
 *         VoiceModelAlgType() below) since the real array's actual runtime
 *         contents/size are CStorage-internal and not recovered.
 *           algType > 9 or == 0: falls through to a SEPARATE precondition set
 *             (msg+0x14 must be exactly 0, a DIFFERENT opaque global byte
 *             DAT_0af0e465 must be in [1,9], subindex must be 0) that, when
 *             satisfied, converges on the EXACT SAME .text address as the "!=3,
 *             subindex==0" case (0x089174cb) -- a real, confirmed-by-disassembly
 *             convergence, not an approximation; modeled by calling the same
 *             VoiceModelS0() helper.
 *           algType == 1: bail, no dispatch at all.
 *           algType in [2,9] and subindex(msg+0x16) <= 5 (unsigned): a real 6-entry
 *             jump table at 0x08f1bb04 -- entries 0/3 are the EXACT SAME .text
 *             addresses as the "!=3" branch's own subindex 0/3 cases
 *             (0x089174cb/0x08917512); entries 1/2/4/5 are separately-compiled code
 *             at different addresses (0x08917945/0x08917972) that compute the
 *             IDENTICAL formula to the "!=3" branch's own subindex 1/2/4/5 cases,
 *             confirmed byte-for-byte by independent register tracing of both call
 *             sites -- modeled here by sharing one helper per formula (VoiceModelS0/
 *             VoiceModelS1or2/VoiceModelS3/VoiceModelS4or5) called from both
 *             branches, not duplicated.
 *           algType in [2,9] and subindex > 5: THE ONE GENUINE DEEP LEAF (real call
 *             0x08917209 `call CStorage::GetInstance()`, a bounds-checked array of
 *             further-vtabled objects at [result+0x48], a real Api+0x94 assert on
 *             bounds violation whose file argument is the real string at 0x8f25dc4,
 *             "../../../../../OPOS/Projects/x2100/Modules/Storage/
 *             MOSSAlgorithmDatabase/MOSSAlgorithmDatabase.h" -- confirms this is a
 *             MOSS-algorithm voice-model-database dispatch, a whole unmodeled class
 *             hierarchy, matching this header's original "CSTGMultisampleBankUUIDBase"
 *             naming), further vtable calls at [obj+0x18]/[obj+0x1c], a memcpy of
 *             message fields into a stack buffer, and a final "ESMOSS"-scope EditApi
 *             dispatch. NOT implemented -- see VoiceModelMossAlgorithmDispatch()
 *             below, a precisely-scoped Tier-B stub (silent no-op, matching the real
 *             code's own silent-bail behavior on every guard failure it has). This is
 *             the ONLY unimplemented piece of this function.
 *
 * All 17 JT1 case bodies (S = subindex/msg+0x16, F/C/W = msg+0x1a/0x18/0x1c
 * respectively, V = msg+0x14) were hand-traced and are byte-exact transcriptions of
 * real register arithmetic, not guesses -- most are a uniform "table[F] -> {code,
 * value}, optionally offset by V/S/W" shape (kVM_A/B0to5/D/E4_5/F6_7/G/H/J/K/M),
 * matching this file's established s_akbyAP convention. THREE are genuinely bespoke
 * non-table register math, confirmed by hand and NOT forced into the uniform shape
 * (per this file's own header comment warning):
 *   - S==1/2 (0x0891775e) and its JT2-S==1/2 twin (0x08917945): zeroes msg+0x14 as a
 *     real side effect, then value=(byte)msg+0x1a (direct, unindexed),
 *     code=(S+0x30)&0xff (S used as a literal addend, not a table lookup at all).
 *   - S==10 (0x08917647): a real 3-way branch on F (F==0: byte lookup by W in a
 *     4-entry table at 0x8f1be4a; F==1: byte lookup by W in a 2-entry table at
 *     0x8f1be46, minus 1; F>=2: W verbatim) feeding one operand, combined with a
 *     15-entry {code,value} table at 0x8f1be4f indexed by F for the other, with TWO
 *     independent 0xff sentinel checks (one on the F-branch result, one on the
 *     table's own code byte).
 *   - S==13 (0x08917592) and S==15 (0x089178de): both branch on msg+0x18 (a signed
 *     16-bit "C" field) into 2-3 further sub-cases each (C==2 / C in {0,1} with its
 *     own per-F table incl. a real embedded 0x3d-sentinel special case / C==0xffff
 *     wildcard for S==13; C==0xffff / C==0 for S==15) -- see VoiceModelS13()/
 *     VoiceModelS15() below for the exact real arithmetic.
 * Every {code,value} table's real bytes were read directly from .rodata (readelf -l
 * VA->file-offset, then a raw byte read) at the EXACT address each case's own
 * disassembly references -- not transcribed from the decompile's opaque table names.
 *
 * Two real, confirmed-safe-to-simplify duplicate GetScopeId("ESProg") fetches (S==13's
 * C==2 sub-case, S==15's C==0 sub-case each call GetScopeId twice in the real
 * disassembly) are collapsed to one call here -- GetScopeId is a pure, side-effect-free
 * lookup at every call site in this file (same status as EditApiGetScopeId's own
 * existing callers), so this changes no observable behavior.
 *
 * Real, currently-dead-but-preserved unguarded-read hazard (same "avoid true C++ UB,
 * no behavioral change for any real caller" convention as CombiMsgHandlerReadBuf()):
 * S==10's two direct W-indexed byte tables (no bound check on W in the real ground
 * truth) are defensively modulo-clamped against their real backing byte arrays.
 * (S==15 turned out to have its OWN real top-of-case F<=6 bound check this section's
 * first trace pass missed -- see the WILDCARD/MAIN correction note above -- so its
 * own table read needs no defensive clamp at all; a modulo-clamp was applied there in
 * this section's first draft and has been removed as unnecessary.)
 *
 * Real per-case GetScopeId() call-ORDER note: most JT1/JT2/WILDCARD cases check their
 * own F bound FIRST and only call GetScopeId() if it passes (confirmed by direct
 * disassembly of every one of them) -- so scope resolution is threaded through this
 * reconstruction's own per-case Compute*() helpers (free functions, pure math, no
 * class access) and only actually fetched by the two class-member dispatch methods
 * (VoiceModelMainDispatch()/VoiceModelWildcardDispatch()) once a helper reports its
 * own bound check passed. TWO cases are real, confirmed exceptions to this order and
 * are NOT run through that shared bool-returning shape:
 *   - S==1/2 (0x0891775e) has no bound check at all (always dispatches), so the
 *     ordering question is moot -- kept as a Compute*() helper anyway for uniformity.
 *   - S==13 (0x08917592) calls GetScopeId() UNCONDITIONALLY at the very top of the
 *     case, before even reading msg+0x18 -- confirmed by direct disassembly (no
 *     `cmp`/`ja` of any kind precedes the real `call [vtbl+0x28]` here, unlike every
 *     other case). Modeled as its own private static member function,
 *     VoiceModelS13(), which fetches scope itself and may still bail with no dispatch
 *     afterward -- exactly reproducing this real, unconditional-fetch-then-maybe-bail
 *     shape rather than forcing it into the shared "bound passed -> fetch -> send"
 *     helper convention every other case genuinely follows.
 */

extern CSystemApi *Api; /* mains.cpp */

namespace {
/* Real Api+0x94 soft-assert-report idiom, same convention as tempo.cpp/mains.cpp/
 * chunk_man.cpp/chunk_server.cpp/edit_server.cpp/config_manager.cpp.
 */
inline void ApiAssert(const char *file, int line)
{
	typedef void (*Fn)(void *, const char *, const char *, int);
	void *vtbl = *(void **)Api;
	Fn fn = *(Fn *)((char *)vtbl + 0x94);
	fn(Api, "Assertion failed in module %s, line %i.\n", file, line);
}
} /* namespace */

/* Real, opaque globals -- purposes not traced beyond their one real use each in this
 * function, same "confirm one field, don't guess the rest" convention as
 * DAT_0af0df1e above.
 */
unsigned char DAT_0af0e465 = 0; /* 0xaf0e465, real bss byte, WILDCARD/fallback gate */
int DAT_0acadb30 = 0;           /* 0xacadb30, real bss dword, WILDCARD gate */

/* Real "algorithm descriptor" byte array, base 0x0af0e049, real stride 0x41c (1052)
 * bytes per "value" slot -- confirmed via direct disassembly (`movzx eax, BYTE PTR
 * [edi+0xaf0e049]` with edi = msg's own raw "value" field * 0x41c). This is
 * CStorage-internal runtime data (part of the same MOSS voice-model database the deep
 * leaf below is unmodeled for), not a compile-time table -- backed here by a
 * zero-initialized, size-bounded placeholder (real array's true extent unknown from
 * this export) with a defensive modulo-clamp, same convention as
 * CombiMsgHandlerReadBuf()'s own edit-buffer placeholder. All-zero means algType==0
 * for every V, which routes into the (real, harmless) "fallback" path below rather
 * than ever reaching the deep MOSS leaf -- a faithful default given this data is
 * genuinely unrecovered, not a shortcut.
 */
static unsigned char DAT_0af0e049_AlgTypeTable[0x4000];
static inline unsigned char VoiceModelAlgType(unsigned int value)
{
	size_t off = ((size_t)value * 0x41cu) % sizeof(DAT_0af0e049_AlgTypeTable);
	return DAT_0af0e049_AlgTypeTable[off];
}

/* Real {code,value} byte-pair tables and small direct-index tables, every address
 * read straight out of .rodata (readelf -l VA->file-offset, raw byte read) at the
 * exact address each case body's own disassembly references -- see this section's
 * own header comment for the real address of each.
 */
static const unsigned char kVM_Q[12]      = { 0x2f,0x01, 0x2f,0x00, 0x2f,0x03, 0x2f,0x02, 0x2f,0x04, 0x2f,0x05 }; /* 0x8f1bd4c, WILDCARD S==0 */
static const unsigned char kVM_R[18]      = {
	0xff,0xff, 0xff,0xff, 0xff,0xff, 0x02,0x02, 0x02,0x00, 0x02,0x01, 0x02,0x03, 0x02,0x04, 0x02,0x05,
}; /* 0x8f1bd58, WILDCARD S==3 */
static const unsigned char kVM_A[4]       = { 0x33,0x4d, 0x33,0x51 }; /* 0x8f1bd6a, idx0 (subindex==0xffff) */
static const unsigned char kVM_B0to5[12]  = { 0x2c,0x01, 0x2c,0x00, 0x2c,0x03, 0x2c,0x02, 0x2c,0x04, 0x2c,0x05 }; /* 0x8f1bd6e, S==0 */
static const unsigned char kVM_D[18]      = {
	0x4d,0x06, 0x4d,0x02, 0x4d,0x03, 0x4d,0x01, 0x4d,0x04, 0x4d,0x05, 0x4d,0x07, 0x4d,0x08, 0x4d,0x09,
}; /* 0x8f1bd7a, S==3 */
static const unsigned char kVM_E4_5[58]   = {
	0x4f,0x00, 0x4f,0x01, 0x4f,0x02, 0x4f,0x03, 0x4f,0x04, 0x4f,0x01, 0x4f,0x02, 0x4f,0x03, 0x4f,0x04, 0x4f,0x01,
	0x4f,0x06, 0x4f,0x05, 0x4f,0x01, 0x4f,0x07, 0x4f,0x08, 0x4f,0x01, 0x4f,0x09, 0x4f,0x0a, 0x4f,0x01, 0x4f,0x0b,
	0x4f,0x0c, 0x4f,0x0d, 0x4f,0x0e, 0x4f,0x0f, 0x4f,0x10, 0x4f,0x01, 0x4f,0x03, 0x4f,0x11, 0x4f,0x12,
}; /* 0x8f1bda0, S==4/5 (and JT2's separately-compiled S==4/5 twin) */
static const unsigned char kVM_F6_7[42]   = {
	0x35,0x00, 0x35,0x05, 0x35,0x06, 0x35,0x07, 0x35,0x08, 0x35,0x03, 0x35,0x04, 0x35,0x01, 0x35,0x09, 0x35,0x02,
	0x35,0x0b, 0x35,0x0a, 0x35,0x0c, 0x35,0x0d, 0x35,0x0e, 0x35,0x0f, 0x35,0x10, 0x35,0x11, 0x35,0x12, 0x35,0x13, 0x35,0x14,
}; /* 0x8f1bde0, S==6/7 */
static const unsigned char kVM_G[30]      = {
	0x49,0x00, 0x49,0x01, 0x49,0x1d, 0x49,0x08, 0x49,0x09, 0xff,0xff, 0xff,0xff, 0x49,0x0c, 0x49,0x0d, 0x49,0x11,
	0x49,0x12, 0x49,0x16, 0x49,0x17, 0x49,0x1b, 0x49,0x1c,
}; /* 0x8f1be0a, S==8 (0xff,0xff real sentinel at F=5,6) */
static const unsigned char kVM_H[30]      = {
	0x41,0x00, 0x41,0x01, 0x41,0x1e, 0x41,0x09, 0x41,0x0a, 0xff,0xff, 0xff,0xff, 0x41,0x0d, 0x41,0x0e, 0x41,0x12,
	0x41,0x13, 0x41,0x17, 0x41,0x18, 0x41,0x1c, 0x41,0x1d,
}; /* 0x8f1be28, S==9 (0xff,0xff real sentinel at F=5,6) */
static const unsigned char kVM_I_main[30] = {
	0x2f,0x00, 0x2f,0x01, 0x2f,0x14, 0x2f,0x08, 0x2f,0x09, 0x2f,0x0c, 0x2f,0x0d, 0x2f,0x10, 0x2f,0x11, 0xff,0xff,
	0xff,0xff, 0xff,0xff, 0xff,0xff, 0x2f,0x18, 0x2f,0x19,
}; /* 0x8f1be4f, S==10 main table */
static const unsigned char kVM_I_dir46[4] = { 0x01, 0x03, 0x05, 0x06 }; /* 0x8f1be46, S==10 F==1 direct table */
static const unsigned char kVM_I_dir4a[8] = { 0x00, 0x02, 0x04, 0xff, 0x07, 0x2f, 0x00, 0x2f }; /* 0x8f1be4a, S==10 F==0 direct table */
static const unsigned char kVM_J[40]      = {
	0x39,0x01, 0x39,0x00, 0x39,0x02, 0x39,0x05, 0x39,0x11, 0x39,0x06, 0x39,0x07, 0x39,0x08, 0x39,0x09, 0x39,0x13,
	0x39,0x15, 0x39,0x16, 0x39,0x0f, 0x39,0x10, 0x39,0x0b, 0x39,0x0c, 0x39,0x0d, 0x39,0x0e, 0x39,0x03, 0x39,0x04,
}; /* 0x8f1be80, S==11 */
static const unsigned char kVM_K[26]      = {
	0x33,0x00, 0x33,0x02, 0x33,0x03, 0x33,0x05, 0x33,0x06, 0x33,0x04, 0x33,0x07, 0x33,0x08, 0x33,0x09, 0x33,0x50,
	0x33,0x4f, 0x39,0x12, 0x33,0x01,
}; /* 0x8f1bea8, S==12 (F==11 entry, {0x39,0x12}, real code-byte anomaly vs. the
    * table's otherwise-uniform 0x33 -- transcribed as read, not "fixed", same
    * spillover-adjacency class as this file's own CombiMsgHandler kHandleCombi*
    * table notes) */
static const unsigned char kVM_K_flag[13] = { 1,1,1,1,1,1,1,1,1,0,0,0,1 }; /* 0x8f1c484, S==12 flag
    * table -- real bytes immediately inside kGlobalMsgWaveSeqFlag's own documented
    * real base (0x8f1c481) but BEFORE that array's own transcribed range (which
    * starts at the shifted 0x8f1c491) -- no conflict, just the previously-
    * untranscribed portion of the same real global array, confirmed by direct read
    * here. */
static const unsigned char kVM_L[46]      = {
	0x3d,0x01, 0x3d,0x00, 0x3d,0x02, 0x3d,0x06, 0x3d,0x07, 0x3d,0x08, 0x3d,0x09, 0x3d,0x0a, 0x3d,0x10, 0x3d,0x0f,
	0x3b,0x01, 0x3d,0x17, 0x3d,0x0b, 0x3d,0x0c, 0x3d,0x0d, 0x3d,0x0e, 0x3d,0x11, 0x3d,0x14, 0x3b,0x02, 0x3d,0x18,
	0x3d,0x03, 0x3d,0x04, 0x3d,0x05,
}; /* 0x8f1bee0, S==13 non-negative-C table (0x3d = the real "special" sentinel code
    * byte checked at every index except F==9/18, which are real 0x3b) */
static const unsigned char kVM_L_wild[2]  = { 0x3b, 0x00 }; /* 0x8f1bf0e, S==13 C==0xffff table */
static const unsigned char kVM_M[14]      = { 0x45,0x01, 0x45,0x00, 0x45,0x04, 0x45,0x05, 0x45,0x02, 0x45,0x06, 0x45,0x07 }; /* 0x8f1bf10, S==14 */
static const unsigned char kVM_N[32]      = {
	0x47,0x00, 0x47,0x01, 0x47,0x04, 0x47,0x02, 0x47,0x03, 0x47,0x06, 0x47,0x07, 0x00,0x00,
	0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
}; /* 0x8f1bf1e, S==15, 7 real entries (F bound <=6, confirmed by a real top-of-case
    * bound check at 0x089178de this pass's first trace pass missed -- see this
    * section's own header comment correction) */

/* --- per-S dispatch helpers ------------------------------------------------------
 *
 * Free functions (not class members): each performs ONLY its own real bound check
 * plus the case's own {code,value} math, returning false (no dispatch, matching the
 * real ground truth exactly) if the bound check fails -- no class access needed, no
 * scope resolution done here at all. VoiceModelMainDispatch()/
 * VoiceModelWildcardDispatch() below (which DO have EditApiGetScopeId access) call
 * these first and only fetch scope + dispatch via VoiceModelSend() if a helper
 * returns true, reproducing each case's own real "bound-check-then-scope-then-
 * dispatch" instruction order exactly (see this section's own header comment for the
 * two real exceptions, S==1/2 and S==13, that don't follow this shape -- S==1/2 has
 * no bound check at all so the distinction is moot; S==13 is its own private static
 * member function, VoiceModelS13(), further down).
 */
static inline void VoiceModelSend(unsigned char *p, unsigned char scope, unsigned char code, unsigned char value)
{
	SetWithoutUpdatingSTG(scope, code, value, p + 0x20);
}

/* idx0 (dispatch index 0, real subindex sentinel 0xffff -- NOT the same as S==0). */
static bool VoiceModelComputeIdx0(unsigned char *p, unsigned char &code, unsigned char &value)
{
	unsigned int F = *(uint16_t *)(p + 0x1a);
	if (F > 1) return false;
	unsigned int V = *(uint16_t *)(p + 0x14);
	code = (unsigned char)(kVM_A[F * 2] + V);
	value = kVM_A[F * 2 + 1];
	return true;
}

static bool VoiceModelComputeS0(unsigned char *p, unsigned char &code, unsigned char &value)
{
	unsigned int F = *(uint16_t *)(p + 0x1a);
	if (F > 5) return false;
	code = kVM_B0to5[F * 2];
	value = kVM_B0to5[F * 2 + 1];
	return true;
}

static bool VoiceModelComputeS1or2(unsigned char *p, int S, unsigned char &code, unsigned char &value)
{
	*(uint16_t *)(p + 0x14) = 0; /* real side effect: zeroes msg's own "value" field, unconditional */
	value = p[0x1a]; /* real: direct byte read, no bound check, no table */
	code = (unsigned char)(S + 0x30);
	return true;
}

static bool VoiceModelComputeS3(unsigned char *p, unsigned char &code, unsigned char &value)
{
	unsigned int F = *(uint16_t *)(p + 0x1a);
	if (F > 8) return false;
	unsigned int V = *(uint16_t *)(p + 0x14);
	code = (unsigned char)(kVM_D[F * 2] + V);
	value = kVM_D[F * 2 + 1];
	return true;
}

static bool VoiceModelComputeS4or5(unsigned char *p, int S, unsigned char &code, unsigned char &value)
{
	unsigned int F = *(uint16_t *)(p + 0x1a);
	if (F > 28) return false;
	unsigned int V = *(uint16_t *)(p + 0x14);
	code = (unsigned char)(V * 2 + kVM_E4_5[F * 2] + S - 4);
	value = kVM_E4_5[F * 2 + 1];
	return true;
}

static bool VoiceModelComputeS6or7(unsigned char *p, int S, unsigned char &code, unsigned char &value)
{
	unsigned int F = *(uint16_t *)(p + 0x1a);
	if (F > 20) return false;
	unsigned int V = *(uint16_t *)(p + 0x14);
	code = (unsigned char)(V * 2 + kVM_F6_7[F * 2] + S - 6);
	value = kVM_F6_7[F * 2 + 1];
	return true;
}

static bool VoiceModelComputeS8(unsigned char *p, unsigned char &code, unsigned char &value)
{
	unsigned int F = *(uint16_t *)(p + 0x1a);
	if (F > 14) return false;
	if (kVM_G[F * 2] == 0xff) return false; /* real sentinel, F==5/6 */
	int16_t W = (int16_t)*(uint16_t *)(p + 0x1c);
	int adj = (F <= 1) ? (int)(W * 2) : (int)W;
	value = (unsigned char)(adj + kVM_G[F * 2 + 1]);
	unsigned int V = *(uint16_t *)(p + 0x14);
	code = (unsigned char)(kVM_G[F * 2] + (unsigned char)V);
	return true;
}

static bool VoiceModelComputeS9(unsigned char *p, unsigned char &code, unsigned char &value)
{
	unsigned int F = *(uint16_t *)(p + 0x1a);
	if (F > 14) return false;
	if (kVM_H[F * 2] == 0xff) return false; /* real sentinel, F==5/6 */
	int16_t W = (int16_t)*(uint16_t *)(p + 0x1c);
	int adj = (F <= 1) ? (int)(W * 2) : (int)W;
	value = (unsigned char)(adj + kVM_H[F * 2 + 1]);
	unsigned int V = *(uint16_t *)(p + 0x14);
	code = (unsigned char)(kVM_H[F * 2] + (unsigned char)V);
	return true;
}

static bool VoiceModelComputeS10(unsigned char *p, unsigned char &code, unsigned char &value)
{
	unsigned int F = *(uint16_t *)(p + 0x1a);
	if (F > 14) return false;
	int W = (int)(int16_t)*(uint16_t *)(p + 0x1c);
	int ecx;
	if (F == 0) {
		ecx = kVM_I_dir4a[(unsigned)W % sizeof(kVM_I_dir4a)];
	} else if (F == 1) {
		ecx = kVM_I_dir46[(unsigned)W % sizeof(kVM_I_dir46)] - 1;
	} else {
		ecx = W; /* real: verbatim, no lookup at all for F>=2 */
	}
	if (ecx == 0xff) return false; /* real: first sentinel check, on the F-branch result */
	code = kVM_I_main[F * 2];
	if (code == 0xff) return false; /* real: second, independent sentinel check, on the table's own code byte */
	value = (unsigned char)(ecx + kVM_I_main[F * 2 + 1]);
	return true;
}

static bool VoiceModelComputeS11(unsigned char *p, unsigned char &code, unsigned char &value)
{
	unsigned int F = *(uint16_t *)(p + 0x1a);
	if (F > 19) return false;
	int16_t W = (int16_t)*(uint16_t *)(p + 0x1c);
	int adj = ((F - 10) <= 1) ? (int)(W * 2) : (int)W;
	value = (unsigned char)(adj + kVM_J[F * 2 + 1]);
	unsigned int V = *(uint16_t *)(p + 0x14);
	code = (unsigned char)(kVM_J[F * 2] + V);
	return true;
}

static bool VoiceModelComputeS12(unsigned char *p, unsigned char &code, unsigned char &value)
{
	unsigned int F = *(uint16_t *)(p + 0x1a);
	if (F > 12) return false;
	int16_t W = (int16_t)*(uint16_t *)(p + 0x1c);
	int adj = kVM_K_flag[F] ? (int)(W * 10) : 0;
	value = (unsigned char)(adj + kVM_K[F * 2 + 1]);
	unsigned int V = *(uint16_t *)(p + 0x14);
	code = (unsigned char)(kVM_K[F * 2] + V);
	return true;
}

static bool VoiceModelComputeS14(unsigned char *p, unsigned char &code, unsigned char &value)
{
	unsigned int F = *(uint16_t *)(p + 0x1a);
	if (F > 6) return false;
	unsigned int V = *(uint16_t *)(p + 0x14);
	code = (unsigned char)(kVM_M[F * 2] + V);
	value = kVM_M[F * 2 + 1];
	return true;
}

static bool VoiceModelComputeS15(unsigned char *p, unsigned char &code, unsigned char &value)
{
	/* real top-of-case bound check, 0x089178de -- this section's first trace pass
	 * missed it and wrongly modeled this case's own fallback table read as
	 * unbounded; it is not (F is always <=6 by the time either sub-branch below
	 * runs), see header comment correction.
	 */
	unsigned int F = *(uint16_t *)(p + 0x1a);
	if (F > 6) return false;

	unsigned int C = *(uint16_t *)(p + 0x18);
	unsigned int V = *(uint16_t *)(p + 0x14);

	if (C == 0) {
		value = p[0x1a]; /* real: direct byte read, no table */
		code = (unsigned char)(V + 0x4b);
		return true;
	}
	if (C != 0xffff) return false;

	int16_t W = (int16_t)*(uint16_t *)(p + 0x1c);
	int adj;
	unsigned int ti;
	if (F == 2) {
		adj = (int)W;
		ti = 2;
	} else if (F == 5 || F == 6) {
		adj = (int)(W * 2);
		ti = F;
	} else {
		adj = 0;
		ti = F; /* real: F is {0,1,3,4} here, already <=6 -- always in-bounds, no clamp needed */
	}
	value = (unsigned char)(adj + kVM_N[ti * 2 + 1]);
	code = (unsigned char)(V + kVM_N[ti * 2]);
	return true;
}

static bool VoiceModelComputeWildcardS0(unsigned char *p, unsigned char &code, unsigned char &value)
{
	unsigned int F = *(uint16_t *)(p + 0x1a);
	if (F > 5) return false;
	code = kVM_Q[F * 2];
	value = kVM_Q[F * 2 + 1];
	return true;
}

static bool VoiceModelComputeWildcardS3(unsigned char *p, unsigned char &code, unsigned char &value)
{
	unsigned int F = *(uint16_t *)(p + 0x1a);
	if (F > 8) return false;
	code = kVM_R[F * 2];
	value = kVM_R[F * 2 + 1];
	return true;
}

/* The ONE genuine deep leaf -- see this section's own header comment for the exact
 * real evidence (call site 0x08917209, CStorage::GetInstance(); assert file string
 * 0x8f25dc4, ".../Storage/MOSSAlgorithmDatabase/MOSSAlgorithmDatabase.h"). NOT
 * implemented: a real, entirely unmodeled MOSS algorithm voice-model database class
 * hierarchy (bounds-checked array of further-vtabled objects, two more vtable calls,
 * a memcpy, an "ESMOSS"-scope EditApi dispatch). Every real failure path in this leaf
 * (bounds violation, null slot, either vtable call returning <=0) silently returns
 * without dispatching anything -- modeled here as an unconditional silent no-op, the
 * same "stub returning a fixed sentinel/no-op is the natural bail" convention this
 * file already uses for CToneAdjustTool::ConvertParamToLinear/
 * CPrograms::GetProgramPointer (see this file's own CombiMsgHandler section).
 */
static void VoiceModelMossAlgorithmDispatch(unsigned char *p, int algType, int subindex)
{
	(void)p; (void)algType; (void)subindex;
	/* TODO: real MOSS algorithm database dispatch, out of scope -- see header. */
}

/* --- S==13's own case body (real address 0x08917592) -----------------------------
 *
 * The one real exception to the "bound check then scope" order every other case
 * follows: GetScopeId("ESProg") is called UNCONDITIONALLY at the very top, before
 * msg+0x18 ("C") is even read -- confirmed by direct disassembly (no `cmp`/`ja`
 * precedes the real `call [vtbl+0x28]` here). May still end up not dispatching
 * anything after that unconditional fetch (C outside {2} union {<=1 signed}, or F
 * out of the non-negative table's own <=0x16 bound) -- transcribed exactly, not
 * routed through the shared Compute*()-then-maybe-fetch shape every other case uses,
 * since that shape would incorrectly skip the real GetScopeId() call on those bail
 * paths.
 */
void CSTGUnsolMsgHandler::VoiceModelS13(unsigned char *p)
{
	unsigned char scope = EditApiGetScopeId("ESProg");

	int16_t V = (int16_t)*(uint16_t *)(p + 0x14);
	int16_t C = (int16_t)*(uint16_t *)(p + 0x18);

	if (C == 2) {
		unsigned char value = p[0x1a]; /* real: direct byte read, no table */
		unsigned char code = (unsigned char)((uint16_t)V + 0x43);
		VoiceModelSend(p, scope, code, value);
		return;
	}
	if (C > 1) return; /* real: C not in {2} union {<=1 signed} -> bail */

	int16_t W = (int16_t)*(uint16_t *)(p + 0x1c);
	if (C < 0) {
		if (C != -1) return; /* real: only the 0xffff wildcard continues */
		unsigned char code = (unsigned char)((uint16_t)V + kVM_L_wild[0]);
		unsigned char value = (unsigned char)((uint16_t)W + kVM_L_wild[1]);
		VoiceModelSend(p, scope, code, value);
		return;
	}

	/* C == 0 or C == 1 */
	unsigned int F = *(uint16_t *)(p + 0x1a);
	if (F > 0x16) return;
	if (kVM_L[F * 2] == 0x3d) {
		int esiVal = C + V * 2;
		unsigned char code = (unsigned char)(esiVal + kVM_L[F * 2]);
		unsigned char value = (unsigned char)((uint16_t)W + kVM_L[F * 2 + 1]);
		VoiceModelSend(p, scope, code, value);
	} else {
		unsigned char code = (unsigned char)((uint16_t)V + kVM_L[F * 2]);
		unsigned char value = (unsigned char)((uint16_t)W + kVM_L[F * 2 + 1]);
		VoiceModelSend(p, scope, code, value);
	}
}

/* --- the two real dispatch entry points (private static, need EditApiGetScopeId) - */

void CSTGUnsolMsgHandler::VoiceModelWildcardDispatch(unsigned char *p)
{
	if (DAT_0acadb30 != 0) { VoiceModelMainDispatch(p); return; } /* real: re-enters MAIN, not a bail */
	if (*(int *)(p + 8) != 0) return;

	int S = (int16_t)*(uint16_t *)(p + 0x16);
	unsigned char code = 0, value = 0;
	bool ok;
	if (S == 0)      ok = VoiceModelComputeWildcardS0(p, code, value);
	else if (S == 3) ok = VoiceModelComputeWildcardS3(p, code, value);
	else             return;

	if (!ok) return;
	unsigned char scope = EditApiGetScopeId("ESSampling");
	VoiceModelSend(p, scope, code, value);
}

void CSTGUnsolMsgHandler::VoiceModelMainDispatch(unsigned char *p)
{
	if ((DAT_0af0df1e & 7) == 3) {
		if (*(int *)(p + 8) != 0) return;
		unsigned int V = *(uint16_t *)(p + 0x14);
		unsigned char algType = VoiceModelAlgType(V);

		if (algType > 9 || algType == 0) {
			if (V != 0) return;
			unsigned char gByte = DAT_0af0e465;
			if (gByte > 9 || gByte == 0) return;
			int S = (int16_t)*(uint16_t *)(p + 0x16);
			if (S != 0) return;
			unsigned char code, value;
			if (!VoiceModelComputeS0(p, code, value)) return;
			unsigned char scope = EditApiGetScopeId("ESProg");
			VoiceModelSend(p, scope, code, value);
			return;
		}
		if (algType == 1) return;

		int S = (int16_t)*(uint16_t *)(p + 0x16);
		if ((unsigned int)S <= 5) {
			unsigned char code = 0, value = 0;
			bool ok;
			switch (S) {
			case 0: ok = VoiceModelComputeS0(p, code, value); break;
			case 1: case 2: ok = VoiceModelComputeS1or2(p, S, code, value); break;
			case 3: ok = VoiceModelComputeS3(p, code, value); break;
			case 4: case 5: ok = VoiceModelComputeS4or5(p, S, code, value); break;
			default: ok = false; break;
			}
			if (ok) {
				unsigned char scope = EditApiGetScopeId("ESProg");
				VoiceModelSend(p, scope, code, value);
			}
			return;
		}
		VoiceModelMossAlgorithmDispatch(p, algType, S);
		return;
	}

	if (*(int *)(p + 8) != 0) return;
	unsigned int rawS = *(uint16_t *)(p + 0x16);
	unsigned int idx = (unsigned short)(rawS + 1);
	if (idx > 16) return;

	if (idx == 14) { VoiceModelS13(p); return; } /* the one unconditional-scope exception */

	int S = (int)rawS;
	unsigned char code = 0, value = 0;
	bool ok;
	switch (idx) {
	case 0:  ok = VoiceModelComputeIdx0(p, code, value); break;
	case 1:  ok = VoiceModelComputeS0(p, code, value); break;
	case 2:  case 3:  ok = VoiceModelComputeS1or2(p, S, code, value); break;
	case 4:  ok = VoiceModelComputeS3(p, code, value); break;
	case 5:  case 6:  ok = VoiceModelComputeS4or5(p, S, code, value); break;
	case 7:  case 8:  ok = VoiceModelComputeS6or7(p, S, code, value); break;
	case 9:  ok = VoiceModelComputeS8(p, code, value); break;
	case 10: ok = VoiceModelComputeS9(p, code, value); break;
	case 11: ok = VoiceModelComputeS10(p, code, value); break;
	case 12: ok = VoiceModelComputeS11(p, code, value); break;
	case 13: ok = VoiceModelComputeS12(p, code, value); break;
	case 15: ok = VoiceModelComputeS14(p, code, value); break;
	case 16: ok = VoiceModelComputeS15(p, code, value); break;
	default: ok = false; break;
	}
	if (ok) {
		unsigned char scope = EditApiGetScopeId("ESProg");
		VoiceModelSend(p, scope, code, value);
	}
}

/* CSTGUnsolMsgHandler::VoiceModelMsgHandler(STGMessage&), .text+0x08917100, 2512 bytes. */
void CSTGUnsolMsgHandler::VoiceModelMsgHandler(STGMessage &msg)
{
	unsigned char *p = (unsigned char *)&msg;

	/* real: soft assert, never fatal, always falls through */
	if ((int16_t)*(uint16_t *)(p + 0x14) > 1)
		ApiAssert("MsgProcessor/STGUnsolMsgProcessor/CSTGUnsolMsgHandler.cpp", 0x1074);

	unsigned int target = *(unsigned int *)(p + 0x10);
	bool idMatch = (*(unsigned int *)(p + 0xc) == (unsigned int)CStorage::sm_ucCurrentProg)
	               && (target == (unsigned int)DAT_0af30549);

	if (idMatch || target == 0xffff) {
		VoiceModelMainDispatch(p);
		return;
	}
	if (target == 0xfffe) {
		VoiceModelWildcardDispatch(p);
		return;
	}
	/* real: neither guard matched -- no dispatch */
}
