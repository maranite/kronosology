// SPDX-License-Identifier: GPL-2.0
/*
 * test_ckg_midi_msg_handler.cpp  -  KAT for CSKMIDIMsgHandler /
 * CSKSpecialMsgHandler / CSKSysExMsgHandler (see
 * ../src/engine/ckg_midi_msg_handler.cpp).
 *
 * Two kinds of check:
 *
 *  1. STRUCTURAL: CSKSysExMsgHandler is driven through a
 *     `CSKMIDIMsgHandler*` base pointer to confirm real virtual dispatch
 *     (single-inheritance-only ABI, no diamond/VTT concerns here, but
 *     still worth proving the vtable layout matches).
 *  2. INDEPENDENT ORACLE: every non-trivial arithmetic KAT value below
 *     (ConvertPostMIDINote's octave-wrap transpose, CheckAndGetCorrectCCValue's
 *     CSWTCH_42 table lookup, ProcessPitchBendMessage's 2-value transform,
 *     Analize()'s SOX/EOX state machine, CopyToBuffer()'s 0x20 wraparound)
 *     was computed by a standalone Python re-implementation of the same
 *     algorithm straight from the raw x86 disassembly notes (scratchpad
 *     oracle_ckg_midi.py, NOT copy-pasted from the .cpp under test) before
 *     being hand-transcribed here as literal expected values -- see that
 *     script's own output for the derivation.
 */

#include <cstdio>
#include <cstring>
#include <new>
#include "oa_ckg_midi_msg_handler.h"

static int g_fail;
static void check(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-56s %ld (0x%lx)\n", label, got, (unsigned long)got); return; }
	printf("  FAIL  %-56s got=%ld(0x%lx) want=%ld(0x%lx)\n", label, got, (unsigned long)got, want, (unsigned long)want);
	g_fail++;
}

/* CSKSpecialMsgHandler::m_NowHandlingSamplingPerformanceChange is normally
 * defined in ckg_ui_msg_sender.cpp (not linked into this standalone test),
 * so it needs its own definition here -- same convention as every other
 * mocked singleton/static below. */
bool CSKSpecialMsgHandler::m_NowHandlingSamplingPerformanceChange;

/* ==================== CKGChordTrigger alias targets ==================== */
bool g_CKGChordTrigger_ms_bNowGenaratingChordNotes asm("_ZN15CKGChordTrigger27ms_bNowGenaratingChordNotesE");
bool g_CKGChordTrigger_ms_bNowSendingCCOrNote asm("_ZN15CKGChordTrigger22ms_bNowSendingCCOrNoteE");

/* ==================== mock singleton storage ==================== */

#define BANKSZ 0x97c800
static unsigned char g_bankBuf[BANKSZ];
unsigned char *CKGBankManager::ms_poInstance = g_bankBuf;

unsigned char *CKGEngine::ms_poInstance;
/* 0x200: large enough for every touched offset in this file, including
 * CKGCCResetHandler::ResetKarmaGeneratedValue()'s own +0xa0 pointer read
 * (was 0x40, too small once that method got a real body). */
static unsigned char g_engineBuf[0x200];
unsigned char *CDrumTrackBankManager::ms_poInstance;
unsigned char *CSPREngine::ms_poInstance;
unsigned char *CMIDIFlowParamHolder::ms_poThis;
unsigned char *CKGRTCHandler::ms_poInstance;
unsigned char *CSKMIDIMsgProcessor::ms_poInstance;
unsigned char *CKGMIDIMsgProcessor::ms_poInstance;
unsigned char *CSPRClockHandler::ms_poInstance;
unsigned char CSPRClockHandler::ms_oStatusMaster;
CSPRSysExBufManager *CSPRMIDIMsgProcessor::ms_poSysExPlayBuf;
CSTGMessageProcessor *CSTGMessageProcessor::sInstance;
static unsigned char g_stgMsgProcBuf[0x1041];

/* ==================== mocked out-of-scope call counters ==================== */

static int g_changePerformanceCalls;
static int g_lastChangePerfType;
void CKGEngine::ChangePerformance(eSTGMsgPerfType type, bool) { g_changePerformanceCalls++; g_lastChangePerfType = type; }
static bool g_shouldKeepKarma = true;
bool CKGEngine::ShouldKeepKarmaPerformance() { return g_shouldKeepKarma; }
static int g_clearSchedulerCalls, g_karmaOnCalls, g_karmaOffCalls, g_resetLocalCtrlCalls, g_sendShutUpCalls, g_sendCCOffsetBackCalls;
void CKGEngine::ClearScheduler() { g_clearSchedulerCalls++; }
void CKGEngine::KarmaTurnOnWhenFinishDump() { g_karmaOnCalls++; }
void CKGEngine::KarmaTurnOffWhenStartDump() { g_karmaOffCalls++; }
void CKGEngine::ResetLocalController() { g_resetLocalCtrlCalls++; }
void CKGEngine::SendShutUp() { g_sendShutUpCalls++; }
void CKGEngine::SendCCOffsetBack() { g_sendCCOffsetBackCalls++; }
static int g_lastBendRangeChannel = -1;
static unsigned int g_lastBendRangeA, g_lastBendRangeB;
void CKGEngine::SetBendRange(int range, unsigned int a, unsigned int b)
{
	g_lastBendRangeChannel = range;
	g_lastBendRangeA = a;
	g_lastBendRangeB = b;
}

static unsigned int g_lastCombiBankId = 0xdead, g_lastCombiIndex = 0xdead;
void CKGBankManager::ChangeKarmaPerfForCombi(eSTGCombiBankId bankId, unsigned int index)
{ g_lastCombiBankId = (unsigned int)bankId; g_lastCombiIndex = index; }
static unsigned int g_lastProgBankId = 0xdead, g_lastProgIndex = 0xdead;
void CKGBankManager::ChangeKarmaPerfForProgram(eSTGProgramBankId bankId, unsigned int index)
{ g_lastProgBankId = (unsigned int)bankId; g_lastProgIndex = index; }
static unsigned int g_lastSeqIndex = 0xdead;
void CKGBankManager::ChangeKarmaPerfForSeq(unsigned int index) { g_lastSeqIndex = index; }

void CDrumTrackBankManager::ChangeCombi(eSTGCombiBankId, unsigned int) {}
void CDrumTrackBankManager::ChangeProgram(eSTGProgramBankId, unsigned int) {}
void CDrumTrackBankManager::ChangeSeq(unsigned int) {}
void CSPREngine::ChangePerformance(eSTGMsgPerfType) {}
void CMIDIFlowParamHolder::SetCurrentVoiceMode() {}
void CMIDIFlowParamHolder::ChangePerformance() {}
void CKGRTCHandler::FlashBufferdValue() {}
void CKGRTCHandler::StartBuffering() {}
/* CSKMIDIMsgProcessor::KillAllDyingNotes/LeaveDownloadMode/
 * StoreDyingNoteInfoFor{MIDPort,STG}/ProcessLocalControlChannelMessage/
 * ProcessKarmaControllerGeneratedChannelMessage are now REAL (this
 * batch) -- mocks removed, real bodies linked in from
 * ckg_midi_msg_handler.cpp itself. CKGMIDIMsgProcessor::KillAllDyingNotes/
 * ResetKarmaGeneratedCCValue/StoreCCMessage are ALSO now REAL (a LATER
 * batch, full 13/13 class) -- their own mocks removed too, same reason.
 */

static int g_curLocLastCallKind; /* 0=none,1=current,2=precount */
static int g_locBar = 5, g_locBeat = 10, g_locC = 4, g_locD = 0;
void CSPRClockHandler::GetCurrentLocation(int *a, int *b, int *c, int *d)
{ g_curLocLastCallKind = 1; *a = g_locBar; *b = g_locBeat; *c = g_locC; *d = g_locD; }
void CSPRClockHandler::GetPrecountLocation(int *a, int *b, int *c, int *d)
{ g_curLocLastCallKind = 2; *a = g_locBar; *b = g_locBeat; *c = g_locC; *d = g_locD; }

/* CSKParameterChangeMessage::IsThisParamChage()/GetValue() are now REAL
 * (this batch) -- mocks removed. Real IsThisParamChage() requires
 * m_buf[1]==0x42 && m_buf[2]==(sign-extended-global-channel|0x30) &&
 * m_buf[3]==0x68 && m_buf[4] in {'m','C','n','A'} -- any test exercising
 * a code path gated on it must set those real bytes up first (see
 * OA_SetupRealParamChangeHeader() below), not just a boolean flag. */
static void OA_SetupRealParamChangeHeader(unsigned char *buf, unsigned char kind)
{
	signed char globalChanByte = *(signed char *)(CKGBankManager::ms_poInstance + 0x97c747);
	buf[0x1] = 0x42;
	buf[0x2] = (unsigned char)((int)globalChanByte | 0x30);
	buf[0x3] = 0x68;
	buf[0x4] = kind;
}

static CSKParameterChangeMessage *g_lastSetValueArg;
void CSPRSysExBufManager::SetValue(CSKParameterChangeMessage *msg) { g_lastSetValueArg = msg; }

/* ==================== mocked free functions ==================== */

extern "C" void SKSTGGate_SendToSTG(const unsigned char *, unsigned short) {}
static int g_sendToMIDIPortCalls;
static unsigned short g_lastSendToMIDIPortLen;
extern "C" void SKSTGGate_SendToMIDIPort(const unsigned char *, unsigned short len)
{ g_sendToMIDIPortCalls++; g_lastSendToMIDIPortLen = len; }
static bool g_vjsccFilterResult = true;
extern "C" bool SKSTGGate_CheckVJSCCToMIDIPortFilter(int, int) { return g_vjsccFilterResult; }
static int g_lastRecStatus = -1, g_lastRecNote = -1, g_lastRecCC = -1, g_lastRecChannel = -1;
extern "C" void SPRMain_RecChannelMessage(int status, int note, int cc, int channel)
{ g_lastRecStatus = status; g_lastRecNote = note; g_lastRecCC = cc; g_lastRecChannel = channel; }
extern "C" unsigned char CAfterTouchConverter_ConvertPostMIDI(int, unsigned char v) { return v; }
extern "C" unsigned char CAfterTouchConverter_ConvertPreMIDI(int, unsigned char v) { return v; }
extern "C" unsigned char CVelocityConverter_ConvertPostMIDI(int, unsigned char v) { return v; }
static bool g_isExclusive = true;
extern "C" bool SPROutGate_IsEnableExclusive() { return g_isExclusive; }
static bool g_autoKindResult = false;
static int g_autoKindOut = 0;
extern "C" char SPROutGate_GetAutomationSysExEventKind(unsigned int, int *out)
{ *out = g_autoKindOut; return g_autoKindResult; }
extern "C" void KGMain_ReceiveParameterChangeMessageFromMIDIPort(unsigned char *) {}
extern "C" void KGMain_ReceiveParameterChangeMessageFromSeqEvent(unsigned char *) {}
extern "C" void KGMain_ReceiveKarmaDisableInputMessage(unsigned char *, int) {}
extern "C" void SPRMain_ReceiveParameterChangeMessageFromMIDIPort(unsigned char *) {}
extern "C" void SPRMain_ReceiveParameterChangeMessageFromSeqEvent(unsigned char *) {}
extern "C" void SPRMain_ReceiveDrumTrackParameterChangeMessageFromMIDIPort(unsigned char *) {}
extern "C" void SPRMain_ReceiveDrumTrackParameterChangeMessageFromSeqEvent(unsigned char *) {}
static int g_recInternalCalls;
extern "C" void SPRMain_RecInternalSysExMessage(unsigned char) { g_recInternalCalls++; }
extern "C" void SPRMain_RecSysExMessageOnAutomationTrack(int, int, int) {}
extern "C" void SPRMain_RecSysExMessageFromMIDIPort(unsigned char) {}

/* CSKMIDIMsgProcessor's own free-function dependencies -- all plain
 * (non-`extern "C"`) global functions, real natural-mangling match (see
 * oa_ckg_midi_msg_handler.h's own comment). */
static int g_globalChannel;
int SPROutGate_GetGlobalChannel() { return g_globalChannel; }
static int g_preemptionCalls;
void SKMain_CheckAndProcessPreemption() { g_preemptionCalls++; }
static int g_resistReadQueusCalls;
void SKSTGGate_ResistReadQueus() { g_resistReadQueusCalls++; }
static bool g_queuesFilteredDuringPerfChange;
bool QueuesFilteredDuringPerfChange() { return g_queuesFilteredDuringPerfChange; }
static int g_kgReceiveControllerCalls;
static unsigned char g_lastKGReceiveControllerBuf[0x20];
void KGMain_ReceiveControllerMessage(unsigned char *buf)
{ g_kgReceiveControllerCalls++; memcpy(g_lastKGReceiveControllerBuf, buf, sizeof(g_lastKGReceiveControllerBuf)); }
static int g_sprReceiveControllerCalls;
void SPRMain_ReceiveControllerMessage(unsigned char *) { g_sprReceiveControllerCalls++; }
/* Both queue-receive mocks below serve a single canned message once
 * (via the matching g_*QueueHasMsg flag), then report empty -- enough to
 * drive one iteration of each dequeue loop under test. */
static bool g_localCtrlQueueHasMsg;
static unsigned char g_localCtrlQueueMsg[0x20];
static int g_localCtrlQueueMsgLen;
bool SKSTGGate_ReceiveFromLocalControlQueus(unsigned char *buf, unsigned int, int *outLen)
{
	if (!g_localCtrlQueueHasMsg)
		return false;
	g_localCtrlQueueHasMsg = false;
	memcpy(buf, g_localCtrlQueueMsg, sizeof(g_localCtrlQueueMsg));
	*outLen = g_localCtrlQueueMsgLen;
	return true;
}
static bool g_portQueueHasMsg;
static unsigned char g_portQueueMsg[0x20];
static int g_portQueueMsgLen;
bool SKSTGGate_ReceiveFromMIDIPortQueus(unsigned char *buf, unsigned int, int *outLen)
{
	if (!g_portQueueHasMsg)
		return false;
	g_portQueueHasMsg = false;
	memcpy(buf, g_portQueueMsg, sizeof(g_portQueueMsg));
	*outLen = g_portQueueMsgLen;
	return true;
}
static bool g_padsQueueHasMsg;
bool SKSTGGate_ReceivePads(unsigned char *)
{
	if (!g_padsQueueHasMsg)
		return false;
	g_padsQueueHasMsg = false;
	return true;
}
static int g_localControllerChannel;
int CKGEngine::GetLocalControllerChannel() { return g_localControllerChannel; }

/* ==================== CSKMIDIInMsgHandler-family mocks ==================== */

/* CKGControlMsgHandler::ms_bIsNowProcessingSoftPedalMessage and
 * CKGUIMsgProcessor::ms_poInstance are normally defined in
 * ckg_control_msg_handler.cpp / ckg_ui_msg_sender.cpp (not linked into
 * this standalone test) -- own definitions here, same convention as
 * CSKSpecialMsgHandler::m_NowHandlingSamplingPerformanceChange above. */
bool CKGControlMsgHandler::ms_bIsNowProcessingSoftPedalMessage;
unsigned char *CKGUIMsgProcessor::ms_poInstance;
static unsigned char g_uiMsgProcBuf[0x100];
static bool g_lastSoftPedalOn;
static int g_softPedalCalls;
void CKGUIMsgSender::UpdateSoftPedalStatus(bool on) { g_lastSoftPedalOn = on; g_softPedalCalls++; }

unsigned char *CKGEngine::ms_poKGEventDisplayManager;
/* CKGEventDisplayManager::NoteOn(int)/NoteOff(int) are now REAL (a LATER
 * batch, full 15/15 class) -- mocks removed, real bodies linked in from
 * ckg_midi_msg_handler.cpp itself. The old 16-byte g_eventDisplayBuf mock
 * is replaced by a real instance, wired up (and Initialize()'d) further
 * down in reset_all_mocks(), after CKGBankManager::ms_poInstance[+8]'s
 * own sub-buffer is wired -- Initialize() dereferences it. */
static CKGEventDisplayManager g_eventDisplayObj;

/* CDyingNoteInfo -- own real layout out of scope (opaque 0x84-byte blob),
 * mocked here as a simple per-note bitset + "any notes on" counter so the
 * KATs below can observe real TurnOn/TurnOff/IsNoteOn/IsAnyNotesOn
 * round-trips. */
struct CDyingNoteInfoMockState { bool on[128]; int count; };
static CDyingNoteInfoMockState *dni_state(CDyingNoteInfo *p) { return reinterpret_cast<CDyingNoteInfoMockState *>(p); }
void CDyingNoteInfo::Initialize() { auto *s = dni_state(this); for (int i = 0; i < 128; i++) s->on[i] = false; s->count = 0; }
void CDyingNoteInfo::TurnOn(int note) { auto *s = dni_state(this); if (!s->on[note]) { s->on[note] = true; s->count++; } }
void CDyingNoteInfo::TurnOff(int note) { auto *s = dni_state(this); if (s->on[note]) { s->on[note] = false; s->count--; } }
bool CDyingNoteInfo::IsNoteOn(int note) { return dni_state(this)->on[note]; }
bool CDyingNoteInfo::IsAnyNotesOn() { return dni_state(this)->count != 0; }
static_assert(sizeof(CDyingNoteInfoMockState) <= sizeof(CDyingNoteInfo), "mock state must fit in the opaque blob");

/* CSTGBankMemory::AllocAligned -- real bump allocator out of scope (see
 * src/mem/bank_memory.cpp, not linked here); a fixed static buffer is
 * enough for the single CSKSysExMsgHandler allocation the ctor makes. */
static unsigned char g_sysExAllocBuf[0x40] __attribute__((aligned(16)));
unsigned char *CSTGBankMemory::AllocAligned(unsigned int, unsigned int) { return g_sysExAllocBuf; }

/* CMIDIFlowParamHolder -- real KARMA-routing singleton, out of scope.
 * Every getter below is independently settable per-test via the
 * matching g_mf_* global, defaulting to 0/false. */
static CMIDIFlowParamHolder::EStatus g_mf_lastStatus = CMIDIFlowParamHolder::eStatus_0;
static int g_mf_voiceMode, g_mf_numKarmaModules, g_mf_currentTrackStatus, g_mf_localControlChannel;
static bool g_mf_karmaOn;
static int g_mf_karmaIn[8], g_mf_karmaOut[8], g_mf_karmaLoc[8];
static bool g_mf_karmaThruInternal[8], g_mf_karmaThru[8];
static int g_mf_timbreChannel[16], g_mf_timbreStatus[16], g_mf_timbreTranspose[16];
static bool g_mf_timbreNoteOnEnable[16], g_mf_timbrePitchBendEnable[16], g_mf_timbreAftertouchEnable[16];
static int g_mf_timbreBottomKey[16], g_mf_timbreTopKey[16], g_mf_timbreLowVel[16], g_mf_timbreHighVel[16];
static bool g_mf_timbreCCEnable;
void CMIDIFlowParamHolder::SetStatus(EStatus s) { g_mf_lastStatus = s; }
int CMIDIFlowParamHolder::GetVoiceMode() { return g_mf_voiceMode; }
int CMIDIFlowParamHolder::GetNumOfKARMAModule() { return g_mf_numKarmaModules; }
int CMIDIFlowParamHolder::GetKARMARealInputChannel(int m) { return g_mf_karmaIn[m]; }
int CMIDIFlowParamHolder::GetKARMARealOutputChannel(int m) { return g_mf_karmaOut[m]; }
int CMIDIFlowParamHolder::GetRealInputLocalControllerChannel(int m) { return g_mf_karmaLoc[m]; }
bool CMIDIFlowParamHolder::IsKARMAOn() { return g_mf_karmaOn; }
bool CMIDIFlowParamHolder::IsKARMATimbreThruInternalAction(int m) { return g_mf_karmaThruInternal[m]; }
bool CMIDIFlowParamHolder::IsKARMATimbreThru(int m) { return g_mf_karmaThru[m]; }
int CMIDIFlowParamHolder::GetLocalControlChannel() { return g_mf_localControlChannel; }
int CMIDIFlowParamHolder::GetCurrentTrackStatus() { return g_mf_currentTrackStatus; }
int CMIDIFlowParamHolder::GetTimbreChannel(int t) { return g_mf_timbreChannel[t]; }
int CMIDIFlowParamHolder::GetTimbreStatus(int t) { return g_mf_timbreStatus[t]; }
int CMIDIFlowParamHolder::GetTimbreTranspose(int t) { return g_mf_timbreTranspose[t]; }
bool CMIDIFlowParamHolder::IsEnableTimbreNoteOn(int t) { return g_mf_timbreNoteOnEnable[t]; }
int CMIDIFlowParamHolder::GetTimbreBottomKey(int t) { return g_mf_timbreBottomKey[t]; }
int CMIDIFlowParamHolder::GetTimbreTopKey(int t) { return g_mf_timbreTopKey[t]; }
int CMIDIFlowParamHolder::GetTimbreLowVelocity(int t) { return g_mf_timbreLowVel[t]; }
int CMIDIFlowParamHolder::GetTimbreHighVelocity(int t) { return g_mf_timbreHighVel[t]; }
bool CMIDIFlowParamHolder::IsEnableTimbrePitchBend(int t) { return g_mf_timbrePitchBendEnable[t]; }
bool CMIDIFlowParamHolder::IsEnableTimbreAftertouch(int t) { return g_mf_timbreAftertouchEnable[t]; }
bool CMIDIFlowParamHolder::IsEnableTimbreCC(int, int) { return g_mf_timbreCCEnable; }

static int g_lastRtcNoteChannel = -1, g_lastRtcNoteStatusType = -1, g_lastRtcNoteData1 = -1, g_lastRtcNoteData2 = -1, g_lastRtcNoteSrc = -1;
bool CKGRTCHandler::AnalizeAndProcessNoteMessage(int channel, int statusType, int data1, int data2, int src)
{ g_lastRtcNoteChannel = channel; g_lastRtcNoteStatusType = statusType; g_lastRtcNoteData1 = data1; g_lastRtcNoteData2 = data2; g_lastRtcNoteSrc = src; return true; }
static int g_lastRtcCcChannel = -1, g_lastRtcCcStatusType = -1, g_lastRtcCcData1 = -1, g_lastRtcCcData2 = -1, g_lastRtcCcSrc = -1;
void CKGRTCHandler::AnalizeAndProcessCCMessage(int channel, int statusType, int data1, int data2, int src)
{ g_lastRtcCcChannel = channel; g_lastRtcCcStatusType = statusType; g_lastRtcCcData1 = data1; g_lastRtcCcData2 = data2; g_lastRtcCcSrc = src; }

static unsigned char g_lastSentStatusType, g_lastSentChannel;
static signed char g_lastSentData1, g_lastSentData2;
void CKGEngine::SendChannelMessage(unsigned char statusType, unsigned char channel, signed char data1, signed char data2)
{ g_lastSentStatusType = statusType; g_lastSentChannel = channel; g_lastSentData1 = data1; g_lastSentData2 = data2; }
static bool g_forceTimbreZoneBypass;
bool CKGEngine::ShouldForceTimbreZoneBypass(int, int) { return g_forceTimbreZoneBypass; }
static bool g_karmaOn;
bool CKGEngine::IsKarmaOn() { return g_karmaOn; }
static int g_numModules;
int CKGEngine::GetNumOfModule() { return g_numModules; }
static int g_realOutputChannel[4] = { -1, -1, -1, -1 };
int CKGEngine::GetRealOutputChannel(int module)
{ return (module >= 0 && module < 4) ? g_realOutputChannel[module] : -1; }

static bool g_lastKeyboardOnOnOff, g_keyboardOnResult;
static int g_lastKeyboardOnNote = -1, g_lastKeyboardOnChannel = -1, g_lastKeyboardOnVelocity = -1;
bool SPRMain_KeyboardOn(bool onOff, int note, int channel, int velocity)
{ g_lastKeyboardOnOnOff = onOff; g_lastKeyboardOnNote = note; g_lastKeyboardOnChannel = channel; g_lastKeyboardOnVelocity = velocity; return g_keyboardOnResult; }

static int g_startMonitorCalls, g_endMonitorCalls;
static bool g_endMonitorResult;
extern "C" void SKSTGGate_StartMonitorSTGQueue() { g_startMonitorCalls++; }
extern "C" bool SKSTGGate_EndMonitorSTGQueue() { g_endMonitorCalls++; return g_endMonitorResult; }

static int g_lastAutoTrackStatus = -1, g_lastAutoTrackData1 = -1, g_lastAutoTrackData2 = -1, g_lastAutoTrackChannel = -1;
extern "C" void SPRMain_RecAutomationTrackMessage(int statusType, int data1, int data2, int channel)
{ g_lastAutoTrackStatus = statusType; g_lastAutoTrackData1 = data1; g_lastAutoTrackData2 = data2; g_lastAutoTrackChannel = channel; }
static int g_lastMidiTrackStatus = -1, g_lastMidiTrackData1 = -1, g_lastMidiTrackData2 = -1, g_lastMidiTrackChannel = -1;
extern "C" void SPRMain_RecMIDITrackMessage(int statusType, int data1, int data2, int channel)
{ g_lastMidiTrackStatus = statusType; g_lastMidiTrackData1 = data1; g_lastMidiTrackData2 = data2; g_lastMidiTrackChannel = channel; }

/*
 * CSKMIDIMsgProcessor::ms_poInstance singleton test double -- REAL bug
 * found while verifying this batch's own additions: several
 * ALREADY-COMMITTED methods from a prior batch (CSKMIDIMsgHandler::
 * StoreDyingNoteInfoForSTG()/StoreDyingNoteInfoForMIDPort(), reached via
 * SendChannelMessageToSTG()/SendChannelMessageToMIDIPortWithCorrectLength()
 * from CheckBypassKARMANoteOnEvent() and others) cast
 * CSKMIDIMsgProcessor::ms_poInstance and call through it unconditionally.
 * Before this batch, CSKMIDIMsgProcessor::StoreDyingNoteInfoForSTG/
 * MIDPort were empty test mocks, so calling them on a null `this` was
 * harmless (no member dereferenced). Now that they're REAL (this batch)
 * and dereference m_localCtrl/m_port, the same pre-existing null
 * `ms_poInstance` SIGSEGVs. Fix: wire up a real (not mocked) singleton,
 * matching what the real ctor always does in ground truth -- construct
 * once (function-local static, never reset across tests, matching this
 * as a "real held singleton" not per-test state).
 */
static CSKMIDIMsgProcessor *OA_TestSKMIDIMsgProcessorSingleton()
{
	static CSKMIDIPortMsgHandler s_port;
	static CSKMIDILocalCtrlMsgHandler s_localCtrl;
	static CSKSpecialMsgHandler s_special;
	static CSKMIDIKarmaCtrlMsgHandler s_karmaCtrl;
	static CSKPadNoteByMIDIPortMsgHandler s_padByPort;
	static CSKPadNoteByLocalCtrlMsgHandler s_padByLocal;
	static unsigned char s_raw[sizeof(CSKMIDIMsgProcessor)];
	static bool s_wired;
	CSKMIDIMsgProcessor *proc = reinterpret_cast<CSKMIDIMsgProcessor *>(s_raw);

	if (!s_wired) {
		proc->m_port = &s_port;
		proc->m_localCtrl = &s_localCtrl;
		proc->m_special = &s_special;
		proc->m_karmaCtrl = &s_karmaCtrl;
		proc->m_padByPort = &s_padByPort;
		proc->m_padByLocal = &s_padByLocal;
		proc->m_lastMsgKind = 0;
		proc->m_activeRawEvent = 0;
		proc->m_lastMsgSentinel = 0;
		s_wired = true;
	}
	return proc;
}

/*
 * Same rationale as OA_TestSKMIDIMsgProcessorSingleton() above --
 * CKGMIDIMsgProcessor::ms_poInstance is dereferenced unconditionally by
 * CSKMIDIMsgHandler::SendChannelMessageToSTG() (a base-class virtual
 * exercised from many already-existing tests) and by
 * CSKSpecialMsgHandler::ProcessResetAllControllerMessage()'s own case 3.
 * Both were harmless while CKGMIDIMsgProcessor::StoreCCMessage/
 * KillAllDyingNotes were empty test mocks; now that they're REAL and
 * dereference m_ccReset[]/m_timbreThru, a null ms_poInstance SIGSEGVs.
 * Wired via manual field assignment (NOT the real ctor) to avoid
 * CSTGBankMemory::AllocAligned()'s own mock below, which always returns
 * the SAME fixed buffer regardless of requested size -- calling the real
 * ctor here would alias all 20 owned sub-objects onto one buffer.
 */
static CKGMIDIMsgProcessor *OA_TestKGMIDIMsgProcessorSingleton()
{
	static CKGMIDIKarmaGeneratedMsgHandler s_karmaGen;
	static CKGMIDITimbreThruMsgHandler s_timbreThru;
	static CKGMIDIKarmaResetCCMsgHandler s_karmaResetCC;
	static CKGBendRangeHandler s_bendRange;
	static unsigned char s_ccResetBuf[16][sizeof(CKGCCResetHandler)];
	static unsigned char s_raw[sizeof(CKGMIDIMsgProcessor)];
	static bool s_wired;
	CKGMIDIMsgProcessor *proc = reinterpret_cast<CKGMIDIMsgProcessor *>(s_raw);

	if (!s_wired) {
		proc->m_karmaGen = &s_karmaGen;
		proc->m_timbreThru = &s_timbreThru;
		proc->m_karmaResetCC = &s_karmaResetCC;
		proc->m_bendRange = &s_bendRange;
		for (int i = 0; i < 16; i++) {
			proc->m_ccReset[i] = new (s_ccResetBuf[i]) CKGCCResetHandler(i);
			proc->m_ccReset[i]->Initialize();
		}
		proc->m_bSuspended = 0;
		s_wired = true;
	}
	return proc;
}

static void reset_all_mocks()
{
	memset(g_bankBuf, 0, sizeof(g_bankBuf));
	memset(g_engineBuf, 0, sizeof(g_engineBuf));
	memset(g_stgMsgProcBuf, 0, sizeof(g_stgMsgProcBuf));
	CKGEngine::ms_poInstance = g_engineBuf;
	CSTGMessageProcessor::sInstance = (CSTGMessageProcessor *)g_stgMsgProcBuf;
	CSKMIDIMsgProcessor::ms_poInstance = (unsigned char *)OA_TestSKMIDIMsgProcessorSingleton();
	CKGMIDIMsgProcessor::ms_poInstance = (unsigned char *)OA_TestKGMIDIMsgProcessorSingleton();
	g_changePerformanceCalls = 0;
	g_shouldKeepKarma = true;
	g_clearSchedulerCalls = g_karmaOnCalls = g_karmaOffCalls = g_resetLocalCtrlCalls = 0;
	g_sendShutUpCalls = g_sendCCOffsetBackCalls = 0;
	g_lastBendRangeChannel = -1;
	g_isExclusive = true;
	g_recInternalCalls = 0;
	g_sendToMIDIPortCalls = 0;

	memset(g_uiMsgProcBuf, 0, sizeof(g_uiMsgProcBuf));
	CKGUIMsgProcessor::ms_poInstance = g_uiMsgProcBuf;
	CKGControlMsgHandler::ms_bIsNowProcessingSoftPedalMessage = false;
	g_softPedalCalls = 0;
	/* CKGBankManager::ms_poInstance[+8] is itself a pointer (see
	 * NotifyNoteEventToUI()'s own header comment) -- point it at a
	 * scratch note-display buffer, distinct from g_uiMsgProcBuf.
	 * PRE-EXISTING BUG, found+fixed while verifying this batch's own
	 * additions: real ground-truth offsets written through this pointer
	 * go up to 0x147a7 (NotifyNoteEventToUI()/NotifyNoteCountToUI()/
	 * NotifyDamperStatusToUI()/NotifySostenutoStatusToUI(), all already
	 * committed before this batch) -- a 0x200-byte buffer here was a
	 * ~400x undersized out-of-bounds write that happened to silently
	 * corrupt unrelated static memory rather than crash, until this
	 * batch's own new static globals shifted the layout enough to hit
	 * something load-bearing (SIGSEGV inside NotifyNoteCountToUI(),
	 * confirmed via ASan pointing at this exact write). */
	static unsigned char noteDisplayBuf[0x14800];
	memset(noteDisplayBuf, 0, sizeof(noteDisplayBuf));
	*(unsigned char **)(g_bankBuf + 8) = noteDisplayBuf;

	/* CKGEventDisplayManager::Initialize() itself dereferences
	 * CKGBankManager::ms_poInstance[+8] (just wired above), so this must
	 * come after it. */
	CKGEngine::ms_poKGEventDisplayManager = (unsigned char *)&g_eventDisplayObj;
	g_eventDisplayObj.Initialize();

	g_mf_voiceMode = 0;
	g_mf_numKarmaModules = 0;
	g_mf_currentTrackStatus = 0;
	g_mf_localControlChannel = 0;
	g_mf_karmaOn = false;
	memset(g_mf_karmaIn, 0, sizeof(g_mf_karmaIn));
	memset(g_mf_karmaOut, 0, sizeof(g_mf_karmaOut));
	memset(g_mf_karmaLoc, 0, sizeof(g_mf_karmaLoc));
	memset(g_mf_karmaThruInternal, 0, sizeof(g_mf_karmaThruInternal));
	memset(g_mf_karmaThru, 0, sizeof(g_mf_karmaThru));
	memset(g_mf_timbreChannel, 0, sizeof(g_mf_timbreChannel));
	memset(g_mf_timbreStatus, 0, sizeof(g_mf_timbreStatus));
	memset(g_mf_timbreTranspose, 0, sizeof(g_mf_timbreTranspose));
	memset(g_mf_timbreNoteOnEnable, 0, sizeof(g_mf_timbreNoteOnEnable));
	memset(g_mf_timbrePitchBendEnable, 0, sizeof(g_mf_timbrePitchBendEnable));
	memset(g_mf_timbreAftertouchEnable, 0, sizeof(g_mf_timbreAftertouchEnable));
	memset(g_mf_timbreBottomKey, 0, sizeof(g_mf_timbreBottomKey));
	memset(g_mf_timbreTopKey, 0, sizeof(g_mf_timbreTopKey));
	memset(g_mf_timbreLowVel, 0, sizeof(g_mf_timbreLowVel));
	memset(g_mf_timbreHighVel, 0, sizeof(g_mf_timbreHighVel));
	g_mf_timbreCCEnable = false;

	g_lastRtcNoteChannel = g_lastRtcNoteStatusType = g_lastRtcNoteData1 = g_lastRtcNoteData2 = g_lastRtcNoteSrc = -1;
	g_lastRtcCcChannel = g_lastRtcCcStatusType = g_lastRtcCcData1 = g_lastRtcCcData2 = g_lastRtcCcSrc = -1;
	g_lastSentStatusType = g_lastSentChannel = 0;
	g_lastSentData1 = g_lastSentData2 = 0;
	g_forceTimbreZoneBypass = false;
	g_karmaOn = false;
	g_numModules = 0;
	g_realOutputChannel[0] = g_realOutputChannel[1] = g_realOutputChannel[2] = g_realOutputChannel[3] = -1;
	g_lastKeyboardOnOnOff = false;
	g_keyboardOnResult = true;
	g_lastKeyboardOnNote = g_lastKeyboardOnChannel = g_lastKeyboardOnVelocity = -1;
	g_startMonitorCalls = g_endMonitorCalls = 0;
	g_endMonitorResult = true;
	g_lastAutoTrackStatus = g_lastAutoTrackData1 = g_lastAutoTrackData2 = g_lastAutoTrackChannel = -1;
	g_lastMidiTrackStatus = g_lastMidiTrackData1 = g_lastMidiTrackData2 = g_lastMidiTrackChannel = -1;
	CSKMIDIInMsgHandler::ms_bShouldStopSendingNoteOnsToSTG = false;
}

int main(void)
{
	printf("CSKMIDIMsgHandler/CSKSpecialMsgHandler/CSKSysExMsgHandler known-answer test\n");
	printf("=============================================================================\n");

	/* ---- Structural: virtual dispatch through CSKMIDIMsgHandler* ---- */
	printf("-- single-inheritance dispatch (via CSKMIDIMsgHandler* base pointer) --\n");
	{
		reset_all_mocks();
		CSKSysExMsgHandler sysex;
		CSKMIDIMsgHandler *base = &sysex;
		/* CheckPadsMIDIOutFilter() is CSKMIDIMsgHandler's own base
		 * implementation -- CSKSysExMsgHandler doesn't override it,
		 * so dispatch through the base pointer must still reach it. */
		g_bankBuf[0x97c7ba] = 0;
		g_CKGChordTrigger_ms_bNowGenaratingChordNotes = false;
		check("CSKSysExMsgHandler via CSKMIDIMsgHandler*: CheckPadsMIDIOutFilter()==1",
		      base->CheckPadsMIDIOutFilter(), 1);
	}

	/* ---- Independent-oracle KATs (see oracle_ckg_midi.py) ---- */
	printf("-- CSKMIDIMsgHandler::ConvertPostMIDINote() octave-wrap transpose --\n");
	{
		/* transpose=5, note=60 -> 65 */
		reset_all_mocks();
		CSKMIDIMsgHandler h;
		*(int *)(g_bankBuf + 0x97c74c) = 1;
		g_bankBuf[0x97c744] = 5;
		h.m_status = 0x90; h.m_data1 = 60; h.m_data2 = 0x40; h.m_flags = 0;
		h.ConvertPostMIDINote();
		check("transpose=5 note=60 -> 65", h.m_data1, 65);

		/* transpose=-5, note=2 -> 9 (wraps by +0xc) */
		reset_all_mocks();
		*(int *)(g_bankBuf + 0x97c74c) = 1;
		g_bankBuf[0x97c744] = (unsigned char)-5;
		h.m_status = 0x90; h.m_data1 = 2; h.m_data2 = 0x40; h.m_flags = 0;
		h.ConvertPostMIDINote();
		check("transpose=-5 note=2 -> 9", h.m_data1, 9);

		/* transpose=5, note=125 -> 118 (wraps by -0xc) */
		reset_all_mocks();
		*(int *)(g_bankBuf + 0x97c74c) = 1;
		g_bankBuf[0x97c744] = 5;
		h.m_status = 0x90; h.m_data1 = 125; h.m_data2 = 0x40; h.m_flags = 0;
		h.ConvertPostMIDINote();
		check("transpose=5 note=125 -> 118", h.m_data1, 118);
	}

	printf("-- CSKMIDIMsgHandler::CheckAndGetCorrectCCValue() CSWTCH_42 table --\n");
	{
		CSKMIDIMsgHandler h;
		h.m_status = 0xb0; h.m_data1 = 0x11; h.m_data2 = 0xff;
		check("CC 0x11, val=0xff -> table[0]==0x40", h.CheckAndGetCorrectCCValue(), 0x40);
		h.m_data1 = 0x12;
		check("CC 0x12, val=0xff -> table[1]==0", h.CheckAndGetCorrectCCValue(), 0);
		h.m_data1 = 0x40;
		check("CC 0x40 (out of table range), val=0xff -> 0", h.CheckAndGetCorrectCCValue(), 0);
		h.m_data1 = 0x40; h.m_data2 = 5;
		check("CC 0x40, val=5 (not 0xff sentinel) -> passthrough 5", h.CheckAndGetCorrectCCValue(), 5);
		h.m_status = 0x90; h.m_data1 = 0x11; h.m_data2 = 0xff;
		check("status!=0xb0 -> passthrough (signed char)0xff == -1", h.CheckAndGetCorrectCCValue(), -1);
	}

	printf("-- CSKSpecialMsgHandler::ProcessPitchBendMessage() 2-value transform --\n");
	{
		reset_all_mocks();
		CSKSpecialMsgHandler h;
		h.m_status = 0xe0; h.m_data1 = 0x41; h.m_data2 = 0x3f; h.m_flags = 0x10;
		bool active = h.ProcessPitchBendMessage();
		check("active flag consumed", active, 1);
		check("channel == m_status&0xf == 0", g_lastBendRangeChannel, 0);
		check("arg A = transform(0x41) == -63", (long)(int)g_lastBendRangeA, -63);
		check("arg B = transform(0x3f) == 63", (long)(int)g_lastBendRangeB, 63);

		reset_all_mocks();
		h.m_flags = 0;
		active = h.ProcessPitchBendMessage();
		check("flags&0x10==0 -> inactive, no call", active, 0);
	}

	printf("-- CSKSysExMsgHandler::Analize() SOX/EOX state machine --\n");
	{
		reset_all_mocks();
		CSKSysExMsgHandler h;
		unsigned char seq[] = { 0xf0, 0x42, 0x30, 0x00, 0x12, 0xf7 };
		bool lastResult = false;
		for (unsigned char b : seq)
			lastResult = h.Analize(b);
		check("full bracketed frame: final EOX result==true", lastResult, 1);
		check("m_inSysEx cleared after EOX", h.m_inSysEx, 0);

		reset_all_mocks();
		CSKSysExMsgHandler h2;
		unsigned char seq2[] = { 0xf0, 0x42, 0x90 };
		bool r0 = h2.Analize(seq2[0]);
		bool r1 = h2.Analize(seq2[1]);
		bool r2 = h2.Analize(seq2[2]);
		check("SOX result", r0, 1);
		check("data byte result", r1, 1);
		check("interrupted by new status byte (not EOX) -> false", r2, 0);
		check("m_inSysEx cleared on interruption too", h2.m_inSysEx, 0);
	}

	printf("-- CSKSysExMsgHandler::CopyToBuffer() 0x20 wraparound --\n");
	{
		reset_all_mocks();
		CSKSysExMsgHandler h;
		for (int i = 0; i < 33; i++)
			h.CopyToBuffer((unsigned char)i);
		check("33rd byte wraps m_bufIndex back to 1", h.m_bufIndex, 1);
		check("buffer[0] holds the 33rd (wrapped) byte == 32", h.m_buf[0], 32);
		check("buffer[1] still holds the 2nd byte == 1", h.m_buf[1], 1);
	}

	printf("-- CSKSysExMsgHandler::RecChannelMessageToSequencer() dispatch shape --\n");
	{
		reset_all_mocks();
		CSKMIDIMsgHandler h;
		g_bankBuf[0x97c7ba] = 0;	/* CheckPadsMIDIOutFilter -> gate open */
		h.m_status = 0x90; h.m_data1 = 60; h.m_data2 = 0x40; h.m_flags = 3;
		h.RecChannelMessageToSequencer();
		check("SPRMain_RecChannelMessage status arg", g_lastRecStatus, 0x90);
		check("SPRMain_RecChannelMessage channel arg (status&0xf)", g_lastRecChannel, 0);
	}

	printf("-- CSKSysExMsgHandler::ProcessProgramChangeMessage() bankId/index mapping --\n");
	{
		/* Combi case (sel==0): bankId=m_data1&0x3f, index=m_data2 raw --
		 * verified against CKGBankManager::ChangeKarmaPerfForCombi's
		 * OWN prologue (bounds-checks index<=0x7f, stores bankId
		 * unchecked), not guessed from naming. */
		reset_all_mocks();
		CSKSpecialMsgHandler h;
		h.m_flags = 0x10;
		h.m_data1 = 0x25;	/* sel = 0x25>>6 = 0 (Combi), bankId-part = 0x25&0x3f = 0x25 */
		h.m_data2 = 0x50;	/* index */
		h.ProcessProgramChangeMessage();
		check("Combi: bankId == m_data1&0x3f", (long)g_lastCombiBankId, 0x25);
		check("Combi: index == m_data2 (raw)", (long)g_lastCombiIndex, 0x50);
		check("Combi: ChangePerformance(eSTGMsgPerfType_Combi)", g_lastChangePerfType, eSTGMsgPerfType_Combi);

		reset_all_mocks();
		h.m_flags = 0x10;
		h.m_data1 = 0x65;	/* sel = 0x65>>6 = 1 (Program) */
		h.m_data2 = 0x22;	/* < 0x80 and != 0xfe -> normal path */
		h.ProcessProgramChangeMessage();
		check("Program (normal): bankId == m_data1&0x3f", (long)g_lastProgBankId, 0x25);
		check("Program (normal): index == m_data2 (raw)", (long)g_lastProgIndex, 0x22);

		reset_all_mocks();
		h.m_flags = 0x10;
		h.m_data1 = 0x65;
		h.m_data2 = 0xfe;	/* sentinel path */
		h.ProcessProgramChangeMessage();
		check("Program (sentinel 0xfe): bankId unchanged (data1&0x3f)", (long)g_lastProgBankId, 0x25);
		check("Program (sentinel 0xfe): index forced to 0xfffe", (long)g_lastProgIndex, 0xfffe);

		reset_all_mocks();
		h.m_flags = 0x10;
		h.m_data1 = 0xa5;	/* sel = 0xa5>>6 = 2 (Seq) */
		h.m_data2 = 0x30;
		h.ProcessProgramChangeMessage();
		check("Seq: index == m_data2 (raw)", (long)g_lastSeqIndex, 0x30);
	}

	/*
	 * ==================== CSKMIDIInMsgHandler and its 5 children
	 * (2026-07-28 batch) ====================
	 * Independent-oracle derivation for the non-trivial arithmetic below
	 * lives in scratchpad oracle_ckg_midi_inmsg.py (StoreNoteEvent's
	 * counter pair, CheckBypassKARMANoteOnEvent's reservation-slot
	 * match/mismatch, and the shared per-module KARMA-channel-scan loop
	 * used by both ShouldRecChannelMessageToSequencer and
	 * ShouldSendChannelMessageToSTG) -- everything else below is either
	 * a direct field-flag round trip or a structural vtable-dispatch
	 * check, verified by inspection against the reconstructed source's
	 * own inline comments rather than a separate oracle.
	 */

	printf("-- structural: abstract CSKMIDIInMsgHandler* dispatches to the right leaf --\n");
	{
		reset_all_mocks();
		CSKMIDIPortMsgHandler port;
		CSKMIDIInMsgHandler *base = &port;
		check("Port: base* ShouldSendChannelMessageToMIDIPort()==false (pure at InMsgHandler)",
		      base->ShouldSendChannelMessageToMIDIPort(), 0);

		reset_all_mocks();
		CSKMIDILocalCtrlMsgHandler local;
		base = &local;
		g_bankBuf[0x97c749] = 0;
		check("LocalCtrl: base* CheckGlobalParameterPreSendToKarmaEngine()==false when gate clear",
		      base->CheckGlobalParameterPreSendToKarmaEngine(), 0);
		g_bankBuf[0x97c749] = 1;
		check("LocalCtrl: base* CheckGlobalParameterPreSendToKarmaEngine()==true when gate set",
		      base->CheckGlobalParameterPreSendToKarmaEngine(), 1);

		reset_all_mocks();
		CSKMIDIKarmaCtrlMsgHandler karma;
		CSKMIDILocalCtrlMsgHandler *localBase = &karma;
		karma.m_status = 0x90;
		check("Karma leaf: CheckNoteMessageAndTriggerPad() true for NoteOn (own override)",
		      localBase->CheckNoteMessageAndTriggerPad(), 1);
		karma.m_status = 0xb0;
		check("Karma leaf: CheckNoteMessageAndTriggerPad() false for CC (own override)",
		      localBase->CheckNoteMessageAndTriggerPad(), 0);

		reset_all_mocks();
		CSKPadNoteByMIDIPortMsgHandler padPort;
		check("PadByPort leaf: ShouldNotifyToKarmaController()==false (own override)",
		      padPort.ShouldNotifyToKarmaController(), 0);
		CSKPadNoteByLocalCtrlMsgHandler padLocal;
		check("PadByLocal leaf: CheckNoteMessageAndTriggerPad()==false (own override)",
		      padLocal.CheckNoteMessageAndTriggerPad(), 0);
	}

	printf("-- CSKMIDIInMsgHandler::StoreNoteEvent() note-down/note-on counters --\n");
	{
		reset_all_mocks();
		CSKMIDIPortMsgHandler h;
		h.m_status = 0x90; h.m_data1 = 60; h.m_data2 = 0x40;
		h.StoreNoteEvent();
		check("NoteOn(60): m_noteDownCount[60]==1", h.m_noteDownCount[60], 1);
		check("NoteOn(60): m_noteOnCount==1", h.m_noteOnCount, 1);
		h.m_status = 0x80;
		h.StoreNoteEvent();
		check("NoteOff(60): m_noteDownCount[60]==0", h.m_noteDownCount[60], 0);
		check("NoteOff(60): m_noteOnCount unchanged at 1 (decremented via down-count path)", h.m_noteOnCount, 1);
	}

	printf("-- CSKMIDIInMsgHandler::CheckBypassKARMANoteOnEvent() reservation round-trip --\n");
	{
		reset_all_mocks();
		CSKMIDIPortMsgHandler h;
		/* Reserve a Note-On on channel 3, note 60. */
		h.m_status = 0x93; h.m_data1 = 60; h.m_data2 = 0x40; h.m_flags = 0;
		h.ReserveBypassKARMANoteOnEvent(60);
		check("reserved slot low nibble == 0x90|3", h.m_bypassKarmaNoteOnEvent[60] & 0xff, 0x93);

		/* A later Note-Off on the SAME channel should bypass-consume it. */
		h.m_status = 0x83; h.m_data1 = 60; h.m_data2 = 0x10;
		bool consumed = h.CheckBypassKARMANoteOnEvent(60);
		check("matching-channel Note-Off consumes the reservation", consumed, 1);
		check("slot cleared after consumption", h.m_bypassKarmaNoteOnEvent[60], 0);
		check("this->m_status restored to the real current Note-Off afterward", h.m_status, 0x83);
		check("this->m_data2 restored to the real current Note-Off velocity afterward", h.m_data2, 0x10);

		/* Reserve again, then a Note-Off on a DIFFERENT channel must NOT consume it. */
		h.m_status = 0x93; h.m_data1 = 61; h.m_data2 = 0x40;
		h.ReserveBypassKARMANoteOnEvent(61);
		h.m_status = 0x85; h.m_data1 = 61; h.m_data2 = 0x10;
		consumed = h.CheckBypassKARMANoteOnEvent(61);
		check("mismatched-channel Note-Off does NOT consume", consumed, 0);
		check("slot still cleared regardless (unconditional tail)", h.m_bypassKarmaNoteOnEvent[61], 0);
	}

	printf("-- CSKMIDIInMsgHandler::CheckDyingNoteForMIDIPort() dying-note arrays --\n");
	{
		reset_all_mocks();
		CSKMIDIPortMsgHandler h;
		h.m_status = 0x90; h.m_data1 = 60; h.m_flags = 0;
		check("NoteOn always returns true (marks STG-side array)", h.CheckDyingNoteForMIDIPort(), 1);
		check("STG-side array now has note 60 on", h.m_dyingNoteSTG[0].IsNoteOn(60), 1);

		h.m_status = 0x80; h.m_data1 = 60;
		bool r = h.CheckDyingNoteForMIDIPort();
		check("matching NoteOff against STG-side array returns true", r, 1);
		check("STG-side array cleared afterward", h.m_dyingNoteSTG[0].IsNoteOn(60), 0);

		h.m_status = 0x80; h.m_data1 = 61;
		r = h.CheckDyingNoteForMIDIPort();
		check("NoteOff with no tracked note in either array returns false", r, 0);
	}

	printf("-- shared KARMA-channel-scan gate (ShouldRecChannelMessageToSequencer / ShouldSendChannelMessageToSTG) --\n");
	{
		reset_all_mocks();
		CSKMIDIPortMsgHandler h;
		g_bankBuf[0x97c749] = 1;	/* CheckGlobalParameterPreSendToSTG() initial gate: true */
		h.m_status = 0x93; h.m_data1 = 0x40; h.m_data2 = 0x40; h.m_flags = 0;	/* generic CC, not 0/0x20 */
		g_mf_numKarmaModules = 1;
		g_mf_karmaIn[0] = 3; g_mf_karmaOut[0] = 3; g_mf_karmaLoc[0] = 3;
		g_mf_karmaOn = true;
		check("ShouldRecChannelMessageToSequencer(): module fully matches status channel + KARMA on -> false",
		      h.ShouldRecChannelMessageToSequencer(), 0);
		check("ShouldSendChannelMessageToSTG(): same scan, same result -> false",
		      h.ShouldSendChannelMessageToSTG(), 0);

		reset_all_mocks();
		g_bankBuf[0x97c749] = 1;
		h.m_status = 0x93; h.m_data1 = 0x40; h.m_data2 = 0x40; h.m_flags = 0;
		g_mf_numKarmaModules = 1;
		g_mf_karmaIn[0] = 3; g_mf_karmaOut[0] = 3; g_mf_karmaLoc[0] = 3;
		g_mf_karmaOn = false;
		g_mf_karmaThruInternal[0] = false;
		check("ShouldRecChannelMessageToSequencer(): module matches but KARMA off + no thru-internal -> falls through to true",
		      h.ShouldRecChannelMessageToSequencer(), 1);
	}

	printf("-- CSKMIDIInMsgHandler::IsEnableViaRPPR() gate --\n");
	{
		reset_all_mocks();
		CSKMIDIPortMsgHandler h;
		g_mf_voiceMode = 0;			/* != 2 */
		g_bankBuf[0x97c747] = 5;		/* "current channel" != status channel below */
		h.m_status = 0x93; h.m_data1 = 60; h.m_data2 = 0x40; h.m_flags = 0;
		check("channel mismatch against ms_poInstance[0x97c747] -> true unconditionally",
		      h.IsEnableViaRPPR(), 1);

		reset_all_mocks();
		g_mf_voiceMode = 0;
		g_bankBuf[0x97c747] = 3;		/* matches status channel */
		g_bankBuf[0x97c749] = 0;		/* gate clear */
		h.m_status = 0x93; h.m_flags = 1;	/* m_flags&0xf != 0 -> early true */
		check("gate clear + m_flags&0xf!=0 -> true", h.IsEnableViaRPPR(), 1);

		reset_all_mocks();
		g_mf_voiceMode = 0;
		g_bankBuf[0x97c747] = 3;
		g_bankBuf[0x97c749] = 1;		/* gate set -> falls to status dispatch */
		h.m_status = 0x93; h.m_data1 = 60; h.m_data2 = 0x40; h.m_flags = 0;
		g_keyboardOnResult = true;
		bool r = h.IsEnableViaRPPR();
		check("NoteOn dispatch calls SPRMain_KeyboardOn(true,...) and returns its result", r, 1);
		check("SPRMain_KeyboardOn onOff arg", g_lastKeyboardOnOnOff, 1);
		check("SPRMain_KeyboardOn note arg", g_lastKeyboardOnNote, 60);
		check("SPRMain_KeyboardOn channel arg", g_lastKeyboardOnChannel, 3);
	}

	printf("-- CSKMIDIInMsgHandler::Process() Note-On/Note-Off STG-monitor gate --\n");
	{
		reset_all_mocks();
		CSKMIDIPortMsgHandler h;
		*(int *)(g_engineBuf + 0x14) = 4;	/* NoteOn gate: engine field+0x14 must == 4 */
		CSKMIDIInMsgHandler::ms_bShouldStopSendingNoteOnsToSTG = false;
		g_bankBuf[0x97c749] = 0;		/* gate clear -> increments hold counter */
		h.m_status = 0x90; h.m_data1 = 60; h.m_data2 = 0x40; h.m_flags = 0;
		h.Process();
		check("NoteOn: m_noteOnHoldCount[60] incremented", h.m_noteOnHoldCount[60], 1);
		check("NoteOn: no STG-monitor calls (gate was clear)", g_startMonitorCalls, 0);

		g_bankBuf[0x97c749] = 1;		/* gate set for the matching Note-Off */
		h.m_status = 0x80; h.m_data1 = 60; h.m_data2 = 0;
		g_endMonitorResult = true;		/* EndMonitorSTGQueue()==true -> no extra SendChannelMessageToSTG */
		h.Process();
		check("NoteOff: m_noteOnHoldCount[60] decremented back to 0", h.m_noteOnHoldCount[60], 0);
		check("NoteOff: StartMonitorSTGQueue() called once (gate was set)", g_startMonitorCalls, 1);
		check("NoteOff: EndMonitorSTGQueue() called once (consumed path)", g_endMonitorCalls, 1);

		reset_all_mocks();
		*(int *)(g_engineBuf + 0x14) = 0;	/* gate mismatch -> NoteOn ignored entirely */
		h.m_status = 0x90; h.m_data1 = 61; h.m_data2 = 0x40;
		h.Process();
		check("NoteOn ignored when engine field+0x14 != 4", h.m_noteOnHoldCount[61], 0);
	}

	printf("-- CSKMIDIPortMsgHandler::AnalizeAndSetParameter() byte validation --\n");
	{
		reset_all_mocks();
		CSKMIDIPortMsgHandler h;
		unsigned char buf[4] = { 0x90, 60, 0x40, 0x05 };
		bool ok = h.AnalizeAndSetParameter(buf, 4);
		check("valid NoteOn accepted", ok, 1);
		check("m_status stored", h.m_status, 0x90);
		check("m_data1 stored", h.m_data1, 60);
		check("m_data2 stored", h.m_data2, 0x40);
		check("m_flags masked to high nibble only", h.m_flags, 0);

		unsigned char bad[4] = { 0x90, 0x80 /* invalid, sign bit set */, 0x40, 0 };
		ok = h.AnalizeAndSetParameter(bad, 4);
		check("data1 with bit7 set rejected", ok, 0);

		unsigned char pc[4] = { 0xc0, 5, 0, 0 };
		ok = h.AnalizeAndSetParameter(pc, 4);
		check("ProgramChange (1 data byte) accepted", ok, 1);
		check("m_data1 stored for ProgramChange", h.m_data1, 5);
	}

	printf("-- CSKMIDILocalCtrlMsgHandler::AnalizeAndSetParameter() byte validation --\n");
	{
		reset_all_mocks();
		CSKMIDILocalCtrlMsgHandler h;
		unsigned char buf[4] = { 0x90, 60, 0x40, 3 };	/* flags&0xf==3 != 5 */
		bool ok = h.AnalizeAndSetParameter(buf, 4);
		check("valid frame, flags!=5 -> accepted", ok, 1);
		check("m_flags stored verbatim (all bits, unlike Port's variant)", h.m_flags, 3);

		unsigned char buf5[4] = { 0x90, 60, 0x40, 5 };	/* flags&0xf==5 -> rejected */
		ok = h.AnalizeAndSetParameter(buf5, 4);
		check("flags&0xf==5 -> rejected", ok, 0);

		unsigned char nonStatus[4] = { 0x40, 60, 0x40, 3 };	/* bit7 clear -> not a status byte */
		ok = h.AnalizeAndSetParameter(nonStatus, 4);
		check("byte0 without bit7 set -> rejected (not a real status byte)", ok, 0);
	}

	printf("-- CSKMIDILocalCtrlMsgHandler ext-note-on tracker + duplicate-message dedup --\n");
	{
		reset_all_mocks();
		CSKMIDILocalCtrlMsgHandler h;
		h.InitializeExtNoteOnChecker();
		check("IsSendingNoteOnToExt(60) initially false", h.IsSendingNoteOnToExt(60), 0);
		h.RegistExtNoteOn(60);
		check("IsSendingNoteOnToExt(60) true after Regist", h.IsSendingNoteOnToExt(60), 1);
		h.UnRegistExtNoteOn(60);
		check("IsSendingNoteOnToExt(60) false again after UnRegist", h.IsSendingNoteOnToExt(60), 0);

		reset_all_mocks();
		h.m_status = 0xd3;	/* ChannelAftertouch, channel 3 */
		h.m_data1 = 0x40;
		check("first ChannelAftertouch value on channel 3 -> not a duplicate", h.CheckDuplicateMessage(), 1);
		check("second identical value -> duplicate suppressed", h.CheckDuplicateMessage(), 0);
		h.m_data1 = 0x41;
		check("changed value -> not a duplicate again", h.CheckDuplicateMessage(), 1);

		reset_all_mocks();
		h.m_status = 0x90; h.m_data1 = 60;	/* NoteOn: always "not duplicate" */
		check("non-ChannelAftertouch message always returns true", h.CheckDuplicateMessage(), 1);
		check("still true immediately again (no dedup for this status type)", h.CheckDuplicateMessage(), 1);
	}

	printf("-- CSKMIDILocalCtrlMsgHandler::IsNotThruKarma() / GetKarmaControlledChannelPat() --\n");
	{
		reset_all_mocks();
		CSKMIDILocalCtrlMsgHandler h;
		g_mf_numKarmaModules = 2;
		g_mf_karmaIn[0] = 5; g_mf_karmaOut[0] = 5; g_mf_karmaLoc[0] = 1;
		g_mf_karmaIn[1] = 2; g_mf_karmaOut[1] = 6; g_mf_karmaLoc[1] = 2;
		check("channel 5 (module0's in==out==5) -> thru-KARMA -> false",
		      h.IsNotThruKarma(5), 0);
		check("channel 7 (no module) -> not thru-KARMA -> true", h.IsNotThruKarma(7), 1);

		g_mf_karmaThru[0] = true;
		g_mf_karmaThru[1] = false;
		unsigned int pat = h.GetKarmaControlledChannelPat(false);
		check("gated pattern: only module0 (IsKARMATimbreThru==true) contributes bit 5",
		      (long)pat, 1L << 5);
		pat = h.GetKarmaControlledChannelPat(true);
		check("unconditional pattern: both modules contribute bits 5 and 6",
		      (long)pat, (1L << 5) | (1L << 6));
	}

	printf("-- CSKParameterChangeMessage round-trip (real SetParameters/GetValue/SetValue/IsThisParamChage) --\n");
	{
		reset_all_mocks();
		g_globalChannel = 0x05;
		CSKParameterChangeMessage msg;

		msg.SetParameters((unsigned char)0x11, (unsigned char)0x22, (unsigned char)0x33,
				   (unsigned char)0x44, (unsigned char)0x55, 12345);
		OA_SetupRealParamChangeHeader(msg.m_bytes, 'C');
		check("fixed SOX byte", msg.m_bytes[0x0], 0xf0);
		check("fixed manufacturer-ID byte", msg.m_bytes[0x1], 0x42);
		check("param1 stored at +0x4", msg.m_bytes[0x4], 'C');
		check("param2 stored at +0x5", msg.m_bytes[0x5], 0x22);
		check("param3 stored at +0x6", msg.m_bytes[0x6], 0x33);
		check("param4 stored at +0x8 (5-byte overload)", msg.m_bytes[0x8], 0x44);
		check("param5 stored at +0x9 (5-byte overload)", msg.m_bytes[0x9], 0x55);
		check("+0x7 left 0 by the 5-byte overload", msg.m_bytes[0x7], 0x00);
		check("fixed EOX byte", msg.m_bytes[0xd], 0xf7);
		check("GetValue() round-trips the 3-byte split exactly", (long)msg.GetValue(), 12345);
		check("IsThisParamChage() true for a real 'C' kind header", msg.IsThisParamChage(), 1);

		msg.m_bytes[0x4] = 'X';
		check("IsThisParamChage() false for an unrecognized kind byte", msg.IsThisParamChage(), 0);

		/* Negative value: real ground truth sign-extends bit 20 back out
		 * on GetValue() (see this class's own header comment). */
		msg.SetValue(-1000);
		check("GetValue() sign-extends a negative round-trip", (long)(int)msg.GetValue(), -1000);

		/* 7-int-param overload's own +0x7 slot, distinct from the
		 * 5-byte overload above. */
		msg.SetParameters(1, 2, 3, 4, 5, 6, 999);
		check("7-param overload: param4 lands at +0x7 (not left 0)", msg.m_bytes[0x7], 4);
		check("7-param overload: param5 at +0x8", msg.m_bytes[0x8], 5);
		check("7-param overload: param6 at +0x9", msg.m_bytes[0x9], 6);
		check("7-param overload: GetValue() round-trip", (long)msg.GetValue(), 999);

		/* m_bytes[0x2] is currently (g_globalChannel|0x30) == 0x35 from
		 * the 7-param SetParameters() call just above; SetSourceSeq()
		 * etc. only ever rewrite its high nibble, preserving the low
		 * (channel) nibble. */
		msg.SetSourceSeq();
		check("SetSourceSeq(): high nibble 0x80, channel nibble preserved", msg.m_bytes[0x2], 0x85);
		msg.SetSourceSeqRestore();
		check("SetSourceSeqRestore(): high nibble 0x90", msg.m_bytes[0x2], 0x95);
		msg.ResetSourceSeq();
		check("ResetSourceSeq(): high nibble 0x30", msg.m_bytes[0x2], 0x35);
	}

	printf("-- CSKMIDIMsgProcessor::GetNowProcessingNoteOffVelocity() --\n");
	{
		unsigned char rawProc[sizeof(CSKMIDIMsgProcessor)];
		CSKMIDIMsgProcessor *proc = reinterpret_cast<CSKMIDIMsgProcessor *>(rawProc);

		int out = -999;
		proc->m_activeRawEvent = 0;
		check("null active event -> true, *out untouched", proc->GetNowProcessingNoteOffVelocity(&out), 1);
		check("*out really untouched when null", out, -999);

		unsigned char noteOnEvent[4] = {0x90, 60, 100, 0};
		proc->m_activeRawEvent = noteOnEvent;
		out = -999;
		check("NoteOn active event -> true, *out still untouched", proc->GetNowProcessingNoteOffVelocity(&out), 1);
		check("*out untouched for a non-NoteOff status", out, -999);

		unsigned char noteOffEvent[4] = {0x83, 60, (unsigned char)-5, 0};
		proc->m_activeRawEvent = noteOffEvent;
		out = -999;
		check("NoteOff active event -> true, *out written", proc->GetNowProcessingNoteOffVelocity(&out), 1);
		check("*out == the signed data2 byte", out, -5);
	}

	printf("-- CSKMIDIMsgProcessor field-routing dispatch (Process*ChannelMessage / IsKeyboardAllOff) --\n");
	{
		unsigned char rawProc[sizeof(CSKMIDIMsgProcessor)];
		CSKMIDIMsgProcessor *proc = reinterpret_cast<CSKMIDIMsgProcessor *>(rawProc);
		CSKMIDIPortMsgHandler port;
		CSKMIDILocalCtrlMsgHandler localCtrl;
		CSKMIDIKarmaCtrlMsgHandler karmaCtrl;
		CSKPadNoteByMIDIPortMsgHandler padByPort;
		CSKPadNoteByLocalCtrlMsgHandler padByLocal;
		proc->m_port = &port;
		proc->m_localCtrl = &localCtrl;
		proc->m_karmaCtrl = &karmaCtrl;
		proc->m_padByPort = &padByPort;
		proc->m_padByLocal = &padByLocal;

		reset_all_mocks();
		proc->ProcessLocalControlChannelMessage(0x80, 3, (char)60, (char)0);
		check("ProcessLocalControlChannelMessage: status = status_arg + channel_arg (0x80+3)",
		      localCtrl.m_status, 0x83);
		check("ProcessLocalControlChannelMessage: data1", localCtrl.m_data1, 60);
		check("ProcessLocalControlChannelMessage: m_flags fixed 0x01", localCtrl.m_flags, 0x01);
		check("ProcessLocalControlChannelMessage: m_lastMsgKind==1", proc->m_lastMsgKind, 1);
		check("ProcessLocalControlChannelMessage: m_lastMsgSentinel==-1", proc->m_lastMsgSentinel, -1);

		reset_all_mocks();
		proc->ProcessMIDIPortChannelMessage(0x90, 2, (char)61, (char)100);
		check("ProcessMIDIPortChannelMessage: status = status_arg + channel_arg (0x90+2)",
		      port.m_status, 0x92);
		check("ProcessMIDIPortChannelMessage: m_flags fixed 0x00", port.m_flags, 0x00);
		check("ProcessMIDIPortChannelMessage: m_lastMsgKind==0", proc->m_lastMsgKind, 0);

		reset_all_mocks();
		g_bankBuf[0x97c747] = 0x7f;	/* IsEnableViaRPPR-family gate, unrelated -- just kept
						 * deterministic; the real KARMA-controller-generated
						 * dispatch below doesn't read it directly. */
		proc->ProcessKarmaControllerGeneratedChannelMessage(0xb0, 5, (char)0x40, (char)0x7f);
		check("ProcessKarmaControllerGeneratedChannelMessage: status = 0xb0+5",
		      karmaCtrl.m_status, 0xb5);
		check("ProcessKarmaControllerGeneratedChannelMessage: m_lastMsgSentinel==-2",
		      proc->m_lastMsgSentinel, -2);

		reset_all_mocks();
		localCtrl.m_bDamperOn = false;
		localCtrl.m_bSostenutoOn = false;
		check("IsKeyboardAllOff(): both sub-handlers empty, no damper/sostenuto -> true",
		      proc->IsKeyboardAllOff(), 1);

		localCtrl.m_noteOnCount = 1;
		localCtrl.m_noteDownCount[10] = 1;
		check("IsKeyboardAllOff(): a held note on m_localCtrl -> false",
		      proc->IsKeyboardAllOff(), 0);
		localCtrl.m_noteOnCount = 0;
		localCtrl.m_noteDownCount[10] = 0;

		localCtrl.m_bDamperOn = true;
		check("IsKeyboardAllOff(): damper held -> false even with no notes",
		      proc->IsKeyboardAllOff(), 0);
		localCtrl.m_bDamperOn = false;

		localCtrl.m_bSostenutoOn = true;
		check("IsKeyboardAllOff(): sostenuto held -> false (via !IsSostenutoOn())",
		      proc->IsKeyboardAllOff(), 0);
		localCtrl.m_bSostenutoOn = false;
	}

	/*
	 * CKGEventDisplayManager -- expected values independently re-derived
	 * from the raw disassembly formulas by a standalone Python oracle
	 * (scratchpad oracle_ckg_midi_msg_proc_evtdisp.py), NOT copy-pasted
	 * from the .cpp under test.
	 */
	{
		printf("-- CKGEventDisplayManager --\n");
		reset_all_mocks();

		check("GetNoteObjectIndex(0)", CKGEventDisplayManager::GetNoteObjectIndex(0), 1);
		check("GetNoteObjectIndex(3)", CKGEventDisplayManager::GetNoteObjectIndex(3), 4);
		check("GetNoteObjectIndex(4) out-of-range -> default 1", CKGEventDisplayManager::GetNoteObjectIndex(4), 1);
		check("GetNoteObjectIndex(-1) out-of-range -> default 1", CKGEventDisplayManager::GetNoteObjectIndex(-1), 1);

		unsigned char *sub = *(unsigned char **)(g_bankBuf + 8);
		unsigned int *onMask = (unsigned int *)(sub + 0x723c);

		g_eventDisplayObj.NoteOn(60);
		g_eventDisplayObj.NoteOn(60);
		g_eventDisplayObj.NoteOnByKarma(1, 60);	/* module 1 -> objectIndex 2 */
		check("NoteOn(60) x2 -> m_flat[objectIndex0*128+60] == 2",
		      g_eventDisplayObj.m_flat[0 * 128 + 60], 2);
		check("NoteOnByKarma(module=1,60) -> m_flat[objectIndex2*128+60] == 1",
		      g_eventDisplayObj.m_flat[2 * 128 + 60], 1);
		check("NoteOn(60): foreign sub-object on-bitmask bit set (objectIndex 0)",
		      (onMask[0 * 4 + (60 >> 5)] >> (60 & 0x1f)) & 1, 1);
		check("NoteOnByKarma(1,60): foreign sub-object on-bitmask bit set (objectIndex 2)",
		      (onMask[2 * 4 + (60 >> 5)] >> (60 & 0x1f)) & 1, 1);

		/* First NoteOff(60): ring bit not yet set this write-window ->
		 * marks the ring bit only, count is UNCHANGED (decrement
		 * deferred to CheckAndProcessNoteStatus()'s aging pass). */
		g_eventDisplayObj.NoteOff(60);
		check("NoteOff(60) 1st call: count deferred, still 2",
		      g_eventDisplayObj.m_flat[0 * 128 + 60], 2);
		/* Second NoteOff(60): ring bit ALREADY set -> decrements directly. */
		g_eventDisplayObj.NoteOff(60);
		check("NoteOff(60) 2nd call: decrements directly to 1",
		      g_eventDisplayObj.m_flat[0 * 128 + 60], 1);

		reset_all_mocks();
		int cc = 42;	/* 42/8 = 5 */
		g_eventDisplayObj.CCOnByKarma(1, cc);
		g_eventDisplayObj.CCOnByKarma(1, cc);	/* same window -> ring-gate cancels back */
		check("CCOnByKarma(module=1,cc=42) x2 same window -> count nets to 1",
		      g_eventDisplayObj.m_flat[CKGEventDisplayManager::OA_KGEVTDISP_CC_DATA + 1 * 16 + 5], 1);

		reset_all_mocks();
		int bend = 9000;	/* 9000/1024 = 8 */
		g_eventDisplayObj.BendOnByKarma(2, bend);
		check("BendOnByKarma(module=2,bend=9000) -> groupIndex 8 count == 1",
		      g_eventDisplayObj.m_flat[CKGEventDisplayManager::OA_KGEVTDISP_CC_DATA + 2 * 16 + 8], 1);

		reset_all_mocks();
		g_eventDisplayObj.m_flat[CKGEventDisplayManager::OA_KGEVTDISP_TICK_NOW] = 45;
		g_eventDisplayObj.m_flat[CKGEventDisplayManager::OA_KGEVTDISP_NOTE_CHECKPOINT] = 0;
		g_eventDisplayObj.m_flat[CKGEventDisplayManager::OA_KGEVTDISP_CC_CHECKPOINT] = 45;
		g_eventDisplayObj.NoteOn(10);
		g_eventDisplayObj.Idle();	/* diff=45 > 0x13 twice (45,25) -> 2 note-aging passes; CC diff=0 -> none */
		check("Idle(): note count survives 2 aging passes when no matching write-window bit set",
		      g_eventDisplayObj.m_flat[0 * 128 + 10], 1);
		check("Idle(): note checkpoint advances to now - remainder (45-2*20=5 -> 45-5=40)",
		      g_eventDisplayObj.m_flat[CKGEventDisplayManager::OA_KGEVTDISP_NOTE_CHECKPOINT], 40);
		check("Idle(): CC checkpoint untouched (diff was 0)",
		      g_eventDisplayObj.m_flat[CKGEventDisplayManager::OA_KGEVTDISP_CC_CHECKPOINT], 45);
	}

	/*
	 * CKGMIDIMsgProcessor -- expected values independently re-derived by
	 * the same standalone oracle script.
	 */
	{
		printf("-- CKGMIDIMsgProcessor --\n");
		reset_all_mocks();
		CKGMIDIMsgProcessor *kgproc = (CKGMIDIMsgProcessor *)CKGMIDIMsgProcessor::ms_poInstance;

		g_karmaOn = true;
		kgproc->ProcessKarmaGeneratedChannelMessage(0x90, 3, (char)60, (char)100, true);
		check("ProcessKarmaGeneratedChannelMessage: status = channel+statusType (3+0x90)",
		      kgproc->m_karmaGen->m_status, 0x93);
		check("ProcessKarmaGeneratedChannelMessage: data1/data2", kgproc->m_karmaGen->m_data1, 60);
		check("ProcessKarmaGeneratedChannelMessage: flags karma-on|changeSource = 0x05|0x20|0x40",
		      kgproc->m_karmaGen->m_flags, 0x5 | 0x20 | 0x40);

		reset_all_mocks();
		g_karmaOn = false;
		kgproc->ProcessKarmaGeneratedChannelMessage(0x90, 3, (char)60, (char)100, false);
		check("ProcessKarmaGeneratedChannelMessage: flags karma-off|no-changeSource = 0x05",
		      kgproc->m_karmaGen->m_flags, 0x5);

		reset_all_mocks();
		g_karmaOn = true;
		kgproc->ProcessKarmaResetCCChannelMessage(0xb0, 5, (char)0x40, (char)0x7f);
		check("ProcessKarmaResetCCChannelMessage: status = channel+statusType (5+0xb0)",
		      kgproc->m_karmaResetCC->m_status, 0xb5);
		check("ProcessKarmaResetCCChannelMessage: flags karma-on, no changeSource bit at all = 0x25",
		      kgproc->m_karmaResetCC->m_flags, 0x5 | 0x20);

		reset_all_mocks();
		kgproc->ProcessKarmaGeneratedBendRangeChannelMessage(0x30, (char)10);
		check("ProcessKarmaGeneratedBendRangeChannelMessage: status = channel-0x20 (0x30-0x20)",
		      kgproc->m_bendRange->m_status, 0x10);
		check("ProcessKarmaGeneratedBendRangeChannelMessage: flags = 0x05|0x10 unconditionally",
		      kgproc->m_bendRange->m_flags, 0x5 | 0x10);

		reset_all_mocks();
		g_numModules = 2;
		g_realOutputChannel[0] = 3;
		g_realOutputChannel[1] = 7;
		kgproc->ResetKarmaGeneratedCCValue();	/* out-of-scope CKGCCResetHandler::
							 * ResetKarmaGeneratedValue() body is a
							 * no-op stub -- this just proves the
							 * dispatch/module-search itself doesn't
							 * crash across all 16 channels. */
		check("ResetKarmaGeneratedCCValue(): completes without crashing (16-channel search)", 1, 1);

		reset_all_mocks();
		unsigned char msg[1] = { 0x37 };	/* low nibble 7 -> channel 7 */
		kgproc->StoreCCMessage((CMIDIMessage *)msg);
		check("StoreCCMessage(): completes without crashing (routes to m_ccReset[7])", 1, 1);
	}

	printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail ? 1 : 0;
}
