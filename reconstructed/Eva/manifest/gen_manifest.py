#!/usr/bin/env python3
"""
gen_manifest.py - regenerate manifest/eva_functions.csv from the Ghidra static export.

Mirrors reconstructed/OA/manifest's role for Eva: one row per function in the real
binary, tracking pending -> reconstructed -> compiles -> verified. Source of truth is
/home/share/Decomp/EVA_Decomp/eva_export/{functions,symbols}.csv (already generated,
see [[oa_ghidra_decomp_export]]'s sibling export for Eva -- do not re-run Ghidra
analysis, this script only reads the existing static export).

functions.csv has entry/size/signature but NOT demangled Class::method names for most
member functions; symbols.csv has the demangled name (namespace column) + the mangled
label. This script joins the two on entry address so the manifest carries a real
Class::method name wherever one exists.

Usage: python3 gen_manifest.py > eva_functions.csv
(or just `python3 gen_manifest.py` -- writes eva_functions.csv next to this script)
"""
import csv
import os

EXPORT_DIR = "/home/share/Decomp/EVA_Decomp/eva_export"
OUT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "eva_functions.csv")

# Addresses this pass has actually reconstructed (Stage 1 boot path). Update this list
# as later stages land -- kept as a plain literal set here rather than inferring status
# from which src/ files exist, since a src file can implement several functions.
RECONSTRUCTED = {
    "0804ca70",  # _start (CRT entry, not reproduced -- provided by the real toolchain's crt1.o)
    "0804cd50",  # main
    "0804cd10",  # Ouch
    "08e27ea0",  # USTGUserAPI::Connect
    "08e1dde0",  # USTGAPILCDControl::LoadStoredSettings
    "08e4f250",  # CCommDriver::getInstance()
    "08e4f6e0",  # CCommDriver::getInstance(char**)
    "0804e070",  # COmegaInterface::COmegaInterface
    "0804db60",  # COmegaInterface::~COmegaInterface
    "0804e0a0",  # COmegaInterface::GetSysApi
    "0804e0f0",  # COmegaInterface::Init (body reconstructed; several callees still stubbed externs)
    "0804e450",  # COmegaInterface::Run
    "0804e4f0",  # COmegaInterface::Stop
    "0804e590",  # COmegaInterface::Close
    "0824cc40",  # CEditor::CPanelIfcTask::SetMargin

    # --- Stage 3: COmegaInterface::Init()'s own direct callees (2026-07-22) ---
    "0804cb70",  # SetConfigInfo
    "0804d9e0",  # Mains
    "08e4fe70",  # MMainPanelDriver
    "08e4f750",  # MMainHIDDriver
    "0823e840",  # MMainAlphaKeybCtrl
    "08e57680",  # MMainLinuxDriver
    "08249fb0",  # MMainEditor
    "089ee440",  # MMainPanel
    "08240ef0",  # MMainBatchDiskMan
    "08bd1e60",  # MMainESCommon
    "08bfd8e0",  # MMainESProg
    "08bea9c0",  # MMainESEffect
    "08c4b130",  # MMainESCombi
    "08c5eca0",  # MMainESGlobal
    "08bedd80",  # MMainESMOSS
    "08d61b00",  # MMainESSampling
    "08e0a280",  # MMainESSetList
    "08c95fe0",  # MMainESSong
    "08ddc580",  # MMainESDisk
    "0805d4c0",  # CKernel::CKernel(int)
    "0805d820",  # CKernel::~CKernel()
    "0805dba0",  # CKernel::InitSystemLayer
    "0805db90",  # CKernel::GetSysApi (the real one -- not COmegaInterface::GetSysApi's forwarder)
    "0804db70",  # OmegaSchedulingThread
    "0804dd10",  # OmegaInitThread
    "0804dd80",  # OmegaTimingThread

    # --- Stage 4: link-completion pass (2026-07-22) -- reached a real, full link.
    # Only genuinely faithful (Tier A) transcriptions are listed here; the many
    # Tier-B link-stubs (real signature, empty body, not behaviorally reconstructed
    # -- see README.md's Stage 4 section for the full list and the tier convention)
    # are deliberately left "pending", not "reconstructed".
    "080a6be0",  # COmegaPtrArray::COmegaPtrArray
    "080a6ca0",  # COmegaPtrArray::Destroy
    "080a7200",  # COmegaPtrArray::FindIndex
    "080a6f20",  # COmegaPtrArray::RemoveAtIndex
    "080a7310",  # COmegaPtrArray::Shrink
    "08062380",  # CScheduler::CScheduler
    "08062b40",  # CScheduler::InsertLevel
    "08063120",  # CScheduler::Enable
    "0805fca0",  # CModuleManager::Setup
    "0805feb0",  # CModuleManager::Config
    "080600c0",  # CModuleManager::AdjustTaskMask
    "08060350",  # CModuleManager::Start
    "0805efa0",  # CModuleManager::AddModule (Stage 6, 2026-07-25: upgraded Tier-B -> Tier A)
    "08061ca0",  # CModuleManager::EnableUpdate (Stage 6, 2026-07-25: upgraded Tier-B -> Tier A)
    "0805add0",  # CErrorHandler::~CErrorHandler
    "0806ca50",  # CSysApiInstance::Cleanup
    "0806b550",  # CSysApiInstance::AddModule
    "080562f0",  # CConfigManager::AssignEditServerIDs
    "0805e630",  # CKernel::Exec
    "0805dcf0",  # CKernel::InitUserLayer (own call sequence faithful; several callees are Tier-B)
    "08e31e80",  # CSTGHandle::Access
    "08e32150",  # CSTGHandleCache::Initialize
    "08e280f0",  # USTGUserAPI::SendSTGMessageWithSource
    "08e280b0",  # USTGUserAPI::ConnectPanelFifo
    "08e4f5d0",  # CCommDriver::CCommDriver(char**)
    "0807c330",  # CModule::CModule(char const*)
    "08e4fd50",  # CHIDDriver::CHIDDriver
    "08e50050",  # CLinuxPanelDriver::CLinuxPanelDriver
    "0804db50",  # sched_sig_handler (real body is genuinely empty, 1 byte)
    "080d2a00",  # MMainEditMan
    "0814d000",  # MMainViewer
    "081693d0",  # MMainSeqTimer
    "08105a70",  # MMainFileMan (wrapper faithful; CFileMan::CFileMan itself is Tier-B)
    "08179ca0",  # MMainSysEx
    "080cb9e0",  # MMainChunkMan
    "0807fbe0",  # MMainRTRouter
    "080cf850",  # MMainDumpMan
    "08160db0",  # MMainResMan (wrapper faithful; CResMan::CResMan itself is Tier-A
                 # since the Stage 6 CFileMan/CResMan ctor batch, 2026-07-25 -- this
                 # comment was stale, see "081523a0" below and the 2026-07-26 mJob
                 # follow-up fix further down)

    # --- Api/SysApiInstance crash fix (2026-07-23): the real mechanism behind Api's
    # own value, found via a live kronos_vm boot test hitting a NULL dereference in
    # MMainEditMan(). See README.md's own section on this pass for the full writeup.
    "080632e0",  # CGlobalObjectBase::CGlobalObjectBase
    "08063270",  # CGlobalObjectBase::~CGlobalObjectBase (D1, complete-object)
    "08063290",  # CGlobalObjectBase::~CGlobalObjectBase (D0, deleting)
    "0804cc10",  # CGlobalObjectBase::PreKernelConstructor
    "0804cc20",  # CGlobalObjectBase::PostKernelConstructor
    "0804cc30",  # CGlobalObjectBase::PreKernelDestructor
    "0804cc40",  # CGlobalObjectBase::PostKernelDestructor
    "0805da90",  # CKernel::AddGlobalObject
    "0805db40",  # CKernel::RemoveGlobalObject
    "080a6c10",  # COmegaPtrArray::COmegaPtrArray(int,int,int)
    "080a6da0",  # COmegaPtrArray::Add
    "0806cc50",  # global.constructors.keyed.to.SysApiInstance (sets Api = SysApiInstance)
    "080d2560",  # global.constructors.keyed.to.EditApiInstance
    "08167d30",  # global.constructors.keyed.to.SeqApiInstance
    "080bfd60",  # global.constructors.keyed.to.ChkApiInstance
    "080cef10",  # global.constructors.keyed.to.DumpApiInstance
    "08165f70",  # global.constructors.keyed.to.RMApiInstance
    "0817a5c0",  # global.constructors.keyed.to.g_oSysExApiInstance
    "080878a0",  # global.constructors.keyed.to.RTRouterApiInstance (own ApiInstance
                 # sequence faithful; 2 unrelated coincidentally-grouped globals --
                 # kInvalidBytePair/kPitchBendDefault -- not modeled, see mains.cpp)

    # --- Stage 6: breadth sweep, first batch (2026-07-25) -- CScheduler's own
    # per-tick dispatch substrate, closing out its last 2 Tier-B methods that sat on
    # the already-reconstructed boot path.
    "080623e0",  # CScheduler::Exec
    "0805ec70",  # CLevelManagerArray::Add
    "0805ee90",  # CLevelManagerArray::Find

    # --- Stage 2 IPC substrate, closed out (2026-07-25) -- USTGUserAPI's remaining
    # real send/receive/teardown methods + CSTGHandle's remaining 2 methods, all
    # Tier A. See README.md's "Stage 2: IPC substrate closed out" section.
    "08e27f90",  # USTGUserAPI::Disconnect
    "08e28070",  # USTGUserAPI::ConnectUnsolicitedFifo
    "08e282d0",  # USTGUserAPI::ReadMessage
    "08e28350",  # USTGUserAPI::ReadMessageWithTimeout
    "08e28470",  # USTGUserAPI::ReadUnsolicitedMessage
    "08e28220",  # USTGUserAPI::SendPanelMessage

    # Stage 6 breadth sweep, 2026-07-25: USTGAPIControl::SaveRandomSeed()/
    # ForceErPShutdown() + USTGAPIFsck::GenericMumount() promoted from Tier-B
    # link-stubs to real bodies (stg_unsol_msg_handler.cpp).
    "08e1d090",  # USTGAPIControl::SaveRandomSeed()
    "08e1cbe0",  # USTGAPIControl::ForceErPShutdown(unsigned short)
    "08e27120",  # USTGAPIFsck::GenericMumount(char const* const*)
    "08e285c0",  # USTGUserAPI::GetProgress
    "08e28560",  # USTGUserAPI::IncrementProgress
    "08e284f0",  # USTGUserAPI::SetProgress
    "08e31ff0",  # CSTGHandle::Release
    "08e32090",  # CSTGHandle::GetSize
    "08e321a0",  # CSTGHandleCache::Cleanup

    # --- Stage 6: breadth sweep, batch 2 (2026-07-25) -- CModule's real vtable
    # (ground-truth 7-slot sizing, omega_vtables.h/.cpp), CTaskBuffer (new class), and
    # CLevelManager::RunLevel() made genuinely real using them. See README.md's
    # "Stage 6: breadth sweep, batch 2" section.
    "0805ea10",  # CLevelManager::RunLevel
    "08055f20",  # CTaskBuffer::SendBuffer
    "08055ec0",  # CTaskBuffer::~CTaskBuffer

    # --- Stage 6: breadth sweep, batch 4 (2026-07-25) -- CCommDriver::setupfifoname()
    # upgraded Tier-B -> Tier A (directly on the primary boot path: main() ->
    # CCommDriver::getInstance(argv) -> ctor -> setupfifoname()), plus its own
    # Eva_IsSimulation()/Eva_IsSimulationSVGA() dependencies, split into their own
    # TU (src/init/app_mode.cpp). See README.md's "Stage 6: breadth sweep, batch 4"
    # section, including the real no-NULL-check crash bug found in setupfifoname()
    # and the survey findings for CEditor::CPanelIfcTask/USTGAPILCDControl/CKernel
    # (all confirmed to have no further genuinely-reachable methods).
    "08e4f310",  # CCommDriver::setupfifoname
    "0804cd30",  # Eva_IsSimulation
    "0804cd40",  # Eva_IsSimulationSVGA

    # --- Stage 6: breadth sweep, batch 5 (2026-07-25) -- CModule::AdjustTaskMask()
    # upgraded Tier-B -> Tier A. Genuinely boot-path-reachable now that
    # CModuleManager::AddModule() (batch 3) populates mModules: called once per
    # registered module by CModuleManager::AdjustTaskMask() from
    # CKernel::InitSystemLayer()/InitUserLayer(). Real body clears bit 0x02 of each
    # of the module's own tasks' +0x4c mask byte -- the same gate byte/bit
    # CLevelManager::RunLevel() (batch 2) checks. See module.h/module.cpp.
    "0807c640",  # CModule::AdjustTaskMask

    # --- Stage 6: breadth sweep, batch 6 (2026-07-25) -- CSTGUnsolMsgHandler, a
    # whole 30-method class found 100% unclaimed by a broad nm -C class-inventory
    # sweep (same technique that found OA.ko's CSTGControlMsgHandler). Real,
    # non-zero-caller entry point confirmed by disassembly: constructed inside
    # CEditor::CPanelIfcTask::CPanelIfcTask (.text+0x0824b7e0, itself called from
    # CEditor::Setup(), dispatched by CModuleManager::Setup()'s per-module virtual
    # call). 18 of the class's 30 real methods reconstructed (ctor, both real
    # destructor-shaped functions, HandleMessage()'s dispatch table, EndHandling(),
    # SendValueSlider/Encoder, EnterGlobalObjectEdit, 8 confirmed-empty no-ops);
    # the remaining 12 are genuinely deep per-subsystem STG message handlers,
    # Tier-B link-stubs. See include/stg_unsol_msg_handler.h.
    "0891c090",  # CSTGUnsolMsgHandler::CSTGUnsolMsgHandler
    "089b9e30",  # CSTGUnsolMsgHandler::~CSTGUnsolMsgHandler (complete-object, "ResetVTable")
    "089b9e40",  # CSTGUnsolMsgHandler::~CSTGUnsolMsgHandler (deleting, "DeletingDtor")
    "089162e0",  # CSTGUnsolMsgHandler::HandleMessage
    "0891c290",  # CSTGUnsolMsgHandler::EndHandling
    "0891c1f0",  # CSTGUnsolMsgHandler::SendValueSlider
    "0891c240",  # CSTGUnsolMsgHandler::SendValueEncoder
    "0891c3c0",  # CSTGUnsolMsgHandler::EnterGlobalObjectEdit
    "0891c1c0",  # CSTGUnsolMsgHandler::Initialize (confirmed-empty)
    "0891c1d0",  # CSTGUnsolMsgHandler::InitializeForSong (confirmed-empty)
    "0891c1e0",  # CSTGUnsolMsgHandler::BeginHandling (confirmed-empty)
    "08916230",  # CSTGUnsolMsgHandler::TestControlMsgHandler (confirmed-empty, static)
    "08916240",  # CSTGUnsolMsgHandler::ASKMsgHandler (confirmed-empty, static)
    "08916250",  # CSTGUnsolMsgHandler::CalibrationMsgHandler (confirmed-empty, static)
    "08916260",  # CSTGUnsolMsgHandler::FrontPanelMsgHandler (confirmed-empty, static)
    "08916270",  # CSTGUnsolMsgHandler::KLMMsgHandler (confirmed-empty, static)
    "0824cc30",  # CEditor::CPanelIfcTask::GetMargin (real companion to SetMargin, added while re-touching this header)

    # --- CSTGUnsolMsgHandler batch 2 follow-up (2026-07-25, commit 72a2909) -- 5 of
    # batch 6's 12 remaining Tier-B handlers made real (CStorage guard + EditApi
    # scope-id/set-param vtable dispatch + a real .rodata byte table each). This
    # entry backfills gen_manifest.py's own literal address set for that commit,
    # which updated manifest/eva_functions.csv directly via a one-off script and
    # never touched this file -- see include/stg_unsol_msg_handler.h for the
    # per-handler writeup.
    "08916d90",  # CSTGUnsolMsgHandler::PatchMsgHandler
    "08916600",  # CSTGUnsolMsgHandler::EffectMgrMsgHandler
    "08916840",  # CSTGUnsolMsgHandler::EffectMsgHandler
    "08917ad0",  # CSTGUnsolMsgHandler::HDRTrackMsgHandler
    "08916b00",  # CSTGUnsolMsgHandler::SetListMsgHandler

    # --- CSTGUnsolMsgHandler batch 3 (2026-07-25) -- EffectSlotMsgHandler, the one
    # handler batch 2 deferred for a different reason (intricate goto/switch + a
    # reused stack buffer, not deep subsystem reach). Fully resolved by tracing every
    # buffer write against its call site's own `len` argument (see header comment):
    # each write is a single, fully-determined byte, and every call transmitting it
    # uses len==1, so the buffer's other 3 bytes -- which Ghidra's decompile carries
    # forward via `_1_3_`/CONCAT31 idioms -- are never actually read by the real
    # callee. Switch/jump-table structure (15 entries, real .rodata 0x08f1bb1c) and
    # both real byte tables (HandleEffectSlotMsg's own s_akbyAP + CSWTCH_231)
    # cross-checked instruction-by-instruction against the decompile.
    "08917cd0",  # CSTGUnsolMsgHandler::EffectSlotMsgHandler

    # --- CSTGUnsolMsgHandler batch 4 (2026-07-26) -- GlobalMsgHandler, the one
    # genuine new lead identified by the prior session's Tier-B re-survey (its only
    # out-of-scope dependency, SetWithoutUpdatingSTG(), is a real 4-arg IPA-cloned
    # free function called twice, return value unused, safely stubbed). Two real,
    # genuinely asymmetric restore-guard shapes hand-verified via objdump -dr against
    # the real .text+0x08918b50 body (case 0's snapshotted iVar8, case 2's real
    # `goto LAB_089192c8` double-beginRestore re-entry) -- see header/.cpp.
    "08918b50",  # CSTGUnsolMsgHandler::GlobalMsgHandler

    # --- CSTGUnsolMsgHandler batch 5 (2026-07-26) -- ProgramSlotMsgHandler, promoted
    # from Tier B as a follow-up to the same session's 5-handler recheck (which had
    # traced ~80% of this by hand but stopped short of a verified reconstruction).
    # Real size 1849 bytes (0x08918410..0x08918b49 -- Ghidra's own "size=1792" label
    # undercounts trailing out-of-line branch targets, same pattern already seen for
    # EffectSlotMsgHandler). New real dependencies CMMI/CModeManager/CKGMsgProcessor,
    # all stubbed file-local (real signatures, opaque-but-safe-to-call bodies), same
    # convention as SetWithoutUpdatingSTG() above.
    "08918410",  # CSTGUnsolMsgHandler::ProgramSlotMsgHandler

    # --- CSTGUnsolMsgHandler batch 6 (2026-07-26) -- ProgramMsgHandler, promoted from
    # Tier B as a follow-up to the same session's 5-handler recheck. Real 9-way jump
    # table on msg+8 (.rodata 0x08f1bb88), 9 real per-subtype {code,value} byte-pair
    # tables all confirmed via direct objdump/raw .rodata reads (including a genuine
    # 2-byte coincidental-overlap between two of them, see .cpp's own header comment).
    # New file-local stubs: HandleProgToneAdjustParam (shared with CombiMsgHandler's
    # own case 3), CMMI/CModeManager (reused from ProgramSlotMsgHandler).
    "08919fd0",  # CSTGUnsolMsgHandler::ProgramMsgHandler

    # --- CSTGUnsolMsgHandler batch 7 (2026-07-26) -- CombiMsgHandler, promoted from
    # Tier B after this same day's own deferral (see eva_stg_programslot_programmsg_
    # reconstructed_combimsg_deferred_2026-07-26.md). Real size 3184 bytes
    # (0x08919360..0x08919fd0 -- same "decompiler undercounts trailing branch targets"
    # pattern already seen for EffectSlotMsgHandler/ProgramSlotMsgHandler). Physical
    # .text layout groups guard/table code BY SCOPE STRING across cases rather than by
    # case number -- every case's own table base was independently re-derived by
    # tracing forward from its own real GetScopeId call site via objdump -dr, per the
    # deferred note's own recommended method; several addresses disagree with that
    # note's own unverified proximity-based guesses. New real dependencies
    # CStorage::GetInstance()/CPrograms::GetProgramPointer/IsCopyableBank/
    # CToneAdjustTool::ConvertParamToLinear, all stubbed file-local -- the last one's
    # RETURN VALUE is genuinely consumed (not just gated) by one branch, stubbed to a
    # fixed sentinel.
    "08919360",  # CSTGUnsolMsgHandler::CombiMsgHandler

    # --- Stage 6: breadth sweep, CTask::CTask() reconstruction batch (2026-07-25).
    # CORRECTS the stale note that used to sit here (see git history) -- CTask::CTask()
    # genuinely IS called in ground truth (CEditor::CPanelIfcTask's and CPoller's own
    # ctors, both confirmed via direct objdump -dr inspection), and CModule::Add(CTask*)
    # is the real mTasks-populating method neither this project's own batch 2 nor
    # batch 5 knew existed -- itself boot-path-reachable via CEditor::Setup() ->
    # CModuleManager::Setup(). See task.h/module.h for the full writeup.
    "0807ee80",  # CTask::CTask
    "0807bd10",  # CLimiterMan::CLimiterMan
    "0807c410",  # CModule::Add(CTask*)

    # --- Stage 6: breadth sweep, CClientCommServer/CSysExMsgTaskBase follow-up
    # (2026-07-25). Corrects batch 6's own "no confirmed caller found in a quick
    # check, lower confidence" verdict on CClientCommServer: a THOROUGH check found
    # a real, confirmed, boot-path-adjacent caller chain (CKernel::InitUserLayer()
    # -> CConfigManager::SetupSysex(), upgraded to Tier A here -- was an empty
    # Tier-B stub -- -> SysExApi->RegisterMessageClient() virtual dispatch ->
    # CSexServiceTask::RegisterMessageClient() [not reconstructed, out of scope] ->
    # CClientCommServer::CClientCommServer()). CSysExMsgTaskBase's own real caller
    # (CDumpTask::CDumpTask, itself constructed by CDumpManMod::Setup(), dispatched
    # by the already-real CModuleManager::Setup()/AddModule() spine) confirmed the
    # same way. See include/client_comm_server.h/sysex_msg_task_base.h for the full
    # writeup, including why most of both classes stay Tier B (a genuinely separate,
    # un-reconstructed CEvBuffersPool/CEvent event-buffer-pool subsystem for
    # CClientCommServer; CTask::SetMask()/~CTask()/CTask::Add(COutLink*), all three
    # a CONCURRENT agent's CTask/CModule-family territory this session, for
    # CSysExMsgTaskBase).
    "08056b90",  # CConfigManager::SetupSysex
    "08173430",  # CClientCommServer::CheckIncomingSexCRCByte
    "0816f610",  # CClientCommServer::ComputeCRCByte
    "080a64f0",  # CSysExMsgTaskBase::Exec(CMessage&)
    "08184ed0",  # CSysExMsgTaskBase::OnSexLinkError (confirmed-empty)
    "08184ec0",  # CSysExMsgTaskBase::OnReceiveMessage (confirmed-empty)
    "08184ee0",  # CSysExMsgTaskBase::OnTimeout (confirmed-empty)

    # --- Stage 6: breadth sweep, follow-up pass same day (2026-07-25) -- the
    # CEvBuffersPool/CEvent subsystem the block above deferred is now real,
    # reconstructed via direct objdump -dr -M intel transcription (no Ghidra
    # decompile needed, small/mechanical enough). Unblocks CClientCommServer's own
    # ctor/dtor + 5 more leaf methods (10/26 Tier A total now). See
    # include/ev_buffers_pool.h/event.h/client_comm_server.h for the full writeup.
    "0807f100",  # CEvBuffersPool::CEvBuffersPool (ctor)
    "0807f0b0",  # CEvBuffersPool::~CEvBuffersPool (D1)
    "0807f0d0",  # CEvBuffersPool::~CEvBuffersPool (D0)
    "0807f400",  # CEvBuffersPool::Alloc
    "0807f4b0",  # CEvBuffersPool::Free
    "0807f660",  # CEvBuffersPool::Lock
    "08182520",  # CEvent::~CEvent
    "0816ecc0",  # CClientCommServer::CClientCommServer (ctor)
    "0816f240",  # CClientCommServer::~CClientCommServer
    "0816f2c0",  # CClientCommServer::SendMessageToClient
    "0816f370",  # CClientCommServer::SendToSysExLink
    "0816f3a0",  # CClientCommServer::RetryTXPacket
    "0816f3d0",  # CClientCommServer::TXData
    "08170010",  # CClientCommServer::OnProcessRetry
    "0816ffd0",  # CClientCommServer::OnRxMsgWhenInWAIT

    # --- Stage 6: breadth sweep, batch 2026-07-25b (CConfigManager's remaining
    # CKernel::InitUserLayer() bring-up steps + BPM/MPQN). Broad nm -C sweep of the
    # ground-truth binary for unclaimed territory (explicitly avoiding CTask/CModule/
    # CLimiterMan/CSysExMsgTaskBase/CSTGUnsolMsgHandler, a concurrent agent's own
    # territory this session); found this cluster by finishing off CConfigManager's
    # own already-identified Tier-B stub group (SetupSysex was already done in the
    # prior batch above). SetupRouting confirmed genuinely a 1-byte `return;` in the
    # real binary. ConfigureSeqTimer's own real tail unconditionally calls
    # BPM::SetLowerLimit()/SetUpperLimit() (tempo.cpp, new) -- config_info.cpp's
    # own SeqTimerInfo placeholder had to be given sane non-zero BPM defaults
    # (40/240) instead of the usual all-zero convention, since BPM's own real
    # SetLowerLimit/SetUpperLimit divide by their argument with no zero-guard (a
    # real hazard this reconstruction pass's own placeholder data would otherwise
    # trigger). ChkApi/SeqApi/RTRouterApi's own vtable arrays (mains.cpp) bumped
    # past their old 6-slot bound to safely cover the new real offsets dispatched
    # through (same "give headroom" fix already applied to EditApiInstance's own
    # array). See config_manager.cpp/config_info.cpp/tempo.h/tempo.cpp for the
    # full writeup.
    "08056d80",  # CConfigManager::SetupRouting (confirmed-empty)
    "08056d90",  # CConfigManager::MakeConnections
    "08056a50",  # CConfigManager::RegisterChunkServer
    "08056840",  # CConfigManager::LinkRTRouterTracks
    "08056ed0",  # CConfigManager::ConfigureSeqTimer
    "0816ba80",  # BPM::SetLowerLimit(unsigned int)
    "0816bb10",  # BPM::SetUpperLimit(unsigned int)
    "0816bc00",  # BPM::_GLOBAL__I_sm_LowerLimit (static ctor -> MPQN defaults)

    # --- Stage 6: SetMask/~CTask batch (2026-07-25). CTask::SetMask(EMask) and
    # CTask::~CTask() reconstructed (task.h/task.cpp) -- both were the two real,
    # currently-unavailable dependencies the prior batch's own CSysExMsgTaskBase
    # writeup named as blocking 6 of its 14 manifest rows (ctor, SetTimeout, Exec(),
    # dtor + its own 2 non-virtual thunks). CLimiterMan::~CLimiterMan()
    # (limiter_man.h/.cpp) reconstructed too -- CTask::~CTask()'s own real
    # dependency (destroys the embedded CLimiterMan sub-object). All 6 of the
    # previously-blocked CSysExMsgTaskBase rows below are now promoted to Tier A
    # (sysex_msg_task_base.h/.cpp) -- the ctor's ECanTransmit==1 branch and
    # SendMsg/EventToMessage/MessageToEvent stay Tier B, a DIFFERENT, unrelated
    # blocker (the CSysExMsgClientOutLink/CSexServiceTask output-link subsystem).
    "0807bbc0",  # CLimiterMan::~CLimiterMan (D1)
    "0807e350",  # CTask::~CTask (D1)
    "0807e670",  # non-virtual thunk to CTask::~CTask (D1)
    "0807e6c0",  # non-virtual thunk to CTask::~CTask (D0)
    "0807e840",  # CTask::SetMask(EMask)
    "080a65e0",  # CSysExMsgTaskBase::CSysExMsgTaskBase (ctor)
    "080a67c0",  # CSysExMsgTaskBase::SetTimeout(ushort)
    "080a65a0",  # CSysExMsgTaskBase::Exec() (0-arg)
    "08184ef0",  # CSysExMsgTaskBase::~CSysExMsgTaskBase (D1)
    "08184f10",  # non-virtual thunk to CSysExMsgTaskBase::~CSysExMsgTaskBase (D1)
    "08184f70",  # non-virtual thunk to CSysExMsgTaskBase::~CSysExMsgTaskBase (D0)

    # --- Stage 6: breadth sweep, CModuleManager "module factory array" batch
    # (2026-07-25). Reconstructs the distinct CModuleManager sub-structure
    # (mConstructors, +0x1c..+0x34, own count/array at absolute +0x28/+0x30) that
    # the prior "batch 2026-07-25b" above deferred CreateUserModules()/
    # CreateFMDrivers() for. AddConstructor()/RemoveConstructor() (module_manager.h/
    # .cpp) are the real read/write side; CSysApiInstance::AddConstructor()
    # (sysapi_instance.h/.cpp) is the real forwarder mains.cpp's
    # RegisterModuleDescriptor() reaches through Api's own vtable slot +0x40 --
    # confirmed by direct byte read of the ground-truth binary's installed
    # CSysApiInstance vtable (VA 08e81008+0x40 == 0x0806b530), and wired for real in
    # omega_vtables.cpp (PTR__CSysApiInstance_08e81008[16], previously a dead
    # EvaVTableStub no-op -- same "Tier-B stub leaves a real array permanently
    # empty" bug class as CModuleManager::AddModule()/mModules, Stage 6 batch 3).
    # CreateUserModules()/CreateFMDrivers() themselves (config_manager.cpp) are real,
    # safe no-ops on this pass's own traced boot path given today's zero-initialized
    # sm_ptCreateInfo/sm_ptFMDriverInfo placeholders (config_info.cpp) -- each
    # table's own first name field gates the entire function, same "safe to zero"
    # property already established for SetupRouting/MakeConnections/
    # RegisterChunkServer/LinkRTRouterTracks. See module_manager.h/config_manager.cpp
    # for the full writeup.
    "0805f660",  # CModuleManager::AddConstructor(CModuleConstructor&)
    "0805f990",  # CModuleManager::RemoveConstructor(CModuleConstructor&)
    "0806b530",  # CSysApiInstance::AddConstructor(CModuleConstructor&)
    "08056440",  # CConfigManager::CreateUserModules
    "08056760",  # CConfigManager::CreateFMDrivers

    # --- Stage 6: breadth sweep, MMainESCommon/MMainESGlobal survey batch
    # (2026-07-25). Broad nm -C survey of all 13 untraced mains.cpp MMainXxx
    # shims found the entire 10-member CModule+CEditServer "edit server"
    # family (ESCommon/ESProg/ESEffect/ESCombi/ESGlobal/ESMOSS/ESSampling/
    # ESSetList/ESSong/ESDisk) shares one identical shell shape wrapping a
    # generic, real, shared descriptor-based Get/Set engine (CDataHandler/
    # CEditServer, edit_server.h) -- reconstructed here using CESCommon
    # (es_common.h) as the representative instance. Each of the 10 classes'
    # own Setup()-owned "CXxxTask" is a genuinely deep CSK/CForm-scale
    # god-object (52-1092 real methods per nm -C, all 10 individually
    # counted) and stays out of scope, same boundary as CFileMan/CResMan.
    # Neither CDataHandler/CEditServer nor CESCommon are reachable from this
    # reconstruction's own currently-wired boot path yet -- construction is
    # gated behind CConfigManager::CreateUserModules() (above), itself a
    # real no-op given today's zero-initialized sm_ptCreateInfo placeholder.
    # Verified via verify/test_edit_server.cpp (30 checks) +
    # verify/test_es_common.cpp (6 checks, including a byte-exact
    # sizeof(CESCommon)==0x40064 check confirming the multiple-inheritance
    # layout matches CESCommonModuleConstructor::Create()'s own real malloc
    # size). Along the way, corrected this batch's own initial assumption
    # about Get()/Set()'s return-code convention (neither is a simple
    # success/failure boolean -- see edit_server.h's own Get()/Set() comments).
    "0806d390",  # CDataHandler::AddDescriptors(CObjectBase*, SDescriptor*, int, bool)
    "0806d8d0",  # CDataHandler::FindDescriptor(uchar,uchar,uchar,SDescriptor*&,CObjectBase*&) const
    "0806e050",  # CDataHandler::FindDescriptor(CObjectBase*, uchar CObjectBase::*, SDescriptor*&) const
    "08070970",  # CEditServer::CEditServer(char const*)
    "08070820",  # CEditServer::~CEditServer() (complete-object dtor)
    "08070a80",  # CEditServer::FindDescriptor(uchar,uchar,uchar) const
    "0806fa90",  # CEditServer::SetDefault(uchar,uchar,uchar)
    "0806fb40",  # CEditServer::Get(uchar,uchar,uchar,void*,uint)
    "080700b0",  # CEditServer::Set(uchar,uchar,uchar,void const*,uint,EEditSource)
    "08c1a680",  # CEditServer::PutNotify(uchar,uchar)
    "08bd1e00",  # CESCommon::CESCommon(char const*, int)
    "08bd1be0",  # CESCommon::~CESCommon() (complete-object dtor)
    "08bd1d80",  # CESCommon::Setup()
    "08bd1bd0",  # CESCommon::Start(), confirmed-empty
    "08bd1bc0",  # CESCommon::Config(), confirmed-empty

    # --- Stage 6: breadth sweep, SECOND CClientCommServer follow-up pass same day
    # (2026-07-25). Promotes 4 more of the 16 methods the first follow-up pass left
    # Tier B (14/26 Tier A total now). Biggest finding: what that pass called
    # `mUnknown04` ("some kind of retry-in-flight flag, not fully confirmed") is
    # actually this class's own top-level protocol state (IDLE=0/SENT=1/WAIT=2) --
    # confirmed independently by OnReceiveSysExBuffer()'s own 3-way dispatch and
    # OnRxPacket()'s own state transitions -- renamed mUnknown04 -> mState. The other
    # 12 methods stay Tier B, each for a distinct, examined reason (see
    # include/client_comm_server.h's own SCOPE section): PrepareMsgBuffer()/
    # UnprepareBuffer()/EventToMessage()/MessageToEvent() need a real, non-trivial
    # byte-packing wire transform not yet decoded bit-exact; Error() is real but
    # ~2KB of heavily-shared-tail-duplicated code judged disproportionate to
    # de-duplicate correctly this pass; OnReceiveMessage() needs CMessage's own
    # out-of-scope internals; OnRxMsgWhenInIDLE/SENT and OnRxSexWhenInIDLE/SENT (the
    # other 4 members of the IDLE/SENT/WAIT dispatch family besides
    # OnRxSexWhenInWAIT, reconstructed here) depend on the same two blockers.
    "08172860",  # CClientCommServer::OnRxSexWhenInWAIT
    "08173210",  # CClientCommServer::OnReceiveSysExBuffer
    "08172320",  # CClientCommServer::OnRxPacket
    "08170090",  # CClientCommServer::TransmitSexAnswer

    # --- Stage 6: CSysExMsgClientOutLink follow-up pass (2026-07-25). Reconstructs
    # the real COutLink/COutLinkMono/CSysExMsgOutLink/CSysExMsgClientOutLink output-
    # link family (out_link.h/.cpp) -- the subsystem the prior CClientCommServer/
    # CSysExMsgTaskBase passes both deferred as "genuinely separate, un-reconstructed"
    # (client_comm_server.h/sysex_msg_task_base.h). CTask::Add(COutLink*) (task.h/
    # .cpp) is real too -- confirmed via direct objdump -dr reading after Ghidra's own
    # decompile mis-resolved its tail-jmp as a zero-argument indirect call; it's the
    # SAME Api vtable slot 0x12c notification CModule::Add(CTask*) already uses,
    # just triggered from the COutLink-registration path. This unblocks the final 3
    # CSysExMsgTaskBase methods (SendMsg/EventToMessage/MessageToEvent, all 14/14
    # Tier A now) and gives CSexServiceTask::TransmitSysEx()/COutLinkMono::OutMono()
    # their real bodies where CClientCommServer::SendMessageToClient() calls through
    # (client_comm_server.h's own counting-stub placeholder for OutMono() removed).
    # CSexServiceTask::TransmitSysEx() itself stays a minimal stub (client_comm_server.
    # cpp) -- its only real dependency, CSexInputTask::TransmitSysEx(), is a 1374-byte
    # CSexMatrix routing engine, a genuinely disproportionate sub-effort of its own,
    # documented but not pursued. CSysExApiInstance::{EventToMessage,MessageToEvent}
    # (sysex_msg_task_base.cpp) are likewise minimal linkage-only counting stubs -- the
    # class itself (config_manager.cpp's own earlier CSysExApi survey) is out of scope.
    "0807e870",  # CTask::Add(COutLink*)
    "0807cb20",  # COutLink::COutLink(CTask const&, char const*, EDirection, ushort, int)
    "0807d1f0",  # COutLink::TestResult(EMessageResult, CLink*)
    "0807d2e0",  # COutLinkMono::COutLinkMono(CTask const&, char const*, EDirection, ushort)
    "0807d3c0",  # COutLinkMono::OutMono(ushort, void*, ushort)
    "080a69f0",  # CSysExMsgOutLink::CSysExMsgOutLink(CTask const&, char const*)
    "080a5aa0",  # CSysExMsgClientOutLink::CSysExMsgClientOutLink(CTask const&)
    "080a5ad0",  # CSysExMsgClientOutLink::SendMessage(uchar, uchar const*, uchar)
    "080a6730",  # CSysExMsgTaskBase::SendMsg(uchar const*, uchar)
    "080a68c0",  # CSysExMsgTaskBase::EventToMessage(CLinkedEvent const*, uchar*, uchar&)
    "080a6970",  # CSysExMsgTaskBase::MessageToEvent(uchar const*, uchar, CLinkedEvent*)

    # --- Stage 6: breadth sweep, DumpManager cluster batch (2026-07-25). Found via a
    # broad nm -C sweep of what CDumpManMod::Setup() (one of the 6 small "MMainXxx(void)"
    # derived modules mains.cpp installs a real vtable for but had never traced the
    # bodies of) actually constructs. CCircByteBuffer/CDumpBuffer/CDumpManStateMachine/
    # CDumpMachine/CDumpTask/CBufferingTask/CDumpManMod -- see circ_byte_buffer.h/
    # dump_buffer.h/dump_man_state_machine.h/dump_task.h/buffering_task.h/dump_man_mod.h
    # for the full reachability chain and per-class writeup. This is the first time
    # this reconstruction's own wired call graph genuinely exercises CSysExMsgTaskBase's
    # ECanTransmit==1 branch and CTask::Add(COutLink*) end to end (both previously
    # "ground-truth reachable but dead in this reconstruction", task.h/sysex_msg_task_
    # base.h) -- CDumpTask IS-A CSysExMsgTaskBase and is constructed with canTransmit=1.
    "080ce1c0",  # CCircByteBuffer::CCircByteBuffer(ulong)
    "080ce160",  # CCircByteBuffer::~CCircByteBuffer() (non-deleting)
    "080ce190",  # CCircByteBuffer::~CCircByteBuffer() (deleting)
    "080ce140",  # CCircByteBuffer::Reset()
    "080ce3e0",  # CCircByteBuffer::Read(uchar*, ulong)
    "080ce310",  # CCircByteBuffer::Write(uchar const*, ulong)
    "080ceff0",  # CDumpBuffer::CDumpBuffer(ulong)
    "080cefa0",  # CDumpBuffer::~CDumpBuffer() (non-deleting)
    "080cefc0",  # CDumpBuffer::~CDumpBuffer() (deleting)
    "080cef70",  # CDumpBuffer::Reset()
    "080cf9b0",  # CDumpManStateMachine::CDumpManStateMachine()
    "080cf950",  # CDumpManStateMachine::~CDumpManStateMachine() (non-deleting)
    "080cf980",  # CDumpManStateMachine::~CDumpManStateMachine() (deleting)
    "080cfa30",  # CDumpManStateMachine::Init()
    "080cf4d0",  # CDumpMachine::CDumpMachine(CDumpTask&)
    "08186810",  # CDumpMachine::~CDumpMachine() (non-deleting)
    "08186830",  # CDumpMachine::~CDumpMachine() (deleting)
    "080cf2c0",  # CDumpMachine::SetTimeout(ushort)
    "080cf4a0",  # CDumpMachine::SendSexMessage(uchar const*, uchar)
    "080cf410",  # CDumpMachine::PutMessage(uchar const*, uchar) -- real forward, Tier-B target
    "080d1b10",  # CDumpTask::CDumpTask(CModule const&)
    "080d19d0",  # CDumpTask::~CDumpTask() (non-deleting)
    "080d1a60",  # CDumpTask::~CDumpTask() (deleting)
    "080d1c20",  # CDumpTask::OnGetMessage(uchar const*, uchar) -- real forward, Tier-B target
    "080d18d0",  # CDumpTask::OnReceiveMessage(uchar, uchar const*, uchar) -- real forward
    "08186880",  # CDumpTask::OnTimeout() -- re-derived from raw objdump -dr (Ghidra's own
                 # decompile mis-resolved the real tail call as a bogus zero-arg double
                 # indirection)
    "080cdd50",  # CBufferingTask::CBufferingTask(CModule const&)
    "080cdc10",  # CBufferingTask::~CBufferingTask() (non-deleting)
    "080cdcb0",  # CBufferingTask::~CBufferingTask() (deleting)
    "080ce120",  # CBufferingTask::GetDumpLength(ulong&) const
    "080cf820",  # CDumpManMod::CDumpManMod()
    "080cf650",  # CDumpManMod::Setup()
    "080cf500",  # CDumpManMod::Config() (confirmed genuinely empty)
    "080cf510",  # CDumpManMod::Start() (confirmed genuinely empty)

    # --- Re-check of the DumpManager cluster batch (2026-07-26): promoted
    # CDumpBuffer::Read()/Write() and CDumpMachine::ReadPacket()/WritePacket()/
    # IsDumpEnded() from Tier B to real bodies -- still NOT reachable from this
    # reconstruction's own wired call graph (their real callers stay the
    # out-of-scope CDumpManStateMachine state-handler family, plus a separately-
    # confirmed-dead-in-ground-truth-itself CDumpApiInstance/DumpApi path), but
    # small, self-contained (zero new dependencies), and complete this cluster's
    # own CDumpBuffer/CDumpMachine classes to 100% real -- see dump_buffer.h's own
    # updated header comment for the full length-tracking algorithm writeup,
    # including a confirmed (not guessed) Read()/Write() field-polarity asymmetry.
    "080cf030",  # CDumpBuffer::Read(uchar*, ulong)
    "080cf170",  # CDumpBuffer::Write(uchar const*, ulong)
    "080cf2f0",  # CDumpMachine::ReadPacket(uchar*, uchar)
    "080cf380",  # CDumpMachine::WritePacket(uchar const*, uchar)
    "080cf250",  # CDumpMachine::IsDumpEnded()

    # --- Stage 6: breadth sweep, THIRD CClientCommServer follow-up pass same day
    # (2026-07-25). Promotes 9 of the 10 methods the second follow-up pass left
    # Tier B (23/26 Tier A total now -- only OnReceiveMessage(CMessage const&) stays
    # Tier B, CMessage itself being genuinely out of scope). Transcribed from the
    # Ghidra decompile export (Decomp/EVA_Decomp/eva_export/functions) rather than
    # raw objdump this time -- the byte-packing loops are hand-unrolled 7-deep,
    # which made a decompile cross-check the safer source of truth. Biggest finding:
    # PrepareMsgBuffer()/UnprepareBuffer() are a matched DECODE/ENCODE pair for a
    # MIDI-SysEx-style 8-to-7-bit-safe framing (flag byte first, then up to 7
    # payload bytes per group) -- confirmed bit-for-bit against the decompile's own
    # literal shift/mask constants for all 7 positions, not guessed. This ALSO
    # unblocked the other 4 members of the IDLE/SENT/WAIT dispatch family
    # (OnRxMsgWhenInIDLE/SENT, OnRxSexWhenInIDLE/SENT) that the prior pass predicted
    # would still need Error()'s own body -- a fresh dependency check found ZERO
    # remaining CMessage/CSexMatrix-shaped calls in any of them once
    # PrepareMsgBuffer()/UnprepareBuffer()/Error() were real. See
    # include/client_comm_server.h's own updated header comment for the full
    # per-method writeup.
    "081706d0",  # CClientCommServer::PrepareMsgBuffer(uchar*, uchar&, uchar const*, uchar)
    "08170a00",  # CClientCommServer::UnprepareBuffer(CLinkedEvent*, uchar const*, uchar, uchar)
    "081736f0",  # CClientCommServer::EventToMessage(CLinkedEvent const*, uchar*, uchar&)
    "08173970",  # CClientCommServer::MessageToEvent(uchar const*, uchar, CLinkedEvent*)
    "0816f830",  # CClientCommServer::Error(EErrNotifyMode)
    "08172990",  # CClientCommServer::OnRxSexWhenInIDLE(ESexMsgType, uchar const*, uchar, uchar)
    "08172bf0",  # CClientCommServer::OnRxSexWhenInSENT(ESexMsgType, uchar const*, uchar, uchar)
    "08171510",  # CClientCommServer::OnRxMsgWhenInIDLE(uchar const*, uchar, uchar)
    "08171db0",  # CClientCommServer::OnRxMsgWhenInSENT(uchar const*, uchar, uchar)
    "08172010",  # CClientCommServer::OnReceiveMessage(CMessage const&) -- 2026-07-27 closeout,
                 # promoted from Tier B (see client_comm_server.h); closes CClientCommServer 26/26

    # --- Stage 6: breadth sweep, CHIDDriver/CLinuxPanelDriver batch (2026-07-25).
    # Found via a fresh nm -C class-inventory sweep for boot-path-DIRECT (not just
    # -adjacent) unclaimed territory: both classes are MMainHIDDriver/
    # MMainPanelDriver's own direct-construction targets (mains.cpp), constructed
    # unconditionally every boot, before any of Mains()'s own config-table gating
    # applies -- previously declared as opaque `__thiscall` ctor-only call-contract
    # externs with a bare, undersized `void* = 0` vtable scalar (the same
    # undersized-vtable bug class found repeatedly elsewhere in this project, though
    # not yet load-bearing here since nothing dispatched through either vtable
    # before this batch). Both fully reconstructed -- real ctor/dtor/all named
    # methods, byte-exact real vtables (read directly off .rodata, not inferred) --
    # see include/hid_driver.h/include/panel_driver.h for the full writeup.
    # CHIDDriver is a genuine Linux-evdev USB keyboard driver (bustype-scans
    # /sys/class/input, reads raw `struct input_event` records, decodes scancodes
    # via a real 127-byte .rodata lookup table, tracks a modifier bitmask) --
    # found and preserved (not fixed) one genuine ground-truth bug along the way:
    # GetKeyboardEvent() computes its own "isKeyDown" output field from a byte
    # GetEvent() never actually writes (real uninitialized-stack read in the
    # binary itself). CLinuxPanelDriver's PutCommand() confirmed and generalized
    # STGMessage's `{u16,u16,u32,u32[,u32]}` wire shape via a new
    # USTGAPIFrontPanel (SetLED/SetLEDBlinking/ResetLED/SetLED16Bit/Beep), all
    # bottoming out in the already-real USTGUserAPI::SendPanelMessage().
    "08e4f7e0",  # CHIDDriver::Open(void*)
    "08e4f7f0",  # CHIDDriver::Close(void*)
    "08e4f800",  # CHIDDriver::PutEvent(IHIDDriver::SUsbKeybEvent&)
    "08e4f810",  # CHIDDriver::GetKeyboardEvent(IAlphaKeybEvent::SKeyboardEvt&)
    "08e4f8e0",  # CHIDDriver::ReadOvercurrentCondition()
    "08e4f8f0",  # CHIDDriver::EnableAfterOvercurrent()
    "08e4f900",  # CHIDDriver::SetTypematicRateDelay(uchar)
    "08e4f910",  # CHIDDriver::SetLeds(uchar)
    "08e4f920",  # CHIDDriver::GetEvent(IHIDDriver::SUsbKeybEvent*)
    "08e4fb20",  # CHIDDriver::KeyboardIsConnected()
    "08e4fc80",  # CHIDDriver::~CHIDDriver() (complete-object)
    "08e4fce0",  # CHIDDriver::~CHIDDriver() (deleting)
    "08e4fd50",  # CHIDDriver::CHIDDriver(char const*, char const*, char const*)
    "08e4fdf0",  # IHIDDriver::GetDriverClass()
    "08e4fee0",  # CLinuxPanelDriver::Open(void*)
    "08e4fef0",  # CLinuxPanelDriver::Close(void*)
    "08e4ff00",  # CLinuxPanelDriver::PutEvent(CPanelDriver::SEvent&)
    "08e4ff10",  # CLinuxPanelDriver::PutCommand(CPanelDriver::SCommand*)
    "08e4ffa0",  # CLinuxPanelDriver::GetEvent(CPanelDriver::SEvent*)
    "08e4ffe0",  # CLinuxPanelDriver::~CLinuxPanelDriver() (complete-object)
    "08e50010",  # CLinuxPanelDriver::~CLinuxPanelDriver() (deleting)
    "08e50050",  # CLinuxPanelDriver::CLinuxPanelDriver(char const*)
    "08e500d0",  # CPanelDriver::GetDriverClass()
    "08e1d440",  # USTGAPIFrontPanel::SetLED(uint)
    "08e1d480",  # USTGAPIFrontPanel::SetLEDBlinking(uint)
    "08e1d4c0",  # USTGAPIFrontPanel::ResetLED(uint)
    "08e1d500",  # USTGAPIFrontPanel::SetLED16Bit(uint, ushort)
    "08e1d550",  # USTGAPIFrontPanel::Beep()

    # Stage 6 breadth sweep, 2026-07-25 -- small-derived-module follow-up batch
    # (edit_man.h/chunk_man.h/seq_timer.h/message_port.h): CEditMan/CMainTask,
    # CChunkMan/CChkBaseTask/CChkCmd, CSeqTimer, CMessagePort. CTimerEngine/
    # CChkCmdBG were Tier-B stubs at the time (NOT listed here) -- both are now
    # fully real, see the dedicated CHeap/CTimerEngine batch entries further down.
    "080d2810",  # CEditMan::CEditMan()
    "080d2790",  # CEditMan::Setup()
    "080d2640",  # CEditMan::Config()
    "080d2650",  # CEditMan::Start()
    "080d2840",  # CEditMan::RegisterServer(CEditServer const*)
    "080d2860",  # CEditMan::UnregisterServer(CEditServer const*)
    "080d2880",  # CEditMan::GetServerScope(CEditServer const*)
    "080d28a0",  # CEditMan::GetServerScope(char const*)
    "080d28c0",  # CEditMan::RegisterClient(CEditClient const*)
    "080d28e0",  # CEditMan::UnregisterClient(CEditClient const*)
    "080d2900",  # CEditMan::FindDescriptor(uchar, uchar, uchar, CEditServer**)
    "080d29c0",  # CEditMan::SetDefault(uchar, uchar, uchar)
    "080d3130",  # CEditMan::CMainTask::CMainTask(CModule const&)
    "080d3230",  # CEditMan::CMainTask::RegisterServer(CEditServer const*)
    "080d32e0",  # CEditMan::CMainTask::UnregisterServer(CEditServer const*)
    "080d3340",  # CEditMan::CMainTask::GetServerScope(CEditServer const*)
    "080d3350",  # CEditMan::CMainTask::GetServerScope(char const*)
    "080d34b0",  # CEditMan::CMainTask::RegisterClient(CEditClient const*)
    "080d3500",  # CEditMan::CMainTask::UnregisterClient(CEditClient const*)
    "080d3550",  # CEditMan::CMainTask::FindDescriptor(uchar, uchar, uchar, CEditServer**)
    "080d35f0",  # CEditMan::CMainTask::Get(uchar, uchar, uchar, void*, uint)
    "080d36b0",  # CEditMan::CMainTask::Set(uchar, uchar, uchar, void const*, uint, EEditSource)
    "080d3780",  # CEditMan::CMainTask::SetDefault(uchar, uchar, uchar)
    "080d3820",  # CEditMan::CMainTask::Notify(uchar, uchar, uchar)
    "080cb930",  # CChunkMan::CChunkMan()
    "080cb880",  # CChunkMan::Setup()
    "080cb6a0",  # CChunkMan::Config()
    "080cb740",  # CChunkMan::Start()
    "080bfec0",  # CChkBaseTask::CChkBaseTask(CModule const&, char const*, ETaskLevel, CTask::EScheduleFlag)
    "080c0ea0",  # CChkCmd::CChkCmd(CModule const&)
    "081693a0",  # CSeqTimer::CSeqTimer(char const*)
    "08169310",  # CSeqTimer::Setup()
    "081692f0",  # CSeqTimer::Config()
    "08169300",  # CSeqTimer::Start()
    "0814cea0",  # CMessagePort::CMessagePort()
    "0814b6c0",  # CMessagePort::Setup()
    "0814b6d0",  # CMessagePort::Config()
    "0814b6e0",  # CMessagePort::Start()

    # Stage 6 breadth sweep, 2026-07-25 follow-up batch (heap.h/chunk_man.h/
    # timer_engine.h/seq_timer.h): CHeap (ctor/dtor only -- ~10 further Insert/
    # RemoveHead/MoveUp/MoveDown/etc methods stay pending, no traced caller) ->
    # CChkCmdBG promoted to real; CWheelsContainer/CExternalClock/CInternalClock
    # (ctor/dtor only -- ~24 further CTimerEngine wheel/sync methods stay
    # pending) -> CTimerEngine promoted to real.
    "0809ccf0",  # CHeap::CHeap()
    "0809cda0",  # CHeap::CHeap(int, int)
    "0809cec0",  # CHeap::~CHeap()
    "080c1380",  # CChkCmdBG::CChkCmdBG(CModule const&)
    "080c11b0",  # CChkCmdBG::~CChkCmdBG() (non-deleting)
    "0816ddd0",  # CWheelsContainer::CWheelsContainer()
    "0816de20",  # CWheelsContainer::~CWheelsContainer()
    "0816d070",  # CExternalClock::CExternalClock()
    "08195d30",  # CExternalClock::~CExternalClock() (non-deleting)
    "0816ec60",  # CInternalClock::CInternalClock()
    "08195e30",  # CInternalClock::~CInternalClock() (non-deleting)
    "0816bf40",  # CTimerEngine::CTimerEngine(CModule const&)
    "0816be00",  # CTimerEngine::~CTimerEngine() (non-deleting)

    # CFileMan/CResMan ctor batch (Stage 6 breadth sweep, 2026-07-25 -- the "What's
    # still open" CFileMan/CResMan ctor batch, file_man.h/res_man.h/
    # chunk_on_demand.h/cz_util.h). Setup()/Config() (CResMan) and ~60/~50 further
    # CFileMan/CResMan methods (FDisk/RegisterDriver/Save/...) remain Tier-B/pending
    # -- genuine god-object depth, out of scope for this pass.
    "081011e0",  # CFileMan::CFileMan()
    "080fc710",  # CFileMan::Setup()
    "080fc720",  # CFileMan::Config()
    "080fc730",  # CFileMan::Start()
    "080fc740",  # CFileMan::IsBackgroundJobsEnabled()
    "080fc750",  # CFileMan::EnableBackgroundJobs(int)
    "081523a0",  # CResMan::CResMan()
    "08151300",  # CResMan::Start()
    "0814d080",  # CChunkOnDemand::CChunkOnDemand()
    "080be680",  # CZ::StrCmpIgnoreCase(char const*, char const*)
    "080a7080",  # COmegaPtrArray::RemoveAll(int)

    # CSysApiInstance::RegisterApi() batch (Stage 6 breadth sweep, 2026-07-25) --
    # promoted from an empty Tier-B link-stub to a real, disassembly-verified body.
    # Confirmed boot-path-reachable: 7 real mains.cpp call sites (MMainEditMan/
    # MMainSeqTimer/MMainSysEx/MMainChunkMan/MMainRTRouter/MMainDumpMan/MMainResMan),
    # and the real target of Api's own vtable slot +0xa4. See sysapi_instance.h.
    "0806bab0",  # CSysApiInstance::RegisterApi(char const*, CApiBase*)

    # CEditor dedicated batch (Stage 6 breadth sweep, 2026-07-25 -- editor.h/.cpp).
    # CEditor's own 15 direct methods + ctor/dtor, all real (own control flow
    # faithfully reconstructed even where a callee is itself a Tier-B stub, same
    # convention as CModule::Add()'s own Api-vtable-slot calls elsewhere in this
    # list). CEditor::CMainTask's ctor (0824ad90) stays OUT of this list -- only
    # partially reconstructed (its real CTask::CTask() base-construction statement,
    # not the Peg/CDesktop tail). CEditor::CPanelIfcTask's own ctor (0824b7e0) is
    # now FULLY reconstructed (see the dedicated CPanelIfcTask batch below) --
    # promoted out of this "stays pending" note.
    "08249cd0",  # CEditor::CEditor(char const*, char const*)
    "082498f0",  # CEditor::~CEditor() [D1]
    "082498a0",  # CEditor::Config()
    "082498b0",  # CEditor::Start()
    "08249b60",  # CEditor::Setup()
    "08249df0",  # CEditor::SetLEDStatus(ELedCode, CPanelCfg::ELedState)
    "08249e30",  # CEditor::SetLEDStatus(int, unsigned short, unsigned short)
    "08249e80",  # CEditor::SetLEDStatus(CPanelCfg::ELedState)
    "08249eb0",  # CEditor::ShortBeep()
    "08249ee0",  # CEditor::ShortBeepPolite()
    "08249f20",  # CEditor::EnterDiagnostics(int)
    "08249f50",  # CEditor::IsSwitchPressed(EButtonCode)
    "08249f60",  # CEditor::IsShowCost()
    "08249f70",  # CEditor::EnterCheckHardware(int)
    "08249f80",  # CEditor::StopScreenRefresh()
    "0824af80",  # CEditor::CMainTask::IsSwitchPressed(EButtonCode) -- fully real,
                 # zero Peg dependency (pure bitmask test)
    "0824afc0",  # CEditor::CMainTask::IsShowCost() -- fully real, pure global read

    # CParameterString (editor.h's own dependency, parameter_string.h/.cpp) --
    # fully self-contained "key=value,..." parser, entirely real, no deferred
    # parts.
    "080b8ac0",  # CParameterString::CParameterString(char const*)
    "080b8ed0",  # CParameterString::~CParameterString()
    "080b8fc0",  # CParameterString::GetParamStr(char const*)
    "080b9020",  # CParameterString::DecToInt(char const*&)
    "080b90f0",  # CParameterString::HexToInt(char const*&)

    # CEditable + CAlphaKeybIfcTask (Stage 6 breadth sweep, 2026-07-25 --
    # editable.h/.cpp, alpha_keyb_ifc_task.h/.cpp). CAlphaKeybIfcTask is one of
    # CEditor::Setup()'s "ALPHAKEYBOARD=Yes" fan-out targets, previously flagged
    # "NOT tractable" by the dedicated CEditor batch above -- re-investigated and
    # found genuinely tractable (CEditable is a real, top-level, non-nested class,
    # not part of the CEditor namespace). Reconstructed standalone this pass;
    # WIRED into editor.cpp's Setup()/Start() by a later pass (2026-07-26,
    # eva_alphakeyb_ifc_wiring or similar note -- see README) once
    # CConfigManager::CreateUserModules() was confirmed live (config_info.cpp's
    # real "EditorClass" row passes literal "ALPHAKEYBOARD=Yes", so this branch
    # is genuinely taken on the real boot path, not a dead condition). ProcessCode
    # (08245960, 963 bytes) stays a Tier-B stub (genuine per-keycode dispatch
    # depth, same bar as CEditor::CMainTask::Exec()).
    "0806e310",  # CEditable::CEditable(CEditServer*)
    "0806e320",  # CEditable::AddDescriptorsMap(CObjectBase*, SDescriptor*, bool)
    "08245e10",  # CAlphaKeybIfcTask::CAlphaKeybIfcTask(CEditor const&)
    "08245d40",  # CAlphaKeybIfcTask::~CAlphaKeybIfcTask() [D1]
    "08245eb0",  # CAlphaKeybIfcTask::Setup() -- literal 1-byte `ret`, wired into CEditor::Start()

    # CEditor::CPanelIfcTask dedicated batch (Stage 6, 2026-07-25 --
    # panel_ifc_task.h/.cpp), following the CEditor batch's own "finding for a
    # future dedicated pass" note above. Full ctor (was a partial CTask::CTask()-
    # only link-stub) + every instance method that routes through the now-2-overload
    # COutLinkMono::OutMono() or the (Peg-toolkit-gap, inert-stand-in)
    # PegMessageQueue::Push(). Verified via verify/test_panel_ifc_task.cpp (43
    # checks). CPanelCfg (out_link.h-adjacent, panel_ifc_task.h) is a real,
    # nm-C-confirmed class name -- its own ctor has no separate symbol (inlined
    # into CPanelIfcTask's own ctor by GCC), only its D1 dtor is separately
    # addressable.
    "0824b7e0",  # CEditor::CPanelIfcTask::CPanelIfcTask(CEditor const&, PegScreen*)
    "0824b3b0",  # CEditor::CPanelIfcTask::~CPanelIfcTask() [D1]
    "0824c270",  # CEditor::CPanelIfcTask::SetLEDStatus(ELedCode, CPanelCfg::ELedState)
    "0824c2c0",  # CEditor::CPanelIfcTask::SetLEDStatus(int, unsigned short, unsigned short)
    "0824c310",  # CEditor::CPanelIfcTask::SetLEDStatus(CPanelCfg::ELedState)
    "0824c860",  # CEditor::CPanelIfcTask::ShortBeep()
    "0824c890",  # CEditor::CPanelIfcTask::EnterDiagnostics(int)
    "0824b980",  # CEditor::CPanelIfcTask::SetupPanelInterface()
    "0824c8d0",  # CEditor::CPanelIfcTask::SetAllLED(int)
    "0824ba20",  # CEditor::CPanelIfcTask::OnTouchPanelEvent(CPanelOut::STouchPanelEvt const*)
    "0824bbd0",  # CEditor::CPanelIfcTask::OnButtonEvent(CPanelOut::SButtonEvt const*)
    "0824b440",  # CEditor::CPanelIfcTask::Exec() [0-arg override, real vtable slot 2]
    "0824c000",  # CEditor::CPanelIfcTask::Exec(CMessage&) [1-arg override, real vtable slot 3]
    "0807d330",  # COutLinkMono::OutMono(unsigned short, unsigned long) -- 2nd real overload
    "0898fbb0",  # CPanelCfg::~CPanelCfg() [D1/D2, comdat-merged]
    "0824be00",  # CEditor::CPanelIfcTask::OnAnalogEvent(CPanelOut::SAnalogEvt const*) -- Stage 6 breadth sweep, 2026-07-25
    "0824bdb0",  # CEditor::CPanelIfcTask::OnEncoderEvent(CPanelOut::SEncoderEvt const*) -- Stage 6 breadth sweep, 2026-07-25

    # --- Stage 6 breadth sweep, 2026-07-26: nm -C sweep found a stale/wrong header
    # comment on an already-Tier-B CEvBuffersPool method -- re-verified against
    # objdump -dr directly and promoted to Tier A.
    "0807f040",  # CEvBuffersPool::PostKernelDestructor(unsigned long)

    # --- 2026-07-26: CTask::RegisterIfc() finally unblocked -- its own
    # TVector<SRegisteredIfc,1>::MakeCapacity() dependency (the ONLY thing keeping it
    # Tier B) transcribed directly from objdump -dr, the first real TVector<T,1>::
    # MakeCapacity() anywhere in this project. See task.h header comment.
    "0807ec90",  # CTask::RegisterIfc(CIfcUnknown*)
    "08182220",  # TVector<CTask::SRegisteredIfc,1>::MakeCapacity(unsigned int)

    # --- 2026-07-26: CEditor::Setup()'s fan-out re-survey (following the
    # CreateUserModules() unlock) -- CChunkServer (chunk_server.h) + CEditor::
    # CChunkServerTask (editor.h), the LAST previously-deferred fan-out target,
    # wired in UNCONDITIONALLY (not gated, unlike CAlphaKeybIfcTask). Exec
    # (CMessage&) (080cc0d0) stays Tier B -- see chunk_server.h. Load()
    # (080cbfd0) was ALSO originally left Tier B here but promoted later the
    # same day -- see the standalone entry further below.
    "080cbcf0",  # CChunkServer::CChunkServer(CModule const&, EAccessMode)
    "080cbb90",  # CChunkServer::~CChunkServer() [D1]
    "080cba90",  # CChunkServer::OnUnlock(...)
    "080cbaa0",  # CChunkServer::OnRelock(...)
    "080cbab0",  # CChunkServer::OnBegin(...)
    "080cbac0",  # CChunkServer::OnEnd(...)
    "080cbad0",  # CChunkServer::OnSave(CChunk*, unsigned char, unsigned char*, unsigned long)
    "080cbae0",  # CChunkServer::OnSave(unsigned long&, unsigned char const*&, ...)
    "080cbaf0",  # CChunkServer::OnLoad(CChunk*, unsigned char, unsigned char*, unsigned long)
    "080cbb00",  # CChunkServer::OnLoad(unsigned long, unsigned char*, ...)
    "080cbb10",  # CChunkServer::OnAbort(ECommand)
    "080cbb20",  # CChunkServer::OnStoppedByUser(ECommand)
    "080cbb30",  # CChunkServer::GetSaveBuffSize(unsigned char, unsigned char, unsigned char) const
    "080cbdc0",  # CChunkServer::GetServerID(int) const
    "080cbdf0",  # CChunkServer::Unlock(unsigned char, unsigned char, unsigned char*)
    "080cbe30",  # CChunkServer::GetServerHandle(unsigned char) const
    "08245f50",  # CEditor::CChunkServerTask::CChunkServerTask(CModule const&)
    "0898f6a0",  # CEditor::CChunkServerTask::~CChunkServerTask() [D1]
    "08245ec0",  # CEditor::CChunkServerTask::OnSave(CChunk*, ...)
    "08245ed0",  # CEditor::CChunkServerTask::GetSaveBuffSize(...) const

    # --- 2026-07-26 (later same day): CChunkServer::Load() promoted Tier B ->
    # Tier A. Ghidra's own decompiler failed on it ("Could not recover
    # jumptable"); recovered directly from objdump -dr register tracing
    # instead (same technique as CTask::RegisterIfc()'s own MakeCapacity()
    # promotion above). See chunk_server.h header comment for the full
    # mAccessMode-keyed tail-call-through-own-vtable derivation.
    "080cbfd0",  # CChunkServer::Load(CChunk*, unsigned long, unsigned char*, unsigned char, unsigned char*, unsigned long)

    # --- 2026-07-26 (later same day): CPoller reassessed with fresh eyes now
    # that CTask::SetMask() is real -- the ctor is fully tractable, no genuine
    # Peg-toolkit blocker (that label was always wrong, see poller.h header
    # comment). 4 of 29 methods promoted this batch (ctor/dtor/3 const
    # accessors) plus the CIfcClient nested class (ctor/PutAnalogEvt/
    # FlushAnalogEvts) and its own TVector<CIfcClient*,1>::MakeCapacity(). The
    # remaining ~20 Msg*/RegisterClient/InitAnalogs/InitButtons/Exec() methods
    # stay deferred for real reasons (size, and a genuinely separate CMessage
    # prerequisite this project hasn't reconstructed at all yet) -- see
    # poller.h's own header comment for the full list and reasoning.
    "089ef740",  # CPoller::CPoller(CModule const&, char const*)
    "089ef490",  # CPoller::~CPoller() [D1]
    "089f3000",  # CPoller::FindUnconnected() const
    "089f3150",  # CPoller::IsValidHandle(unsigned int) const
    "089f3180",  # CPoller::IsRegisteredHandle(unsigned int) const
    "089ef620",  # CPoller::CIfcClient::CIfcClient(CTask const&, char const*, int)
    "089ef670",  # CPoller::CIfcClient::PutAnalogEvt(CPanelOut::SAnalogEvt const&)
    "089ef6f0",  # CPoller::CIfcClient::FlushAnalogEvts()
    "089f7280",  # TVector<CPoller::CIfcClient*,1>::MakeCapacity(unsigned int)

    # --- 2026-07-26 (later same day): CPanel reconstructed (panel.h/.cpp) --
    # closes the gap poller.h's own header comment flagged (CPanel::Setup() is
    # CPoller's real, definitive ground-truth constructor call site).
    # CPanelConstructor::CPanelConstructor() (089ee3b0) is NOT included -- confirmed
    # unreachable (MMainPanel() builds its own descriptor inline, never calls it).
    "089ee340",  # CPanelConstructor::Create(char const*, char const*, int)
    "089ee780",  # CPanel::CPanel(char const*, char const*)
    "089ee6e0",  # CPanel::Setup()
    "089ee530",  # CPanel::Config()
    "089ee520",  # CPanel::Start()
    "089ee560",  # CPanel::~CPanel() [D0, deleting]
    "089ee620",  # CPanel::~CPanel() [D1]

    # --- 2026-07-26 (broad nm-C sweep, third batch): re-examined the "~20 Msg*
    # methods need a genuinely separate CMessage prerequisite" framing directly
    # against ground truth. Wrong for 8 of the smaller ones -- this project's own
    # established CMessage-forward-declared-incomplete-type convention
    # (CChunkServer::Exec()/CSysExMsgTaskBase::Exec()) applies here just as well,
    # nobody had actually tried it on CPoller's own smaller handlers yet. See
    # poller.h's own header comment (2026-07-26 UPDATE) for the full per-method
    # derivation. RegisterClient() itself (089f31c0) stayed Tier-B stubbed at the
    # time this comment was written -- see the 2026-07-26 RegisterClient batch
    # entry below, now fully reconstructed.
    "089f0150",  # CPoller::MsgShortBeep(CMessage&)
    "089f0420",  # CPoller::MsgRequestAnalogInputValue(CMessage&) const
    "089f1990",  # CPoller::MsgUnregisterClient(CMessage&)
    "089f2010",  # CPoller::MsgSetEncoderClient(CMessage&)
    "089f2090",  # CPoller::MsgSetTouchPanelClient(CMessage&)
    "089f2110",  # CPoller::MsgSetKeyboardClient(CMessage&)
    "089f53f0",  # CPoller::MsgRegisterClientByVal(CMessage&)
    "089f5470",  # CPoller::MsgRegisterClientByRef(CMessage&)

    # --- 2026-07-26 (RegisterClient reconstruction batch): CPoller::RegisterClient()
    # itself, the common real callee of the two MsgRegisterClientByXxx() wrappers
    # above -- see poller.h's own header comment for the full derivation (2603 bytes,
    # objdump -dr -M intel register tracing, 2 deliberate simplifications, 1 opaque
    # CLink-family pointer chain).
    "089f31c0",  # CPoller::RegisterClient(unsigned int&, char const*, char const*)

    # --- 2026-07-26 (later same day): CBatchDiskMan unlock batch (batch_disk_man.h/
    # .cpp, edit_task.h/.cpp) -- same "CModuleConstructor factory currently stubbed,
    # real per-module class not yet reconstructed" unlock shape as CPanel above,
    # closing the gap [[eva_mopup_sweep_2026-07-26_negative]] re-verified and
    # correctly deferred (CBatchDiskMan::Setup() constructs 2 brand-new classes).
    # CEditTask is fully real (every real dependency -- CTask/CEditable/COutLinkMono
    # -- already reconstructed). CBatchDiskMainTask (batch_disk_main_task.h) was
    # ORIGINALLY a Tier-B substitute here -- UPDATE (2026-07-26, "size is not depth"
    # re-check batch): its real ctor/dtor are now ALSO real, see the dedicated
    # CBatchDiskMainTask block below. Its own heaviest methods (PreloadDir 2940B,
    # PreloadGroup 1148B, PrepareGroupsForPreload 1336B, AddItemToPreload 359B,
    # Exec(CMessage&) 703B) remain genuinely deferred (CZ/CRMJob-driven business
    # logic, CChunkServer/CTimerEngine-scale) -- NOT included below.
    "08243d80",  # CBatchDiskManConstructor::Create(char const*, char const*, int)
    "082436e0",  # CBatchDiskMan::CBatchDiskMan(char const*, char const*)
    "082435d0",  # CBatchDiskMan::Setup()
    "08243330",  # CBatchDiskMan::Config()
    "08243320",  # CBatchDiskMan::Start()
    "082437c0",  # CBatchDiskMan::IsBusy() const
    "082437e0",  # CBatchDiskMan::IsPreloadRunning() const
    "082433c0",  # CBatchDiskMan::~CBatchDiskMan() [D1]
    "082434c0",  # CBatchDiskMan::~CBatchDiskMan() [D0, deleting]
    "08243b80",  # CEditTask::CEditTask(CModule const&)
    "08243ac0",  # CEditTask::DoPreload()
    "08243ca0",  # CEditTask::GetOutLinkName() const
    "08243af0",  # CEditTask::~CEditTask() [D1]
    "08243b20",  # CEditTask::~CEditTask() [D0, deleting]

    # CAlphaKeybCtrl/CAlphaKeybCtrlTask (Eva CAlphaKeybCtrl/CAlphaKeybCtrlTask batch,
    # 2026-07-26). Closes the gap [[eva_mopup_sweep_2026-07-26_negative]] correctly
    # deferred as "same scale as the CChunkServer/CTimerEngine sub-object batches"
    # purely on CAlphaKeybCtrlTask::CAlphaKeybCtrlTask()'s own raw size (4289 bytes) --
    # re-investigated and found mechanically table-driven (15x inlined "build one
    # CKeyboardLayout" sequence, see keyboard_layout.h), not algorithmically deep. The
    # genuinely deep, out-of-scope piece (COutLinkIfcBase/COutLinkIfc<T>/
    # CMarshaller<T>) turned out to be much smaller and off to the side -- see
    # alpha_keyb_ctrl_task.h.
    "0823e750",  # CAlphaKeybCtrl::CAlphaKeybCtrl(char const*, char const*)
    "0823e4a0",  # CAlphaKeybCtrl::~CAlphaKeybCtrl() [D1]
    "0823e560",  # CAlphaKeybCtrl::~CAlphaKeybCtrl() [D0, deleting]
    "0823e620",  # CAlphaKeybCtrl::Setup()
    "0823e470",  # CAlphaKeybCtrl::Config()
    "0823e480",  # CAlphaKeybCtrl::Start()
    "0823e6c0",  # CAlphaKeybCtrlConstructor::Create(char const*, char const*, int)
    "0823f2a0",  # CAlphaKeybCtrlTask::CAlphaKeybCtrlTask(CModule const&, char const*)
    "0823e9d0",  # CAlphaKeybCtrlTask::~CAlphaKeybCtrlTask() [D1]
    "0823ef00",  # CAlphaKeybCtrlTask::~CAlphaKeybCtrlTask() [D0, deleting]
    "0823e930",  # CAlphaKeybCtrlTask::Exec()
    "0823ef50",  # CAlphaKeybCtrlTask::Initialize()
    "0823efc0",  # CAlphaKeybCtrlTask::SetCtrlCondition(unsigned char, bool)
    "0823f0f0",  # CAlphaKeybCtrlTask::ProcessEvent(IAlphaKeybEvent::SKeyboardEvt&)
    # COutLinkIfcBase::GetDirectIfcPtr() -- fully reconstructed for real as a free
    # function (alpha_keyb_ctrl_task.cpp's own GetDirectIfcPtr()), since it only
    # touches the mCodeIfc sub-object's own self-contained +0x40/+0x44 fields.
    "0807b8e0",  # COutLinkIfcBase::GetDirectIfcPtr(COutLinkIfcBase::CLevelLocker&) const
    "0823efb0",  # CAlphaKeybCtrlTask::SetRateDelay() -- "size is not depth" re-check
                 # batch, 2026-07-26: 6-byte trivial `return 1;`, unrelated to the
                 # CMarshaller framework, simply missed by the original unlock batch.

    # --- 2026-07-26 (Eva "size is not depth" re-check batch): CBatchDiskMainTask's
    # own real ctor/dtor, previously a Tier-B substitute (see batch_disk_main_task.h's
    # own header comment for the full re-derivation). Re-traced via objdump -dr -M
    # intel with the specific question "is the ctor's OWN logic tractable if CZ stays
    # opaque" -- yes: every instruction is a subobject ctor call or a literal field
    # store, no algorithmic depth of its own. Pulled in 4 small, equally-mechanical
    # dependency classes (CRMJob, CRMApiCallBack, CDirEntry, COutLinkMulti) to make
    # this real without touching CZ's own internals. Corrected a prior claim (this
    # class is genuine CTask+CEditable+CRMApiCallBack triple inheritance, not
    # CTask+CRMApiCallBack with CEditable misattributed to CTask's own +8 mIfcThunk
    # slot -- see header comment). PrepareGroupsForPreload()/PreloadDir()/
    # PreloadGroup()/AddItemToPreload()/Exec(CMessage&) remain genuinely deferred
    # (the real CZ/CRMJob-driven business logic itself).
    "08241920",  # CBatchDiskMainTask::CBatchDiskMainTask(CModule const&, char const*)
    "08241040",  # CBatchDiskMainTask::~CBatchDiskMainTask() [D1]
    "082411d0",  # CBatchDiskMainTask::~CBatchDiskMainTask() [D0, deleting]
    "08241230",  # CBatchDiskMainTask::IsBusy() const
    "08241250",  # CBatchDiskMainTask::IsPreloadRunning() const
    "081660d0",  # CRMJob::CRMJob()
    "08166180",  # CRMJob::~CRMJob() [D1]
    "08071640",  # CDirEntry::CDirEntry()
    "08071540",  # CDirEntry::~CDirEntry() [D1]
    "080715c0",  # CDirEntry::~CDirEntry() [D0, deleting]
    "0807d620",  # COutLinkMulti::COutLinkMulti(CTask const&, char const*, COutLink::EDirection, unsigned short)
    "0818fa20",  # CRMApiCallBack::~CRMApiCallBack() [D1]
    "0818f1f0",  # CRMApiCallBack::OnSetRes(CRMApiCallBack::ERMResult) -- confirmed empty
    "0818f200",  # CRMApiCallBack::OnLoadRes(CRMApiCallBack::ERMResult) -- confirmed empty
    "0818f210",  # CRMApiCallBack::OnLoad(CRMApiCallBack::ERMResult) -- confirmed empty
    "0818f220",  # CRMApiCallBack::OnSave(CRMApiCallBack::ERMResult) -- confirmed empty
    "0818f230",  # CRMApiCallBack::OnDelete(CRMApiCallBack::ERMResult) -- confirmed empty

    # --- 2026-07-26 (Eva "size is not depth" 4-class re-check follow-up, same batch
    # as CRMJob/CDirEntry/COutLinkMulti above): CRMJob::CRMJob() being made real
    # unblocked two ALREADY-counted addresses that had stale "malloc but do not
    # construct" Tier-B annotations -- no new addresses added here, just fidelity
    # fixes to entries already in this set:
    #   "081523a0" CResMan::CResMan() -- mJob is now placement-constructed via the
    #     real CRMJob::CRMJob() (confirmed via direct disasm: malloc@plt immediately
    #     followed by call CRMJob::CRMJob()), not a raw untyped malloc'd blob.
    #   "08165f70" global.constructors.keyed.to.RMApiInstance -- same fix, same
    #     confirmed disasm pattern, for its own separate CRMJob instance.
    # Checked (via ghidra full disasm + symbols.csv) whether CDirEntry's other ~38
    # methods, CRMJob::Reset()/CopyObject()/operator=/copy-ctor, or COutLinkMulti's
    # CheckDestinationFamily/OnConnect/OnDisconnect/OnCreateLink/OutMulti(x4) had any
    # newly-tractable real caller -- confirmed ZERO direct call sites anywhere in the
    # 22MB binary for all of the COutLinkMulti/CRMJob candidates (only reachable via
    # TVector<CRMJob,1>-driven CJobStack machinery or an un-traced vtable dispatch,
    # both already-documented out-of-scope dependencies); CDirEntry's one in-project
    # caller (CBatchDiskMainTask::PreloadDir(), 0xc3c/3132 bytes) is itself still
    # Tier-B and disproportionately large, so none of its sibling accessors were
    # promoted either. See LESSON_vtable_dispatch_stub_gap.md's own re-check.

    # --- 2026-07-26 (CLEDBlinker/CPoller final-prerequisites follow-up batch):
    # new CLEDBlinker class (led_blinker.h/.cpp) -- the smallest "whole new class"
    # unlock in this project so far (6 methods, no vtable) -- reconstructed to close
    # CPoller's own last 3 deferred small handlers (see poller.h's own UPDATE note).
    "089ee1d0",  # CLEDBlinker::CLEDBlinker()
    "089ee200",  # CLEDBlinker::~CLEDBlinker()
    "089ee210",  # CLEDBlinker::Register(ELedCode)
    "089ee270",  # CLEDBlinker::Unregister(ELedCode)
    "089ee2c0",  # CLEDBlinker::Unregister(int, unsigned short)
    "089ee300",  # CLEDBlinker::Exec()
    "089eff00",  # CPoller::MsgSetLed(CMessage&)
    "089f0070",  # CPoller::MsgSetLed16bits(CMessage&)
    "089f01a0",  # CPoller::MsgBackupLEDs(CMessage&)

    # --- 2026-07-26 (same session, FindRegisteredClient batch): the concrete next
    # candidate the CLEDBlinker batch's own closing note flagged -- see poller.h's
    # own per-method header comments. Both Msg*() wrappers' large raw byte counts
    # (2601B/2590B) turned out to be ground truth's own inlined DUPLICATE of the
    # scan, not real depth -- modeled as real calls to FindRegisteredClient() instead.
    "089f25e0",  # CPoller::FindRegisteredClient(char const*, char const*) const
    "089f0470",  # CPoller::MsgGetClientHandleByRef(CMessage&) const
    "089f0f00",  # CPoller::MsgGetClientHandleByVal(CMessage&) const

    # --- 2026-07-26 (CLocaleManager closeout batch): CLocaleManager's own ctor/
    # GetInstance()/wrappers were already Tier A (alpha_keyb_ctrl_task.h's own
    # CLocaleManager writeup, same-day earlier batch) but never actually added to
    # this manifest -- correcting that gap here. AddLayout()/GetLayout(EKeyboardLayout)
    # (previously "genuine algorithmic depth", locale_manager.h) turned out to be the
    # same "size is not depth" misjudgment this project keeps re-finding -- a plain
    # TVector<CKeyboardLayout const*,1> push_back-with-grow and linear scan,
    # reconstructed for real this batch (locale_manager.cpp). GetLayout(char const*)
    # (08079f60) stays out of scope -- no real caller anywhere in this project.
    "08079ab0",  # CLocaleManager::CLocaleManager()
    "08079ad0",  # CLocaleManager::GetInstance()
    "08079b40",  # CLocaleManager::AddKeyboardLayout(CKeyboardLayout const*)
    "08079b50",  # CLocaleManager::GetKeyboardLayout(EKeyboardLayout) const
    "08079ba0",  # CKeyboardLayoutManager::AddLayout(CKeyboardLayout const*)
    "08079e30",  # CKeyboardLayoutManager::GetLayout(EKeyboardLayout) const

    # --- 2026-07-26 (CPoller closeout batch): the last 2 genuinely-deferred small
    # handlers named by the FindRegisteredClient batch's own closing note. Direct
    # `objdump -dr -M intel` register tracing confirmed which of mHandleTable1/
    # mHandleTable2 is the ANALOG vs. BUTTON table (poller.h's own field comments
    # previously guessed both the wrong way around -- corrected this batch), plus
    # both functions' own real .rodata lookup tables (verbatim byte dump, see
    # poller.cpp's s_analogCode[]/s_buttonPrimaryCode[]/s_buttonAltCode[]). CPoller's
    # own remaining surface after this: both Exec() overloads only (see poller.h's
    # own header comment for why those stay out of scope this batch -- Exec() is a
    # real 12-way jump-table hardware-event-drain loop, all-real callees but
    # RegisterClient()-scale depth; Exec(CMessage&) is a MUCH larger, ~6747-byte
    # name-string command dispatcher with 94 strcmp() call sites, genuinely a
    # separate, dedicated-batch-scale reconstruction).
    "089f2190",  # CPoller::MsgSetAnalogClient(CMessage&)
    "089f1a10",  # CPoller::MsgSetButtonClient(CMessage&)

    # --- 2026-07-26 (nm -C sweep, mains.cpp/config_manager.cpp XxxApiInstance setter
    # batch): 3 small Tier-B empty-body link-stubs promoted to real bodies, found by
    # grepping this project's own "Tier B"/"Tier-B" doc comments for a still-open,
    # tractable name. All 3 use the same real Api+0x94 soft-assert idiom already
    # established in tempo.cpp/chunk_man.cpp/chunk_server.cpp/task.cpp; neither of
    # CChkApiInstance/CRMApiInstance/CEditApiInstance is (or needs to become) a full
    # reconstructed class -- each stays a locally-scoped opaque-buffer shim, same as
    # before, only these 3 specific methods became real. Fixed a real, previously
    # latent buffer-undersizing bug this promotion exposed: mains.cpp's own
    # `ChkApiInstance[4]` static byte buffer was one real field (4 bytes) too small
    # for SetOwnerModule()'s own self+0x4 write once that write became real; bumped to
    # 8 bytes (see mains.cpp's own comment). See verify/test_mains_api_setters.cpp.
    "080bfcd0",  # CChkApiInstance::SetOwnerModule(CChunkMan*)
    "081655e0",  # CRMApiInstance::SetResMan(CResMan*)
    "080d23e0",  # CEditApiInstance::AssignScope(char const*, unsigned char)

    # --- 2026-07-26 (Exec() 0-arg batch): the 12-way jump-table hardware-event-drain
    # loop named by the CPoller closeout batch's own note above. Direct `objdump -dr
    # -M intel` register tracing of the whole 3213-byte body plus a real `objdump -s
    # -j .rodata` byte dump of the 12-entry jump table (.rodata+0x08f7c268). Modeled
    # the type-11 ANALOG ring-push and the second-pass client flush as real calls to
    # the already-reconstructed CIfcClient::PutAnalogEvt()/FlushAnalogEvts() instead
    # of re-inlining ground truth's own byte-identical duplicates of that same logic
    # (same "duplicate real ground-truth function per call site, modeled as a call
    # instead" precedent FindRegisteredClient()'s own wrappers already established).
    # CPoller's only remaining genuinely-deferred surface after this is
    # Exec(CMessage&) (6747B, the real per-message string-command dispatcher, ~94
    # strcmp() call sites -- a completely separate mechanism, out of scope here).
    "089ee7d0",  # CPoller::Exec()

    # --- 2026-07-26 (Exec(CMessage&) closeout batch): the OTHER Exec() overload named
    # just above -- a prior pass's "~94 strcmp() sites, not a numeric switch" reading
    # was a real misdiagnosis, corrected via a full `objdump -dr -M intel` branch-
    # target CFG reachability walk. There IS a real 15-way jump table (.rodata+
    # 0x8f7c298) on the LOW BYTE of CMessage's own +0x8 word; every one of its 15
    # cases turned out to be ground truth's own inlined duplicate (3 of them: a real
    # direct call) of one of the 15 already-real Msg*() sibling methods above --
    # modeled as real calls to those siblings (same "duplicate real ground-truth
    # function per call site" precedent used throughout this file), collapsing the
    # two ~700-instruction duplicated-FindRegisteredClient()-scan cases (the true
    # source of the ~94 strcmp() count) down to one-line wrappers each. This closes
    # CPoller fully -- no remaining deferred surface of its own.
    "089f54f0",  # CPoller::Exec(CMessage&)

    # --- 2026-07-26 (final-closeout batch): CORRECTS the "closes CPoller fully"
    # claim immediately above -- InitButtons()/InitAnalogs() were still Tier-B
    # link-stubs at that point ("needs CMessage machinery", poller.h's own prior
    # note), a real remaining gap. Same misdiagnosis class as Exec(CMessage&)'s
    # own prior "~94 strcmp() sites" note: the 2925B/2919B size is GCC re-inlining
    # RegisterClient()'s own already-real Phase-1 scan, not algorithmic depth. Net
    # effect: a plain loop over a real, byte-dumped .rodata name-pair table
    # calling the already-real RegisterClient() sibling. Both now Tier A -- see
    # poller.h's own per-method header comments for the full derivation. CPoller
    # now has genuinely ZERO remaining deferred surface (verified: CPanel, its
    # own real, definitive ground-truth caller, panel.h, is fully real too).
    "089f4830",  # CPoller::InitButtons()
    "089f3c80",  # CPoller::InitAnalogs()

    # --- Bookkeeping fix (2026-07-26): backfills 3 prior commits (4fd12a5, 5960feb,
    # e11eaa4) that reconstructed real functions but never updated this manifest.

    # 4fd12a5 -- CDirEntry's 9 non-virtual predicate/accessor methods +
    # HasValidLongNameExt() (the real vtable-slot-2 target GetName()/GetExt()
    # dispatch through). See include/dir_entry.h.
    "08072660",  # CDirEntry::IsEmpty() const
    "08072670",  # CDirEntry::IsDeleted() const
    "08072680",  # CDirEntry::IsReserved() const
    "08072650",  # CDirEntry::IsLabel() const
    "08072640",  # CDirEntry::IsDir() const
    "080726d0",  # CDirEntry::IsParentDir() const
    "080726f0",  # CDirEntry::IsCurrentDir() const
    "08071500",  # CDirEntry::HasValidLongNameExt() const (real vtable slot 2 target)
    "080723b0",  # CDirEntry::GetName() const
    "080723e0",  # CDirEntry::GetExt() const

    # 5960feb -- broad Tier-B recheck sweep, 6 small forwarders/setters promoted
    # Tier B -> Tier A. (CScheduler::Enable's own address, 08063120, was already
    # tracked above -- only its return-type fidelity changed, not its address.)
    "080631c0",  # CScheduler::EnableUpdate(int)
    "0806b3a0",  # CSysApiInstance::EnableMultiTask(int)
    "0806aa00",  # CSysApiInstance::WriteMessageToHost(int, int)
    "0805afb0",  # CErrorHandler::EnableUpdate(int)
    "0806ce80",  # CTracer::EnableUpdate(int) (ckernel.cpp file-local class)
    "0804d200",  # HAL_GetSystemTime()

    # e11eaa4 -- CNotifyList, a whole previously-unmodeled small class (7 named
    # methods, 9 addresses since both D1/D0 dtors are separate symbols, same
    # convention as every other class in this manifest). See include/notify_list.h.
    "08070c40",  # CNotifyList::CNotifyList()
    "08070b40",  # CNotifyList::~CNotifyList() [D1]
    "08070b60",  # CNotifyList::~CNotifyList() [D0, deleting]
    "08071000",  # CNotifyList::Put(unsigned char, unsigned char, unsigned char)
    "080710e0",  # CNotifyList::GetList()
    "08071120",  # CNotifyList::ReleaseList(SNotifyEvent*, SNotifyEvent*)
    "08071160",  # CNotifyList::ReleaseList(SNotifyEvent*)
    "08070e60",  # CNotifyList::GrowEventsList()
    "08070bb0",  # CNotifyList::PostKernelDestructor(unsigned long)

    # --- Tier A, batch 8 (2026-07-27): CSTGUnsolMsgHandler::VoiceModelMsgHandler,
    # promoted from Tier B. Both real jump tables (17 entries @0x08f1bac0, 6 entries
    # @0x08f1bb04) fully case-traced via `objdump -dr -M intel`; ~90% mechanical
    # EditApi/SetWithoutUpdatingSTG dispatch, with exactly ONE genuine deep leaf (a
    # CStorage::GetInstance()-based "MOSS algorithm" voice-model-database dispatch,
    # real call site 0x08917209) left as a precisely-scoped Tier-B stub,
    # VoiceModelMossAlgorithmDispatch() -- not a separately manifested address, it has
    # no real symbol of its own (inlined case logic in the real binary, not a
    # separate function). See stg_unsol_msg_handler.h/.cpp's own header comments for
    # the full case-by-case reconstruction.
    "08917100",  # CSTGUnsolMsgHandler::VoiceModelMsgHandler

    # --- WORKAROUND #4 (mains.cpp), vtable-dispatch-stub-gap sweep, 2026-07-27:
    # CRTRouterApiInstance/CRMApiInstance phase-hook overrides -- reconstructed at
    # the time but never added to this manifest. Backfilled here.
    "08085790",  # CRTRouterApiInstance::PreKernelConstructor(unsigned long)
    "08086330",  # CRTRouterApiInstance::PostKernelDestructor(unsigned long)
    "08164d80",  # CRMApiInstance::PostKernelDestructor(unsigned long)

    # --- CJobStack, construction/destruction-only re-check (Eva "size is not
    # depth", 2026-07-27): re-traced RMApiInstance's own remaining
    # PreKernelConstructor gap (08165490) specifically scoped to what it needs,
    # rather than restating the prior "genuinely deep" verdict. The class's own
    # 8 AddLoadRes/AddLoadFile/AddSave/AddDelete/AddSetRes/ExecutePendingCmds
    # job-queue methods stay unreconstructed/out of scope (genuinely deep,
    # unreachable business logic) -- NOT added here. Only the construction/
    # destruction path itself, which is small and self-contained, is manifested
    # as reconstructed. See include/job_stack.h.
    "08165490",  # CRMApiInstance::PreKernelConstructor(unsigned long)
    "0814da30",  # CJobStack::CJobStack()
    "0814d6c0",  # CJobStack::~CJobStack() [D1]
    "0814d870",  # CJobStack::~CJobStack() [D0, deleting]

    # --- CEditClient, construction/destruction-only re-check (Eva "size is not
    # depth", 2026-07-27, 3rd re-open of this exact class): re-traced fresh from
    # `objdump -dr -M intel` rather than restating the prior "needs a genuine
    # open-chaining hash-table + free-list-allocator template" verdict. That
    # verdict was right that the ctor installs 2 real `PointerHash<K,V>` template
    # instantiations (RTTI-confirmed: "PointerHash<CEditControl*, CEditControl>"
    # and "PointerHash<long, CEditControl>", Eva VA 0x8e81558/0x8e81568) but wrong
    # that this blocks the ctor/dtor -- a whole-binary xref sweep shows these are
    # PointerHash's ONLY 2 instantiations anywhere, both consumed only by this
    # ctor, and construction never calls into the template's own Add/Find/Node/
    # Iterator machinery (just 2x malloc+vtable-install+memset). CEditClient's 4
    # other real named methods (BlockRegister/Register/Unregister/NotifyControls)
    # and PointerHash<K,V>'s own methods stay unreconstructed/out of scope -- NOT
    # added here. Also promotes the 2 CEditApiInstance::RegisterClient()/
    # UnregisterClient() trampolines the ctor/dtor call through (confirmed via
    # xref sweep to have no other caller anywhere) -- modeled as free functions
    # `EditApiInstance_RegisterClient()`/`UnregisterClient()`, edit_man.cpp,
    # matching that file's existing `EditApiInstance_RegisterServer()` etc.
    # convention. See include/edit_man.h.
    "0806e470",  # CEditClient::CEditClient()
    "0806e3f0",  # CEditClient::~CEditClient() [D1]
    "080d1ea0",  # CEditApiInstance::RegisterClient(CEditClient const*)
    "080d1e70",  # CEditApiInstance::UnregisterClient(CEditClient const*)

    # --- ControlMsgHandler re-trace (Eva "size is not depth", 2026-07-27, applying
    # the same lens used above on CJobStack/CEditClient to the LAST remaining Tier-B
    # CSTGUnsolMsgHandler handler). Unlike those two, this one stayed MOSTLY Tier B:
    # a full jump/call-target-vs-owning-case-range audit of the real disassembly found
    # its 44-entry outer switch has been heavily cross-jumped/tail-merged by GCC into
    # one ~2KB interconnected CFG hub (physically table slot 20's own code,
    # 0x0891b890..0891c090, entered by ~20 of the 44 subcodes including both of
    # subcode 6's/7's own nested sub-jump-tables) -- reconstructing any of those
    # subcodes requires reconstructing the hub too, which is where essentially all 18
    # previously-documented out-of-scope subsystem calls live. Exactly 6 of the 44
    # outer subcodes (9, 10, 11, 16, 37, 38) are provably self-contained (zero
    # crossjump in either direction) and depend only on already-real targets -- made
    # real inline inside ControlMsgHandler() itself (not separate symbols, matching
    # VoiceModelMsgHandler's own inlined-case convention). NOT marking
    # 0x0891ac70 (ControlMsgHandler itself) reconstructed here -- 38 of its 44
    # subcodes are still Tier-B stubs, see stg_unsol_msg_handler.h/.cpp for the full
    # per-subcode evidence. The two NEW real leaf functions this pulled in:
    "08e1cad0",  # USTGAPIControl::BeginLongErPActivity()
    "08e1cb10",  # USTGAPIControl::EndLongErPActivity()

    # --- CLimiterBase / CWrProtCircularQueue (Eva "size is not depth" re-check
    # batch, 2026-07-27, applying the same lens to HARDWARE_REVIEW_LOG.md's
    # "COutLinkIfcBase/CMarshaller<T> framework" registry bullet, which claimed "no
    # concrete instantiation exists yet" -- already half-wrong (CAlphaKeybCtrlTask's
    # own mCodeIfc, alpha_keyb_ctrl_task.h) and this batch found a 2nd, independent
    # one: CLimiterBase::Init() builds a COutLinkIfc<ILimiterNotify>/
    # CMarshaller<ILimiterNotify> sub-object via the same idiom. CLimiterBase itself
    # has ZERO callers anywhere in the whole 22MB ground-truth binary (confirmed by
    # grepping every `call` target in `objdump -dr` output) -- reconstructed anyway
    # for structural completeness, same "small + self-contained, closes a
    # registry-flagged gap" precedent as CJobStack's ctor/dtor. Init(CTask&,
    # unsigned int)/Write()/PopMessage() and CWrProtCircularQueue::Write()/
    # StaticRead()/SeekNextRead() stay Tier B (genuinely intricate wraparound
    # message-framing state machine, or genuinely blocked on the un-reconstructed
    # COutLinkIfcBase ctor) -- see limiter_base.h for the full per-method writeup.
    "0807a360",  # CLimiterBase::CWrProtCircularQueue::CWrProtCircularQueue(int)
    "0807a1c0",  # CLimiterBase::CWrProtCircularQueue::~CWrProtCircularQueue (D1)
    "0807a2f0",  # CLimiterBase::CWrProtCircularQueue::~CWrProtCircularQueue (D0)
    "0807a3a0",  # CLimiterBase::CWrProtCircularQueue::Init(int)
    "0807a9c0",  # CLimiterBase::CWrProtCircularQueue::IsEmpty() const
    "0807aa30",  # CLimiterBase::CWrProtCircularQueue::CountIntegers(uint)
    "0807aa50",  # CLimiterBase::CLimiterBase(int, ETaskLevel, TMsgFn, TMsgFn)
    "0807a210",  # CLimiterBase::~CLimiterBase (D1)
    "0807a270",  # CLimiterBase::~CLimiterBase (D0)
    "0807af80",  # CLimiterBase::SendWithAnswer(uchar, void*, uint) -- real forward
    "0807afa0",  # CLimiterBase::SendNoAnswer(uchar, void const*, uint) -- real forward

    # --- CKGMsgProcessor ctor/dtor/GetInstance() (Eva deferred-registry re-check
    # batch, 2026-07-27, same "size is not depth" lens re-applied to
    # HARDWARE_REVIEW_LOG.md's own "CKGMsgProcessor" citation as one of
    # ControlMsgHandler's still-out-of-scope downstream subsystems -- that verdict
    # stays accurate for CKGMsgProcessor's own real message-processing methods
    # (SetGEMax/Process/CheckAndSet*/GetKarmaNotes/ClearInvalidNotesCCsDisplay, all
    # genuine Karma-note-generation logic, still NOT in this set), but its own
    # construction/destruction turned out small and fully self-contained: 9 mallocs
    # + fixed-offset field writes + one already-documented Api+0x9c vtable
    # dispatch (timer_engine.h's ApiGetDefault9c(), reused verbatim), no CZ/
    # CStorage/CMMI/CModeManager dependency of its own. See kg_msg_processor.h for
    # the full per-field writeup, including 2 real, faithfully-transcribed
    # ground-truth quirks: an uninitialized ctor field (+0x29) and a destructor
    # that frees its 7 polymorphic handler sub-objects but never its 2 plain data
    # buffers (+0x20/+0x24). No live caller of this real reconstruction on this
    # project's own traced boot path (GetInstance()'s one existing real caller,
    # ProgramSlotMsgHandler in stg_unsol_msg_handler.cpp, deliberately keeps using
    # its own separate file-local opaque stub -- see kg_msg_processor.h's own
    # REACHABILITY note) -- reconstructed for structural completeness, same
    # precedent as CLimiterBase/CJobStack above.
    "08913620",  # CKGMsgProcessor::CKGMsgProcessor()
    "08913860",  # CKGMsgProcessor::~CKGMsgProcessor() (D1/D2, identical)
    "089138f0",  # CKGMsgProcessor::GetInstance()

    # --- Eva deferred-registry re-trace, 2026-07-27: CZ(unsigned)/~CZ() real bodies
    # (cz_util.h) + the CBDApiInstance batch (bd_api_instance.h), correcting the
    # prior "RegisterLoader is a genuine dead end" false verdict (a mangled-name
    # grep bug, not a real absence of callers -- see bd_api_instance.h/mains.cpp).
    "080ba5f0",  # CZ::CZ(unsigned int) -- real body, was an all-zero stub
    "08185c00",  # CZ::~CZ() (canonical out-of-line copy) -- real body, was a no-op
    "08243800",  # CBatchDiskMan::IsPreloadRunning(unsigned char, char const*) const
    "08243980",  # CBDApiInstance::RegisterLoader(CBatchDiskMan*)
    "08243920",  # CBDApiInstance::IsBusy() const
    "082438c0",  # CBDApiInstance::IsPreloadRunning() const
    "08243830",  # CBDApiInstance::IsPreloadRunning(unsigned char, char const*) const
    "082456d0",  # TVector<CBatchDiskMan*,1>::MakeCapacity(unsigned int)
}

# CBDApiInstance re-checked (Stage 6 breadth sweep, 2026-07-25) -- its 6 methods
# (IsPreloadRunning x2/IsBusy/RegisterLoader/dtor x2, .text+0x08243830..0x082455e0)
# stay NOT in RECONSTRUCTED. RegisterLoader(CBatchDiskMan*) (08243980) -- the one
# method with a plausible real caller -- was checked directly: zero call sites found
# anywhere in the 37,795-function export (confirmed by grepping every decompile for
# the mangled symbol, not just BDApiInstance's own file). A confirmed, thorough
# negative result, not a gap in this search -- deliberately NOT reproduced by
# CBatchDiskManConstructorCreate() (mains.cpp), see that function's own comment.
#
# UPDATE (2026-07-26, later same day, CBatchDiskMan unlock batch): the
# "CBatchDiskMan's own constructor is itself never invoked on this project's traced
# boot path" claim above is now STALE/CORRECTED -- `PTR__CBatchDiskManConstructor_
# 08eabe08`'s own Create slot now routes to the real `CBatchDiskManConstructorCreate()`
# (mains.cpp), the same unlock CPanel/CEditor already got, so `CBatchDiskMan` IS now
# constructed for real via `CConfigManager::CreateUserModules()`'s own "BatchDiskManClass"
# row (config_info.cpp row 0). Kept here for its own historical context, not deleted.
#
# UPDATE (2026-07-27, CBDApiInstance batch, bd_api_instance.h): the ENTIRE "zero call
# sites found... confirmed, thorough negative result" claim above is now RETRACTED,
# not just stale -- it was a false negative caused by a mangled-name grep bug (wrong
# Itanium length-prefix digits: "CBDApiInstance"/"CBatchDiskMan" are 14/13 characters,
# not 13/12). A correct `objdump -dr | grep "call.*8243980"` finds exactly ONE real
# call site, inside `CBatchDiskManConstructor::Create()` (08243d80) -- the SAME
# function the UPDATE directly above already established is now boot-path-reachable.
# All 6 of `CBDApiInstance`'s own methods (bd_api_instance.h) are now genuinely
# tractable and reconstructed for real: `RegisterLoader()` is boot-path-reachable and
# now wired into `CBatchDiskManConstructorCreate()`; `IsBusy()`/`IsPreloadRunning()`
# x2/the dtor have zero callers anywhere in ground truth itself (confirmed directly,
# same status as `CLimiterBase`) and are reconstructed for structural completeness
# only. See RECONSTRUCTED above for the 4 new addresses (dtor x2 NOT included --
# nothing dispatches virtually on this object, see bd_api_instance.h).

# CTask::RegisterIfc (0807ec90, 472 bytes) is NOW in RECONSTRUCTED (2026-07-26, see
# above) -- this note is stale/superseded, kept only for its own historical context.
# Its own TVector<SRegisteredIfc,1>::MakeCapacity() dependency (08182220, 539 bytes)
# is also now reconstructed, task.cpp's own static
# TVector_SRegisteredIfc_MakeCapacity() helper -- the first TVector<T,1>::
# MakeCapacity() transcription anywhere in this project (ckernel.h's own
# "unreconstructed template base" note predates this; sibling instantiations for
# OTHER element types, e.g. CRTRouterApi::SConnection/CPool::SPool at the same
# 539-byte size, are each their own separate ground-truth symbol and stay
# unreconstructed until something actually needs them).
# CPoller's ctor (.text+0x089ef740) is NOW reconstructed (2026-07-26 reassessment,
# see RECONSTRUCTED above / poller.h) -- this note is stale/superseded, kept only for
# its own historical context. Fresh `objdump -dr` verification found the ctor was
# ALWAYS fully tractable once SetMask() was real: the 2 fixed-size handle-table fills
# are plain Duff's-device-unrolled 0xFFFFFFFF loops (mechanical, same idiom as every
# other bulk-fill in this project) and the Api+0xac lookup is the SAME "call shape
# only, real meaning undecoded" treatment already given to Api+0xa0 (system_api.h) --
# not a new or deeper kind of blocker. The class was never "genuine Peg-toolkit
# depth" either (an even earlier batch summary's label, corrected in task.h's own
# header comment on 2026-07-25 already, then re-verified from scratch this batch) --
# CPoller is a plain CTask-derived class, not Peg/UI-editor-toolkit-adjacent at all.
# CPoller's own real, definitive ground-truth caller was found to be
# `CPanel::Setup()` (.text+0x089ee6e0) -- NOW RECONSTRUCTED (2026-07-26, later same
# day, panel.h/.cpp) along with the rest of `CPanel` and `CPanelConstructor::Create()`
# (mains.cpp's `PTR__CPanelConstructor_08f7c2f0` now routes to the real
# `CPanelConstructorCreate`, same fix shape as `CEditorConstructorCreate()`) --
# this note is stale/superseded, kept only for its own historical context. 9 of
# CPoller's 29 methods (ctor,
# dtor, 3 const accessors, the CIfcClient nested class's 3 methods, and its own
# TVector<CIfcClient*,1>::MakeCapacity()) are now Tier A; the remaining ~20 (every
# Msg*(CMessage&) handler, RegisterClient/InitAnalogs/InitButtons, both Exec()
# overrides) stay deferred for real reasons -- size, and a genuinely separate
# `CMessage` prerequisite this project hasn't reconstructed at all -- not toolkit
# depth. See poller.h's own header comment for the full derivation and the complete
# deferred-method list.
# CEditor::CPanelIfcTask's own ctor (.text+0x0824b7e0) is now FULLY reconstructed
# (Stage 6 dedicated CPanelIfcTask batch, 2026-07-25, see RECONSTRUCTED above) --
# this note is stale/superseded, kept only for its own historical context (the
# multiple-inheritance COutLinkMono sub-object + adjustment thunk work it once
# flagged as deferred is now transcribed, panel_ifc_task.h/.cpp).
# CScheduler::InsertTask(CTask const&) (.text+0x08062d80) -- the
# obvious ground-truth candidate for populating CLevelManager's own SEPARATE
# per-level task array (distinct from CModule::mTasks) -- exists but has ZERO
# confirmed direct callers found this batch; left as an open lead
# (level_manager_array.h), not fabricated.

# COmegaInterface::ExitRequested is declared but its body is a no-op stand-in (the real
# vtable-slot-0x7c indirect call isn't resolved) -- deliberately NOT in RECONSTRUCTED.
#
# OmegaExitThread (0804deb0) is NOT in RECONSTRUCTED -- grepped for across all 37,795
# exported function bodies with zero callers found anywhere in the binary; not reachable
# from the traced boot path (or, as far as this export shows, from anywhere at all).


def load_symbol_names(path):
    """address(lowercase, no 0x) -> best demangled 'Namespace::name' from symbols.csv."""
    best = {}
    with open(path, newline="", encoding="utf-8", errors="replace") as f:
        for row in csv.DictReader(f):
            addr = row.get("address", "").lower()
            if not addr or addr.startswith("external"):
                continue
            if row.get("symbol_type") != "Function":
                continue
            ns = row.get("namespace", "")
            name = row.get("name", "")
            qualified = f"{ns}::{name}" if ns and ns not in ("Global", "<EXTERNAL>") else name
            # Prefer the first ANALYSIS-sourced Function row seen for a given address.
            if addr not in best:
                best[addr] = qualified
    return best


def main():
    sym_names = load_symbol_names(os.path.join(EXPORT_DIR, "symbols.csv"))

    with open(os.path.join(EXPORT_DIR, "functions.csv"), newline="", encoding="utf-8", errors="replace") as f, \
         open(OUT_PATH, "w", newline="", encoding="utf-8") as out:
        reader = csv.DictReader(f)
        writer = csv.writer(out)
        writer.writerow(["address", "name", "qualified_name", "size_bytes", "calling_convention", "status"])

        n_total = 0
        n_recon = 0
        for row in reader:
            addr = row["entry"].lower()
            qualified = sym_names.get(addr, row["name"])
            status = "reconstructed" if addr in RECONSTRUCTED else "pending"
            writer.writerow([addr, row["name"], qualified, row["size_bytes"], row["calling_convention"], status])
            n_total += 1
            if status == "reconstructed":
                n_recon += 1

    print(f"wrote {OUT_PATH}: {n_total} functions, {n_recon} reconstructed ({100.0*n_recon/n_total:.3f}%)")


if __name__ == "__main__":
    main()
