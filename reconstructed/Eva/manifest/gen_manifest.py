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
    "0816be00",  # CTimerEngine::~CTimerEngine() (D2, non-deleting)
    "0816be90",  # CTimerEngine::~CTimerEngine() (D0, deleting) -- 2026-07-28 gap fix,
                 # see comment below: commit bb606a3's own backfill of the 00885a4
                 # batch listed the D2 half but missed this D0 sibling

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
    # (CMessage&) (080cc0d0) originally stayed Tier B here -- see chunk_server.h.
    # STALE: promoted to Tier A the same day by fa6ac5c ("CChunkServer::
    # Exec(CMessage&) promoted Tier B -> Tier A", closes the last Tier B method
    # in CChunkServer), but that commit never touched this manifest -- backfilled
    # 2026-07-28, see the standalone entry further below (search "fa6ac5c"). Load()
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

    # --- Eva fresh broad-survey pass, 2026-07-27: cross-referenced every real
    # PreKernelConstructor/PostKernelConstructor/PreKernelDestructor/
    # PostKernelDestructor override in the ground-truth binary (symbols.csv) against
    # this project's own CGlobalObjectBase-derived coverage. Found 4 real overriding
    # classes never touched at all: CResFamily, CPool, CSlotPool, CKernelDeathNotifier.
    # CPool/CSlotPool deferred (need an unmodeled TVector<T,N> template + a
    # CSlotStateFree singleton, and their owning classes CSysExSnifferBase::
    # CSampleItem/CSeqScheduler are themselves deep unmodeled subsystems) -- see
    # PROJECT_BRAIN status.md for the full writeup. CResFamily/CKernelDeathNotifier
    # ARE genuinely tractable (same "size is not depth" narrow-scope precedent as
    # CJobStack/CRTRouterApiInstance above) and reconstructed for real this pass.

    # CKernelDeathNotifier (kernel_death_notifier.h/.cpp): trivial 8-byte class, one
    # real global (g_oKernelDeathNotifier), one real override (PreKernelDestructor).
    "0817cbe0",  # CKernelDeathNotifier::PreKernelDestructor(unsigned long)
    "0817cbf0",  # CKernelDeathNotifier::~CKernelDeathNotifier() [D1]
    "0817cc10",  # CKernelDeathNotifier::~CKernelDeathNotifier() [D0, deleting]
    "0805e780",  # global.constructors.keyed.to.g_oKernelDeathNotifier

    # CResFamily (res_family.h/.cpp): construction/destruction + the 2 phase-hook
    # overrides only. The class's own 15 business-logic methods (SetName/SetSize/
    # Clear/GetFreeRes/GetResFreePosZ/SetLoadType/GetLoadedElemPos/
    # GetLoadedElemCount/GetLoadedElem/RemoveLoadedElem/AppendLoadedElem/
    # IsLoadedResListFull x2/GetGroupCount/GetCountZ/AddGroupElem) stay
    # unreconstructed/out of scope -- same CZ-container-adjacent boundary
    # CConfigManager::CreateResourceFamilies() itself already has (config_manager.cpp)
    # -- NOT added here.
    "08063480",  # CResFamily::CResFamily()
    "0817ce10",  # CResFamily::~CResFamily() [D1]
    "0817cfb0",  # CResFamily::~CResFamily() [D0, deleting]
    "080633a0",  # CResFamily::PostKernelConstructor(unsigned long)
    "08063300",  # CResFamily::PostKernelDestructor(unsigned long)
    "08069b90",  # global.constructors.keyed.to.g_atResFamilies (32-instance array)

    # --- Eva CPool/CSlotPool follow-up, 2026-07-27: TVector<T,N> template traced for
    # real (tvector.h) against TVector<CPool::SPool,1>'s own ~TVector()/MakeCapacity(),
    # cross-checked for general shape against TVector<CZ,1>/TVector<CLogicUnit,1>/
    # TVector<CLimiterBase*,1>'s own MakeCapacity. Unlocked both CPool (pool.h/.cpp) and
    # CSlotPool (slot_pool.h/.cpp) construction/destruction + real phase-hook overrides
    # -- same "size is not depth" narrow-scope precedent as CResFamily above. CSlotPool
    # additionally needed CSlotStateFree (a State-pattern singleton, slot_pool.h/.cpp) --
    # small and real (1 field, no ground-truth ctor symbol -- GCC inlined it into a
    # merged _GLOBAL__I_ static initializer, same shape as CKernelDeathNotifier's own
    # g_oKernelDeathNotifier). Both owning objects (CSysExSnifferBase::CSampleItem::
    # sm_oPool @ ds:0x930a360, CSeqScheduler::sm_oSlotPool) and CSlotPool's own per-slot
    # record type's owning class (CSeqSlot, CSeqScheduler-adjacent) stay deep, entirely
    # unmodeled subsystems -- not added here. Verified: host `make verify` green (2 new
    # binaries, test_pool.cpp/test_slot_pool.cpp, 0 checks failed each) + a real period-
    # correct Debian Lenny (glibc 2.7/GCC 4.3.2) chroot cross-build+link ("LINK OK").
    "081859c0",  # TVector<CPool::SPool,1>::MakeCapacity(unsigned int)
    "08182c10",  # TVector<CPool::SPool,1>::~TVector() [D1/D2]
    "08182c60",  # TVector<CPool::SPool,1>::~TVector() [D0, deleting]
    "080b92a0",  # CPool::CPool(unsigned int, int)
    "08182c30",  # CPool::~CPool() [D1]
    "08182d90",  # CPool::~CPool() [D0, deleting]
    "080b94c0",  # CPool::Alloc()
    "080b9190",  # CPool::Free(void*)
    "080b9210",  # CPool::Expand(CPool::SPool&, unsigned int)
    "080b9140",  # CPool::PostKernelDestructor(unsigned long)
    "081692b0",  # CSlotPool::CSlotPool(unsigned int)
    "081690e0",  # CSlotPool::~CSlotPool() [D1/D2]
    "08169150",  # CSlotPool::~CSlotPool() [D0, deleting]
    "08168cb0",  # CSlotPool::PreKernelConstructor(unsigned long)
    "08168c80",  # CSlotPool::PostKernelDestructor(unsigned long)
    "08169260",  # CSeqSlot::CSeqSlot() (element ctor -- matches CSlotPool::SSlot's own
                  # field-init pattern byte-for-byte; CSeqSlot itself not otherwise
                  # reconstructed, see slot_pool.h)
    "08195af0",  # CSlotStateFree::~CSlotStateFree() [D1/D2]
    "08195c20",  # CSlotStateFree::~CSlotStateFree() [D0, deleting]
    "081959e0",  # CSlotStateFree::GetStateName_debug() const
    "0816b860",  # global.constructors.keyed.to.CSlotStateFree::s_oInstance (merged
                  # _GLOBAL__I_ -- only the CSlotStateFree slice is modeled, see
                  # slot_pool.h)

    # --- USTGAPIXxx thin-IPC-facade family, non-CValue slice (2026-07-27) ---
    # Fresh nm -C class-inventory sweep (same methodology that found CResFamily/
    # CPool/CSlotPool) confirmed ustg_user_api.h's own header comment: ~150
    # per-subsystem USTGAPIXxx::UpdateYyy() wrapper classes were still genuinely
    # unclaimed. Reconstructed every method in this family that does NOT take a
    # `CValue const&` argument (4 real methods do -- CValue's own layout is not
    # modeled anywhere in this project, deliberately deferred, see
    # ustg_api_wrappers.h's own header comment for the precise finding).
    # Verified: host `make verify` green (new test_ustg_api_wrappers.cpp, 25
    # byte-exact wire-format checks, 0 failed) + full existing suite unaffected +
    # real Lenny cross-build+link ("LINK OK").
    "08e1b6f0",  # USTGAPICombi::SharedMemCombiDump(unsigned, unsigned, eSTGMsgPerfType)
    "08e1b810",  # USTGAPICombi::UpdateCombiParameter(...)
    "08e1b880",  # USTGAPICombi::UpdateVectorMotionParameter(...)
    "08e1b8f0",  # USTGAPICombi::UpdateControllerInfoParameter(...)
    "08e1b960",  # USTGAPICombi::UpdateToneAdjustParameter(...)
    "08e1b9e0",  # USTGAPICombi::UpdateAudioInputParameter(...)
    "08e1ba50",  # USTGAPICombi::UpdateEffectBalanceParameter(...)
    "08e1bac0",  # USTGAPICombi::UpdateSequenceMetronomeParameter(unsigned, int, int)
    "08e1d2d0",  # USTGAPIEffect::UpdateParam(...)
    "08e1d350",  # USTGAPIEffectMgr::UpdateEffectLFOParameter(...)
    "08e1d3c0",  # USTGAPIEffectSlot::UpdateParam(...)
    "08e1d590",  # USTGAPIGlobal::UpdateGlobalParameter(unsigned, unsigned, int)
    "08e1d5e0",  # USTGAPIHDRTrack::UpdateHDRTrackParameter(unsigned, unsigned, unsigned, int)
    "08e1d0e0",  # USTGAPIDrumkitData::SetCurrentKitId(unsigned)
    "08e1d1d0",  # USTGAPIDrumkitData::SharedMemDrumKitDump(int)
    "08e1ec60",  # USTGAPIPatch::UpdateOscSelectByType(...)
    "08e22fd0",  # USTGAPIProgramSlot::UpdateProgramSlotParameter(...)
    "08e23050",  # USTGAPIProgramSlot::UpdateProgramSlotEnabled(...)
    "08e24ad0",  # USTGAPISetList::UpdateSlotParam(int, int, int, int)
    "08e24d60",  # USTGAPIWaveSequenceData::SetCurrentSequenceId(unsigned)
    "08e24e40",  # USTGAPIWaveSequenceData::SharedMemWaveSequenceDump(int)

    # --- USTGAPIKLM / USTGAPICDAudio / USTGAPISampling primitives (2026-07-27) ---
    # Follow-up to the batch above: traced the 5 sibling classes that batch's own
    # header comment flagged "confirmed different-shaped" (USTGAPIKLM, CDAudio,
    # MIDI, PCMBanks, Sampling) via direct objdump -dr -M intel reads, not guesses.
    # USTGAPIKLM (14/15 methods -- the real installed-EXs-product/license table,
    # CSTGHandle::Access()-based, not message-send-based; InstallOptionFile
    # deliberately deferred, see ustg_api_klm.h) and USTGAPICDAudio (12/12 methods)
    # both turned out genuinely tractable. USTGAPICDAudio unlocked 4 real
    # USTGAPISampling primitive methods it depends on (SharedScratch/
    # SendSimpleMessage/ReceiveSimpleMessage/ReceiveMessage) -- confirmed these are
    # a shared "simple 3-int command" layer at least 3 subsystems build on, NOT
    # the same field layout as the USTGAPIXxx family above (see ustg_api_sampling.h's
    # header comment for the precise field-role difference). USTGAPIMIDI (device-
    # queue I/O against CSTGMidiQueue, real __assert_fail-guarded bounds checks) and
    # the bulk of USTGAPIPCMBanks/USTGAPISampling (46/51 methods each, own
    # STGMessages built directly, not through these 4 primitives) re-confirmed
    # genuinely deeper -- left deferred, precise leads recorded in
    # ustg_api_sampling.h's own header comment for a future pass.
    # Verified: host `make verify` green (new test_ustg_api_cdaudio.cpp, 26
    # byte-exact/behavioral checks incl. the 2 CValue serialization rules, 0
    # failed) + full existing 47-binary suite unaffected. USTGAPIKLM's own
    # CSTGHandle::Access()/`/proc/.oacmd`-dependent methods compile clean
    # (`make objs`) but are not host-KAT-tested at the byte level -- same
    # already-accepted "real shared-memory/file I/O boundary, not mockable
    # host-side" limitation as CSTGHandle::Access() itself (stg_handle.cpp).
    "08e1d640",  # USTGAPIKLM::RescanInstalledProducts()
    "08e1d650",  # USTGAPIKLM::GetNumberOfProductsInstalled(unsigned&)
    "08e1d690",  # USTGAPIKLM::GetProductInfo(unsigned, CValue&, bool&, unsigned&, char*, char*, char*)
    "08e1d7e0",  # USTGAPIKLM::IsProductAuthorized(unsigned)
    "08e1d840",  # USTGAPIKLM::GetProductIdentifier(unsigned, CValue&)
    "08e1d890",  # USTGAPIKLM::GetProductOptionFileName(unsigned, char*)
    "08e1d8e0",  # USTGAPIKLM::GetProductLongName(unsigned, char*)
    "08e1d930",  # USTGAPIKLM::GetProductFullName(unsigned, char*)
    "08e1d9d0",  # USTGAPIKLM::GetProductShortName(unsigned, char*)
    "08e1da20",  # USTGAPIKLM::GetNumberOfItemsInProduct(unsigned, unsigned&)
    "08e1da70",  # USTGAPIKLM::GetProductItemInfo(unsigned, unsigned, eSTGOptionType&, CValue&, char*)
    "08e1db50",  # USTGAPIKLM::GetProductItemType(unsigned, unsigned, eSTGOptionType&)
    "08e1db90",  # USTGAPIKLM::GetProductItemName(unsigned, unsigned, char*)
    "08e1dbd0",  # USTGAPIKLM::SetAuthString(unsigned, char*)
    "08e48da0",  # SendCommandRescanInstalledProducts() -- "SO:*" to /proc/.oacmd
    "08e48bc0",  # SendCommandAuthorizeOption(char const*) -- "AU:%s" to /proc/.oacmd
    "08e1b3f0",  # USTGAPICDAudio::PlayStandby(char const*, unsigned long, unsigned, unsigned long, unsigned)
    "08e1b480",  # USTGAPICDAudio::PlayStart()
    "08e1b4b0",  # USTGAPICDAudio::PlayStop()
    "08e1b4e0",  # USTGAPICDAudio::GetCurrentPosition(unsigned long&, EAudioStatus&)
    "08e1b570",  # USTGAPICDAudio::SetLevel(unsigned char)
    "08e1b5a0",  # USTGAPICDAudio::SetChanLevel(unsigned char, unsigned char)
    "08e1b5d0",  # USTGAPICDAudio::SetChanPan(unsigned char, unsigned char)
    "08e1b600",  # USTGAPICDAudio::SetChanBusSelect(unsigned char, eSTGAPIBusIDOut)
    "08e1b630",  # USTGAPICDAudio::SetChanSend1Level(unsigned char, unsigned char)
    "08e1b660",  # USTGAPICDAudio::SetChanSend2Level(unsigned char, unsigned char)
    "08e1b690",  # USTGAPICDAudio::SetChanFXControlBus(unsigned char, eSTGAPIFXCtrlBus)
    "08e1b6c0",  # USTGAPICDAudio::SetChanHDRBus(unsigned char, eSTGAPIHDRBus)
    "08e230d0",  # USTGAPISampling::SharedScratch()
    "08e24870",  # USTGAPISampling::ReceiveMessage(char*, int, int, int)
    "08e24970",  # USTGAPISampling::ReceiveSimpleMessage(int, unsigned long&)
    "08e24a80",  # USTGAPISampling::SendSimpleMessage(int, int, int)

    # --- CFileIoBase, full class, 52/52 methods (2026-07-27 storage-cluster batch) ---
    # Fresh nm -C class-inventory sweep found a dense, previously-100%-untouched
    # DiskUtil/CustomFs storage cluster (CFileIoBase/CDDriverIO/CScsiDriverBase/
    # CFilesys/CDiskUtil). CFileIoBase is the pure abstract interface layer -- every
    # method a fixed-sentinel stub, 30 calling the real Api+0x94 assert-report slot
    # first, 19 (incl. get_iotype()) returning immediately. Mangled symbol names of
    # the compiled TU verified byte-for-byte identical (nm -C diff, 50/50 unique
    # names match) against the real binary's own CFileIoBase:: symbols -- confirms
    # every parameter type/overload was transcribed correctly, not just plausible.
    # Concrete per-media overrides (CFileIoUnknown/CFileIoCdda/CFileIoKge/
    # CFileIoUdf) and the rest of the cluster (CDDriverIO/CScsiDriverBase/CFilesys/
    # CDiskUtil) are deliberately out of scope for this pass -- see
    # include/file_io_base.h's own header comment.
    "08318480",  # CFileIoBase::get_iotype()
    "08318490",  # CFileIoBase::fmount(EDevice_Id)
    "083184d0",  # CFileIoBase::fmount(EDevice_Id, EMountIoType, int*)
    "08318510",  # CFileIoBase::funmount(EDevice_Id)
    "08318550",  # CFileIoBase::fopen(char const*, char const*)
    "08318590",  # CFileIoBase::fclose(int)
    "083185d0",  # CFileIoBase::fread(void*, unsigned int, unsigned int, int)
    "08318610",  # CFileIoBase::fwrite(void const*, unsigned int, unsigned int, int)
    "08318650",  # CFileIoBase::fseek(int, long, int)
    "08318690",  # CFileIoBase::ftell(int)
    "083186d0",  # CFileIoBase::fflush(int)
    "08318710",  # CFileIoBase::resize(int, unsigned int)
    "08318750",  # CFileIoBase::format(EDevice_Id, int)
    "08318790",  # CFileIoBase::format(EDevice_Id, int, EFatType)
    "083187a0",  # CFileIoBase::freebytes(EDevice_Id)
    "083187b0",  # CFileIoBase::totalfreeclus(EDevice_Id)
    "083187f0",  # CFileIoBase::chdir(char const*)
    "08318800",  # CFileIoBase::getwd(EDevice_Id, char*)
    "08318810",  # CFileIoBase::dir(char const*, int, unsigned long&, CFileDirEntry*)
    "08318820",  # CFileIoBase::rename(char const*, char const*)
    "08318860",  # CFileIoBase::remove(char const*)
    "083188a0",  # CFileIoBase::mkdir(char const*)
    "083188e0",  # CFileIoBase::rmdir(char const*)
    "08318920",  # CFileIoBase::getmediainfo(EDevice_Id, CMediaInfo*)
    "08318930",  # CFileIoBase::play(EDevice_Id, unsigned char, unsigned char, unsigned long, unsigned long)
    "08318940",  # CFileIoBase::stop(EDevice_Id)
    "08318950",  # CFileIoBase::pause(EDevice_Id)
    "08318960",  # CFileIoBase::resume(EDevice_Id)
    "08318970",  # CFileIoBase::ffscan(EDevice_Id, unsigned char, unsigned char, unsigned long, unsigned long)
    "08318980",  # CFileIoBase::rewscan(EDevice_Id, unsigned char, unsigned char, unsigned long, unsigned long)
    "08318990",  # CFileIoBase::stopscan(EDevice_Id)
    "083189a0",  # CFileIoBase::getcurpos(EDevice_Id, EAudioStatusMMC*, unsigned char*, unsigned char*, int, int)
    "083189b0",  # CFileIoBase::getmaxtrk(EDevice_Id, unsigned char*)
    "083189c0",  # CFileIoBase::getmaxidx(EDevice_Id, unsigned char, unsigned char*)
    "083189d0",  # CFileIoBase::gettrklen(EDevice_Id, unsigned char, unsigned long*)
    "083189e0",  # CFileIoBase::getidxlen(EDevice_Id, unsigned char, unsigned char, unsigned char, unsigned long*)
    "083189f0",  # CFileIoBase::finalize(EDevice_Id)
    "08318a30",  # CFileIoBase::settestmode(EDevice_Id, int)
    "08318a70",  # CFileIoBase::fdummywrite(unsigned int, unsigned int, int)
    "08318ab0",  # CFileIoBase::getfilelbaarray(EDevice_Id, int, CFileLbaArray*)
    "08318af0",  # CFileIoBase::opennextpath(EDevice_Id)
    "08318b30",  # CFileIoBase::closepath(EDevice_Id, int)
    "08318b70",  # CFileIoBase::sortdir(EDevice_Id)
    "08318bb0",  # CFileIoBase::isodir(EDevice_Id, udf_iso_rec*, udf_iso_rec*)
    "08318bf0",  # CFileIoBase::getemphasized(int, int*)
    "08318c30",  # CFileIoBase::getmaxclusterno(EDevice_Id)
    "08318c70",  # CFileIoBase::scandisk(EDevice_Id, unsigned long, unsigned long, unsigned long*, unsigned long*)
    "08318cb0",  # CFileIoBase::optimizemedium(EDevice_Id, unsigned long, unsigned long*, int)
    "08318cf0",  # CFileIoBase::chmod(char const*, unsigned char)
    "08318e90",  # CFileIoBase::CFileIoBase()
    "08995480",  # CFileIoBase::~CFileIoBase() (D1, complete object)
    "089954e0",  # CFileIoBase::~CFileIoBase() (D0, deleting)

    # --- CFileIoUnknown, full class, 6/6 overridden methods + dtor pair
    # (2026-07-27 storage-cluster follow-up). First of the 4 concrete
    # CFileIoBase subclasses file_io_base.h's own "OUT OF SCOPE" list deferred
    # (CFileIoUnknown/CFileIoCdda/CFileIoKge/CFileIoUdf) -- picked as the
    # smallest/most self-contained (6 overrides vs CFileIoCdda's 41). 3 trivial
    # sentinel overrides (get_iotype/fmount/funmount) + 3 genuine forwarding
    # bodies reaching into out-of-scope CDDriverIO/CFilesys/CMediaInfo, modeled
    # as inert EvaVTableStub-style stand-ins (same convention as
    # panel_ifc_task.cpp) so `make verify` stays fully linkable. No separate
    # ctor symbol exists in the real binary (trivial-derived-ctor folding);
    # dtor pair (D1/D0) matches CFileIoBase's own compiler-bookkeeping shape.
    # See include/file_io_unknown.h for the full per-method writeup.
    "08318d30",  # CFileIoUnknown::get_iotype()
    "08318d40",  # CFileIoUnknown::fmount(EDevice_Id)
    "08318d50",  # CFileIoUnknown::funmount(EDevice_Id)
    "08318d60",  # CFileIoUnknown::getmediainfo(EDevice_Id, CMediaInfo*)
    "08318dc0",  # CFileIoUnknown::format(EDevice_Id, int, EFatType)
    "08318e20",  # CFileIoUnknown::format(EDevice_Id, int)
    "08995490",  # CFileIoUnknown::~CFileIoUnknown() (D1, complete object)
    "089954a0",  # CFileIoUnknown::~CFileIoUnknown() (D0, deleting)

    # --- CScsiDriverBase, ctor/dtor/GetResultOfScsiCommandAsync + all 35
    # standalone SetParamXxx CDB builders (2026-07-28 storage-cluster
    # follow-up). Picked over CFileIoCdda (41 overrides, all far smaller than
    # this) and CFileIoUdf (format() alone is 4791 bytes of real VFAT logic,
    # not tractable in one batch); "CFileIoKge" from file_io_base.h's original
    # out-of-scope list does not exist in the binary (a real but unrelated
    # CFileKge class was mistaken for it in an earlier pass). The remaining 11
    # methods (SetCommandParameter/Execute/AfterProcess + 6 AfterProcessXxx,
    # the giant jump-table dispatcher family) are deferred -- documented in
    # include/scsi_driver_base.h, not reconstructed this pass. Independently
    # confirmed real finding: none of the 35 SetParamXxx below has any caller
    # anywhere in the binary (Execute() calls SetCommandParameter's own
    # inlined duplicate of the same logic instead) -- dead code, faithfully
    # reconstructed anyway per the "decompile everything" standing goal.
    "083086e0",  # CScsiDriverBase::CScsiDriverBase(unsigned char)
    "083086a0",  # CScsiDriverBase::~CScsiDriverBase() (D1, complete object)
    "083086c0",  # CScsiDriverBase::~CScsiDriverBase() (D0, deleting)
    "083086b0",  # CScsiDriverBase::GetResultOfScsiCommandAsync()
    "083092e0",  # CScsiDriverBase::SetParamGetEventStatusNtf(SDriverIOPbuf*)
    "08309340",  # CScsiDriverBase::SetParamSendEvent(SDriverIOPbuf*)
    "083093a0",  # CScsiDriverBase::SetParamGetPerformance(SDriverIOPbuf*)
    "08309440",  # CScsiDriverBase::SetParamReadDVDStructure(SDriverIOPbuf*)
    "083094e0",  # CScsiDriverBase::SetParamSendDVDStructure(SDriverIOPbuf*)
    "08309540",  # CScsiDriverBase::SetParamReportKey(SDriverIOPbuf*)
    "083095e0",  # CScsiDriverBase::SetParamSendKey(SDriverIOPbuf*)
    "08309640",  # CScsiDriverBase::SetParamWriteAndVerify(SDriverIOPbuf*)
    "083096b0",  # CScsiDriverBase::SetParamFormatUnit(SDriverIOPbuf*)
    "08309710",  # CScsiDriverBase::SetParamReadFmtCapacity(SDriverIOPbuf*)
    "08309770",  # CScsiDriverBase::SetParamGetConfiguration(SDriverIOPbuf*)
    "083097d0",  # CScsiDriverBase::SetParamTestUnitReady(SDriverIOPbuf*)
    "08309830",  # CScsiDriverBase::SetParamInquiry(SDriverIOPbuf*)
    "08309890",  # CScsiDriverBase::SetParamRmvLock(SDriverIOPbuf*)
    "083098c0",  # CScsiDriverBase::SetParamRequestSense(SDriverIOPbuf*)
    "08309900",  # CScsiDriverBase::SetParamModeSense10(SDriverIOPbuf*)
    "08309960",  # CScsiDriverBase::SetParamModeSelect6(SDriverIOPbuf*)
    "083099e0",  # CScsiDriverBase::SetParamModeSelect10(SDriverIOPbuf*)
    "08309a30",  # CScsiDriverBase::SetParamReadCapacity(SDriverIOPbuf*)
    "08309a80",  # CScsiDriverBase::SetParamReadDiskInfo(SDriverIOPbuf*)
    "08309ad0",  # CScsiDriverBase::SetParamReadTrackInfo(SDriverIOPbuf*)
    "08309b30",  # CScsiDriverBase::SetParamReserveTrack(SDriverIOPbuf*)
    "08309b90",  # CScsiDriverBase::SetParamSeek(SDriverIOPbuf*)
    "08309be0",  # CScsiDriverBase::SetParamReadWrite(SDriverIOPbuf*, int)
    "08309c80",  # CScsiDriverBase::SetParamSyncCache(SDriverIOPbuf*)
    "08309cd0",  # CScsiDriverBase::SetParamCloseSession(SDriverIOPbuf*)
    "08309d20",  # CScsiDriverBase::SetParamBlank(SDriverIOPbuf*)
    "08309d70",  # CScsiDriverBase::SetParamReadTocAtip(SDriverIOPbuf*)
    "08309dd0",  # CScsiDriverBase::SetParamPlayAudio(SDriverIOPbuf*)
    "08309e40",  # CScsiDriverBase::SetParamReadSub(SDriverIOPbuf*)
    "08309e90",  # CScsiDriverBase::SetParamReadCD(SDriverIOPbuf*)
    "08309f30",  # CScsiDriverBase::SetParamSetSpeed(SDriverIOPbuf*)
    "08309f90",  # CScsiDriverBase::SetParamReadBufferCapacity(SDriverIOPbuf*)
    "08309fe0",  # CScsiDriverBase::SetParamStartStop(SDriverIOPbuf*)
    "0830a030",  # CScsiDriverBase::SetParamCheckWriteProtect(SDriverIOPbuf*)
    "0830a080",  # CScsiDriverBase::SetParamSleep(SDriverIOPbuf*)

    # --- CStorageConverterBase, the 256-method Ext{X}toInt{Y} combinatorial
    # matrix (2026-07-28, storage-cluster follow-up -- found via a fresh
    # pending-manifest class-count sweep, same technique as OA.ko's
    # CKGSeqBackupCommonParam/CKGSeqBackupModuleParam batch). Reconstructed by a
    # scripted objdump -dr -M intel -> Python instruction-pattern decoder that
    # classified all 256 bodies by exact byte size (1/19/22/37, zero anomalies)
    # and mechanically resolved every forwarding thunk's vtable-slot read
    # against a direct .rodata dump of "vtable for CStorageConverterBase"
    # (0x08fcc9c0). Verified rule, zero exceptions: (X=0,Y=0) is the one real
    # memcpy identity copy; X>Y is a tail-call thunk to Ext{Y}toInt{Y} (120
    # instances); X<=Y excluding (0,0) is a bare no-op ret (135 instances,
    # including all 15 non-zero diagonals -- every internal version other than
    # 0000 is unimplemented in this build). See include/storage_converter_base.h
    # for the full derivation and src/init/storage_converter_base.cpp. 256 KAT
    # checks in verify/test_storage_converter_base.cpp use an INDEPENDENT
    # black-box rule (does the destination buffer end up copied or untouched),
    # not a re-check of this same size/offset classification. CheckVersion/
    # ValidateExt/Save/Load/Open/Close/the 16 ExttoIntY real-body methods/the 16
    # ValidateExtY methods/ctor are deliberately deferred -- documented in the
    # header. Open() has 2 real external callers (.text+0x08df76b9/0x08df778d,
    # not yet traced to a caller class) -- the only evidence anything in this
    # class is live; nothing in this matrix itself is reachable from outside
    # the class (independently confirmed via a whole-binary call grep, same
    # "real but dead code" finding as CScsiDriverBase's own SetParamXxx family).
    "08dea8f0",  # CStorageConverterBase::Ext0000toInt0000(CConvertStorageParam const&) const
    "08de9180",  # CStorageConverterBase::Ext0001toInt0000(CConvertStorageParam const&) const
    "08de91a0",  # CStorageConverterBase::Ext0002toInt0000(CConvertStorageParam const&) const
    "08de91c0",  # CStorageConverterBase::Ext0003toInt0000(CConvertStorageParam const&) const
    "08de91e0",  # CStorageConverterBase::Ext0004toInt0000(CConvertStorageParam const&) const
    "08de9200",  # CStorageConverterBase::Ext0005toInt0000(CConvertStorageParam const&) const
    "08de9220",  # CStorageConverterBase::Ext0006toInt0000(CConvertStorageParam const&) const
    "08de9240",  # CStorageConverterBase::Ext0007toInt0000(CConvertStorageParam const&) const
    "08de9260",  # CStorageConverterBase::Ext0008toInt0000(CConvertStorageParam const&) const
    "08de9280",  # CStorageConverterBase::Ext0009toInt0000(CConvertStorageParam const&) const
    "08de92a0",  # CStorageConverterBase::Ext000AtoInt0000(CConvertStorageParam const&) const
    "08de92c0",  # CStorageConverterBase::Ext000BtoInt0000(CConvertStorageParam const&) const
    "08de92e0",  # CStorageConverterBase::Ext000CtoInt0000(CConvertStorageParam const&) const
    "08de9300",  # CStorageConverterBase::Ext000DtoInt0000(CConvertStorageParam const&) const
    "08de9320",  # CStorageConverterBase::Ext000EtoInt0000(CConvertStorageParam const&) const
    "08de9340",  # CStorageConverterBase::Ext000FtoInt0000(CConvertStorageParam const&) const
    "08de9360",  # CStorageConverterBase::Ext0000toInt0001(CConvertStorageParam const&) const
    "08de9370",  # CStorageConverterBase::Ext0001toInt0001(CConvertStorageParam const&) const
    "08de9380",  # CStorageConverterBase::Ext0002toInt0001(CConvertStorageParam const&) const
    "08de93a0",  # CStorageConverterBase::Ext0003toInt0001(CConvertStorageParam const&) const
    "08de93c0",  # CStorageConverterBase::Ext0004toInt0001(CConvertStorageParam const&) const
    "08de93e0",  # CStorageConverterBase::Ext0005toInt0001(CConvertStorageParam const&) const
    "08de9400",  # CStorageConverterBase::Ext0006toInt0001(CConvertStorageParam const&) const
    "08de9420",  # CStorageConverterBase::Ext0007toInt0001(CConvertStorageParam const&) const
    "08de9440",  # CStorageConverterBase::Ext0008toInt0001(CConvertStorageParam const&) const
    "08de9460",  # CStorageConverterBase::Ext0009toInt0001(CConvertStorageParam const&) const
    "08de9480",  # CStorageConverterBase::Ext000AtoInt0001(CConvertStorageParam const&) const
    "08de94a0",  # CStorageConverterBase::Ext000BtoInt0001(CConvertStorageParam const&) const
    "08de94c0",  # CStorageConverterBase::Ext000CtoInt0001(CConvertStorageParam const&) const
    "08de94e0",  # CStorageConverterBase::Ext000DtoInt0001(CConvertStorageParam const&) const
    "08de9500",  # CStorageConverterBase::Ext000EtoInt0001(CConvertStorageParam const&) const
    "08de9520",  # CStorageConverterBase::Ext000FtoInt0001(CConvertStorageParam const&) const
    "08de9540",  # CStorageConverterBase::Ext0000toInt0002(CConvertStorageParam const&) const
    "08de9550",  # CStorageConverterBase::Ext0001toInt0002(CConvertStorageParam const&) const
    "08de9560",  # CStorageConverterBase::Ext0002toInt0002(CConvertStorageParam const&) const
    "08de9570",  # CStorageConverterBase::Ext0003toInt0002(CConvertStorageParam const&) const
    "08de9590",  # CStorageConverterBase::Ext0004toInt0002(CConvertStorageParam const&) const
    "08de95b0",  # CStorageConverterBase::Ext0005toInt0002(CConvertStorageParam const&) const
    "08de95d0",  # CStorageConverterBase::Ext0006toInt0002(CConvertStorageParam const&) const
    "08de95f0",  # CStorageConverterBase::Ext0007toInt0002(CConvertStorageParam const&) const
    "08de9610",  # CStorageConverterBase::Ext0008toInt0002(CConvertStorageParam const&) const
    "08de9630",  # CStorageConverterBase::Ext0009toInt0002(CConvertStorageParam const&) const
    "08de9650",  # CStorageConverterBase::Ext000AtoInt0002(CConvertStorageParam const&) const
    "08de9670",  # CStorageConverterBase::Ext000BtoInt0002(CConvertStorageParam const&) const
    "08de9690",  # CStorageConverterBase::Ext000CtoInt0002(CConvertStorageParam const&) const
    "08de96b0",  # CStorageConverterBase::Ext000DtoInt0002(CConvertStorageParam const&) const
    "08de96d0",  # CStorageConverterBase::Ext000EtoInt0002(CConvertStorageParam const&) const
    "08de96f0",  # CStorageConverterBase::Ext000FtoInt0002(CConvertStorageParam const&) const
    "08de9710",  # CStorageConverterBase::Ext0000toInt0003(CConvertStorageParam const&) const
    "08de9720",  # CStorageConverterBase::Ext0001toInt0003(CConvertStorageParam const&) const
    "08de9730",  # CStorageConverterBase::Ext0002toInt0003(CConvertStorageParam const&) const
    "08de9740",  # CStorageConverterBase::Ext0003toInt0003(CConvertStorageParam const&) const
    "08de9750",  # CStorageConverterBase::Ext0004toInt0003(CConvertStorageParam const&) const
    "08de9770",  # CStorageConverterBase::Ext0005toInt0003(CConvertStorageParam const&) const
    "08de9790",  # CStorageConverterBase::Ext0006toInt0003(CConvertStorageParam const&) const
    "08de97b0",  # CStorageConverterBase::Ext0007toInt0003(CConvertStorageParam const&) const
    "08de97d0",  # CStorageConverterBase::Ext0008toInt0003(CConvertStorageParam const&) const
    "08de97f0",  # CStorageConverterBase::Ext0009toInt0003(CConvertStorageParam const&) const
    "08de9810",  # CStorageConverterBase::Ext000AtoInt0003(CConvertStorageParam const&) const
    "08de9830",  # CStorageConverterBase::Ext000BtoInt0003(CConvertStorageParam const&) const
    "08de9850",  # CStorageConverterBase::Ext000CtoInt0003(CConvertStorageParam const&) const
    "08de9870",  # CStorageConverterBase::Ext000DtoInt0003(CConvertStorageParam const&) const
    "08de9890",  # CStorageConverterBase::Ext000EtoInt0003(CConvertStorageParam const&) const
    "08de98b0",  # CStorageConverterBase::Ext000FtoInt0003(CConvertStorageParam const&) const
    "08de98d0",  # CStorageConverterBase::Ext0000toInt0004(CConvertStorageParam const&) const
    "08de98e0",  # CStorageConverterBase::Ext0001toInt0004(CConvertStorageParam const&) const
    "08de98f0",  # CStorageConverterBase::Ext0002toInt0004(CConvertStorageParam const&) const
    "08de9900",  # CStorageConverterBase::Ext0003toInt0004(CConvertStorageParam const&) const
    "08de9910",  # CStorageConverterBase::Ext0004toInt0004(CConvertStorageParam const&) const
    "08de9920",  # CStorageConverterBase::Ext0005toInt0004(CConvertStorageParam const&) const
    "08de9940",  # CStorageConverterBase::Ext0006toInt0004(CConvertStorageParam const&) const
    "08de9960",  # CStorageConverterBase::Ext0007toInt0004(CConvertStorageParam const&) const
    "08de9980",  # CStorageConverterBase::Ext0008toInt0004(CConvertStorageParam const&) const
    "08de99a0",  # CStorageConverterBase::Ext0009toInt0004(CConvertStorageParam const&) const
    "08de99c0",  # CStorageConverterBase::Ext000AtoInt0004(CConvertStorageParam const&) const
    "08de99e0",  # CStorageConverterBase::Ext000BtoInt0004(CConvertStorageParam const&) const
    "08de9a00",  # CStorageConverterBase::Ext000CtoInt0004(CConvertStorageParam const&) const
    "08de9a20",  # CStorageConverterBase::Ext000DtoInt0004(CConvertStorageParam const&) const
    "08de9a40",  # CStorageConverterBase::Ext000EtoInt0004(CConvertStorageParam const&) const
    "08de9a60",  # CStorageConverterBase::Ext000FtoInt0004(CConvertStorageParam const&) const
    "08de9a80",  # CStorageConverterBase::Ext0000toInt0005(CConvertStorageParam const&) const
    "08de9a90",  # CStorageConverterBase::Ext0001toInt0005(CConvertStorageParam const&) const
    "08de9aa0",  # CStorageConverterBase::Ext0002toInt0005(CConvertStorageParam const&) const
    "08de9ab0",  # CStorageConverterBase::Ext0003toInt0005(CConvertStorageParam const&) const
    "08de9ac0",  # CStorageConverterBase::Ext0004toInt0005(CConvertStorageParam const&) const
    "08de9ad0",  # CStorageConverterBase::Ext0005toInt0005(CConvertStorageParam const&) const
    "08de9ae0",  # CStorageConverterBase::Ext0006toInt0005(CConvertStorageParam const&) const
    "08de9b00",  # CStorageConverterBase::Ext0007toInt0005(CConvertStorageParam const&) const
    "08de9b20",  # CStorageConverterBase::Ext0008toInt0005(CConvertStorageParam const&) const
    "08de9b40",  # CStorageConverterBase::Ext0009toInt0005(CConvertStorageParam const&) const
    "08de9b60",  # CStorageConverterBase::Ext000AtoInt0005(CConvertStorageParam const&) const
    "08de9b80",  # CStorageConverterBase::Ext000BtoInt0005(CConvertStorageParam const&) const
    "08de9ba0",  # CStorageConverterBase::Ext000CtoInt0005(CConvertStorageParam const&) const
    "08de9bc0",  # CStorageConverterBase::Ext000DtoInt0005(CConvertStorageParam const&) const
    "08de9be0",  # CStorageConverterBase::Ext000EtoInt0005(CConvertStorageParam const&) const
    "08de9c00",  # CStorageConverterBase::Ext000FtoInt0005(CConvertStorageParam const&) const
    "08de9c20",  # CStorageConverterBase::Ext0000toInt0006(CConvertStorageParam const&) const
    "08de9c30",  # CStorageConverterBase::Ext0001toInt0006(CConvertStorageParam const&) const
    "08de9c40",  # CStorageConverterBase::Ext0002toInt0006(CConvertStorageParam const&) const
    "08de9c50",  # CStorageConverterBase::Ext0003toInt0006(CConvertStorageParam const&) const
    "08de9c60",  # CStorageConverterBase::Ext0004toInt0006(CConvertStorageParam const&) const
    "08de9c70",  # CStorageConverterBase::Ext0005toInt0006(CConvertStorageParam const&) const
    "08de9c80",  # CStorageConverterBase::Ext0006toInt0006(CConvertStorageParam const&) const
    "08de9c90",  # CStorageConverterBase::Ext0007toInt0006(CConvertStorageParam const&) const
    "08de9cb0",  # CStorageConverterBase::Ext0008toInt0006(CConvertStorageParam const&) const
    "08de9cd0",  # CStorageConverterBase::Ext0009toInt0006(CConvertStorageParam const&) const
    "08de9cf0",  # CStorageConverterBase::Ext000AtoInt0006(CConvertStorageParam const&) const
    "08de9d10",  # CStorageConverterBase::Ext000BtoInt0006(CConvertStorageParam const&) const
    "08de9d30",  # CStorageConverterBase::Ext000CtoInt0006(CConvertStorageParam const&) const
    "08de9d50",  # CStorageConverterBase::Ext000DtoInt0006(CConvertStorageParam const&) const
    "08de9d70",  # CStorageConverterBase::Ext000EtoInt0006(CConvertStorageParam const&) const
    "08de9d90",  # CStorageConverterBase::Ext000FtoInt0006(CConvertStorageParam const&) const
    "08de9db0",  # CStorageConverterBase::Ext0000toInt0007(CConvertStorageParam const&) const
    "08de9dc0",  # CStorageConverterBase::Ext0001toInt0007(CConvertStorageParam const&) const
    "08de9dd0",  # CStorageConverterBase::Ext0002toInt0007(CConvertStorageParam const&) const
    "08de9de0",  # CStorageConverterBase::Ext0003toInt0007(CConvertStorageParam const&) const
    "08de9df0",  # CStorageConverterBase::Ext0004toInt0007(CConvertStorageParam const&) const
    "08de9e00",  # CStorageConverterBase::Ext0005toInt0007(CConvertStorageParam const&) const
    "08de9e10",  # CStorageConverterBase::Ext0006toInt0007(CConvertStorageParam const&) const
    "08de9e20",  # CStorageConverterBase::Ext0007toInt0007(CConvertStorageParam const&) const
    "08de9e30",  # CStorageConverterBase::Ext0008toInt0007(CConvertStorageParam const&) const
    "08de9e50",  # CStorageConverterBase::Ext0009toInt0007(CConvertStorageParam const&) const
    "08de9e70",  # CStorageConverterBase::Ext000AtoInt0007(CConvertStorageParam const&) const
    "08de9e90",  # CStorageConverterBase::Ext000BtoInt0007(CConvertStorageParam const&) const
    "08de9eb0",  # CStorageConverterBase::Ext000CtoInt0007(CConvertStorageParam const&) const
    "08de9ed0",  # CStorageConverterBase::Ext000DtoInt0007(CConvertStorageParam const&) const
    "08de9ef0",  # CStorageConverterBase::Ext000EtoInt0007(CConvertStorageParam const&) const
    "08de9f10",  # CStorageConverterBase::Ext000FtoInt0007(CConvertStorageParam const&) const
    "08de9f30",  # CStorageConverterBase::Ext0000toInt0008(CConvertStorageParam const&) const
    "08de9f40",  # CStorageConverterBase::Ext0001toInt0008(CConvertStorageParam const&) const
    "08de9f50",  # CStorageConverterBase::Ext0002toInt0008(CConvertStorageParam const&) const
    "08de9f60",  # CStorageConverterBase::Ext0003toInt0008(CConvertStorageParam const&) const
    "08de9f70",  # CStorageConverterBase::Ext0004toInt0008(CConvertStorageParam const&) const
    "08de9f80",  # CStorageConverterBase::Ext0005toInt0008(CConvertStorageParam const&) const
    "08de9f90",  # CStorageConverterBase::Ext0006toInt0008(CConvertStorageParam const&) const
    "08de9fa0",  # CStorageConverterBase::Ext0007toInt0008(CConvertStorageParam const&) const
    "08de9fb0",  # CStorageConverterBase::Ext0008toInt0008(CConvertStorageParam const&) const
    "08de9fc0",  # CStorageConverterBase::Ext0009toInt0008(CConvertStorageParam const&) const
    "08de9fe0",  # CStorageConverterBase::Ext000AtoInt0008(CConvertStorageParam const&) const
    "08dea000",  # CStorageConverterBase::Ext000BtoInt0008(CConvertStorageParam const&) const
    "08dea020",  # CStorageConverterBase::Ext000CtoInt0008(CConvertStorageParam const&) const
    "08dea040",  # CStorageConverterBase::Ext000DtoInt0008(CConvertStorageParam const&) const
    "08dea060",  # CStorageConverterBase::Ext000EtoInt0008(CConvertStorageParam const&) const
    "08dea080",  # CStorageConverterBase::Ext000FtoInt0008(CConvertStorageParam const&) const
    "08dea0a0",  # CStorageConverterBase::Ext0000toInt0009(CConvertStorageParam const&) const
    "08dea0b0",  # CStorageConverterBase::Ext0001toInt0009(CConvertStorageParam const&) const
    "08dea0c0",  # CStorageConverterBase::Ext0002toInt0009(CConvertStorageParam const&) const
    "08dea0d0",  # CStorageConverterBase::Ext0003toInt0009(CConvertStorageParam const&) const
    "08dea0e0",  # CStorageConverterBase::Ext0004toInt0009(CConvertStorageParam const&) const
    "08dea0f0",  # CStorageConverterBase::Ext0005toInt0009(CConvertStorageParam const&) const
    "08dea100",  # CStorageConverterBase::Ext0006toInt0009(CConvertStorageParam const&) const
    "08dea110",  # CStorageConverterBase::Ext0007toInt0009(CConvertStorageParam const&) const
    "08dea120",  # CStorageConverterBase::Ext0008toInt0009(CConvertStorageParam const&) const
    "08dea130",  # CStorageConverterBase::Ext0009toInt0009(CConvertStorageParam const&) const
    "08dea140",  # CStorageConverterBase::Ext000AtoInt0009(CConvertStorageParam const&) const
    "08dea160",  # CStorageConverterBase::Ext000BtoInt0009(CConvertStorageParam const&) const
    "08dea180",  # CStorageConverterBase::Ext000CtoInt0009(CConvertStorageParam const&) const
    "08dea1a0",  # CStorageConverterBase::Ext000DtoInt0009(CConvertStorageParam const&) const
    "08dea1c0",  # CStorageConverterBase::Ext000EtoInt0009(CConvertStorageParam const&) const
    "08dea1e0",  # CStorageConverterBase::Ext000FtoInt0009(CConvertStorageParam const&) const
    "08dea200",  # CStorageConverterBase::Ext0000toInt000A(CConvertStorageParam const&) const
    "08dea210",  # CStorageConverterBase::Ext0001toInt000A(CConvertStorageParam const&) const
    "08dea220",  # CStorageConverterBase::Ext0002toInt000A(CConvertStorageParam const&) const
    "08dea230",  # CStorageConverterBase::Ext0003toInt000A(CConvertStorageParam const&) const
    "08dea240",  # CStorageConverterBase::Ext0004toInt000A(CConvertStorageParam const&) const
    "08dea250",  # CStorageConverterBase::Ext0005toInt000A(CConvertStorageParam const&) const
    "08dea260",  # CStorageConverterBase::Ext0006toInt000A(CConvertStorageParam const&) const
    "08dea270",  # CStorageConverterBase::Ext0007toInt000A(CConvertStorageParam const&) const
    "08dea280",  # CStorageConverterBase::Ext0008toInt000A(CConvertStorageParam const&) const
    "08dea290",  # CStorageConverterBase::Ext0009toInt000A(CConvertStorageParam const&) const
    "08dea2a0",  # CStorageConverterBase::Ext000AtoInt000A(CConvertStorageParam const&) const
    "08dea2b0",  # CStorageConverterBase::Ext000BtoInt000A(CConvertStorageParam const&) const
    "08dea2d0",  # CStorageConverterBase::Ext000CtoInt000A(CConvertStorageParam const&) const
    "08dea2f0",  # CStorageConverterBase::Ext000DtoInt000A(CConvertStorageParam const&) const
    "08dea310",  # CStorageConverterBase::Ext000EtoInt000A(CConvertStorageParam const&) const
    "08dea330",  # CStorageConverterBase::Ext000FtoInt000A(CConvertStorageParam const&) const
    "08dea350",  # CStorageConverterBase::Ext0000toInt000B(CConvertStorageParam const&) const
    "08dea360",  # CStorageConverterBase::Ext0001toInt000B(CConvertStorageParam const&) const
    "08dea370",  # CStorageConverterBase::Ext0002toInt000B(CConvertStorageParam const&) const
    "08dea380",  # CStorageConverterBase::Ext0003toInt000B(CConvertStorageParam const&) const
    "08dea390",  # CStorageConverterBase::Ext0004toInt000B(CConvertStorageParam const&) const
    "08dea3a0",  # CStorageConverterBase::Ext0005toInt000B(CConvertStorageParam const&) const
    "08dea3b0",  # CStorageConverterBase::Ext0006toInt000B(CConvertStorageParam const&) const
    "08dea3c0",  # CStorageConverterBase::Ext0007toInt000B(CConvertStorageParam const&) const
    "08dea3d0",  # CStorageConverterBase::Ext0008toInt000B(CConvertStorageParam const&) const
    "08dea3e0",  # CStorageConverterBase::Ext0009toInt000B(CConvertStorageParam const&) const
    "08dea3f0",  # CStorageConverterBase::Ext000AtoInt000B(CConvertStorageParam const&) const
    "08dea400",  # CStorageConverterBase::Ext000BtoInt000B(CConvertStorageParam const&) const
    "08dea410",  # CStorageConverterBase::Ext000CtoInt000B(CConvertStorageParam const&) const
    "08dea430",  # CStorageConverterBase::Ext000DtoInt000B(CConvertStorageParam const&) const
    "08dea450",  # CStorageConverterBase::Ext000EtoInt000B(CConvertStorageParam const&) const
    "08dea470",  # CStorageConverterBase::Ext000FtoInt000B(CConvertStorageParam const&) const
    "08dea490",  # CStorageConverterBase::Ext0000toInt000C(CConvertStorageParam const&) const
    "08dea4a0",  # CStorageConverterBase::Ext0001toInt000C(CConvertStorageParam const&) const
    "08dea4b0",  # CStorageConverterBase::Ext0002toInt000C(CConvertStorageParam const&) const
    "08dea4c0",  # CStorageConverterBase::Ext0003toInt000C(CConvertStorageParam const&) const
    "08dea4d0",  # CStorageConverterBase::Ext0004toInt000C(CConvertStorageParam const&) const
    "08dea4e0",  # CStorageConverterBase::Ext0005toInt000C(CConvertStorageParam const&) const
    "08dea4f0",  # CStorageConverterBase::Ext0006toInt000C(CConvertStorageParam const&) const
    "08dea500",  # CStorageConverterBase::Ext0007toInt000C(CConvertStorageParam const&) const
    "08dea510",  # CStorageConverterBase::Ext0008toInt000C(CConvertStorageParam const&) const
    "08dea520",  # CStorageConverterBase::Ext0009toInt000C(CConvertStorageParam const&) const
    "08dea530",  # CStorageConverterBase::Ext000AtoInt000C(CConvertStorageParam const&) const
    "08dea540",  # CStorageConverterBase::Ext000BtoInt000C(CConvertStorageParam const&) const
    "08dea550",  # CStorageConverterBase::Ext000CtoInt000C(CConvertStorageParam const&) const
    "08dea560",  # CStorageConverterBase::Ext000DtoInt000C(CConvertStorageParam const&) const
    "08dea580",  # CStorageConverterBase::Ext000EtoInt000C(CConvertStorageParam const&) const
    "08dea5a0",  # CStorageConverterBase::Ext000FtoInt000C(CConvertStorageParam const&) const
    "08dea5c0",  # CStorageConverterBase::Ext0000toInt000D(CConvertStorageParam const&) const
    "08dea5d0",  # CStorageConverterBase::Ext0001toInt000D(CConvertStorageParam const&) const
    "08dea5e0",  # CStorageConverterBase::Ext0002toInt000D(CConvertStorageParam const&) const
    "08dea5f0",  # CStorageConverterBase::Ext0003toInt000D(CConvertStorageParam const&) const
    "08dea600",  # CStorageConverterBase::Ext0004toInt000D(CConvertStorageParam const&) const
    "08dea610",  # CStorageConverterBase::Ext0005toInt000D(CConvertStorageParam const&) const
    "08dea620",  # CStorageConverterBase::Ext0006toInt000D(CConvertStorageParam const&) const
    "08dea630",  # CStorageConverterBase::Ext0007toInt000D(CConvertStorageParam const&) const
    "08dea640",  # CStorageConverterBase::Ext0008toInt000D(CConvertStorageParam const&) const
    "08dea650",  # CStorageConverterBase::Ext0009toInt000D(CConvertStorageParam const&) const
    "08dea660",  # CStorageConverterBase::Ext000AtoInt000D(CConvertStorageParam const&) const
    "08dea670",  # CStorageConverterBase::Ext000BtoInt000D(CConvertStorageParam const&) const
    "08dea680",  # CStorageConverterBase::Ext000CtoInt000D(CConvertStorageParam const&) const
    "08dea690",  # CStorageConverterBase::Ext000DtoInt000D(CConvertStorageParam const&) const
    "08dea6a0",  # CStorageConverterBase::Ext000EtoInt000D(CConvertStorageParam const&) const
    "08dea6c0",  # CStorageConverterBase::Ext000FtoInt000D(CConvertStorageParam const&) const
    "08dea6e0",  # CStorageConverterBase::Ext0000toInt000E(CConvertStorageParam const&) const
    "08dea6f0",  # CStorageConverterBase::Ext0001toInt000E(CConvertStorageParam const&) const
    "08dea700",  # CStorageConverterBase::Ext0002toInt000E(CConvertStorageParam const&) const
    "08dea710",  # CStorageConverterBase::Ext0003toInt000E(CConvertStorageParam const&) const
    "08dea720",  # CStorageConverterBase::Ext0004toInt000E(CConvertStorageParam const&) const
    "08dea730",  # CStorageConverterBase::Ext0005toInt000E(CConvertStorageParam const&) const
    "08dea740",  # CStorageConverterBase::Ext0006toInt000E(CConvertStorageParam const&) const
    "08dea750",  # CStorageConverterBase::Ext0007toInt000E(CConvertStorageParam const&) const
    "08dea760",  # CStorageConverterBase::Ext0008toInt000E(CConvertStorageParam const&) const
    "08dea770",  # CStorageConverterBase::Ext0009toInt000E(CConvertStorageParam const&) const
    "08dea780",  # CStorageConverterBase::Ext000AtoInt000E(CConvertStorageParam const&) const
    "08dea790",  # CStorageConverterBase::Ext000BtoInt000E(CConvertStorageParam const&) const
    "08dea7a0",  # CStorageConverterBase::Ext000CtoInt000E(CConvertStorageParam const&) const
    "08dea7b0",  # CStorageConverterBase::Ext000DtoInt000E(CConvertStorageParam const&) const
    "08dea7c0",  # CStorageConverterBase::Ext000EtoInt000E(CConvertStorageParam const&) const
    "08dea7d0",  # CStorageConverterBase::Ext000FtoInt000E(CConvertStorageParam const&) const
    "08dea7f0",  # CStorageConverterBase::Ext0000toInt000F(CConvertStorageParam const&) const
    "08dea800",  # CStorageConverterBase::Ext0001toInt000F(CConvertStorageParam const&) const
    "08dea810",  # CStorageConverterBase::Ext0002toInt000F(CConvertStorageParam const&) const
    "08dea820",  # CStorageConverterBase::Ext0003toInt000F(CConvertStorageParam const&) const
    "08dea830",  # CStorageConverterBase::Ext0004toInt000F(CConvertStorageParam const&) const
    "08dea840",  # CStorageConverterBase::Ext0005toInt000F(CConvertStorageParam const&) const
    "08dea850",  # CStorageConverterBase::Ext0006toInt000F(CConvertStorageParam const&) const
    "08dea860",  # CStorageConverterBase::Ext0007toInt000F(CConvertStorageParam const&) const
    "08dea870",  # CStorageConverterBase::Ext0008toInt000F(CConvertStorageParam const&) const
    "08dea880",  # CStorageConverterBase::Ext0009toInt000F(CConvertStorageParam const&) const
    "08dea890",  # CStorageConverterBase::Ext000AtoInt000F(CConvertStorageParam const&) const
    "08dea8a0",  # CStorageConverterBase::Ext000BtoInt000F(CConvertStorageParam const&) const
    "08dea8b0",  # CStorageConverterBase::Ext000CtoInt000F(CConvertStorageParam const&) const
    "08dea8c0",  # CStorageConverterBase::Ext000DtoInt000F(CConvertStorageParam const&) const
    "08dea8d0",  # CStorageConverterBase::Ext000EtoInt000F(CConvertStorageParam const&) const
    "08dea8e0",  # CStorageConverterBase::Ext000FtoInt000F(CConvertStorageParam const&) const

    # --- 2026-07-28 follow-up batch: traced CStorageConverterBase::Open()'s 2 real
    # external callers to CProgConverter::Open() (NOT CFilesys/CDiskUtil as the prior
    # batch's own lead note guessed), which surfaced a whole ~32-class, ~246-method
    # concrete file-format-converter family (storage_format_converters.h). This batch:
    # CStorageConverterBase's own 16 ValidateExtXXXX + Close() (deferred by the prior
    # batch, small enough to finish here), 32 "safe" (no `this` dependency) sibling
    # ValidateExtXXXX across 18 of those classes, and CProgConverter's dtor pair +
    # Close(). CProgConverter's Open()/Load()/Save() and 11 CPCMProgConverter/
    # CMOSSProgConverter ValidateExtXXXX stay deferred -- see storage_converter_base.h/
    # prog_converter.h/storage_format_converters.h for exactly why each.
    "08e07bb0",  # CStorageConverterBase::ValidateExt0000(CConvertStorageParam const&) const
    "08e07bc0",  # CStorageConverterBase::ValidateExt0001(CConvertStorageParam const&) const
    "08e07bd0",  # CStorageConverterBase::ValidateExt0002(CConvertStorageParam const&) const
    "08e07be0",  # CStorageConverterBase::ValidateExt0003(CConvertStorageParam const&) const
    "08e07bf0",  # CStorageConverterBase::ValidateExt0004(CConvertStorageParam const&) const
    "08e07c00",  # CStorageConverterBase::ValidateExt0005(CConvertStorageParam const&) const
    "08e07c10",  # CStorageConverterBase::ValidateExt0006(CConvertStorageParam const&) const
    "08e07c20",  # CStorageConverterBase::ValidateExt0007(CConvertStorageParam const&) const
    "08e07c30",  # CStorageConverterBase::ValidateExt0008(CConvertStorageParam const&) const
    "08e07c40",  # CStorageConverterBase::ValidateExt0009(CConvertStorageParam const&) const
    "08e07c50",  # CStorageConverterBase::ValidateExt000A(CConvertStorageParam const&) const
    "08e07c60",  # CStorageConverterBase::ValidateExt000B(CConvertStorageParam const&) const
    "08e07c70",  # CStorageConverterBase::ValidateExt000C(CConvertStorageParam const&) const
    "08e07c80",  # CStorageConverterBase::ValidateExt000D(CConvertStorageParam const&) const
    "08e07c90",  # CStorageConverterBase::ValidateExt000E(CConvertStorageParam const&) const
    "08e07ca0",  # CStorageConverterBase::ValidateExt000F(CConvertStorageParam const&) const
    "08e07ba0",  # CStorageConverterBase::Close()
    "08df78c0",  # CCombiConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08df78d0",  # CCombiConverter::ValidateExt0001(CConvertStorageParam const&) const
    "08df78f0",  # CCombiConverter::ValidateExt0002(CConvertStorageParam const&) const
    "08df7920",  # CCombiConverter::ValidateExt0003(CConvertStorageParam const&) const
    "08dfc700",  # CDrumKitConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08dfc710",  # CDrumKitConverter::ValidateExt0001(CConvertStorageParam const&) const
    "08dfc720",  # CDrumKitConverter::ValidateExt0002(CConvertStorageParam const&) const
    "08dfc730",  # CDrumKitConverter::ValidateExt0003(CConvertStorageParam const&) const
    "08dfcf80",  # CWaveSeqConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08dfcf90",  # CWaveSeqConverter::ValidateExt0001(CConvertStorageParam const&) const
    "08dfd150",  # CGlobalConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08dfd160",  # CGlobalConverter::ValidateExt0001(CConvertStorageParam const&) const
    "08dfd170",  # CGlobalConverter::ValidateExt0002(CConvertStorageParam const&) const
    "08dfd2d0",  # CGEConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08dfd390",  # CGETemplateConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08dfd440",  # CSongDescConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08dfd450",  # CPatternDescConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08dfd460",  # CCueListConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08dfd470",  # CRegionConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08dfd480",  # CRegionConverter::ValidateExt0001(CConvertStorageParam const&) const
    "08dfd630",  # CSongControlConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08dfd6b0",  # CMidiEventConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08dfd6c0",  # CMasterEventConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08dfd6d0",  # CAudioEventConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08dfd6e0",  # CAutomationEventConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08e02dc0",  # CPatternEventConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08e02dd0",  # CSetListConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08e09f30",  # CSongConverter::ValidateExt0000(CConvertStorageParam const&) const
    "08e09f40",  # CSongConverter::ValidateExt0001(CConvertStorageParam const&) const
    "08e09f50",  # CSongConverter::ValidateExt0002(CConvertStorageParam const&) const
    "08e031f0",  # CSongConverter::ValidateExt0003(CConvertStorageParam const&) const
    "08e07d30",  # CMOSSProgConverter::ValidateExt0004(CConvertStorageParam const&) const
    "08e07d90",  # CProgConverter::~CProgConverter() (D1, complete object)
    "08e07db0",  # CProgConverter::~CProgConverter() (D0, deleting)
    "08df7590",  # CProgConverter::Close()

    # --- 2026-07-28 follow-up-follow-up batch: the real ExtXXXXtoIntYYYY/
    # IntXXXXtoExtYYYY conversion payload (not just ValidateExtXXXX predicates).
    # Found this is NOT a scriptable matrix like the base class's own 256-method
    # one (sizes 1B..0x14c7B, real per-format field-migration/legacy-copy logic) --
    # reconstructed the tractable <~0x200B subset (26 methods) plus 1 new base
    # method (Int0000toExt0000, needed by 3 sibling thunks). Also fixed a real bug
    # this pass: CConvertStorageParam's m_externalBuf/m_size fields were swapped
    # relative to their true offsets (storage_converter_base.h's own correction
    # note has the full derivation) -- affects every already-shipped memcpy-based
    # body above plus the 6 BUFID ValidateExtXXXX formulas (base ValidateExt0000
    # + the 5 "event converters" above), none of which need new manifest rows
    # (already counted) but whose SOURCE changed.
    "08dea920",  # CStorageConverterBase::Int0000toExt0000(CConvertStorageParam const&) const
    "08dfd180",  # CGlobalConverter::Int0002toExt0002(CConvertStorageParam const&) const
    "08dfd620",  # CRegionConverter::Int0001toExt0001(CConvertStorageParam const&) const
    "08dfcfa0",  # CWaveSeqConverter::Int0001toExt0001(CConvertStorageParam const&) const
    "08e07d20",  # CMOSSProgConverter::Ext0004toInt0005(CConvertStorageParam const&) const
    "08dfd190",  # CGlobalConverter::Ext0002toInt0002(CConvertStorageParam const&) const
    "08dfd2f0",  # CGEConverter::Ext0000toInt0000(CConvertStorageParam const&) const
    "08dfd490",  # CRegionConverter::Ext0001toInt0001(CConvertStorageParam const&) const
    "08dfd3a0",  # CGETemplateConverter::Ext0000toInt0000(CConvertStorageParam const&) const
    "08df7940",  # CCombiConverter::Int0003toExt0003(CConvertStorageParam const&) const
    "08dfc740",  # CDrumKitConverter::Int0003toExt0003(CConvertStorageParam const&) const
    "08df47e0",  # CPCMProgConverter::Int0005toExt0005(CConvertStorageParam const&) const
    "08df5c50",  # CMOSSProgConverter::Int0005toExt0005(CConvertStorageParam const&) const
    "08dfc770",  # CDrumKitConverter::Ext0003toInt0003(CConvertStorageParam const&) const
    "08dfcfb0",  # CWaveSeqConverter::Ext0001toInt0001(CConvertStorageParam const&) const
    "08df4f60",  # CPCMProgConverter::Ext0005toInt0005(CConvertStorageParam const&) const
    "08df6620",  # CMOSSProgConverter::Ext0005toInt0005(CConvertStorageParam const&) const
    "08dfd1a0",  # CGlobalConverter::Ext0001toInt0002(CConvertStorageParam const&) const
    "08dfd230",  # CGlobalConverter::Ext0000toInt0002(CConvertStorageParam const&) const
    "08dfd560",  # CRegionConverter::Ext0000toInt0001(CConvertStorageParam const&) const
    "08dfd4c0",  # CRegionConverter::Int0000toExt0000(CConvertStorageParam const&) const
    "08dfd320",  # CGEConverter::Int0000toExt0000(CConvertStorageParam const&) const
    "08dfd3d0",  # CGETemplateConverter::Int0000toExt0000(CConvertStorageParam const&) const
    "08dfd640",  # CSongControlConverter::Int0000toExt0000(CConvertStorageParam const&) const
    "08e03200",  # CSongConverter::Ext0003toInt0003(CConvertStorageParam const&) const
    "08df42f0",  # CProgCombiSongCommonConverter::ConvertToCurrent(CProgCombiSongCommon*, CProgCombiSongCommon0000 const*)

    # 2026-07-28 second follow-up batch: resolved all 3 of the prior pass's
    # documented deferrals (storage_format_converters.h's own top comment has
    # the full derivation -- CDrumKitConverter/CWaveSeqConverter's deferrals
    # turned out to share one dependency, now traced; CProgAncestorConverter
    # was a scope choice, not a blocker, and is now in scope).
    "08dfc620",  # ConvertPartTo0003(CInstPart*, CInstPart0000 const*) -- local/static helper
    "08dfce50",  # CDrumKitConverter::Ext0002toInt0003(CConvertStorageParam const&) const
    "08dfcfe0",  # CWaveSeqConverter::Ext0000toInt0001(CConvertStorageParam const&) const
    "08df4360",  # CProgAncestorConverter::ConvertToCurrent(CProgAncestor*, CProgAncestor0000 const*)
    "08df44c0",  # CProgAncestorConverter::ConvertToCurrent(CProgAncestor*, CProgAncestor0003OASYS const*)

    # 2026-07-28 fresh nm -C class-inventory sweep: CSpecialFuncCCMap (real
    # MIDI-CC-remap data class, special_func_cc_map.h) + its thin CGlobal
    # forwarder (cglobal.h) -- 104 + 101 = 205 methods. Deferred: Save/Load/
    # Update (real FMApi file I/O) and HasMatchingMapping (only reachable from
    # the out-of-scope CESxxxTask "CSK model layer"), plus CGlobal's other 18
    # unrelated methods (category names, drum-track/setlist init, external-
    # setup array) -- none traced this pass.
    "08a42220",  # CSpecialFuncCCMap::DownloadAllToSTG() const
    "08a42cf0",  # CSpecialFuncCCMap::ResetAssignments()
    "08a42e40",  # CSpecialFuncCCMap::Initialize()
    "08a42f00",  # CSpecialFuncCCMap::GetProgramUpMIDIChannel(unsigned char*)
    "08a42f10",  # CSpecialFuncCCMap::SetProgramUpMIDIChannel(char const*)
    "08a42f30",  # CSpecialFuncCCMap::GetProgramUpCCAssign(char*)
    "08a42f40",  # CSpecialFuncCCMap::SetProgramUpCCAssign(char const*)
    "08a42f60",  # CSpecialFuncCCMap::GetProgramDownMIDIChannel(unsigned char*)
    "08a42f70",  # CSpecialFuncCCMap::SetProgramDownMIDIChannel(char const*)
    "08a42f90",  # CSpecialFuncCCMap::GetProgramDownCCAssign(char*)
    "08a42fa0",  # CSpecialFuncCCMap::SetProgramDownCCAssign(char const*)
    "08a42fc0",  # CSpecialFuncCCMap::GetSongStartMIDIChannel(unsigned char*)
    "08a42fd0",  # CSpecialFuncCCMap::SetSongStartMIDIChannel(char const*)
    "08a42ff0",  # CSpecialFuncCCMap::GetSongStartCCAssign(char*)
    "08a43000",  # CSpecialFuncCCMap::SetSongStartCCAssign(char const*)
    "08a43020",  # CSpecialFuncCCMap::GetSongPunchMIDIChannel(unsigned char*)
    "08a43030",  # CSpecialFuncCCMap::SetSongPunchMIDIChannel(char const*)
    "08a43050",  # CSpecialFuncCCMap::GetSongPunchCCAssign(char*)
    "08a43060",  # CSpecialFuncCCMap::SetSongPunchCCAssign(char const*)
    "08a43080",  # CSpecialFuncCCMap::GetTapTempoMIDIChannel(unsigned char*)
    "08a43090",  # CSpecialFuncCCMap::SetTapTempoMIDIChannel(char const*)
    "08a430b0",  # CSpecialFuncCCMap::GetTapTempoCCAssign(char*)
    "08a430c0",  # CSpecialFuncCCMap::SetTapTempoCCAssign(char const*)
    "08a430e0",  # CSpecialFuncCCMap::GetOctaveUpMIDIChannel(unsigned char*)
    "08a430f0",  # CSpecialFuncCCMap::SetOctaveUpMIDIChannel(char const*)
    "08a43110",  # CSpecialFuncCCMap::GetOctaveUpCCAssign(char*)
    "08a43120",  # CSpecialFuncCCMap::SetOctaveUpCCAssign(char const*)
    "08a43140",  # CSpecialFuncCCMap::GetOctaveDownMIDIChannel(unsigned char*)
    "08a43150",  # CSpecialFuncCCMap::SetOctaveDownMIDIChannel(char const*)
    "08a43170",  # CSpecialFuncCCMap::GetOctaveDownCCAssign(char*)
    "08a43180",  # CSpecialFuncCCMap::SetOctaveDownCCAssign(char const*)
    "08a431a0",  # CSpecialFuncCCMap::GetRibbonLockMIDIChannel(unsigned char*)
    "08a431b0",  # CSpecialFuncCCMap::SetRibbonLockMIDIChannel(char const*)
    "08a431d0",  # CSpecialFuncCCMap::GetRibbonLockCCAssign(char*)
    "08a431e0",  # CSpecialFuncCCMap::SetRibbonLockCCAssign(char const*)
    "08a43200",  # CSpecialFuncCCMap::GetRTKnobFuncMIDIChannel(unsigned int, unsigned char*)
    "08a43220",  # CSpecialFuncCCMap::SetRTKnobFuncMIDIChannel(unsigned int, unsigned char const*)
    "08a43250",  # CSpecialFuncCCMap::GetRTKnobFuncCCAssign(unsigned int, char*)
    "08a43270",  # CSpecialFuncCCMap::SetRTKnobFuncCCAssign(unsigned int, char const*)
    "08a432a0",  # CSpecialFuncCCMap::GetPadFuncMIDIChannel(unsigned int, unsigned char*)
    "08a432c0",  # CSpecialFuncCCMap::SetPadFuncMIDIChannel(unsigned int, unsigned char const*)
    "08a432f0",  # CSpecialFuncCCMap::GetPadFuncCCAssign(unsigned int, char*)
    "08a43310",  # CSpecialFuncCCMap::SetPadFuncCCAssign(unsigned int, char const*)
    "08a43340",  # CSpecialFuncCCMap::GetJSXLockMIDIChannel(unsigned char*)
    "08a43350",  # CSpecialFuncCCMap::SetJSXLockMIDIChannel(char const*)
    "08a43370",  # CSpecialFuncCCMap::GetJSXLockCCAssign(char*)
    "08a43380",  # CSpecialFuncCCMap::SetJSXLockCCAssign(char const*)
    "08a433a0",  # CSpecialFuncCCMap::GetJSYLockMIDIChannel(unsigned char*)
    "08a433b0",  # CSpecialFuncCCMap::SetJSYLockMIDIChannel(char const*)
    "08a433d0",  # CSpecialFuncCCMap::GetJSYLockCCAssign(char*)
    "08a433e0",  # CSpecialFuncCCMap::SetJSYLockCCAssign(char const*)
    "08a43400",  # CSpecialFuncCCMap::GetJSPYLockMIDIChannel(unsigned char*)
    "08a43410",  # CSpecialFuncCCMap::SetJSPYLockMIDIChannel(char const*)
    "08a43430",  # CSpecialFuncCCMap::GetJSPYLockCCAssign(char*)
    "08a43440",  # CSpecialFuncCCMap::SetJSPYLockCCAssign(char const*)
    "08a43460",  # CSpecialFuncCCMap::GetJSMYLockMIDIChannel(unsigned char*)
    "08a43470",  # CSpecialFuncCCMap::SetJSMYLockMIDIChannel(char const*)
    "08a43490",  # CSpecialFuncCCMap::GetJSMYLockCCAssign(char*)
    "08a434a0",  # CSpecialFuncCCMap::SetJSMYLockCCAssign(char const*)
    "08a434c0",  # CSpecialFuncCCMap::GetJSXRibLockMIDIChannel(unsigned char*)
    "08a434d0",  # CSpecialFuncCCMap::SetJSXRibLockMIDIChannel(char const*)
    "08a434f0",  # CSpecialFuncCCMap::GetJSXRibLockCCAssign(char*)
    "08a43500",  # CSpecialFuncCCMap::SetJSXRibLockCCAssign(char const*)
    "08a43520",  # CSpecialFuncCCMap::GetJSYRibLockMIDIChannel(unsigned char*)
    "08a43530",  # CSpecialFuncCCMap::SetJSYRibLockMIDIChannel(char const*)
    "08a43550",  # CSpecialFuncCCMap::GetJSYRibLockCCAssign(char*)
    "08a43560",  # CSpecialFuncCCMap::SetJSYRibLockCCAssign(char const*)
    "08a43580",  # CSpecialFuncCCMap::GetJSPYRibLockMIDIChannel(unsigned char*)
    "08a43590",  # CSpecialFuncCCMap::SetJSPYRibLockMIDIChannel(char const*)
    "08a435b0",  # CSpecialFuncCCMap::GetJSPYRibLockCCAssign(char*)
    "08a435c0",  # CSpecialFuncCCMap::SetJSPYRibLockCCAssign(char const*)
    "08a435e0",  # CSpecialFuncCCMap::GetJSMYRibLockMIDIChannel(unsigned char*)
    "08a435f0",  # CSpecialFuncCCMap::SetJSMYRibLockMIDIChannel(char const*)
    "08a43610",  # CSpecialFuncCCMap::GetJSMYRibLockCCAssign(char*)
    "08a43620",  # CSpecialFuncCCMap::SetJSMYRibLockCCAssign(char const*)
    "08a43640",  # CSpecialFuncCCMap::GetSW1FuncMIDIChannel(unsigned char*)
    "08a43650",  # CSpecialFuncCCMap::SetSW1FuncMIDIChannel(char const*)
    "08a43670",  # CSpecialFuncCCMap::GetSW1FuncCCAssign(char*)
    "08a43680",  # CSpecialFuncCCMap::SetSW1FuncCCAssign(char const*)
    "08a436a0",  # CSpecialFuncCCMap::GetSW2FuncMIDIChannel(unsigned char*)
    "08a436b0",  # CSpecialFuncCCMap::SetSW2FuncMIDIChannel(char const*)
    "08a436d0",  # CSpecialFuncCCMap::GetSW2FuncCCAssign(char*)
    "08a436e0",  # CSpecialFuncCCMap::SetSW2FuncCCAssign(char const*)
    "08a43700",  # CSpecialFuncCCMap::GetIncFuncMIDIChannel(unsigned char*)
    "08a43710",  # CSpecialFuncCCMap::SetIncFuncMIDIChannel(char const*)
    "08a43730",  # CSpecialFuncCCMap::GetIncFuncCCAssign(char*)
    "08a43740",  # CSpecialFuncCCMap::SetIncFuncCCAssign(char const*)
    "08a43760",  # CSpecialFuncCCMap::GetDecFuncMIDIChannel(unsigned char*)
    "08a43770",  # CSpecialFuncCCMap::SetDecFuncMIDIChannel(char const*)
    "08a43790",  # CSpecialFuncCCMap::GetDecFuncCCAssign(char*)
    "08a437a0",  # CSpecialFuncCCMap::SetDecFuncCCAssign(char const*)
    "08a437c0",  # CSpecialFuncCCMap::GetChordSwMIDIChannel(unsigned char*)
    "08a437d0",  # CSpecialFuncCCMap::SetChordSwMIDIChannel(char const*)
    "08a437f0",  # CSpecialFuncCCMap::GetChordSwCCAssign(char*)
    "08a43800",  # CSpecialFuncCCMap::SetChordSwCCAssign(char const*)
    "08a43820",  # CSpecialFuncCCMap::GetDTrackEnableMIDIChannel(unsigned char*)
    "08a43830",  # CSpecialFuncCCMap::SetDTrackEnableMIDIChannel(char const*)
    "08a43850",  # CSpecialFuncCCMap::GetDTrackEnableCCAssign(char*)
    "08a43860",  # CSpecialFuncCCMap::SetDTrackEnableCCAssign(char const*)
    "08a43880",  # CSpecialFuncCCMap::GetAftertouchLockMIDIChannel(unsigned char*)
    "08a43890",  # CSpecialFuncCCMap::SetAftertouchLockMIDIChannel(char const*)
    "08a438b0",  # CSpecialFuncCCMap::GetAftertouchLockCCAssign(char*)
    "08a438c0",  # CSpecialFuncCCMap::SetAftertouchLockCCAssign(char const*)
    "08a438e0",  # CSpecialFuncCCMap::GetMappingName(ESpecialCCMapFunction, char const*&)
    "08a0e690",  # CGlobal::GetProgramUpMIDIChannel(unsigned char*)
    "08a0e6b0",  # CGlobal::SetProgramUpMIDIChannel(char const*)
    "08a0e6d0",  # CGlobal::GetProgramUpCCAssign(char*)
    "08a0e6f0",  # CGlobal::SetProgramUpCCAssign(char const*)
    "08a0e710",  # CGlobal::GetProgramDownMIDIChannel(unsigned char*)
    "08a0e730",  # CGlobal::SetProgramDownMIDIChannel(char const*)
    "08a0e750",  # CGlobal::GetProgramDownCCAssign(char*)
    "08a0e770",  # CGlobal::SetProgramDownCCAssign(char const*)
    "08a0e790",  # CGlobal::GetSongStartMIDIChannel(unsigned char*)
    "08a0e7b0",  # CGlobal::SetSongStartMIDIChannel(char const*)
    "08a0e7d0",  # CGlobal::GetSongStartCCAssign(char*)
    "08a0e7f0",  # CGlobal::SetSongStartCCAssign(char const*)
    "08a0e810",  # CGlobal::GetSongPunchMIDIChannel(unsigned char*)
    "08a0e830",  # CGlobal::SetSongPunchMIDIChannel(char const*)
    "08a0e850",  # CGlobal::GetSongPunchCCAssign(char*)
    "08a0e870",  # CGlobal::SetSongPunchCCAssign(char const*)
    "08a0e890",  # CGlobal::GetTapTempoMIDIChannel(unsigned char*)
    "08a0e8b0",  # CGlobal::SetTapTempoMIDIChannel(char const*)
    "08a0e8d0",  # CGlobal::GetTapTempoCCAssign(char*)
    "08a0e8f0",  # CGlobal::SetTapTempoCCAssign(char const*)
    "08a0e910",  # CGlobal::GetOctaveUpMIDIChannel(unsigned char*)
    "08a0e930",  # CGlobal::SetOctaveUpMIDIChannel(char const*)
    "08a0e950",  # CGlobal::GetOctaveUpCCAssign(char*)
    "08a0e970",  # CGlobal::SetOctaveUpCCAssign(char const*)
    "08a0e990",  # CGlobal::GetOctaveDownMIDIChannel(unsigned char*)
    "08a0e9b0",  # CGlobal::SetOctaveDownMIDIChannel(char const*)
    "08a0e9d0",  # CGlobal::GetOctaveDownCCAssign(char*)
    "08a0e9f0",  # CGlobal::SetOctaveDownCCAssign(char const*)
    "08a0ea10",  # CGlobal::GetRibbonLockMIDIChannel(unsigned char*)
    "08a0ea30",  # CGlobal::SetRibbonLockMIDIChannel(char const*)
    "08a0ea50",  # CGlobal::GetRibbonLockCCAssign(char*)
    "08a0ea70",  # CGlobal::SetRibbonLockCCAssign(char const*)
    "08a0ea90",  # CGlobal::GetRTKnobFuncMIDIChannel(unsigned int, unsigned char*)
    "08a0eab0",  # CGlobal::SetRTKnobFuncMIDIChannel(unsigned int, unsigned char const*)
    "08a0ead0",  # CGlobal::GetRTKnobFuncCCAssign(unsigned int, char*)
    "08a0eaf0",  # CGlobal::SetRTKnobFuncCCAssign(unsigned int, char const*)
    "08a0eb10",  # CGlobal::GetPadFuncMIDIChannel(unsigned int, unsigned char*)
    "08a0eb30",  # CGlobal::SetPadFuncMIDIChannel(unsigned int, unsigned char const*)
    "08a0eb50",  # CGlobal::GetPadFuncCCAssign(unsigned int, char*)
    "08a0eb70",  # CGlobal::SetPadFuncCCAssign(unsigned int, char const*)
    "08a0eb90",  # CGlobal::GetJSXLockMIDIChannel(unsigned char*)
    "08a0ebb0",  # CGlobal::SetJSXLockMIDIChannel(char const*)
    "08a0ebd0",  # CGlobal::GetJSXLockCCAssign(char*)
    "08a0ebf0",  # CGlobal::SetJSXLockCCAssign(char const*)
    "08a0ec10",  # CGlobal::GetJSYLockMIDIChannel(unsigned char*)
    "08a0ec30",  # CGlobal::SetJSYLockMIDIChannel(char const*)
    "08a0ec50",  # CGlobal::GetJSYLockCCAssign(char*)
    "08a0ec70",  # CGlobal::SetJSYLockCCAssign(char const*)
    "08a0ec90",  # CGlobal::GetJSPYLockMIDIChannel(unsigned char*)
    "08a0ecb0",  # CGlobal::SetJSPYLockMIDIChannel(char const*)
    "08a0ecd0",  # CGlobal::GetJSPYLockCCAssign(char*)
    "08a0ecf0",  # CGlobal::SetJSPYLockCCAssign(char const*)
    "08a0ed10",  # CGlobal::GetJSMYLockMIDIChannel(unsigned char*)
    "08a0ed30",  # CGlobal::SetJSMYLockMIDIChannel(char const*)
    "08a0ed50",  # CGlobal::GetJSMYLockCCAssign(char*)
    "08a0ed70",  # CGlobal::SetJSMYLockCCAssign(char const*)
    "08a0ed90",  # CGlobal::GetJSXRibLockMIDIChannel(unsigned char*)
    "08a0edb0",  # CGlobal::SetJSXRibLockMIDIChannel(char const*)
    "08a0edd0",  # CGlobal::GetJSXRibLockCCAssign(char*)
    "08a0edf0",  # CGlobal::SetJSXRibLockCCAssign(char const*)
    "08a0ee10",  # CGlobal::GetJSYRibLockMIDIChannel(unsigned char*)
    "08a0ee30",  # CGlobal::SetJSYRibLockMIDIChannel(char const*)
    "08a0ee50",  # CGlobal::GetJSYRibLockCCAssign(char*)
    "08a0ee70",  # CGlobal::SetJSYRibLockCCAssign(char const*)
    "08a0ee90",  # CGlobal::GetJSPYRibLockMIDIChannel(unsigned char*)
    "08a0eeb0",  # CGlobal::SetJSPYRibLockMIDIChannel(char const*)
    "08a0eed0",  # CGlobal::GetJSPYRibLockCCAssign(char*)
    "08a0eef0",  # CGlobal::SetJSPYRibLockCCAssign(char const*)
    "08a0ef10",  # CGlobal::GetJSMYRibLockMIDIChannel(unsigned char*)
    "08a0ef30",  # CGlobal::SetJSMYRibLockMIDIChannel(char const*)
    "08a0ef50",  # CGlobal::GetJSMYRibLockCCAssign(char*)
    "08a0ef70",  # CGlobal::SetJSMYRibLockCCAssign(char const*)
    "08a0ef90",  # CGlobal::GetSW1FuncMIDIChannel(unsigned char*)
    "08a0efb0",  # CGlobal::SetSW1FuncMIDIChannel(char const*)
    "08a0efd0",  # CGlobal::GetSW1FuncCCAssign(char*)
    "08a0eff0",  # CGlobal::SetSW1FuncCCAssign(char const*)
    "08a0f010",  # CGlobal::GetSW2FuncMIDIChannel(unsigned char*)
    "08a0f030",  # CGlobal::SetSW2FuncMIDIChannel(char const*)
    "08a0f050",  # CGlobal::GetSW2FuncCCAssign(char*)
    "08a0f070",  # CGlobal::SetSW2FuncCCAssign(char const*)
    "08a0f090",  # CGlobal::GetIncFuncMIDIChannel(unsigned char*)
    "08a0f0b0",  # CGlobal::SetIncFuncMIDIChannel(char const*)
    "08a0f0d0",  # CGlobal::GetIncFuncCCAssign(char*)
    "08a0f0f0",  # CGlobal::SetIncFuncCCAssign(char const*)
    "08a0f110",  # CGlobal::GetDecFuncMIDIChannel(unsigned char*)
    "08a0f130",  # CGlobal::SetDecFuncMIDIChannel(char const*)
    "08a0f150",  # CGlobal::GetDecFuncCCAssign(char*)
    "08a0f170",  # CGlobal::SetDecFuncCCAssign(char const*)
    "08a0f190",  # CGlobal::GetChordSwMIDIChannel(unsigned char*)
    "08a0f1b0",  # CGlobal::SetChordSwMIDIChannel(char const*)
    "08a0f1d0",  # CGlobal::GetChordSwCCAssign(char*)
    "08a0f1f0",  # CGlobal::SetChordSwCCAssign(char const*)
    "08a0f210",  # CGlobal::GetDTrackEnableMIDIChannel(unsigned char*)
    "08a0f230",  # CGlobal::SetDTrackEnableMIDIChannel(char const*)
    "08a0f250",  # CGlobal::GetDTrackEnableCCAssign(char*)
    "08a0f270",  # CGlobal::SetDTrackEnableCCAssign(char const*)
    "08a0f290",  # CGlobal::GetAftertouchLockMIDIChannel(unsigned char*)
    "08a0f2b0",  # CGlobal::SetAftertouchLockMIDIChannel(char const*)
    "08a0f2d0",  # CGlobal::GetAftertouchLockCCAssign(char*)
    "08a0f2f0",  # CGlobal::SetAftertouchLockCCAssign(char const*)
    "08a0f360",  # CGlobal::GetMappingName(ESpecialCCMapFunction, char const*&)

    # 2026-07-28 fresh nm -C class-inventory sweep, round 2: CRamSample /
    # CMultiSample / CRamSampleRelative (ram_sample.h/.cpp) -- 3 small,
    # non-virtual sample-metadata value classes found back-to-back in the
    # binary (.text+0x08427db0..0x08428340). Real callers: CFileAIF/CFileKSF/
    # CFileSng file-format parsers (not the excluded CESxxxTask family).
    # Deferred: CRamSample::operator=(CUsrSample&) (08427ff0) -- needs 2
    # unmodeled CUsrSample-internal sub-structs, see ram_sample.h.
    "08427db0",  # CRamSample::CRamSample()
    "08427dc0",  # CRamSample::~CRamSample()
    "08427dd0",  # CRamSample::GetFS()
    "08427de0",  # CRamSample::GetLoopTune()
    "08427df0",  # CRamSample::IsNotUse2ndStart()
    "08427e00",  # CRamSample::IsOneShot()
    "08427e10",  # CRamSample::IsPlus12dB()
    "08427e20",  # CRamSample::IsReverse()
    "08427e30",  # CRamSample::SetPlus12dB(int)
    "08427e50",  # CRamSample::GetBank()
    "08427e60",  # CRamSample::GetStartAddress(int)
    "08427e80",  # CRamSample::GetStartAddress()
    "08427e90",  # CRamSample::Get2ndStartAddress()
    "08427ea0",  # CRamSample::GetLoopStartAddress()
    "08427eb0",  # CRamSample::GetEndAddress()
    "08427ec0",  # CRamSample::SetFS(unsigned long)
    "08427ed0",  # CRamSample::SetLoopTune(int)
    "08427ee0",  # CRamSample::GetFlag()
    "08427ef0",  # CRamSample::SetFlag(unsigned int)
    "08427f00",  # CRamSample::SetNotUse2ndStart(int)
    "08427f20",  # CRamSample::SetBank(int)
    "08427f30",  # CRamSample::SetStartAddress(unsigned long)
    "08427f40",  # CRamSample::Set2ndStartAddress(unsigned long)
    "08427f50",  # CRamSample::SetLoopStartAddress(unsigned long)
    "08427f60",  # CRamSample::SetEndAddress(unsigned long)
    "08427f70",  # CRamSample::GetTopAddress()
    "08427f80",  # CRamSample::SetTopAddress(unsigned long)
    "08427f90",  # CRamSample::GetNumOfByte()
    "08427fa0",  # CRamSample::SetNumOfByte(unsigned long)
    "08427fb0",  # CRamSample::GetName()
    "08427fc0",  # CRamSample::Initialize(int, unsigned long, unsigned long)
    "084280f0",  # CMultiSample::CMultiSample()
    "08428100",  # CMultiSample::~CMultiSample()
    "08428110",  # CMultiSample::IsNotUse2ndStart()
    "08428120",  # CMultiSample::SetFlag(int)
    "08428130",  # CMultiSample::GetTopOfRelative()
    "08428140",  # CMultiSample::SetTopOfRelative(int)
    "08428150",  # CMultiSample::GetNumOfRelative()
    "08428160",  # CMultiSample::SetNumOfRelative(int)
    "08428170",  # CMultiSample::GetName()
    "08428180",  # CRamSampleRelative::CRamSampleRelative()
    "08428190",  # CRamSampleRelative::~CRamSampleRelative()
    "084281a0",  # CRamSampleRelative::GetSampleNumber()
    "084281b0",  # CRamSampleRelative::SetSampleNumber(int)
    "084281c0",  # CRamSampleRelative::GetSampleBank()
    "084281d0",  # CRamSampleRelative::SetSampleBank(int)
    "084281e0",  # CRamSampleRelative::GetTopKey()
    "084281f0",  # CRamSampleRelative::SetTopKey(int)
    "08428200",  # CRamSampleRelative::GetOriginalKey()
    "08428210",  # CRamSampleRelative::SetOriginalKey(int)
    "08428220",  # CRamSampleRelative::GetTune()
    "08428230",  # CRamSampleRelative::SetTune(int)
    "08428240",  # CRamSampleRelative::GetLevel()
    "08428250",  # CRamSampleRelative::SetLevel(int)
    "08428260",  # CRamSampleRelative::GetTranspose()
    "08428270",  # CRamSampleRelative::SetTranspose(int)
    "08428280",  # CRamSampleRelative::GetPan()
    "08428290",  # CRamSampleRelative::SetPan(int)
    "084282a0",  # CRamSampleRelative::SetupAsSkipped()
    "084282b0",  # CRamSampleRelative::IsSkipped()
    "084282c0",  # CRamSampleRelative::GetCutoff()
    "084282d0",  # CRamSampleRelative::SetCutoff(int)
    "084282e0",  # CRamSampleRelative::GetResonance()
    "084282f0",  # CRamSampleRelative::SetResonance(int)
    "08428300",  # CRamSampleRelative::GetAttack()
    "08428310",  # CRamSampleRelative::SetAttack(int)
    "08428320",  # CRamSampleRelative::GetDecay()
    "08428330",  # CRamSampleRelative::SetDecay(int)

    # 2026-07-28 fresh nm -C class-inventory sweep, round 3: CSector /
    # CLittleEndObj / CPartitionData / CMBR / CPBR / CPBRex / CPBRFat12Fat16 /
    # CPBRFat12 / CPBRFat16 / CPBRFat32 (partition_table.h/.cpp) -- real MBR
    # (Master Boot Record) and PBR (BIOS Parameter Block / FAT12-16-32
    # Extended BPB) on-disk-format value classes. Two other candidates traced
    # and REJECTED first: the ~40-class CSysExProg/CSysExCombi/... "SysEx dump
    # digest object" family (no real caller anywhere in a full objdump call-
    # xref sweep -- only plausible owner is the already-documented-elsewhere
    # "deep, entirely unmodeled" SysEx sniffer/tree-builder subsystem) and the
    # CD-ROM/Joliet virtual-driver cluster (CCDEntry/CDirCD/CVDrvCD/
    # CCDConfigDir/CVirtualDriverBase/CDriverTaskBase -- CBigEndObj's only
    # real callers are all inside file_io_base.h's own already-documented
    # "OUT OF SCOPE" optical-media driver family). Real callers for THIS
    # cluster (traced via the same objdump xref method): CFileMan::
    # ScanPartitionTable/FDisk/DelLastPartitionsNoCheck (via CMBR::CMBR) and
    # CConfigVD::ConfigVD2/ConfigVD3/IsDOSFormat/IsPartitionBootSector (via
    # CPBR::CPBR and subclasses) -- CFileMan itself is file_man.h's own
    # documented out-of-scope god object, but same "out-of-scope CALLER,
    # in-scope DATA class" split already established for CSpecialFuncCCMap's
    # CESGlobalTask caller. Every sm_wXxxOffset static byte-offset constant
    # independently confirmed (direct .data read) to match the real, public
    # MBR/BPB specification exactly. See partition_table.h for full detail.
    # CMBR's D0 (deleting-destructor) address included per the CDumpTask/
    # CSysExMsgTaskBase precedent (gen_manifest.py entries above) -- a
    # trivial, fully-understood "call D1 then operator delete" wrapper, not
    # separately hand-transcribed as its own C++ method.
    "080a0e80",  # CLittleEndObj::GetWord(unsigned char const*)
    "080a0ea0",  # CLittleEndObj::GetDWord(unsigned char const*)
    "080a0ed0",  # CLittleEndObj::SetWord(unsigned char*, unsigned short)
    "080a0ef0",  # CLittleEndObj::SetDWord(unsigned char*, unsigned long)
    "080a0f20",  # CLittleEndObj::GetInt(unsigned char const*)
    "080a0f50",  # CLittleEndObj::GetShort(unsigned char const*)
    "080a0f70",  # CLittleEndObj::GetLong(unsigned char const*)
    "080a0fa0",  # CLittleEndObj::GetUInt(unsigned char const*)
    "080a0fd0",  # CLittleEndObj::GetUShort(unsigned char const*)
    "080a0ff0",  # CLittleEndObj::GetULong(unsigned char const*)
    "080a1020",  # CLittleEndObj::SetUInt(unsigned char*, unsigned int)
    "080a1050",  # CLittleEndObj::GetU3Byte(unsigned char const*)
    "080d3a30",  # CMBR::~CMBR() (D1, non-deleting)
    "080d3a40",  # CMBR::~CMBR() (D0, deleting)
    "080d3a60",  # CSector::GetMinSize()
    "080d3a70",  # CPartitionData::CPartitionData(unsigned char const*)
    "080d3b60",  # CPartitionData::CPartitionData(CPartitionData const&)
    "080d3bb0",  # CPartitionData::Reset()
    "080d3c00",  # CPartitionData::operator=(CPartitionData const&)
    "080d3c60",  # CPartitionData::SetStatus(unsigned char*, EStatus)
    "080d3c80",  # CPartitionData::SetType(unsigned char*, EType)
    "080d3ca0",  # CPartitionData::IsEmpty(unsigned char const*)
    "080d3cc0",  # CPartitionData::IsExtended(unsigned char const*)
    "080d3cf0",  # CPartitionData::IsExtended(EType)
    "080d3d20",  # CPartitionData::IsEmpty(EType)
    "080d3d30",  # CPartitionData::IsSupported(EType)
    "080d3d80",  # CPartitionData::Reset(unsigned char*)
    "080d3e20",  # CPartitionData::GetMaxMediaGeometry()
    "080d3e30",  # CPartitionData::SetStartCHS(unsigned char*, ushort, ushort, ushort)
    "080d3eb0",  # CPartitionData::SetEndCHS(unsigned char*, ushort, ushort, ushort)
    "080d3f30",  # CPartitionData::SetLBAStartLocation(unsigned char*, unsigned long)
    "080d3f50",  # CPartitionData::GetLBAStartLocation(unsigned char const*)
    "080d3f70",  # CPartitionData::GetType(unsigned char const*)
    "080d3f80",  # CPartitionData::SetPartitionSize(unsigned char*, unsigned long)
    "080d3fa0",  # CPartitionData::IsExtended() const
    "080d3fd0",  # CPartitionData::IsSupported() const
    "080d4020",  # CPartitionData::IsEmpty() const
    "080d4030",  # CPartitionData::CHStoLBA(ushort, ushort, ushort, SMediaGeometry const&)
    "080d4160",  # CPartitionData::LBAtoCHS(uint, ushort&, ushort&, ushort&, SMediaGeometry const&)
    "080d4290",  # CPartitionData::GetSectCeilLBA(uint, unsigned long, SMediaGeometry const&)
    "080d42d0",  # CPartitionData::GetMaxCHS(ushort&, ushort&, ushort&, SMediaGeometry const&)
    "080d4310",  # CPartitionData::AdjustCHS(ushort&, ushort&, ushort&, SMediaGeometry const&)
    "080d43c0",  # CMBR::CMBR(unsigned char const*, ushort, ushort, unsigned long)
    "080d49d0",  # CMBR::GetFirstValidPrimaryPartitionData() const
    "080d4a90",  # CMBR::GetFirstValidPartitionData() const
    "080d4b10",  # CMBR::GetFirstValidPartitionData(int) const
    "080d4c80",  # CMBR::IsValid() const
    "080d4ca0",  # CPBR::CPBR(unsigned char const*)
    "080d4dd0",  # CPBR::SetBytePerSector(unsigned char*, unsigned short)
    "080d4e00",  # CPBR::SetSectPerCluster(unsigned char*, unsigned char)
    "080d4e20",  # CPBR::SetFatOffset(unsigned char*, unsigned short)
    "080d4e50",  # CPBR::SetFatNum(unsigned char*, unsigned char)
    "080d4e70",  # CPBR::SetMaxNumRootEntries(unsigned char*, unsigned short)
    "080d4ea0",  # CPBR::SetNumSectors(unsigned char*, unsigned short)
    "080d4ed0",  # CPBR::SetMediaType(unsigned char*, unsigned char)
    "080d4ef0",  # CPBR::SetNumFatSectors(unsigned char*, unsigned short)
    "080d4f20",  # CPBR::SetSectorPerTrack(unsigned char*, unsigned short)
    "080d4f50",  # CPBR::SetNumHeads(unsigned char*, unsigned short)
    "080d4f80",  # CPBR::SetNumHiddenSectors(unsigned char*, unsigned long)
    "080d4fa0",  # CPBR::SetNumSectorsHuge(unsigned char*, unsigned long)
    "080d4fc0",  # CPBR::GetDefaultFatNum()
    "080d4fd0",  # CPBRex::CPBRex(unsigned char const*)
    "080d4fe0",  # CPBRex::GetDefaultVolumeName()
    "080d4ff0",  # CPBRex::GetVolumeNameSize()
    "080d5000",  # CPBRex::GetNewPartitionSerialNum()
    "080d5020",  # CPBRFat12Fat16::CPBRFat12Fat16(unsigned char const*)
    "080d50c0",  # CPBRFat12Fat16::SetBeginSectSignature(unsigned char*)
    "080d5100",  # CPBRFat12Fat16::GetDefaultFatOffset()
    "080d5110",  # CPBRFat12Fat16::GetVolumeNameOffset()
    "080d5120",  # CPBRFat12Fat16::SetVolumeName(unsigned char*, char const*)
    "080d5160",  # CPBRFat12::CPBRFat12(unsigned char const*)
    "080d5200",  # CPBRFat12::SetFileSystemName(unsigned char*)
    "080d5240",  # CPBRFat12::GetDefaultFileSystemName()
    "080d5250",  # CPBRFat12::GetDefaultFileSystemNameLen()
    "080d5270",  # CPBRFat12::GetSectorPerFat(...)
    "080d52d0",  # CPBRFat16::CPBRFat16(unsigned char const*)
    "080d5370",  # CPBRFat16::SetFileSystemName(unsigned char*)
    "080d53b0",  # CPBRFat16::SetDriveNumber(unsigned char*)
    "080d53e0",  # CPBRFat16::SetExtSignature(unsigned char*)
    "080d53f0",  # CPBRFat16::SetPartitionSerialNum(unsigned char*)
    "080d5430",  # CPBRFat16::GetDefaultFileSystemName()
    "080d5440",  # CPBRFat16::GetDefaultFileSystemNameLen()
    "080d5460",  # CPBRFat16::GetDefaultMaxNumRootEntries()
    "080d5470",  # CPBRFat16::GetSectorPerFat(...)
    "080d54d0",  # CPBRFat32::CPBRFat32(unsigned char const*)
    "080d55f0",  # CPBRFat32::GetFirstRootClusterOffset()
    "080d5600",  # CPBRFat32::GetBackupPBROffsetDefault()
    "080d5610",  # CPBRFat32::GetNumCluster() const
    "080d5630",  # CPBRFat32::SetBeginSectSignature(unsigned char*)
    "080d5670",  # CPBRFat32::SetFileSystemName(unsigned char*)
    "080d56b0",  # CPBRFat32::SetNumFatSectorsHuge(unsigned char*, unsigned long)
    "080d56d0",  # CPBRFat32::SetFatFlag(unsigned char*, unsigned char, int)
    "080d5710",  # CPBRFat32::SetFat32Version(unsigned char*, unsigned char, unsigned char)
    "080d5750",  # CPBRFat32::SetFirstRootCluster(unsigned char*, unsigned long)
    "080d5770",  # CPBRFat32::SetFSISOffset(unsigned char*, unsigned short)
    "080d57a0",  # CPBRFat32::SetBackupPBROffset(unsigned char*)
    "080d57d0",  # CPBRFat32::SetDriveNumber(unsigned char*)
    "080d5800",  # CPBRFat32::SetExtSignature(unsigned char*)
    "080d5810",  # CPBRFat32::SetPartitionSerialNum(unsigned char*)
    "080d5850",  # CPBRFat32::SetVolumeName(unsigned char*, char const*)
    "080d5890",  # CPBRFat32::GetDefaultFileSystemName()
    "080d58a0",  # CPBRFat32::GetDefaultFileSystemNameLen()
    "080d58c0",  # CPBRFat32::GetDefaultFatOffset()
    "080d58d0",  # CPBRFat32::GetVolumeNameOffset()
    "080d58e0",  # CPBRFat32::GetSectorPerFat(...)
    "0818ab90",  # CSector::SetEndSectSignature(unsigned char*)

    # --- STriplet/CResInfo/CResEntry/CResEntryEx batch (2026-07-28) ---
    "08150f00",  # CResInfo::CResInfo()
    "08150e60",  # CResInfo::CResInfo(unsigned char const*)
    "08150f40",  # CResInfo::Deserialize(unsigned char const*)
    "08150fe0",  # CResInfo::Serialize(unsigned char*)
    "08151200",  # CResInfo::SizeOf()
    "08151210",  # CResInfo::SetResName(char const*)
    "08151240",  # CResInfo::ResetResName()
    "081507e0",  # CResEntry::Reset()
    "08150830",  # CResEntry::CResEntry(STriplet, char const*, uchar, uchar, int, int)
    "081508d0",  # CResEntry::CResEntry(STriplet, char const*, ushort, uchar, uchar)
    "08150970",  # CResEntry::CResEntry(CResEntry const&)
    "081509f0",  # CResEntry::operator=(CResEntry const&)
    "08150a60",  # CResEntry::Copy(CResEntry const&)
    "081903a0",  # CResEntry::~CResEntry() D1 (weak)
    "08190390",  # CResEntry::~CResEntry() D2 (weak)
    "08150ac0",  # CResEntryEx::Reset()
    "08150ae0",  # CResEntryEx::CResEntryEx(STriplet, char const*, uchar, uchar, int, int, unsigned)
    "08150bd0",  # CResEntryEx::CResEntryEx(STriplet, char const*, uchar, uchar, int, int)
    "08150b60",  # CResEntryEx::CResEntryEx(STriplet, char const*, ushort, uchar, uchar, unsigned)
    "08150c50",  # CResEntryEx::CResEntryEx(STriplet, char const*, ushort, uchar, uchar)
    "08150cc0",  # CResEntryEx::CResEntryEx(CResEntryEx const&)
    "08150d20",  # CResEntryEx::CResEntryEx(CResEntry const&)
    "08150d80",  # CResEntryEx::operator=(CResEntryEx const&)
    "08150dc0",  # CResEntryEx::operator=(CResEntry const&)
    "08150df0",  # CResEntryEx::CopyEx(CResEntryEx const&)
    "08150e30",  # CResEntryEx::CopyEx(CResEntry const&)
    "081903d0",  # CResEntryEx::~CResEntryEx() D1 (weak)
    "081903c0",  # CResEntryEx::~CResEntryEx() D2 (weak)

    # 2026-07-28 fresh nm -C class-inventory sweep, round 4: CSeqEvent (slice)
    # / CSeqPat / CPatternDataHolder / CDrumTrackPatternDataHolder
    # (seq_pattern_data.h/.cpp) -- a real, fully self-contained sequencer
    # pattern-event-storage family. Two other candidates traced and REJECTED
    # first: the CFATEntry/CFatMap/CShortDirEntry/CVFATEntry FAT/VFAT
    # directory-entry family (89 functions -- looked ideal but real call-xref
    # tracing found several members depend on CZ's real growable container
    # primitives, `Insert`/`RFind`/`Remove`/`Sprintf`/growable
    # `operator=(const char*)`, the exact deep out-of-scope surface
    # cz_util.h already documents), and the CLoadSoundFontMgr/
    # CLoadKontaktBankMgr/CLoadKontaktMultiMgr/CLoadKontaktInstrumentMgr/
    # CLoadKontaktSampleMgr family (97 functions -- dense web of
    # CFMBrowseForm/CPCMManager/CDiskUtil/CFileOperation/CStorage
    # collaborators, classic god-object-network business logic). This
    # cluster's own objdump call-xref scan found ZERO external call targets
    # anywhere in CSeqPat/CPatternDataHolder -- strictly better match for
    # "mechanical, not entangled with unmodeled classes" than the rejected
    # FAT family.
    "08e17c90",  # CSeqPat::Initialize()
    "08e17cd0",  # CSeqPat::Initialize(char const*)
    "08e18070",  # CSeqPat::SetName(char const*)
    "08e183e0",  # CSeqPat::SetName()
    "08e18420",  # CSeqPat::SetEventOffset(unsigned long)
    "08e18450",  # CSeqPat::GetEventOffset()
    "08e18480",  # CSeqPat::SetEvent(unsigned long, CSeqEvent*)
    "08e184d0",  # CSeqPat::GetEvent(unsigned long)
    "08e18510",  # CSeqPat::GetEvent(unsigned long, int)
    "08e18590",  # CDrumTrackPatternDataHolder::Initialize()
    "08e185c0",  # CPatternDataHolder::CPatternDataHolder()
    "08e185d0",  # CPatternDataHolder::SetInfo()
    "08e18780",  # CPatternDataHolder::ClearUnusedArea()
    "08e18a20",  # CPatternDataHolder::GetPat(int)
    "08e18a40",  # CPatternDataHolder::GetEvent(int)
    "08e18a80",  # CPatternDataHolder::GetEvent(int, int)
    "08e18ad0",  # CPatternDataHolder::GetEventDirect(int)
    "08e18ae0",  # CPatternDataHolder::SetEvent(int, CSeqEvent*)
    "08e18b20",  # CPatternDataHolder::GetNumOfEvent(CSeqEvent*, bool)
    "08e18d50",  # CPatternDataHolder::GetNumOfEvent(int)
    "08e18e90",  # CPatternDataHolder::GetNumOfEventsToEnd(int)
    "08e18ff0",  # CPatternDataHolder::GetTotalNumOfEvents()
    "08e19150",  # CPatternDataHolder::GetNextTopEvent(int)
    "08e191b0",  # CPatternDataHolder::GetEventAreaTop()
    "08e191c0",  # CPatternDataHolder::GetEventAreaEnd()
    "08e191e0",  # CPatternDataHolder::GetFreeEventTop()
    "08e191f0",  # CPatternDataHolder::GetPatternTop()
    "08e19200",  # CPatternDataHolder::GetPatternEventTop()

    # --- CObject/CAbstList/CUsrList/CList/CStaticList/CListIter foundational
    # circular doubly-linked-list container family (2026-07-28) -- see list.h.
    "08bd1b90",  # CObject::~CObject() D1
    "08bd1ba0",  # CObject::~CObject() D0
    "08bd04e0",  # CAbstList::~CAbstList() D1
    "08bd0500",  # CAbstList::~CAbstList() D0
    "08bd0890",  # CAbstList::makenode(CObject*)
    "08bd08c0",  # CAbstList::deletenode(SListNode*)
    "08bd08f0",  # CAbstList::deleteallnode()
    "08bd0980",  # CAbstList::findnode(CObject*) const
    "08bd09b0",  # CAbstList::findnode(long) const
    "08bd0ab0",  # CAbstList::remove(SListNode*)
    "08bd0ae0",  # CAbstList::CAbstList()
    "08bd0b00",  # CAbstList::append(CObject*)
    "08bd0b60",  # CAbstList::prepend(CObject*)
    "08bd0bc0",  # CAbstList::insertafter(CObject*, CObject*)
    "08bd0c30",  # CAbstList::insertat(CObject*, long)
    "08bd0e30",  # CAbstList::remove(CObject*)
    "08bd0eb0",  # CAbstList::removefirst()
    "08bd0f00",  # CAbstList::removelast()
    "08bd0f60",  # CAbstList::dispose(CObject*)
    "08bd0fe0",  # CAbstList::prev(CObject*)
    "08bd1020",  # CAbstList::next(CObject*)
    "08bd1060",  # CAbstList::bringfront(CObject*)
    "08bd10f0",  # CAbstList::sendback(CObject*)
    "08bd1170",  # CAbstList::moveup(CObject*)
    "08bd11d0",  # CAbstList::movedown(CObject*)
    "08bd1230",  # CAbstList::disposeall()
    "08bd12a0",  # CAbstList::removeallnode()
    "08bd1330",  # CAbstList::getnumitems()
    "08bd1340",  # CAbstList::firstitem()
    "08bd1360",  # CAbstList::lastitem()
    "08bd1380",  # CAbstList::nthitem(long)
    "08bd1480",  # CAbstList::findindex(CObject*)
    "08bd14c0",  # CAbstList::includes(CObject*)
    "08bd0440",  # CUsrList::deletenode_sub(SListNode*)
    "08bd0480",  # CUsrList::makenode_sub(CObject*)
    "08bd0530",  # CUsrList::~CUsrList() D1
    "08bd06d0",  # CUsrList::~CUsrList() D0
    "08bd1520",  # CUsrList::CUsrList()
    "08bd0460",  # CList::deletenode_sub(SListNode*)
    "08bd04b0",  # CList::makenode_sub(CObject*)
    "08bd05b0",  # CList::~CList() D1
    "08bd0760",  # CList::~CList() D0
    "08bd1500",  # CList::CList()
    "08bd03d0",  # CStaticList::makenode_sub(CObject*)
    "08bd0400",  # CStaticList::deletenode_sub(SListNode*)
    "08bd0630",  # CStaticList::~CStaticList() D1
    "08bd07f0",  # CStaticList::~CStaticList() D0
    "08bd1540",  # CStaticList::CStaticList(int)
    "08bd16b0",  # CListIter::inititer(CAbstList const*)
    "08bd16d0",  # CListIter::CListIter()
    "08bd16f0",  # CListIter::CListIter(CAbstList const&, EIteratorPos)
    "08bd1730",  # CListIter::CListIter(CAbstList const&, CObject*)
    "08bd1780",  # CListIter::CListIter(CAbstList const&, long)
    "08bd18a0",  # CListIter::~CListIter()
    "08bd18b0",  # CListIter::init(CAbstList const&, EIteratorPos)
    "08bd18f0",  # CListIter::init(CAbstList const&, CObject*)
    "08bd1940",  # CListIter::init(CAbstList const&, long)
    "08bd1a60",  # CListIter::totop()
    "08bd1a90",  # CListIter::totail()
    "08bd1ac0",  # CListIter::operator()()
    "08bd1af0",  # CListIter::operator++()
    "08bd1b40",  # CListIter::operator--()

    # --- CWaveformTemplate, LFO waveform-shape generator (2026-07-28, two batches) --
    # see waveform_template.h for the full scope-decision writeup. Batch 1: 8 of 20
    # Equation* bodies (the pure-integer, no-runtime-table ones) plus GetData()/
    # Shape()/dtor. Batch 2 (same day, follow-up): EquationRandomSH1-3/RandomCnt1-3,
    # the multi-branch/table-driven breakpoint generators, reused the same x86
    # instruction-interpreter oracle to derive AND regression-verify (thousands of
    # randomized (x,y,z) per function, 0 mismatches). Still NOT reconstructed: ctor
    # (needs the full 20-entry equation table wired up), MakeShapeTable (confirmed
    # this batch to be inseparable from a real FPU curve-fit path in the same
    # function body), EquationPolyline (confirmed this batch to be a genuine
    # Duff's-device-unrolled variable-length polyline walk, its own dedicated-pass
    # scope), the 5 FPU Equation* bodies, and DrawWave (Peg GUI dependency).
    "089847a0",  # CWaveformTemplate::EquationNone(int,int,int)
    "089847b0",  # CWaveformTemplate::EquationTriangle(int,int,int)
    "08984800",  # CWaveformTemplate::EquationSaw(int,int,int)
    "08984820",  # CWaveformTemplate::EquationSquare(int,int,int)
    "08984840",  # CWaveformTemplate::EquationStepTri4(int,int,int)
    "089848a0",  # CWaveformTemplate::EquationStepTri6(int,int,int)
    "08984960",  # CWaveformTemplate::EquationStepSaw4(int,int,int)
    "089849d0",  # CWaveformTemplate::EquationStepSaw6(int,int,int)
    "08984ae0",  # CWaveformTemplate::EquationRandomSH1(int,int,int)
    "08984bf0",  # CWaveformTemplate::EquationRandomSH2(int,int,int)
    "08984df0",  # CWaveformTemplate::EquationRandomSH3(int,int,int)
    "089851a0",  # CWaveformTemplate::EquationRandomCnt3(int,int,int)
    "08985330",  # CWaveformTemplate::EquationRandomCnt2(int,int,int)
    "08985510",  # CWaveformTemplate::EquationRandomCnt1(int,int,int)
    "08985870",  # CWaveformTemplate::~CWaveformTemplate() (D1/D2 folded, same address)
    "089858e0",  # CWaveformTemplate::GetData(int) const
    "08985990",  # CWaveformTemplate::Shape(char) const
    "08985f80",  # CWaveformTemplate::EquationPolyline(int,int,int,int,const u8*,const char*)

    # --- CVFATEntry, filesystem-metadata series continuation alongside CDirEntry/CZ
    # (see include/vfat_entry.h for full provenance). 16 self-contained methods --
    # zero external calls/relocations, confirmed via objdump -dr. GetAliasChecksum()
    # and both IsLongNameBitArrayEmpty() overloads verified via the direct-execution
    # oracle technique (mmap+PROT_EXEC on the real extracted machine code), 60000
    # randomized trials + 3 exact spot-checks, 0 mismatches. GetSlotIndex()/
    # OnShortNameChanged()/OnShortExtChanged()/Serialize()/Deserialize()/etc left
    # unreconstructed -- see vfat_entry.h's own header comment for exact reasons.
    "081462d0",  # CVFATEntry::ComputeChecksum(unsigned char, unsigned char)
    "08146180",  # CVFATEntry::GetAliasChecksum(unsigned char const*) const
    "081461d0",  # CVFATEntry::GetAliasChecksum() const
    "08145970",  # CVFATEntry::GetMaxNumEntryForLongName()
    "08145980",  # CVFATEntry::GetMaxCharPerEntry()
    "08145990",  # CVFATEntry::GetMaxCharForLongName()
    "081459a0",  # CVFATEntry::GetLongNameMark()
    "08146920",  # CVFATEntry::GetCurrentSlotIndex() const
    "08146930",  # CVFATEntry::GetCurrentAliasChecksum() const
    "08146940",  # CVFATEntry::GetCurrentNumForShortNameExt() const
    "08146950",  # CVFATEntry::GetOutputCodePage() const
    "08142510",  # CVFATEntry::HasValidLongNameExt() const
    "08142520",  # CVFATEntry::OnLongNameChanged()
    "08142530",  # CVFATEntry::OnLongExtChanged()
    "081437c0",  # CVFATEntry::IsLongNameBitArrayEmpty() const
    "081438c0",  # CVFATEntry::IsLongNameBitArrayEmpty(unsigned int) const

    # --- CBitMaskL (2026-07-28, fresh nm -C class-inventory sweep after CVFATEntry's
    # remaining ~28 methods were re-confirmed genuinely out of scope -- see
    # vfat_entry.h's header comment). All 13 unique methods, header-only
    # (include/bit_mask_l.h), zero external calls except is_set()'s single omitted
    # Api+0x94 soft-assert (behaviorally inert, see header comment). All 13 verified
    # via the direct-execution oracle, ~50000 randomized trials, 0 mismatches.
    "0838e350",  # CBitMaskL::ProcessEndian()
    "0838e3a0",  # CBitMaskL::CBitMaskL(short)
    "0838e3c0",  # CBitMaskL::is_set(unsigned long) const
    "0838e440",  # CBitMaskL::is_clear(unsigned long) const
    "0838e460",  # CBitMaskL::set(unsigned long)
    "0838e480",  # CBitMaskL::clear(unsigned long)
    "0838e4a0",  # CBitMaskL::GetMask()
    "0838e4c0",  # CBitMaskL::operator=(unsigned long)
    "0838e4e0",  # CBitMaskL::operator|=(unsigned long)
    "0838e500",  # CBitMaskL::init()
    "0838e510",  # CBitMaskL::init(unsigned long)
    "0838e530",  # CBitMaskL::GetNumOfSetBit()
    "0838e6c0",  # CBitMaskL::getbit(unsigned long&)

    # --- CStream/CIn/COut/CInOut/CNullStr/CMemory (2026-07-28, fresh nm -C
    # class-inventory sweep after CBitMaskL/CVFATEntry; CBigEndObj re-confirmed
    # rejected -- see stream_family.h's own header comment) ---
    "0804cf60",  # CStream::~CStream() D1
    "0804d120",  # CStream::~CStream() D0
    "0804cf70",  # CStream::GetLength() const volatile
    "0804cf80",  # CStream::Tell() const volatile
    "080a1df0",  # CStream::Open(char const*, CStream::EAccessMode)
    "080a2100",  # CStream::CStream()
    "080a2130",  # CStream::IsSought(long&, CStream::ESeekType)
    "0804cf90",  # CIn::~CIn() D1
    "0804d1a0",  # CIn::~CIn() D0
    "0804cfc0",  # CIn::Get(unsigned char&)
    "080a1e20",  # CIn::Open(char const*, CStream::EAccessMode)
    "0804cff0",  # COut::~COut() D1
    "0804d0c0",  # COut::~COut() D0
    "0804d020",  # COut::Put(unsigned char)
    "080a1ec0",  # COut::Open(char const*, CStream::EAccessMode)
    "080a1f60",  # CInOut::~CInOut() D1
    "080a2090",  # CInOut::~CInOut() D0
    "080a2230",  # CInOut::~CInOut() D2
    "080a1fd0",  # CInOut::Open(char const*, CStream::EAccessMode)
    "080a18f0",  # CNullStr::Close()
    "080a1920",  # CNullStr::Read(void*, unsigned int)
    "080a19b0",  # CNullStr::Write(void const*, unsigned int)
    "080a1a50",  # CNullStr::Flush()
    "080a1ab0",  # CNullStr::AreUsingTheNext(unsigned int)
    "080a1ad0",  # CNullStr::CurrBufferCapacity() const
    "080a1af0",  # CNullStr::IsReady() volatile
    "080a1b10",  # CNullStr::Open(char const*, CStream::EAccessMode)
    "080a1b90",  # CNullStr::Seek(long, CStream::ESeekType)
    "080a1bf0",  # CNullStr::~CNullStr() D1
    "080a1c60",  # CNullStr::~CNullStr() D0
    "080a1ce0",  # CNullStr::CNullStr() C2
    "080a1d40",  # CNullStr::CNullStr() C1
    "080a1db0",  # CNullStr::~CNullStr() D2
    "080a1070",  # CMemory::Close()
    "080a10a0",  # CMemory::Flush()
    "080a1100",  # CMemory::AreUsingTheNext(unsigned int)
    "080a1120",  # CMemory::CurrBufferCapacity() const
    "080a1140",  # CMemory::IsReady() volatile
    "080a1160",  # CMemory::Open(char const*, CStream::EAccessMode)
    "080a1220",  # CMemory::Write(void const*, unsigned int)
    "080a1350",  # CMemory::Read(void*, unsigned int)
    "080a1470",  # CMemory::Seek(long, CStream::ESeekType)
    "080a1520",  # CMemory::~CMemory() D1
    "080a15a0",  # CMemory::~CMemory() D0
    "080a1630",  # CMemory::CMemory(unsigned char*, unsigned long, int) C2
    "080a1750",  # CMemory::CMemory(unsigned char*, unsigned long, int) C1
    "080a1880",  # CMemory::~CMemory() D2

    # --- CNameBuff (name_buff.h, 2026-07-28 sweep -- see stream_family.h's own
    # header comment for the CFF investigate-and-reject that preceded this pick) ---
    "0838dd80",  # CNameBuff::CNameBuff()
    "0838dda0",  # CNameBuff::~CNameBuff()
    "0838ddb0",  # CNameBuff::deletearray()
    "0838ddc0",  # CNameBuff::init()
    "0838ddd0",  # CNameBuff::setup(unsigned short)
    "0838df40",  # CNameBuff::setname(char const*, int)
    "0838dfe0",  # CNameBuff::setsize(unsigned long long, int)
    "0838e030",  # CNameBuff::setsecondarysize(unsigned long long, int)
    "0838e080",  # CNameBuff::setfkind(EFileKind, int)
    "0838e0b0",  # CNameBuff::setavailable(bool, int)
    "0838e0e0",  # CNameBuff::getname(int)
    "0838e110",  # CNameBuff::getsize(int)
    "0838e150",  # CNameBuff::getsecondarysize(int)
    "0838e190",  # CNameBuff::getfkind(int)
    "0838e1d0",  # CNameBuff::isavailable(int)

    # --- CEventsPool (events_pool.h, 2026-07-28 sweep) -- the real, corrected
    # type of CLinkedEvent::sm_oEventsPool (event.h's own prior mis-typing fixed
    # this pass); support class for CParamTracer's Append* family below ---
    "0807f8a0",  # CEventsPool::CEventsPool()
    "0807fa70",  # CEventsPool::~CEventsPool()
    "0807fb10",  # CEventsPool::GetNewEvent()

    # --- CParamTracer (param_tracer.h, 2026-07-28 sweep) -- sorted-array MIDI
    # NRPN/RPN tracker, class-inventory sweep after CNameBuff above. Sibling
    # classes CControllerTracer/CCtrlAndParamTracer/CNoteTracer/
    # CNoteTracerTransposer seen via the same nm sweep, deliberately deferred --
    # see param_tracer.h's own header comment for why each one. ---
    "0808fe10",  # CParamTracer::CParamTracer()
    "08090000",  # CParamTracer::CParamTracer(unsigned char, ECtrlChange)
    "080901f0",  # CParamTracer::InitAfterDefaultCtor(unsigned char, ECtrlChange)
    "08090210",  # CParamTracer::Reset()
    "08090230",  # CParamTracer::Erase(SBytePair const&)
    "080903d0",  # CParamTracer::Erase(SBytePair const*)
    "080905e0",  # CParamTracer::ModifyData(SBytePair, SBytePair)
    "08090690",  # CParamTracer::DataInc()
    "080907a0",  # CParamTracer::DataDec()
    "080908b0",  # CParamTracer::First() const
    "080908c0",  # CParamTracer::Next(CParamTracer::SParam const*) const
    "08090970",  # CParamTracer::Find(SBytePair const&) const
    "080909e0",  # CParamTracer::FindEqualOrNext(SBytePair const&) const
    "08090a60",  # CParamTracer::AppendSingleParam(CLinkedEvent*&, SBytePair&, CParamTracer::SParam const&) const
    "08090df0",  # CParamTracer::AppendAllParams(CLinkedEvent*&) const
    "08091010",  # CParamTracer::AppendParamsDontCareAddr(CLinkedEvent*&, SBytePair const*) const
    "08091140",  # CParamTracer::AppendParams(CLinkedEvent*&, SBytePair const*) const
    "08091e90",  # CParamTracer::SetData(SBytePair, SBytePair)
    "08092430",  # CParamTracer::SetDataLSB(unsigned char)
    "08092850",  # CParamTracer::SetDataMSB(unsigned char)
    "08182f40",  # TVector<CParamTracer::SParam, 0>::Insert(CParamTracer::SParam*&, CParamTracer::SParam const*, CParamTracer::SParam const*)

    # --- CKontaktXml (kontakt_xml.h, 2026-07-28 sweep) -- the shared libxml2
    # xmlTextReader wrapper / value-parsing helper underneath the entire ~180-class
    # Kontakt (NKI) import subsystem (confirmed via objdump -dr xref: every
    # CKontaktXxxParameter::AddIndexedParameter dispatch table funnels through
    # UnsignedValue/SignedValue here -- the OA-side sValueGetterTemp pattern's Eva
    # analogue). 25 of 29 real methods reconstructed; TruncateName/UnpackPath/
    # RemoveTrailingCharacters/PathName deliberately deferred -- see
    # kontakt_xml.h's own header comment for why each one. Rest of the ~180-class
    # family (CKontaktGroup/CKontaktZone/CKontaktProgram/... + the
    # CKontaktXxxParameter accessors) untouched, out of scope this pass. ---
    "089c4bc0",  # CKontaktXml::CKontaktXml()
    "089c4b20",  # CKontaktXml::~CKontaktXml() (complete-object)
    "089c4b40",  # CKontaktXml::~CKontaktXml() (deleting)
    "089c4be0",  # CKontaktXml::StateString(CKontaktXml::KontaktState)
    "089c4b60",  # CKontaktXml::AddObject(_xmlTextReader*, unsigned char const*)
    "089c4b30",  # CKontaktXml::AddAttribute(unsigned int, unsigned char const*, unsigned char const*)
    "089c4c00",  # CKontaktXml::ProcessNode(_xmlTextReader*)
    "089c4ed0",  # CKontaktXml::ProcessNodes(_xmlTextReader*, bool)
    "089c4f30",  # CKontaktXml::Parse(_xmlTextReader*)
    "089c4f80",  # CKontaktXml::Parse(char const*)
    "089c4ff0",  # CKontaktXml::SkipNode(_xmlTextReader*)
    "089c5050",  # CKontaktXml::StringIndex(char const**, unsigned char const*)
    "089c50b0",  # CKontaktXml::StringIndex(char const**, unsigned char const*, unsigned int&)
    "089c5170",  # CKontaktXml::StringIndex(char const**, unsigned char const*, char*, unsigned int)
    "089c5210",  # CKontaktXml::StringsEqual(unsigned char const*, char const*)
    "089c5240",  # CKontaktXml::BooleanValue(unsigned char const*)
    "089c52b0",  # CKontaktXml::UnsignedValue(unsigned char const*)
    "089c52e0",  # CKontaktXml::SignedValue(unsigned char const*)
    "089c5310",  # CKontaktXml::FloatValue(unsigned char const*)
    "089c5610",  # CKontaktXml::VolumeLength(unsigned char const*, unsigned int&)
    "089c5680",  # CKontaktXml::DirectoryLength(unsigned char const*, unsigned int&)
    "089c56f0",  # CKontaktXml::FileLength(unsigned char const*, unsigned int&)
    "089c5760",  # CKontaktXml::Append(unsigned char const*, unsigned int&, unsigned int, char*, unsigned int)
    "089c57f0",  # CKontaktXml::AbsolutePath(char const*, char const*, char*, unsigned int)
    "089c58f0",  # CKontaktXml::RemoveNameExtension(char*, unsigned int)

    # --- CKontaktParameter/CKontaktIndexedParameter/CKontaktDynamicParameter +
    # first batch of concrete CKontaktXxxParameter siblings (kontakt_parameter_base.h/
    # kontakt_parameter_family.h, 2026-07-28) -- direct follow-up to the standing lead
    # in kontakt_xml.h's own header ("CKontaktGroupParameter dispatches through
    # CKontaktXml's value parsers, out of scope that pass"). All 3 abstract bases +
    # 10 concrete "Parameter" siblings (Group/Zone/Effect/Filter/Output/Lfo/Loop/
    # Envelope/PlaybackMode/StartCriteria) + CKontaktScriptParameter (the Dynamic
    # family's own first sibling) + the 4 tiny owner-struct setters those bodies
    # call into (CKontaktGroup::SetOutputRouting, CKontaktScript::SetDescription/
    # SetPassword/SetSourceText). Every attribute-name list and jump table was read
    # directly out of .rodata, not guessed. The sibling "CKontaktXxxParameters"
    # (plural) factory family and CKontaktSample/Program/Container Parameter (need
    # the still-deferred CKontaktXml::UnpackPath) are NOT in this pass -- see
    # kontakt_parameter_base.h/kontakt_parameter_family.h's own file headers. ---
    "089c0c50",  # CKontaktParameter::CKontaktParameter(char const**)
    "089c0b10",  # CKontaktParameter::~CKontaktParameter() (complete-object)
    "089c0b60",  # CKontaktParameter::~CKontaktParameter() (deleting)
    "089c0bc0",  # CKontaktParameter::AddAttribute(unsigned int, unsigned char const*, unsigned char const*)
    "089c0c80",  # CKontaktParameter::AddParameter(unsigned char const*)
    "089c0b00",  # CKontaktParameter::AddParameter(unsigned int, unsigned char const*) (1-byte no-op default)
    "089be810",  # CKontaktIndexedParameter::CKontaktIndexedParameter(char const**)
    "089be6c0",  # CKontaktIndexedParameter::~CKontaktIndexedParameter() (complete-object)
    "089be710",  # CKontaktIndexedParameter::~CKontaktIndexedParameter() (deleting)
    "089be770",  # CKontaktIndexedParameter::AddAttribute(unsigned int, unsigned char const*, unsigned char const*)
    "089be840",  # CKontaktIndexedParameter::AddIndexedParameter(unsigned char const*)
    "089be6b0",  # CKontaktIndexedParameter::AddIndexedParameter(unsigned int, unsigned int, unsigned char const*) (1-byte no-op default)
    "089bbe80",  # CKontaktDynamicParameter::CKontaktDynamicParameter(char const**)
    "089bbd30",  # CKontaktDynamicParameter::~CKontaktDynamicParameter() (complete-object)
    "089bbd80",  # CKontaktDynamicParameter::~CKontaktDynamicParameter() (deleting)
    "089bbde0",  # CKontaktDynamicParameter::AddAttribute(unsigned int, unsigned char const*, unsigned char const*)
    "089bbeb0",  # CKontaktDynamicParameter::AddDynamicParameter(unsigned char const*)
    "089bbd20",  # CKontaktDynamicParameter::AddDynamicParameter(unsigned int, char const*, unsigned char const*) (1-byte no-op default)
    "089be160",  # CKontaktGroupParameter::CKontaktGroupParameter(CKontaktGroup*)
    "089be110",  # CKontaktGroupParameter::~CKontaktGroupParameter() (complete-object)
    "089be130",  # CKontaktGroupParameter::~CKontaktGroupParameter() (deleting)
    "089bdd80",  # CKontaktGroupParameter::AddIndexedParameter(unsigned int, unsigned int, unsigned char const*)
    "089c93a0",  # CKontaktZoneParameter::CKontaktZoneParameter(CKontaktZone*)
    "089c9350",  # CKontaktZoneParameter::~CKontaktZoneParameter() (complete-object)
    "089c9370",  # CKontaktZoneParameter::~CKontaktZoneParameter() (deleting)
    "089c9230",  # CKontaktZoneParameter::AddParameter(unsigned int, unsigned char const*)
    "089bc3e0",  # CKontaktEffectParameter::CKontaktEffectParameter(CKontaktEffect*)
    "089bc390",  # CKontaktEffectParameter::~CKontaktEffectParameter() (complete-object)
    "089bc3b0",  # CKontaktEffectParameter::~CKontaktEffectParameter() (deleting)
    "089bc2c0",  # CKontaktEffectParameter::AddParameter(unsigned int, unsigned char const*)
    "089bd320",  # CKontaktFilterParameter::CKontaktFilterParameter(CKontaktFilter*)
    "089bd2d0",  # CKontaktFilterParameter::~CKontaktFilterParameter() (complete-object)
    "089bd2f0",  # CKontaktFilterParameter::~CKontaktFilterParameter() (deleting)
    "089bd100",  # CKontaktFilterParameter::AddParameter(unsigned int, unsigned char const*)
    "089c0160",  # CKontaktOutputParameter::CKontaktOutputParameter(CKontaktOutput*)
    "089c0110",  # CKontaktOutputParameter::~CKontaktOutputParameter() (complete-object)
    "089c0130",  # CKontaktOutputParameter::~CKontaktOutputParameter() (deleting)
    "089c0090",  # CKontaktOutputParameter::AddParameter(unsigned int, unsigned char const*)
    "089bf470",  # CKontaktLfoParameter::CKontaktLfoParameter(CKontaktLfo*)
    "089bf420",  # CKontaktLfoParameter::~CKontaktLfoParameter() (complete-object)
    "089bf440",  # CKontaktLfoParameter::~CKontaktLfoParameter() (deleting)
    "089bf370",  # CKontaktLfoParameter::AddParameter(unsigned int, unsigned char const*)
    "089bf930",  # CKontaktLoopParameter::CKontaktLoopParameter(CKontaktLoop*)
    "089bf8e0",  # CKontaktLoopParameter::~CKontaktLoopParameter() (complete-object)
    "089bf900",  # CKontaktLoopParameter::~CKontaktLoopParameter() (deleting)
    "089bf7f0",  # CKontaktLoopParameter::AddParameter(unsigned int, unsigned char const*)
    "089bc750",  # CKontaktEnvelopeParameter::CKontaktEnvelopeParameter(CKontaktEnvelope*)
    "089bc700",  # CKontaktEnvelopeParameter::~CKontaktEnvelopeParameter() (complete-object)
    "089bc720",  # CKontaktEnvelopeParameter::~CKontaktEnvelopeParameter() (deleting)
    "089bc600",  # CKontaktEnvelopeParameter::AddParameter(unsigned int, unsigned char const*)
    "089c10c0",  # CKontaktPlaybackModeParameter::CKontaktPlaybackModeParameter(CKontaktPlaybackMode*)
    "089c1070",  # CKontaktPlaybackModeParameter::~CKontaktPlaybackModeParameter() (complete-object)
    "089c1090",  # CKontaktPlaybackModeParameter::~CKontaktPlaybackModeParameter() (deleting)
    "089c0f60",  # CKontaktPlaybackModeParameter::AddParameter(unsigned int, unsigned char const*)
    "089c4100",  # CKontaktStartCriteriaParameter::CKontaktStartCriteriaParameter(CKontaktStartCriteria*)
    "089c40b0",  # CKontaktStartCriteriaParameter::~CKontaktStartCriteriaParameter() (complete-object)
    "089c40d0",  # CKontaktStartCriteriaParameter::~CKontaktStartCriteriaParameter() (deleting)
    "089c3fd0",  # CKontaktStartCriteriaParameter::AddParameter(unsigned int, unsigned char const*)
    "089c3450",  # CKontaktScriptParameter::CKontaktScriptParameter(CKontaktScript*)
    "089c3400",  # CKontaktScriptParameter::~CKontaktScriptParameter() (complete-object)
    "089c3420",  # CKontaktScriptParameter::~CKontaktScriptParameter() (deleting)
    "089c3330",  # CKontaktScriptParameter::AddDynamicParameter(unsigned int, char const*, unsigned char const*)
    "089bda60",  # CKontaktGroup::SetOutputRouting(unsigned int, unsigned int)
    "089c32b0",  # CKontaktScript::SetDescription(char const*)
    "089c32f0",  # CKontaktScript::SetPassword(char const*)
    "089c3240",  # CKontaktScript::SetSourceText(char const*)

    # --- 2026-07-28 "Parameters" factory-family follow-up batch: the plural
    # CKontaktParameters/CKontaktIndexedParameters/CKontaktDynamicParameters
    # wrapper family (resolves the previously-flagged "dead single-entry V
    # list" finding), CKontaktXml::UnpackPath (previously deferred), the 3
    # singular family Identifier() overrides ("V" -- resolves CKontaktXml's
    # own previously-unidentified pure virtual), and every UnpackPath-
    # unblocked/owner-setter-unblocked sibling this newly tractable ---
    "089c5340",  # CKontaktXml::UnpackPath(unsigned char const*, char*, unsigned int)
    "089d9390",  # CKontaktParameter::Identifier() const
    "089d9360",  # CKontaktIndexedParameter::Identifier() const
    "089d93b0",  # CKontaktDynamicParameter::Identifier() const
    "089c0d70",  # CKontaktParameters::CKontaktParameters()
    "089c0d20",  # CKontaktParameters::~CKontaktParameters() (complete-object)
    "089c0d40",  # CKontaktParameters::~CKontaktParameters() (deleting)
    "089c0cc0",  # CKontaktParameters::AddObject(_xmlTextReader*, unsigned char const*)
    "089d93a0",  # CKontaktParameters::Identifier() const
    "089be950",  # CKontaktIndexedParameters::CKontaktIndexedParameters()
    "089be900",  # CKontaktIndexedParameters::~CKontaktIndexedParameters() (complete-object)
    "089be920",  # CKontaktIndexedParameters::~CKontaktIndexedParameters() (deleting)
    "089be8a0",  # CKontaktIndexedParameters::AddObject(_xmlTextReader*, unsigned char const*)
    "089d9370",  # CKontaktIndexedParameters::Identifier() const
    "089bbfc0",  # CKontaktDynamicParameters::CKontaktDynamicParameters()
    "089bbf70",  # CKontaktDynamicParameters::~CKontaktDynamicParameters() (complete-object)
    "089bbf90",  # CKontaktDynamicParameters::~CKontaktDynamicParameters() (deleting)
    "089bbf10",  # CKontaktDynamicParameters::AddObject(_xmlTextReader*, unsigned char const*)
    "089d93c0",  # CKontaktDynamicParameters::Identifier() const
    "089be230",  # CKontaktGroupParameters::CKontaktGroupParameters(CKontaktGroup*)
    "089be1e0",  # CKontaktGroupParameters::~CKontaktGroupParameters() (complete-object)
    "089be200",  # CKontaktGroupParameters::~CKontaktGroupParameters() (deleting)
    "089be190",  # CKontaktGroupParameters::MakeIndexedParameter()
    "089c0230",  # CKontaktOutputParameters::CKontaktOutputParameters(CKontaktOutput*)
    "089c01e0",  # CKontaktOutputParameters::~CKontaktOutputParameters() (complete-object)
    "089c0200",  # CKontaktOutputParameters::~CKontaktOutputParameters() (deleting)
    "089c0190",  # CKontaktOutputParameters::MakeParameter()
    "089c9470",  # CKontaktZoneParameters::CKontaktZoneParameters(CKontaktZone*)
    "089c9420",  # CKontaktZoneParameters::~CKontaktZoneParameters() (complete-object)
    "089c9440",  # CKontaktZoneParameters::~CKontaktZoneParameters() (deleting)
    "089c93d0",  # CKontaktZoneParameters::MakeParameter()
    "089bbcf0",  # CKontaktContainerParameters::CKontaktContainerParameters(CKontaktContainer*)
    "089bbca0",  # CKontaktContainerParameters::~CKontaktContainerParameters() (complete-object)
    "089bbcc0",  # CKontaktContainerParameters::~CKontaktContainerParameters() (deleting)
    "089bbc50",  # CKontaktContainerParameters::MakeParameter()
    "089c0ad0",  # CKontaktOutputsParameters::CKontaktOutputsParameters(CKontaktOutputs*)
    "089c0a80",  # CKontaktOutputsParameters::~CKontaktOutputsParameters() (complete-object)
    "089c0aa0",  # CKontaktOutputsParameters::~CKontaktOutputsParameters() (deleting)
    "089c0a30",  # CKontaktOutputsParameters::MakeIndexedParameter()
    "089bbc20",  # CKontaktContainerParameter::CKontaktContainerParameter(CKontaktContainer*)
    "089bbbd0",  # CKontaktContainerParameter::~CKontaktContainerParameter() (complete-object)
    "089bbbf0",  # CKontaktContainerParameter::~CKontaktContainerParameter() (deleting)
    "089bb9f0",  # CKontaktContainerParameter::AddParameter(unsigned int, unsigned char const*)
    "089c3000",  # CKontaktSampleParameter::CKontaktSampleParameter(CKontaktSample*)
    "089c2fb0",  # CKontaktSampleParameter::~CKontaktSampleParameter() (complete-object)
    "089c2fd0",  # CKontaktSampleParameter::~CKontaktSampleParameter() (deleting)
    "089c2d80",  # CKontaktSampleParameter::AddParameter(unsigned int, unsigned char const*)
    "089c0a00",  # CKontaktOutputsParameter::CKontaktOutputsParameter(CKontaktOutputs*)
    "089c09b0",  # CKontaktOutputsParameter::~CKontaktOutputsParameter() (complete-object)
    "089c09d0",  # CKontaktOutputsParameter::~CKontaktOutputsParameter() (deleting)
    "089c0940",  # CKontaktOutputsParameter::AddIndexedParameter(unsigned int, unsigned int, unsigned char const*)
    "089bb9b0",  # CKontaktContainer::SetOriginalSubDirectory(char const*)
    "089c2ad0",  # CKontaktSample::SetFile(char const*)
    "089c2b30",  # CKontaktSample::SetFilePbn(char const*)
    "089c0710",  # CKontaktOutputs::SetPhysicalOutputMapping(unsigned int, unsigned int)

    # --- Kontakt "Parameters" factory-family SECOND follow-up batch (size-deferred
    # CKontaktProgramParameter + every other remaining Kontakt-family sibling this
    # project's deferred-item registry had open), 2026-07-28 ---
    "089c2070",  # CKontaktProgramParameter::CKontaktProgramParameter(CKontaktProgram*)
    "089c2020",  # CKontaktProgramParameter::~CKontaktProgramParameter() (complete-object)
    "089c2040",  # CKontaktProgramParameter::~CKontaktProgramParameter() (deleting)
    "089c1ec0",  # CKontaktProgramParameter::AddParameter(unsigned int, unsigned char const*)
    "089c2140",  # CKontaktProgramParameters::CKontaktProgramParameters(CKontaktProgram*)
    "089c20f0",  # CKontaktProgramParameters::~CKontaktProgramParameters() (complete-object)
    "089c2110",  # CKontaktProgramParameters::~CKontaktProgramParameters() (deleting)
    "089c20a0",  # CKontaktProgramParameters::MakeParameter()
    "089c1b40",  # CKontaktProgram::SetWallpaperFile(char const*)
    "089bb2a0",  # CKontaktBankParameter::CKontaktBankParameter(CKontaktBank*)
    "089bb250",  # CKontaktBankParameter::~CKontaktBankParameter() (complete-object)
    "089bb270",  # CKontaktBankParameter::~CKontaktBankParameter() (deleting)
    "089bb080",  # CKontaktBankParameter::AddIndexedParameter(unsigned int, unsigned int, unsigned char const*)
    "089bb370",  # CKontaktBankParameters::CKontaktBankParameters(CKontaktBank*)
    "089bb320",  # CKontaktBankParameters::~CKontaktBankParameters() (complete-object)
    "089bb340",  # CKontaktBankParameters::~CKontaktBankParameters() (deleting)
    "089bb2d0",  # CKontaktBankParameters::MakeIndexedParameter()
    "089bafe0",  # CKontaktBank::SetSlotMidiChannel(unsigned int, unsigned int)
    "089bb000",  # CKontaktBank::SetSlotSolo(unsigned int, bool)
    "089bb020",  # CKontaktBank::SetSlotMute(unsigned int, bool)
    "089bb040",  # CKontaktBank::SetSlotAuxSendLevel(unsigned int, unsigned int, float)
    "089bb060",  # CKontaktBank::SetSlotReorderIndex(unsigned int, unsigned int)
    "089c3a20",  # CKontaktSendLevelsParameter::CKontaktSendLevelsParameter(CKontaktSendLevels*)
    "089c39d0",  # CKontaktSendLevelsParameter::~CKontaktSendLevelsParameter() (complete-object)
    "089c39f0",  # CKontaktSendLevelsParameter::~CKontaktSendLevelsParameter() (deleting)
    "089c3960",  # CKontaktSendLevelsParameter::AddIndexedParameter(unsigned int, unsigned int, unsigned char const*)
    "089c3940",  # CKontaktSendLevels::SetLevel(unsigned int, float)
    "089bee60",  # CKontaktIntModulatorParameter::CKontaktIntModulatorParameter(CKontaktIntModulator*)
    "089bee10",  # CKontaktIntModulatorParameter::~CKontaktIntModulatorParameter() (complete-object)
    "089bee30",  # CKontaktIntModulatorParameter::~CKontaktIntModulatorParameter() (deleting)
    "089bed20",  # CKontaktIntModulatorParameter::AddParameter(unsigned int, unsigned char const*)
    "089becf0",  # CKontaktIntModulator::SetName(char const*)
    "089bcbc0",  # CKontaktExtModulatorParameter::CKontaktExtModulatorParameter(CKontaktExtModulator*)
    "089bcb70",  # CKontaktExtModulatorParameter::~CKontaktExtModulatorParameter() (complete-object)
    "089bcb90",  # CKontaktExtModulatorParameter::~CKontaktExtModulatorParameter() (deleting)
    "089bca30",  # CKontaktExtModulatorParameter::AddParameter(unsigned int, unsigned char const*)
    "089bca00",  # CKontaktExtModulator::SetName(char const*)
    "089c4af0",  # CKontaktVoiceGroupParameter::CKontaktVoiceGroupParameter(CKontaktVoiceGroup*)
    "089c4aa0",  # CKontaktVoiceGroupParameter::~CKontaktVoiceGroupParameter() (complete-object)
    "089c4ac0",  # CKontaktVoiceGroupParameter::~CKontaktVoiceGroupParameter() (deleting)
    "089c49d0",  # CKontaktVoiceGroupParameter::AddParameter(unsigned int, unsigned char const*)
    "089c49a0",  # CKontaktVoiceGroup::SetName(char const*)
    "089c4490",  # CKontaktTargetParameter::CKontaktTargetParameter(CKontaktTarget*)
    "089c4440",  # CKontaktTargetParameter::~CKontaktTargetParameter() (complete-object)
    "089c4460",  # CKontaktTargetParameter::~CKontaktTargetParameter() (deleting)
    "089c4340",  # CKontaktTargetParameter::AddParameter(unsigned int, unsigned char const*)
    "089c4310",  # CKontaktTarget::SetName(char const*)

    # --- 2026-07-28: CFileIoAkai/CFileIoDos/CFileIoIso9660, the concrete
    # media-I/O drivers deriving from CFileIoBase (52/52 done) -- next
    # storage-cluster batch after CFileIoUnknown. CFileIoDos::format(
    # EDevice_Id, int, EFatType) (0x0831afc0, 0xbd8=3032 bytes) is DEFERRED,
    # see DECOMPILE_ERRORS.md -- no other addresses in this batch are.
    "08317c70",  # CFileIoAkai::get_iotype()
    "08317c80",  # CFileIoAkai::freebytes(EDevice_Id)
    "08317c90",  # CFileIoAkai::getmediainfo(EDevice_Id, CMediaInfo*)
    "08317d10",  # CFileIoAkai::format(EDevice_Id, int)
    "08317d50",  # CFileIoAkai::ftell(int)
    "08317d70",  # CFileIoAkai::dir(char const*, int, unsigned long&, CFileDirEntry*)
    "08317fa0",  # CFileIoAkai::CFileIoAkai()
    "08317ff0",  # CFileIoAkai::ConvertPath(char const*)
    "08318030",  # CFileIoAkai::set_error()
    "08318120",  # CFileIoAkai::getwd(EDevice_Id, char*)
    "08318160",  # CFileIoAkai::chdir(char const*)
    "083181c0",  # CFileIoAkai::fseek(int, long, int)
    "08318200",  # CFileIoAkai::fread(void*, unsigned int, unsigned int, int)
    "08318250",  # CFileIoAkai::fclose(int)
    "08318290",  # CFileIoAkai::fopen(char const*, char const*)
    "08318330",  # CFileIoAkai::funmount(EDevice_Id)
    "083183c0",  # CFileIoAkai::fmount(EDevice_Id)
    "08995430",  # CFileIoAkai::~CFileIoAkai() (D1, complete-object)
    "08995440",  # CFileIoAkai::~CFileIoAkai() (D0, deleting)

    "0831a520",  # CFileIoDos::get_iotype()
    "0831a530",  # CFileIoDos::getmaxclusterno(EDevice_Id)
    "0831a550",  # CFileIoDos::getfilelbaarray(EDevice_Id, int, CFileLbaArray*)
    "0831a5e0",  # CFileIoDos::dir(char const*, int, unsigned long&, CFileDirEntry*)
    "0831a820",  # CFileIoDos::totalfreeclus(EDevice_Id)
    "0831a840",  # CFileIoDos::freebytes(EDevice_Id)
    "0831a860",  # CFileIoDos::ftell(int)
    "0831a880",  # CFileIoDos::getmediainfo(EDevice_Id, CMediaInfo*)
    "0831aa50",  # CFileIoDos::CFileIoDos()
    "0831aaa0",  # CFileIoDos::set_error()
    "0831ab20",  # CFileIoDos::optimizemedium(EDevice_Id, unsigned long, unsigned long*, int)
    "0831ac30",  # CFileIoDos::scandisk(EDevice_Id, unsigned long, unsigned long, unsigned long*, unsigned long*)
    "0831ada0",  # CFileIoDos::fdummywrite(unsigned int, unsigned int, int)
    "0831ae00",  # CFileIoDos::rmdir(char const*)
    "0831ae60",  # CFileIoDos::mkdir(char const*)
    "0831aea0",  # CFileIoDos::remove(char const*)
    "0831af00",  # CFileIoDos::rename(char const*, char const*)
    "0831af40",  # CFileIoDos::getwd(EDevice_Id, char*)
    "0831af80",  # CFileIoDos::chdir(char const*)
    "0831bba0",  # CFileIoDos::resize(int, unsigned int)
    "0831bbe0",  # CFileIoDos::fflush(int)
    "0831bc20",  # CFileIoDos::fseek(int, long, int)
    "0831bc60",  # CFileIoDos::fwrite(void const*, unsigned int, unsigned int, int)
    "0831bcc0",  # CFileIoDos::fread(void*, unsigned int, unsigned int, int)
    "0831bd10",  # CFileIoDos::fclose(int)
    "0831bd50",  # CFileIoDos::fopen(char const*, char const*)
    "0831be90",  # CFileIoDos::funmount(EDevice_Id)
    "0831bf10",  # CFileIoDos::fmount(EDevice_Id)
    "08995570",  # CFileIoDos::~CFileIoDos() (D1, complete-object)
    "08995580",  # CFileIoDos::~CFileIoDos() (D0, deleting)

    "0831c020",  # CFileIoIso9660::get_iotype()
    "0831c030",  # CFileIoIso9660::freebytes(EDevice_Id)
    "0831c040",  # CFileIoIso9660::getmediainfo(EDevice_Id, CMediaInfo*)
    "0831c0d0",  # CFileIoIso9660::format(EDevice_Id, int)
    "0831c120",  # CFileIoIso9660::ftell(int)
    "0831c140",  # CFileIoIso9660::fclose(int)
    "0831c160",  # CFileIoIso9660::funmount(EDevice_Id)
    "0831c1e0",  # CFileIoIso9660::fread(void*, unsigned int, unsigned int, int)
    "0831c2a0",  # CFileIoIso9660::getwd(EDevice_Id, char*)
    "0831c3c0",  # CFileIoIso9660::fseek(int, long, int)
    "0831c470",  # CFileIoIso9660::CFileIoIso9660()
    "0831c4f0",  # CFileIoIso9660::ConvertPathRtfsToCdfs(char const*)
    "0831c5f0",  # CFileIoIso9660::dir(char const*, int, unsigned long&, CFileDirEntry*)
    "0831c830",  # CFileIoIso9660::chdir(char const*)
    "0831c8f0",  # CFileIoIso9660::fopen(char const*, char const*)
    "0831c9e0",  # CFileIoIso9660::set_error()
    "0831ca70",  # CFileIoIso9660::fmount(EDevice_Id)
    "089955c0",  # CFileIoIso9660::~CFileIoIso9660() (D1, complete-object)
    "089955d0",  # CFileIoIso9660::~CFileIoIso9660() (D0, deleting)

    # 2026-07-28: CFileIoCdda/CFileIoUdf -- the last 2 concrete CFileIoBase
    # siblings, previously flagged "less tractable" (CScsiDriverBase batch);
    # a fresh re-survey found both genuinely tractable except one deep
    # method each. CFileIoCdda::getcurpos(EDevice_Id, EAudioStatusMMC*,
    # unsigned char*, unsigned char*, int, int) (0x08318f30, 0x74a=1866
    # bytes) and CFileIoUdf::format(EDevice_Id, int) (0x0831d610,
    # 0x12b7=4791 bytes) are DEFERRED, see DECOMPILE_ERRORS.md -- no other
    # addresses in this batch are.
    "08318ea0",  # CFileIoCdda::get_iotype()
    "08318eb0",  # CFileIoCdda::fflush(int)
    "08318ec0",  # CFileIoCdda::resize(int, unsigned int)
    "08318ed0",  # CFileIoCdda::chdir(char const*)
    "08318ee0",  # CFileIoCdda::dir(char const*, int, unsigned long&, CFileDirEntry*)
    "08318ef0",  # CFileIoCdda::rename(char const*, char const*)
    "08318f00",  # CFileIoCdda::remove(char const*)
    "08318f10",  # CFileIoCdda::mkdir(char const*)
    "08318f20",  # CFileIoCdda::rmdir(char const*)
    "08319680",  # CFileIoCdda::totalfreeclus(EDevice_Id)
    "083196b0",  # CFileIoCdda::freebytes(EDevice_Id)
    "083196d0",  # CFileIoCdda::getmediainfo(EDevice_Id, CMediaInfo*)
    "083197f0",  # CFileIoCdda::getwd(EDevice_Id, char*)
    "08319870",  # CFileIoCdda::format(EDevice_Id, int)
    "083198c0",  # CFileIoCdda::ftell(int)
    "083198e0",  # CFileIoCdda::funmount(EDevice_Id)
    "08319960",  # CFileIoCdda::CFileIoCdda()
    "083199b0",  # CFileIoCdda::ConvertPathRtfsToCdda(char const*)
    "08319ab0",  # CFileIoCdda::set_error()
    "08319b60",  # CFileIoCdda::getemphasized(int, int*)
    "08319ba0",  # CFileIoCdda::getidxlen(EDevice_Id, unsigned char, unsigned char, unsigned char, unsigned long*)
    "08319c00",  # CFileIoCdda::gettrklen(EDevice_Id, unsigned char, unsigned long*)
    "08319c50",  # CFileIoCdda::getmaxidx(EDevice_Id, unsigned char, unsigned char*)
    "08319ca0",  # CFileIoCdda::getmaxtrk(EDevice_Id, unsigned char*)
    "08319ce0",  # CFileIoCdda::writesetup(EDevice_Id, int)
    "08319d20",  # CFileIoCdda::stopscan(EDevice_Id)
    "08319d60",  # CFileIoCdda::rewscan(EDevice_Id, unsigned char, unsigned char, unsigned long, unsigned long)
    "08319dc0",  # CFileIoCdda::ffscan(EDevice_Id, unsigned char, unsigned char, unsigned long, unsigned long)
    "08319e20",  # CFileIoCdda::resume(EDevice_Id)
    "08319e60",  # CFileIoCdda::pause(EDevice_Id)
    "08319ea0",  # CFileIoCdda::stop(EDevice_Id)
    "08319ee0",  # CFileIoCdda::play(EDevice_Id, unsigned char, unsigned char, unsigned long, unsigned long)
    "08319f70",  # CFileIoCdda::fseek(int, long, int)
    "08319fb0",  # CFileIoCdda::fwrite(void const*, unsigned int, unsigned int, int)
    "0831a010",  # CFileIoCdda::fread(void*, unsigned int, unsigned int, int)
    "0831a060",  # CFileIoCdda::fclose(int)
    "0831a0a0",  # CFileIoCdda::fopen(char const*, char const*)
    "0831a260",  # CFileIoCdda::fmount(EDevice_Id, EMountIoType, int*)
    "0831a370",  # CFileIoCdda::settestmode(EDevice_Id, int)
    "0831a3b0",  # CFileIoCdda::finalize(EDevice_Id)
    "08995520",  # CFileIoCdda::~CFileIoCdda() (D1, complete-object)
    "08995530",  # CFileIoCdda::~CFileIoCdda() (D0, deleting)

    "0831cb80",  # CFileIoUdf::get_iotype()
    "0831cb90",  # CFileIoUdf::totalfreeclus(EDevice_Id)
    "0831cbb0",  # CFileIoUdf::freebytes(EDevice_Id)
    "0831cbd0",  # CFileIoUdf::getmediainfo(EDevice_Id, CMediaInfo*)
    "0831ccd0",  # CFileIoUdf::ftell(int)
    "0831ccf0",  # CFileIoUdf::funmount(EDevice_Id)
    "0831cd70",  # CFileIoUdf::dir(char const*, int, unsigned long&, CFileDirEntry*)
    "0831d100",  # CFileIoUdf::CFileIoUdf()
    "0831d180",  # CFileIoUdf::ConvertPathRtfsToUdffs(char const*)
    "0831d1c0",  # CFileIoUdf::formatsub(EDevice_Id, int, unsigned char*, long*)
    "0831d2b0",  # CFileIoUdf::setfmtparam(unsigned char*)
    "0831d2f0",  # CFileIoUdf::set_error()
    "0831d3e0",  # CFileIoUdf::isodir(EDevice_Id, udf_iso_rec*, udf_iso_rec*)
    "0831d430",  # CFileIoUdf::sortdir(EDevice_Id)
    "0831d470",  # CFileIoUdf::closepath(EDevice_Id, int)
    "0831d4b0",  # CFileIoUdf::opennextpath(EDevice_Id)
    "0831d4f0",  # CFileIoUdf::getwd(EDevice_Id, char*)
    "0831d5b0",  # CFileIoUdf::chdir(char const*)
    "0831e8d0",  # CFileIoUdf::fflush(int)
    "0831e910",  # CFileIoUdf::fseek(int, long, int)
    "0831e950",  # CFileIoUdf::fwrite(void const*, unsigned int, unsigned int, int)
    "0831e9b0",  # CFileIoUdf::fread(void*, unsigned int, unsigned int, int)
    "0831ea00",  # CFileIoUdf::fclose(int)
    "0831ea40",  # CFileIoUdf::fmount(EDevice_Id)
    "0831eba0",  # CFileIoUdf::writesetup(EDevice_Id, int)
    "0831ee80",  # CFileIoUdf::chmod(char const*, unsigned char)
    "0831ef40",  # CFileIoUdf::rmdir(char const*)
    "0831f000",  # CFileIoUdf::mkdir(char const*)
    "0831f090",  # CFileIoUdf::remove(char const*)
    "0831f150",  # CFileIoUdf::rename(char const*, char const*)
    "0831f1f0",  # CFileIoUdf::fopen(char const*, char const*)
    "0831f440",  # CFileIoUdf::SetRecoveryParam(EDevice_Id, int)
    "08995610",  # CFileIoUdf::~CFileIoUdf() (D1, complete-object)
    "08995620",  # CFileIoUdf::~CFileIoUdf() (D0, deleting)

    # --- CKorgFile, 2026-07-28 (see korg_file.h for full provenance) ---
    "089c94a0",  # CKorgFile::~CKorgFile() (D1, complete-object)
    "089c94b0",  # CKorgFile::TransferTo(void const*, unsigned int, unsigned int)
    "089c94f0",  # CKorgFile::TransferToEnd()
    "089c9520",  # CKorgFile::TransferFromEnd()
    "089c9550",  # CKorgFile::TransferFrom(void*, unsigned int, unsigned int)
    "089c9590",  # CKorgFile::Write()
    "089c95f0",  # CKorgFile::Read()
    "089c9650",  # CKorgFile::TransferToBegin(unsigned int)
    "089c9690",  # CKorgFile::TransferFromBegin(unsigned int)
    "089c96d0",  # CKorgFile::~CKorgFile() (D0, deleting)
    "089c96f0",  # CKorgFile::SetPath(char const*)
    "089c97a0",  # CKorgFile::CKorgFile(char const*, char const*)
    "089c9880",  # CKorgFile::GetPathName() const
    "089c98b0",  # CKorgFile::GetPathNameNoExtension(char*, unsigned int) const
    "089c9930",  # CKorgFile::GetFolder(char*, unsigned int)
    "089c9990",  # CKorgFile::MakePathFromFolder(char*, char const*, unsigned int)
    "089c9a50",  # CKorgFile::HasExtension(char const*, char const*)
    "089c9a90",  # CKorgFile::AddExtension(char*, unsigned int, char const*)
    "089c9ae0",  # CKorgFile::RemoveExtension(char*)
    "089c9b10",  # CKorgFile::RemoveExtension(char*, char*, unsigned int)
    "089c9b80",  # CKorgFile::ValidExtension(char const*)
    "089c9ba0",  # CKorgFile::ExtractName(char const*, char*, unsigned int)
    "089c9c20",  # CKorgFile::Sanitize(char*)
    "089c9c60",  # CKorgFile::Capitalized(char const*)
    "089c9c90",  # CKorgFile::WriteEmptyFile(_IO_FILE*, unsigned int, unsigned int)
    "089c9da0",  # CKorgFile::NameLength(char const*, unsigned int)
    "089c9e90",  # CKorgFile::MakeName(char const*, char*, unsigned int)
    "089ca000",  # CKorgFile::MakeNameStereo(char const*, char*, unsigned int, char)
    "089ca470",  # CKorgFile::MakeNameRight(char const*, char*, unsigned int)
    "089ca4a0",  # CKorgFile::MakeNameLeft(char const*, char*, unsigned int)
    "089ca4d0",  # CKorgFile::MakeFileName(char*, unsigned int, char const*)

    # --- CControllerTracer / CCtrlAndParamTracer, "Tracer" family follow-up to
    # CParamTracer (2026-07-28) ---
    "0808efa0",  # CControllerTracer::Reset()
    "0808f090",  # CControllerTracer::CControllerTracer()
    "0808f100",  # CControllerTracer::CControllerTracer(unsigned char)
    "0808f170",  # CControllerTracer::EraseCtrl(unsigned char)
    "0808f1c0",  # CControllerTracer::EraseCtrls(unsigned char const*)
    "0808f2a0",  # CControllerTracer::SetDefCtrls(unsigned char const*)
    "0808f320",  # CControllerTracer::AppendChnPressure(CLinkedEvent*&) const
    "0808f410",  # CControllerTracer::AppendPitchBend(CLinkedEvent*&) const
    "0808f520",  # CControllerTracer::AppendFullProgram(CLinkedEvent*&) const
    "0808f770",  # CControllerTracer::AppendCtrl(CLinkedEvent*&, unsigned char) const
    "0808f890",  # CControllerTracer::AppendCtrls(CLinkedEvent*&, unsigned char const*) const
    "0808f9d0",  # CControllerTracer::AppendDefaultChnPressure(CLinkedEvent*&) const
    "0808faa0",  # CControllerTracer::AppendDefaultPitchBend(CLinkedEvent*&) const
    "0808fb80",  # CControllerTracer::AppendDefaultCtrl(CLinkedEvent*&, unsigned char) const
    "0808fcc0",  # CControllerTracer::AppendDefaultCtrls(CLinkedEvent*&, unsigned char const*) const
    "08182de0",  # CControllerTracer::~CControllerTracer() (D1, complete)
    "08182df0",  # CControllerTracer::InitAfterDefaultCtor(unsigned char)
    "08182e00",  # CControllerTracer::UpdateCtrl(unsigned char, unsigned char)
    "08182e20",  # CControllerTracer::~CControllerTracer() (D0, deleting)
    "0808ef70",  # CCtrlAndParamTracer::InitAfterDefaultCtor(unsigned char)
    "0808f000",  # CCtrlAndParamTracer::Reset()
    "08091450",  # CCtrlAndParamTracer::CCtrlAndParamTracer(unsigned char)
    "080918c0",  # CCtrlAndParamTracer::CCtrlAndParamTracer()
    "08091d20",  # CCtrlAndParamTracer::AppendParams(CLinkedEvent*&, SBytePair const*, SBytePair const*) const
    "08091e00",  # CCtrlAndParamTracer::AppendAllParams(CLinkedEvent*&) const
    "08092240",  # CCtrlAndParamTracer::CCtrlAndParamTracer(CCtrlAndParamTracer const&)
    "08092c70",  # CCtrlAndParamTracer::UpdateCtrl(unsigned char, unsigned char)
    "08092e80",  # CCtrlAndParamTracer::operator=(CCtrlAndParamTracer const&)

    # --- CNoteTracer, "Tracer" family follow-up (2026-07-28). CNoteTracerTransposer
    # deliberately deferred -- see note_tracer.h's own header comment (interface
    # CNoteTransposerOwner resolved as a single pure-virtual method, but
    # CNoteTracerTransposer::RendundantInsertion's own override is a genuinely dense
    # ~1400-byte duplicate-note-conflict resolver, out of this pass's scope). ---
    "08093050",  # CNoteTracer::~CNoteTracer() (D1, complete)
    "080930b0",  # CNoteTracer::~CNoteTracer() (D0, deleting)
    "08093790",  # Swap(CNoteTracer&, CNoteTracer&)
    "08093ff0",  # CNoteTracer::CNoteTracer()
    "080940b0",  # CNoteTracer::CNoteTracer(unsigned char)
    "08094170",  # CNoteTracer::CNoteTracer(CNoteTracer const&)
    "08094290",  # CNoteTracer::operator=(CNoteTracer const&)
    "080946b0",  # CNoteTracer::ResetPendingNotes()
    "080947d0",  # CNoteTracer::Remove(unsigned char)
    "08094860",  # CNoteTracer::GetLeftMost() const
    "08094980",  # CNoteTracer::GetRightMost() const
    "08094da0",  # CNoteTracer::CreateBuffer(TDynBuffer<CNoteTracer::CBufferedNote>&, unsigned int)
    "08094e40",  # CNoteTracer::ReallocBuffer(TDynBuffer<CNoteTracer::CBufferedNote>&, unsigned int)
    "08094ed0",  # CNoteTracer::DestroyBuffer(TDynBuffer<CNoteTracer::CBufferedNote>&)
    "08094f20",  # CNoteTracer::SwapBuffer(TDynBuffer<CNoteTracer::CBufferedNote>&)
    "08095200",  # CNoteTracer::Insert(CNoteTracer::CBufferedNote)
    "08095330",  # CNoteTracer::ListNotesOn(CLinkedEvent*&) const
    "080958d0",  # CNoteTracer::ListNotesOn(CLinkedEvent*&, signed char) const
    "08095eb0",  # CNoteTracer::ListNotesOff(CLinkedEvent*&) const
    "08096460",  # CNoteTracer::ListSoundsOn(CLinkedEvent*&) const
    "08096a00",  # CNoteTracer::ListSoundsOff(CLinkedEvent*&) const
    "08096fb0",  # CNoteTracer::ClearEntries()
    "080970d0",  # CNoteTracer::RefreshEntries()
    "08183f60",  # CNoteTracer::RendundantInsertion(CNoteTracer::CBufferedNote&, CNoteTracer::CBufferedNote) (weak, base vtable slot)

    # --- fs_converter.h/pcm_filter.h/kaiser_window.h, PCM/sample-rate conversion
    # utility cluster (2026-07-28) -- see fs_converter.h's own header comment for
    # the 2 larger candidates (CRTRouter/CRTRouterApiInstance, the CFileBase file-
    # format-loader family) traced and REJECTED first. CFsConverterNormal's own
    # Process()/BuildFilterCoeffTable() and CFsCwInterpolation's own Process()/
    # SetFilterCoeffs() overrides are DEFERRED (real, substantial polyphase-FIR
    # ring-buffer resampler core, out of scope this pass) -- their addresses are
    # deliberately NOT included below, see fs_converter.h for the full list.
    "08305b50",  # CKaiserWindowCoeffs::~CKaiserWindowCoeffs() (D1)
    "08305b60",  # CKaiserWindowCoeffs::SetWindowLength(int)
    "08305ba0",  # CKaiserWindowCoeffs::SetSideLobeAttenuation(double)
    "08305bd0",  # CKaiserWindowCoeffs::SetBesselFunctionLength(int)
    "08305c00",  # CKaiserWindowCoeffs::CalcDenomAlpha()
    "08305c30",  # CKaiserWindowCoeffs::GetWindowCoeff(int)
    "08305cb0",  # CKaiserWindowCoeffs::BesselFunction(double)
    "08305ea0",  # CKaiserWindowCoeffs::CalcCoeffAlpha()
    "08305f30",  # CKaiserWindowCoeffs::~CKaiserWindowCoeffs() (D0)
    "08305f50",  # CKaiserWindowCoeffs::CKaiserWindowCoeffs()
    "08304160",  # CDecimationFilterCoeffs::SetSampleRate(int)
    "083041a0",  # CDecimationFilterCoeffs::SetCutoffFreq(int)
    "083041d0",  # CDecimationFilterCoeffs::CalcFreqCoeffs()
    "08304200",  # CDecimationFilterCoeffs::GetDelayOffsetSamples()
    "08304210",  # CDecimationFilterCoeffs::GetDelayOffsetSeconds()
    "08304240",  # CDecimationFilterCoeffs::GetDelayOffset()
    "08304260",  # CDecimationFilterCoeffs::GetFilterCoeff(int)
    "08304300",  # CDecimationFilterCoeffs::SetBesselFunctionLength(int)
    "08304320",  # CDecimationFilterCoeffs::SetSideLobeAttenuation(double)
    "08304340",  # CDecimationFilterCoeffs::SetFilterLength(int)
    "08304380",  # CDecimationFilterCoeffs::~CDecimationFilterCoeffs() (D1)
    "083043a0",  # CDecimationFilterCoeffs::~CDecimationFilterCoeffs() (D0)
    "083043d0",  # CDecimationFilterCoeffs::CDecimationFilterCoeffs()
    "08305fb0",  # COversamplingFilterCoeffs::SetOversamplingRate(int)
    "08305fd0",  # COversamplingFilterCoeffs::GetFilterCoeff(int)
    "08306000",  # COversamplingFilterCoeffs::~COversamplingFilterCoeffs() (D1)
    "08306020",  # COversamplingFilterCoeffs::~COversamplingFilterCoeffs() (D0)
    "08306050",  # COversamplingFilterCoeffs::COversamplingFilterCoeffs()
    "08304460",  # CFsConverterNormal::SetFilterCoeffs(int)
    "083044f0",  # CFsConverterNormal::SetFilterCoeffs(int,int,float,int,int) -- real 1-byte "return;" base stub
    "08304cd0",  # CFsConverterNormal::SetFilterCoeffs(int,int,int,int,int) (BuildFilterCoeffTable, virtual slot 5)
    "08304ab0",  # CFsConverterNormal::GetDelayOffsetSamples()
    "08304ad0",  # CFsConverterNormal::GetDelayOffsetSeconds()
    "08304af0",  # CFsConverterNormal::GetDelayOffset()
    "08304b10",  # CFsConverterNormal::Reset()
    "08304f00",  # CFsConverterNormal::SetBesselFunctionLength(int)
    "08304f20",  # CFsConverterNormal::SetSideLobeAttenuation(double)
    "08304f40",  # CFsConverterNormal::~CFsConverterNormal() (D1)
    "08304fd0",  # CFsConverterNormal::~CFsConverterNormal() (D0)
    "08305060",  # CFsConverterNormal::CFsConverterNormal(int)
    "08305aa0",  # CFsCwInterpolation::~CFsCwInterpolation() (D1)
    "08305ad0",  # CFsCwInterpolation::~CFsCwInterpolation() (D0)
    "08305b00",  # CFsCwInterpolation::CFsCwInterpolation(int)
    "083059f0",  # CFsCwInterpolation::SetFilterCoeffs(int,int,float,int,int)
    "08306080",  # CPcmFilter::~CPcmFilter() (D1)
    "08306090",  # CPcmFilter::SetBitsPerSample(int)
    "083060e0",  # CPcmFilter::IntToFloat(long**, float**, unsigned long, int)
    "08306300",  # CPcmFilter::FloatToInt(float**, long**, unsigned long, int)
    "083064c0",  # CPcmFilter::~CPcmFilter() (D0)
    "083064e0",  # CPcmFilter::CPcmFilter(int)
    "08306530",  # CPcmFilter::BitShift(long**, long**, unsigned long, int, int)
    "08306c60",  # CPcmFilter::Copy(float**, float**, unsigned long, int)
    "08306e90",  # CPcmFilter::Reverse(float**, float**, unsigned long, int)
    "083070f0",  # CPcmFilter::Fade(float**, float**, unsigned long, int, int)
    "08307560",  # CPcmFilter::IsAlwaysBelow(float**, unsigned long, int, float)
    "08307790",  # CPcmFilter::IsSilent(float**, unsigned long, int)
    "08307a50",  # CPcmFilter::GetMaximumAbsValue(float**, unsigned long, int)
    "08307cb0",  # CPcmFilter::ClipAndGetPeakLevels(float**, float**, float*, unsigned long, int)
    "08307fe0",  # CPcmFilter::GetPeakLevels(float**, float*, unsigned long, int)
    "08308270",  # CPcmFilter::Mute(float**, unsigned long, unsigned long)

    # --- CLongBinaryFile + CAudioFile/CAudioFileRead/CAudioFileReadEx/CAudioFileWrite (2026-07-28) ---
    "082faca0",  # CAudioFile::CloseFile()
    "082facd0",  # CAudioFile::GetAudioFileFormat()
    "082face0",  # CAudioFile::SetSampleRate(int)
    "082fad10",  # CAudioFile::GetSampleRate()
    "082fad20",  # CAudioFile::SetLoopEnable(bool)
    "082fad30",  # CAudioFile::SetLoopStart(unsigned long)
    "082fad40",  # CAudioFile::SetLoopEnd(unsigned long)
    "082fad50",  # CAudioFile::GetCurTime(int*, int*, float*)
    "082fadc0",  # CAudioFile::GetCurTimeSecond()
    "082fae10",  # CAudioFile::GetTotalTime(int*, int*, float*)
    "082fae80",  # CAudioFile::GetTotalTimeSecond()
    "082faed0",  # CAudioFile::GetBitsPerSample()
    "082faee0",  # CAudioFile::SetChannelBlockSize(unsigned long)
    "082faf90",  # CAudioFile::SetNumChannels(int)
    "082fafc0",  # CAudioFile::GetNumChannels()
    "082fafd0",  # CAudioFile::FileIsOpen()
    "082fafe0",  # CAudioFile::SetSamplePosition(long long)
    "082fb0b0",  # CAudioFile::SetCurTime(int, int, float)
    "082fb0f0",  # CAudioFile::SetCurTimeSecond(double)
    "082fb130",  # CAudioFile::GetSamplePosition()
    "082fb150",  # CAudioFile::GetAbsSamplePosition()
    "082fb170",  # CAudioFile::GetNumSamples()
    "082fb190",  # CAudioFile::CalcAvgBytesPerSec()
    "082fb1d0",  # CAudioFile::WriteID(char const*)
    "082fb200",  # CAudioFile::ReadChunkHeader(CChunkHeader8*)
    "082fb260",  # CAudioFile::ReadChunkHeader(CChunkHeader12*)
    "082fb2c0",  # CAudioFile::ReadID3v2FrameHeader(CID3v2FrameHeader*)
    "082fb320",  # CAudioFile::FindChunk(char const*, char const*, CChunkHeader12*, long long, long long)
    "082fb3d0",  # CAudioFile::FindChunk(char const*, char const*, CChunkHeader8*, long long, long long)
    "082fb480",  # CAudioFile::FindChunk(char const*, CChunkHeader12*, long long, long long)
    "082fb590",  # CAudioFile::FindChunk(char const*, CChunkHeader8*, long long, long long)
    "082fb680",  # CAudioFile::FindID3v2Frame(char const*, CID3v2FrameHeader*, long long, long long)
    "082fb760",  # CAudioFile::ReadID(char*)
    "082fb7a0",  # CAudioFile::CalcBlockAlign()
    "082fb820",  # CAudioFile::SetBitsPerSample(int)
    "082fb870",  # CAudioFile::Reset()
    "082fbae0",  # CAudioFile::~CAudioFile()
    "082fbb60",  # CAudioFile::SetAudioBufferSize(unsigned long)
    "082fbc10",  # CAudioFile::~CAudioFile()
    "082fbca0",  # CAudioFile::CAudioFile(unsigned long)
    "082fbde0",  # CAudioFile::TimeToSample(int, int, float, int)
    "082fbe20",  # CAudioFile::SampleToTime(long long, int, int*, int*, float*)
    "082fbed0",  # CAudioFile::SecondToTime(double, int*, int*, float*)
    "082fbf30",  # CAudioFile::SecondToSample(double, int)
    "082fbf50",  # CAudioFile::SampleToSecond(long long, int)
    "082fbfa0",  # CAudioFileRead::ReadAudioData(float**, unsigned long, int, float)
    "082fc080",  # CAudioFileRead::ReadAudioData(unsigned char**, unsigned long, int)
    "082fc110",  # CAudioFileRead::Crop(long long, long long)
    "082fc460",  # CAudioFileRead::CropCancel()
    "082fc4f0",  # CAudioFileRead::OpenDsdiffFile()
    "082fc500",  # CAudioFileRead::OpenWsdFile()
    "082fc510",  # CAudioFileRead::OpenDsfFile()
    "082fc520",  # CAudioFileRead::ReadID3v2()
    "082fc530",  # CAudioFileRead::ReadDsdData(unsigned char**, unsigned long, int)
    "082fc540",  # CAudioFileRead::ReadDsdData(float**, unsigned long, int, float)
    "082fc550",  # CAudioFileRead::ReadDsdData2(unsigned char**, unsigned long, int)
    "082fc560",  # CAudioFileRead::ReadDsdData2(float**, unsigned long, int, float)
    "082fc570",  # CAudioFileRead::SetSamplePosition(long long)
    "082fc610",  # CAudioFileRead::ReadPcmData(float**, unsigned long, int)
    "082fc6b0",  # CAudioFileRead::ReadPcmData(long**, unsigned long, int)
    "082fcb90",  # CAudioFileRead::OpenWaveFile()
    "082fd260",  # CAudioFileRead::OpenFile(char const*, int, int)
    "082fd450",  # CAudioFileRead::~CAudioFileRead()
    "082fd490",  # CAudioFileRead::~CAudioFileRead()
    "082fd4e0",  # CAudioFileRead::CAudioFileRead(unsigned long)
    "082fd510",  # CAudioFileReadEx::ReadAudioData(float**, unsigned long, int)
    "082fd5e0",  # CAudioFileReadEx::ReadDsdData(unsigned char**, unsigned long, int)
    "082fd5f0",  # CAudioFileReadEx::ReadDsdData(float**, unsigned long, int)
    "082fd600",  # CAudioFileReadEx::ReadDsdData2(unsigned char**, unsigned long, int)
    "082fd610",  # CAudioFileReadEx::ReadDsdData2(float**, unsigned long, int)
    "082fd620",  # CAudioFileReadEx::SetBufferSkipRate(long)
    "082fd630",  # CAudioFileReadEx::GetBufferSkipRate()
    "082fd640",  # CAudioFileReadEx::ReadPcmData(float**, unsigned long, int)
    "082fd6e0",  # CAudioFileReadEx::CloseFile()
    "082fd6f0",  # CAudioFileReadEx::OpenFile(char const*, int, int)
    "082fd700",  # CAudioFileReadEx::ReadPcmData(long**, unsigned long, int)
    "082fde30",  # CAudioFileReadEx::~CAudioFileReadEx()
    "082fde90",  # CAudioFileReadEx::SetSamplePosition(long long)
    "082fe060",  # CAudioFileReadEx::~CAudioFileReadEx()
    "082fe0d0",  # CAudioFileReadEx::CAudioFileReadEx(unsigned long)
    "082fe1d0",  # CAudioFileReadEx::BufferAudio(CFileBufferParams*)
    "082fe240",  # CAudioFileWrite::WriteWavHeader()
    "082fe340",  # CAudioFileWrite::WriteWavRequiredChunks()
    "082fe6d0",  # CAudioFileWrite::WriteBwfChunk()
    "082fe6e0",  # CAudioFileWrite::WriteWavTextData()
    "082fe8c0",  # CAudioFileWrite::WriteDsdiffHeader()
    "082fe8d0",  # CAudioFileWrite::WriteDsdiffRequiredChunks()
    "082feb30",  # CAudioFileWrite::WriteDsdiffTextData()
    "082feb40",  # CAudioFileWrite::WriteDsfHeader()
    "082fec30",  # CAudioFileWrite::WriteDsfRequiredChunks(long long)
    "082feee0",  # CAudioFileWrite::WriteID3v2()
    "082ff080",  # CAudioFileWrite::WriteID3v2DateAndTime()
    "082ff090",  # CAudioFileWrite::WriteWsdHeader()
    "082ff160",  # CAudioFileWrite::WriteWsdRequiredChunks()
    "082ff170",  # CAudioFileWrite::WriteAudioData(float**, unsigned long, int)
    "082ff1c0",  # CAudioFileWrite::WriteAudioData(unsigned char**, unsigned long, int)
    "082ff230",  # CAudioFileWrite::WriteDsdData(unsigned char**, unsigned long, int)
    "082ff240",  # CAudioFileWrite::WriteDsdData2(unsigned char**, unsigned long, int)
    "082ff250",  # CAudioFileWrite::OpenFile(char const*, int, int)
    "082ff380",  # CAudioFileWrite::WritePcmData(float**, unsigned long, int)
    "082ff410",  # CAudioFileWrite::WritePcmData(long**, unsigned long, int)
    "082ff9d0",  # CAudioFileWrite::SetBitsPerSample(int)
    "082ffa50",  # CAudioFileWrite::WriteWsdTextDataLocal(int, unsigned long)
    "082ffc80",  # CAudioFileWrite::WriteID3v2TextDataLocal(int, char const*)
    "082ffd80",  # CAudioFileWrite::WriteDsdiffTextDataLocal(int, char const*)
    "082ffe40",  # CAudioFileWrite::WriteWavTextDataLocal(int, char const*)
    "08300160",  # CAudioFileWrite::CloseFile()
    "083004d0",  # CAudioFileWrite::~CAudioFileWrite()
    "08300510",  # CAudioFileWrite::~CAudioFileWrite()
    "08300580",  # CAudioFileWrite::CAudioFileWrite(unsigned long)
    "083005b0",  # CLongBinaryFile::MoveToEnd()
    "083005e0",  # CLongBinaryFile::WriteData(long long, int)
    "08300c50",  # CLongBinaryFile::ReadData(int)
    "08300e80",  # CLongBinaryFile::ReadText(char*, int, int)
    "08300ef0",  # CLongBinaryFile::WriteText(char const*, int)
    "083011d0",  # CLongBinaryFile::GetFileName(char*)
    "083011f0",  # CLongBinaryFile::Tell()
    "08301220",  # CLongBinaryFile::Jump(int)
    "08301250",  # CLongBinaryFile::Seek(long long, int)
    "083012a0",  # CLongBinaryFile::Write(void const*, unsigned int)
    "083012e0",  # CLongBinaryFile::Read(void*, unsigned int)
    "08301320",  # CLongBinaryFile::Reset()
    "08301360",  # CLongBinaryFile::Close()
    "08301390",  # CLongBinaryFile::~CLongBinaryFile()
    "083013c0",  # CLongBinaryFile::Open(char const*, int, bool)
    "08301410",  # CLongBinaryFile::~CLongBinaryFile()
    "08301470",  # CLongBinaryFile::CLongBinaryFile()

    # CKorgPath/CKorgLinuxPath/UKontaktOposPath (2026-07-28) -- see korg_path.h,
    # korg_linux_path.h, kontakt_opos_path.h for full provenance. Abstract path
    # value-class base + its one real concrete Linux override, plus the 2 (of 3)
    # UKontaktOposPath OPOS<->Linux conversion helpers this family's own
    # GetOposPath/SetOposPath dispatch through. Also added this batch:
    # CFileOperation::GetLinuxRemapPath (long_binary_file.h) as an extern-only
    # slice of the already out-of-scope CFileOperation god object -- NOT added
    # to RECONSTRUCTED (no real body here, host-stub-backed only, same
    # convention as CFileOperation::Open/Close/Read/Write/Seek/Tell above).
    "089d25c0",  # CKorgPath::~CKorgPath()
    "089d25d0",  # CKorgPath::~CKorgPath()
    "089d25f0",  # CKorgPath::GetOposPath(char*, unsigned int)
    "089d2630",  # CKorgPath::SetOposPath(char const*)
    "089d2680",  # CKorgPath::CKorgPath(char const*)
    "089d26d0",  # CKorgPath::CKorgPath(CKorgPath const*)
    "089d2720",  # CKorgPath::Set(CKorgPath const*, char const*)
    "089d27d0",  # CKorgPath::GetPath(char*, unsigned int) const
    "089d2810",  # CKorgPath::SetPath(char const*)
    "089d2860",  # CKorgPath::GetPathName() const
    "089d28a0",  # CKorgPath::GetPathExtension() const
    "089d28c0",  # CKorgPath::GetPathNameNoExtension(char*, unsigned int) const
    "089d2950",  # CKorgPath::GetFolder() const
    "089d29a0",  # CKorgPath::MakePathFromFolder(char const*) const
    "089d2a50",  # CKorgPath::Find(CKorgPath const&) const
    "089d2b40",  # CKorgPath::HasExtension(char const*, char const*)
    "089d2b80",  # CKorgPath::AddExtension(char*, unsigned int, char const*)
    "089d2bd0",  # CKorgPath::RemoveExtension(char*)
    "089d2c00",  # CKorgPath::RemoveExtension(char*, char*, unsigned int)
    "089d2c70",  # CKorgPath::ValidExtension(char const*)
    "089d2c90",  # CKorgPath::Sanitize(char*)
    "089d2cd0",  # CKorgPath::Capitalized(char const*)
    "089d32e0",  # CKorgPath::Make(char const*)
    "089d2d00",  # CKorgLinuxPath::Separator() const
    "089d2d10",  # CKorgLinuxPath::SetOposPath(char const*)
    "089d2d60",  # CKorgLinuxPath::GetOposPath(char*, unsigned int)
    "089d2d80",  # CKorgLinuxPath::~CKorgLinuxPath()
    "089d2da0",  # CKorgLinuxPath::~CKorgLinuxPath()
    "089d2dd0",  # CKorgLinuxPath::Copy() const
    "089d2e20",  # CKorgLinuxPath::FindRecurse(char const*, CKorgPath const*) const
    "089d30f0",  # CKorgLinuxPath::TemporaryFileUsingExtension(char const*) const
    "089d3280",  # CKorgLinuxPath::CKorgLinuxPath(char const*)
    "089d32b0",  # CKorgLinuxPath::CKorgLinuxPath(CKorgPath const*)
    "089d3330",  # CKorgLinuxPath::Valid(char const*)
    "0846ca20",  # UKontaktOposPath::ConvertOposToLinux(char const*, char*, unsigned int)
    "0846cbe0",  # UKontaktOposPath::ConvertLinuxToOpos(char const*, char*, unsigned int)

    # --- Eva chunk-family batch (2026-07-28, commit pending): CChunkBase/CChunk/
    # CChunkBlock/CChunkOrphan/CChunkInfoItem/CChunkInfoList, 89 methods. See
    # include/chunk_family.h for full ground-truth provenance and the deferred
    # CBackupChunk/CChunkRootWithSeek(WithCRC) sibling survey.
    "0804d050",  # CChunkBase::GetRelSonNestLev() const
    "0804d060",  # CChunkBase::GetAbsSonNumber() const
    "0804d070",  # CChunk::GetRelSonNumber() const
    "0804d090",  # CChunk::GetFather()
    "0804d0a0",  # CChunk::SetFather(CChunkBase*)
    "0804d0b0",  # CChunk::SetRankNumber(unsigned int)
    "080ace90",  # CChunkBase::~CChunkBase()
    "080acf20",  # CChunkBase::GetAllInfo()
    "080acf60",  # CChunkBase::Init()
    "080acfd0",  # CChunkBase::PreClose()
    "080acfe0",  # CChunkBase::PostClose()
    "080ad000",  # CChunkBase::CloseSubChunk(CChunk*&)
    "080ad1d0",  # CChunkBase::Close()
    "080ad330",  # CChunkBase::OnChildDestroy()
    "080ad420",  # CChunk::Close()
    "080ad440",  # CChunk::OnWriteLenAndFlags(unsigned long, unsigned long, unsigned long, unsigned char)
    "080ad4e0",  # CChunk::PreClose()
    "080ad4f0",  # CChunk::PostClose()
    "080ad510",  # CChunkBlock::GetNextSubChunk(CChunk*&)
    "080ad5a0",  # CChunkBlock::AddSubChunk(CChunk*&, SIdVRF)
    "080ad630",  # CChunkBlock::CloseSubChunk(CChunk*&)
    "080ad670",  # CChunkBlock::PostClose()
    "080ad690",  # CChunkBlock::GetRelSonNumber() const
    "080ad730",  # CChunkBlock::OnSetInfo(CChunkInfoItem*)
    "080ad790",  # CChunk::OnSetInfo(CChunkInfoItem*)
    "080ad9f0",  # CChunk::Init()
    "080adbc0",  # CChunkBase::~CChunkBase()
    "080adc60",  # CChunk::~CChunk()
    "080add20",  # CChunkBlock::~CChunkBlock()
    "080ade50",  # CChunk::~CChunk()
    "080adf20",  # CChunkBlock::~CChunkBlock()
    "080ae050",  # CChunkBase::CChunkBase(SChkHeader const&)
    "080ae100",  # CChunkBase::ReadHeader(SChkHeader&)
    "080ae220",  # CChunkBase::WriteHeader(SChkHeader const&)
    "080ae500",  # CChunkBase::ReadBinary(void*, unsigned int)
    "080ae650",  # CChunkBase::WriteBinary(void const*, unsigned int)
    "080ae710",  # CChunkBase::LinkSubChunk(CChunk*&)
    "080aea20",  # CChunkBase::SetStatus()
    "080aeaf0",  # CChunk::CChunk(SChkHeader const&)
    "080aec10",  # CChunk::Skip(unsigned int)
    "080aed80",  # CChunk::Read(void*, unsigned int)
    "080aedc0",  # CChunk::Get(unsigned char&)
    "080aef20",  # CChunk::Write(void const*, unsigned int)
    "080aef60",  # CChunk::Put(unsigned char)
    "080af060",  # CChunk::SetInfo(unsigned char, unsigned char, unsigned char, unsigned char, char*)
    "080af210",  # CChunk::operator>>(unsigned char&)
    "080af330",  # CChunk::operator>>(char&)
    "080af450",  # CChunk::operator>>(signed char&)
    "080af570",  # CChunk::operator>>(unsigned short&)
    "080af6a0",  # CChunk::operator>>(short&)
    "080af7d0",  # CChunk::operator>>(unsigned int&)
    "080af920",  # CChunk::operator>>(unsigned long&)
    "080afa70",  # CChunk::operator>>(int&)
    "080afbc0",  # CChunk::operator>>(long&)
    "080afd10",  # CChunk::operator>>(CZ&)
    "080afe80",  # CChunk::operator<<(unsigned char)
    "080aff50",  # CChunk::operator<<(char)
    "080b0020",  # CChunk::operator<<(signed char)
    "080b00f0",  # CChunk::operator<<(unsigned short)
    "080b01d0",  # CChunk::operator<<(short)
    "080b02b0",  # CChunk::operator<<(unsigned int)
    "080b03d0",  # CChunk::operator<<(unsigned long)
    "080b04f0",  # CChunk::operator<<(int)
    "080b0610",  # CChunk::operator<<(long)
    "080b0730",  # CChunk::operator<<(CZ const&)
    "080b0ba0",  # CChunkBlock::CChunkBlock(SChkHeader const&)
    "080b0d60",  # CChunkBase::AddSubChunk(CChunk*&, SIdVRF)
    "080b1000",  # CChunkBase::GetNextSubChunk(CChunk*&)
    "080b1390",  # CChunkBlock::PreClose()
    "080b1510",  # CChunkBlock::Init()
    "080b1660",  # CChunkBlock::GetAllInfo()
    "080b1cf0",  # CChunkInfoItem::CChunkInfoItem(unsigned char, unsigned char, unsigned char, unsigned char, unsigned char, char const*)
    "080b1de0",  # CChunkInfoItem::CChunkInfoItem()
    "080b1e40",  # CChunkInfoItem::~CChunkInfoItem()
    "080b1e90",  # CChunkInfoItem::SetRankNum(unsigned char)
    "080b1ee0",  # CChunkInfoItem::Serialize(CChunk*)
    "080b2030",  # CChunkInfoItem::DeSerialize(CChunk*)
    "080b2160",  # CChunkInfoList::CChunkInfoList()
    "080b2170",  # CChunkInfoList::~CChunkInfoList()
    "080b2200",  # CChunkInfoList::DestroyAllItem()
    "080b2290",  # CChunkInfoList::Serialize(CChunk*)
    "080b2350",  # CChunkInfoList::DeSerialize(CChunk*)
    "080b24c0",  # CChunkInfoList::Add(CChunkInfoItem*)
    "080b2530",  # CChunkInfoList::GetNext(CChunkInfoItem const*) const
    "080b2550",  # CChunkOrphan::~CChunkOrphan()
    "080b25e0",  # CChunkOrphan::~CChunkOrphan()
    "080b2600",  # CChunkOrphan::CChunkOrphan(SChkHeader const&, unsigned char*, int)
    "081852f0",  # CChunkBase::GetAvailBytes() const
    "08185360",  # CChunkBlock::GetRelSonNestLev() const

    # --- Manifest bookkeeping gap fix (2026-07-28, Eva manifest audit): commit
    # fa6ac5c ("CChunkServer::Exec(CMessage&) promoted Tier B -> Tier A", closes the
    # last Tier B method in CChunkServer, chunk_server.h) reconstructed a real 1336-
    # byte function but never updated this manifest. Found via a systematic pass
    # cross-checking every commit that touched src/include against whether it also
    # touched this file in the same commit -- see also the 0816be90 entry above
    # (same bug class, different commit: bb606a3's own backfill of the 00885a4 batch
    # missed one sibling address).
    "080cc0d0",  # CChunkServer::Exec(CMessage&)

    # --- CKorgRiff, a generic RIFF-style chunked-file base deriving from the
    # already-reconstructed CKorgFile (2026-07-28, fresh nm -C class-inventory
    # sweep -- see include/korg_riff.h for the full "shared root + siblings"
    # provenance, vtable-slot layout, and the deliberately-deferred
    # CKorgKmp/CKorgKsc/CKorgKsf/CKorgProgram sibling family). All 22
    # addresses below (12 distinctly-named real methods; dtor/SwapFile/
    # SwapLittleEndian/SwapBigEndian/Swap each contribute 2-3 addresses for
    # D1/D0 or the short/ushort/uint overload set) verified present in the
    # static export via a direct functions.csv address lookup before being
    # added here. verify/test_korg_riff.cpp: 27 checks, including a real host
    # round-trip (WriteFile() -> fopen/fwrite a real file -> fresh instance
    # ReadFile()s it back) exercising the "NAME"-chunk special case, the
    # virtual ReadChunk() dispatch for an unrecognized tag, and both
    # IsBigEndian() branches of SwapFile()/WriteHeader(). Full make -k verify
    # (83 binaries) stays green (0 FAIL); tools/build_lenny.sh LINK OK
    # against the real on-image ABI.
    "089d1770",  # CKorgRiff::ReadChunk(unsigned int, unsigned int, FILE*)
    "089d17a0",  # CKorgRiff::~CKorgRiff() [D1]
    "089d17c0",  # CKorgRiff::~CKorgRiff() [D0, deleting]
    "089d17f0",  # CKorgRiff::ReadFile(FILE*)
    "089d18e0",  # CKorgRiff::WriteFile(FILE*)
    "089d1990",  # CKorgRiff::CKorgRiff(char const*, char const*)
    "089d19d0",  # CKorgRiff::CNameChunk::GetName(char*, unsigned int) const
    "089d1a00",  # CKorgRiff::CNameChunk::SetName(char const*)
    "089d1a30",  # CKorgRiff::WriteHeader(unsigned int, unsigned int, FILE*)
    "089d1a90",  # CKorgRiff::SwapFile(short&)
    "089d1ac0",  # CKorgRiff::SwapFile(unsigned short&)
    "089d1af0",  # CKorgRiff::SwapFile(unsigned int&)
    "089d1b20",  # CKorgRiff::SwapLittleEndian(short&)
    "089d1b30",  # CKorgRiff::SwapLittleEndian(unsigned short&)
    "089d1b40",  # CKorgRiff::SwapLittleEndian(unsigned int&)
    "089d1b50",  # CKorgRiff::SwapBigEndian(short&)
    "089d1b60",  # CKorgRiff::SwapBigEndian(unsigned short&)
    "089d1b70",  # CKorgRiff::SwapBigEndian(unsigned int&)
    "089d1b80",  # CKorgRiff::Swap(short&)
    "089d1b90",  # CKorgRiff::Swap(unsigned short&)
    "089d1ba0",  # CKorgRiff::Swap(unsigned int&)
    "089da490",  # CKorgRiff::IsBigEndian() const

    # --- CKorgKmp/CKorgKsf/CKorgKsc partial reconstruction (2026-07-28, same-day
    # follow-up on the CKorgRiff batch above). Re-investigated korg_file.h's own
    # deferred-sibling note ("all 3 take a CSTGMultisampleBank*, out of scope") with
    # fresh objdump -d -C tracing of this whole family's entire .text range
    # (0x089ca550..0x089d1780, all 62 methods) -- found ZERO calls to
    # CSTGMultisampleBank or any other out-of-scope class. That claim was a
    # class-NAME collision with an unrelated, genuinely-MOSS-dependent
    # CMultisampleChunk/CSampleChunk/CSampleDataChunk family at .text
    # 0x08e30a50-0x08e31c00 (real signatures take CSTGMultisample*/CSTGSample*/
    # CSTGDrumSample*/CSTGSampleZone*/CSTGPCMBlock*) that happens to share short
    # nested-class names with THIS family's own (different, MOSS-free) nested
    # types of the same name -- see include/korg_kmp.h's file header for the full
    # correction. The REAL reason this family isn't fully reconstructed in one
    # pass: it's a genuinely deep on-disk chunked binary format (CKorgKmp's own
    # ReadChunk() alone dispatches 5+ distinct real chunk tags -- "RLP1"/"RLP2"/
    # "RLP3"/"MNO1"/"MSP1", each with its own fixed-size on-disk record and GCC
    # magic-constant division for record counts), disproportionate to a batch.
    # Reconstructed here: the tractable simple-accessor surface across all 3
    # classes' real ctors, dtors, TypeString()/IsBigEndian()/MakeFolder()/
    # GetUUID()/SetUUID()/GetName()/IsStereoCounterpart()/CanAddSample()/
    # MakeSampleFileName()/SetSampleDataSize(), plus 5 nested value-type
    # accessors (CMultisampleChunk, CMultisampleRelativeChunk, CSampleChunk,
    # CSampleFileNameChunk, CSampleDataChunk) -- 33 addresses below, each
    # verified present in the static export via a direct functions.csv address
    # lookup before being added. Deferred (real next lead, same "genuinely deep"
    # reason): Read()/Write()/ReadChunk()/WriteFile()/ReadFile() on all 3
    # classes, AddSample()/GetSample()/SortSamples()/AddSkippedSamples()/
    # MakeMultisampleFileName() (CKorgKmp), AddProgram()/AddMultisample()(x6)/
    # GetMultisample()(x5)/GetSample()/ReadLine()/SetPath() (CKorgKsc), and
    # CKorgProgram entirely (COscillator::Sort()/::Add() alone 946/1197 bytes).
    # verify/test_korg_kmp.cpp (12 checks incl. a real host mkdir() round-trip
    # and 2 CanAddSample overlap/non-overlap KATs against manually-populated
    # test-hook lists), verify/test_korg_ksf.cpp (7 checks incl. a real
    # sprintf-based MakeSampleFileName KAT and a real malloc/free
    # SetSampleDataSize sequence), verify/test_korg_ksc.cpp (5 checks incl. a
    # real host mkdir() round-trip). Full `make verify` (89 binaries) stays
    # green (0 FAIL).
    "089ccaa0",  # CKorgKmp::CKorgKmp(char const*, char const*, unsigned int, CKorgKmp::KorgType, unsigned int, unsigned int, unsigned int, unsigned int, unsigned char, unsigned char)
    "089cc590",  # CKorgKmp::~CKorgKmp() [D1]
    "089cca80",  # CKorgKmp::~CKorgKmp() [D0, deleting]
    "089cc570",  # CKorgKmp::TypeString(CKorgKmp::KorgType)
    "089d9dd0",  # CKorgKmp::IsBigEndian() const
    "089cb720",  # CKorgKmp::MakeFolder()
    "089cc480",  # CKorgKmp::GetName(char*, unsigned int) const
    "089cc4b0",  # CKorgKmp::IsStereoCounterpart(CKorgKmp const*) const
    "089cb770",  # CKorgKmp::CanAddSample(unsigned char, unsigned char)
    "089cb660",  # CKorgKmp::CMultisampleChunk::GetName(char*, unsigned int) const
    "089cb690",  # CKorgKmp::CMultisampleChunk::SetName(char const*)
    "089cb6c0",  # CKorgKmp::CMultisampleRelativeChunk::GetName(char*, unsigned int) const
    "089cb6f0",  # CKorgKmp::CMultisampleRelativeChunk::SetName(char const*)
    "089d0580",  # CKorgKsf::CKorgKsf(char const*, char const*, unsigned int, CKorgKsf::KorgType, bool)
    "089cff50",  # CKorgKsf::~CKorgKsf() [D1]
    "089cff90",  # CKorgKsf::~CKorgKsf() [D0, deleting]
    "089d09a0",  # CKorgKsf::TypeString(CKorgKsf::KorgType)
    "089da350",  # CKorgKsf::IsBigEndian() const
    "089d0950",  # CKorgKsf::MakeSampleFileName(unsigned int, unsigned int, unsigned int, char*, unsigned int)
    "089d0850",  # CKorgKsf::SetSampleDataSize(unsigned int, bool)
    "089d07b0",  # CKorgKsf::CSampleChunk::GetName(char*, unsigned int) const
    "089d07e0",  # CKorgKsf::CSampleChunk::SetName(char const*)
    "089d0810",  # CKorgKsf::CSampleChunk::GetStartOffsetSamples(unsigned int) const
    "089d0830",  # CKorgKsf::CSampleChunk::SetStartOffsetSamples(unsigned int, unsigned int)
    "089d08c0",  # CKorgKsf::CSampleFileNameChunk::GetSampleFileName(char*, unsigned int) const
    "089d08f0",  # CKorgKsf::CSampleFileNameChunk::SetSampleFileName(char const*)
    "089d0930",  # CKorgKsf::CSampleDataChunk::SetOneShot(bool)
    "089cef70",  # CKorgKsc::CKorgKsc(char const*, char const*, bool, bool)
    "089ceb40",  # CKorgKsc::~CKorgKsc() [D1]
    "089cef50",  # CKorgKsc::~CKorgKsc() [D0, deleting]
    "089ce100",  # CKorgKsc::GetUUID(char*, unsigned int) const
    "089ce150",  # CKorgKsc::SetUUID(char const*)
    "089ce1a0",  # CKorgKsc::MakeFolder()

    # CChkItem/CDumpReqDescr/CDumpHeaderDescr/CChunkClient (src/dump/chunk_client.cpp,
    # include/chunk_client.h), fresh nm -C class-inventory sweep (2026-07-28). Found via
    # buffering_task.h's own pre-existing "mChunkClient -- genuinely separate,
    # un-reconstructed chunk-transfer client class (out of scope)" flag. CChunkClient
    # derives from the already-real CTask (task.h); manual-vtable-as-data-array
    # convention (PTR__CChunkClient_08e857c8, 26 slots, confirmed via direct .rodata
    # byte read). CDumpReqDescr/CDumpHeaderDescr use real C++ virtual (a plain
    # single-base override chain, same convention as chunk_family.h's CChunkBase/
    # CChunk). 64 of 67 methods found in this cluster are reconstructed here;
    # OpenSubChunk()/CloseSubChunk() (real signature, Tier B) and CChunkClient's own
    # D0 "deleting" destructor (080c8e90) stay pending -- see chunk_client.h for the
    # full writeup, including why those 3 (CChunkRootWithSeek/CResourceChunk
    # dependency, D0-vs-D1 dtor precedent) are genuinely out of scope for this batch.
    # verify/test_chunk_client.cpp (62 checks): CChkItem/CDumpReqDescr/
    # CDumpHeaderDescr Serialize/DeSerialize/operator= round-trips, CChunkClient ctor
    # field checks, Abort/StoppedByUser/Save.../Load... "no override -> no-op" default
    # behavior, PrepareList()/OnPrepareMicro() all 3 real branches, LoadRes/SaveRes/
    # MergeRes/LoadResSync real internal state transitions (ungated, no IsXxx hook --
    # OutMono() itself fails cleanly with no live receiver wired up), and Exec(CMessage&)
    # dispatch across every real ECB code including the 2 FailAndReset()/Reset()-driving
    # shapes of ECB 0xe2/0xe5. Full `make verify` (87 binaries) stays green (0 FAIL).
    "080c8b60",  # CChkItem::CChkItem(unsigned char, unsigned char, unsigned char*)
    "080c8bf0",  # CChkItem::CChkItem()
    "080c8c10",  # CChkItem::~CChkItem()
    "080c8c40",  # CChkItem::Serialize(unsigned char*) const
    "080c8cf0",  # CChkItem::DeSerialize(unsigned char*)
    "080cc980",  # CDumpReqDescr::Serialize(unsigned char*, unsigned char) const
    "080cca80",  # CDumpReqDescr::~CDumpReqDescr() [D0, deleting]
    "080ccaf0",  # CDumpReqDescr::Reset()
    "080ccb60",  # CDumpReqDescr::~CDumpReqDescr() [D1]
    "080ccbb0",  # CDumpReqDescr::DeSerialize(unsigned char const*, unsigned char)
    "080cce10",  # CDumpReqDescr::CDumpReqDescr()
    "080cce40",  # CDumpReqDescr::SetMicro(unsigned char, unsigned char, unsigned char*)
    "080ccfe0",  # CDumpReqDescr::SetSingle(CDumpReqDescr::EResource, unsigned char, unsigned char*)
    "080cd170",  # CDumpReqDescr::operator=(CDumpReqDescr const&)
    "080cc610",  # CDumpHeaderDescr::DeSerialize(unsigned char const*, unsigned char)
    "080cc700",  # CDumpHeaderDescr::Serialize(unsigned char*, unsigned char) const
    "080cc7f0",  # CDumpHeaderDescr::Reset()
    "080cc810",  # CDumpHeaderDescr::~CDumpHeaderDescr() [D1]
    "080cc830",  # CDumpHeaderDescr::~CDumpHeaderDescr() [D0, deleting]
    "080cc860",  # CDumpHeaderDescr::CDumpHeaderDescr()
    "080cc890",  # CDumpHeaderDescr::SetMicro(unsigned char, unsigned char, unsigned char*, unsigned long)
    "080cc8d0",  # CDumpHeaderDescr::SetSingle(CDumpReqDescr::EResource, unsigned char, unsigned char*, unsigned long)
    "080cc910",  # CDumpHeaderDescr::operator=(CDumpReqDescr const&)
    "080cc940",  # CDumpHeaderDescr::operator=(CDumpHeaderDescr const&)
    "080c8da0",  # CChunkClient::OnPrepareSingle(CDumpReqDescr const*, TPtrArray<CChkItem>&)
    "080c8db0",  # CChunkClient::~CChunkClient() [D1]
    "080c8e80",  # CChunkClient::~CChunkClient() [non-virtual this-8 thunk]
    "080c8f80",  # CChunkClient::~CChunkClient() [non-virtual this-8 thunk]
    "080c8f90",  # CChunkClient::CChunkClient(CModule const&)
    "080c90d0",  # CChunkClient::Abort()
    "080c9130",  # CChunkClient::StoppedByUser()
    "080c9190",  # CChunkClient::PrepareList(CDumpReqDescr const*, TPtrArray<CChkItem>&)
    "080c9330",  # CChunkClient::SaveDump(CDumpReqDescr const&)
    "080c96d0",  # CChunkClient::LoadDump(CDumpHeaderDescr const&)
    "080c9aa0",  # CChunkClient::SaveFile(char const*, CDumpReqDescr const&)
    "080c9f50",  # CChunkClient::LoadFile(char const*, CDumpHeaderDescr const&)
    "080ca420",  # CChunkClient::OnPrepareMicro(CDumpReqDescr const*, TPtrArray<CChkItem>&)
    "080ca4d0",  # CChunkClient::Reset()
    "080ca580",  # CChunkClient::FailAndReset()
    "080ca720",  # CChunkClient::Exec(CMessage&)
    "080caa40",  # CChunkClient::LoadRes(CResourceChunk*, TPtrArray<CLoadResElem> const&, int)
    "080cad40",  # CChunkClient::LoadResSync(CResourceChunk*, TPtrArray<CLoadResElem> const&)
    "080cadc0",  # CChunkClient::SaveRes(char const*, TPtrArray<CSaveResElem> const&)
    "080cb0e0",  # CChunkClient::MergeRes(CResourceChunk*, CResourceChunk*, TPtrArray<CMergeElem> const*, unsigned long)
    "08185da0",  # CChunkClient::OnAcceptedHd(CDumpHeaderDescr const&)
    "08185db0",  # CChunkClient::OnAcceptedRq(CDumpHeaderDescr const&)
    "08185dc0",  # CChunkClient::OnInternalAbort(CDumpReqDescr const&)
    "08185dd0",  # CChunkClient::OnExternalAbort(CDumpReqDescr const&)
    "08185de0",  # CChunkClient::OnStoppedByUser(CDumpReqDescr const&)
    "08185df0",  # CChunkClient::OnByteCount(unsigned long)
    "08185e00",  # CChunkClient::OnEnd(TObjArray<unsigned char> const*, CDumpReqDescr const&)
    "08185e30",  # CChunkClient::OnBegin()
    "08185e60",  # CChunkClient::OnEnd()
    "08185e90",  # CChunkClient::OnSingleEnd(TPtrArray<CResElemBase>*)
    "08185ea0",  # CChunkClient::IsLoadFileToBeExecuted(char const*, CDumpHeaderDescr const&) const
    "08185eb0",  # CChunkClient::IsSaveFileToBeExecuted(char const*, CDumpReqDescr const&) const
    "08185ec0",  # CChunkClient::IsLoadDumpToBeExecuted(CDumpHeaderDescr const&) const
    "08185ed0",  # CChunkClient::IsSaveDumpToBeExecuted(CDumpReqDescr const&) const
    "08185ee0",  # CChunkClient::IsAbortToBeExecuted() const
    "08185ef0",  # CChunkClient::IsStoppedByUserToBeExecuted() const
    "08185f00",  # CChunkClient::OnGetParamForSaveFile() const
    "08185f50",  # CChunkClient::OnGetParamForSaveDump() const
    "08185fa0",  # CChunkClient::OnGetParamForLoadFile() const
    "08185ff0",  # CChunkClient::OnGetParamForLoadDump() const

    # CSmplMemManager (smpl_mem_manager.h/.cpp), 2026-07-28. 52 of 55 real
    # nm -C addresses reconstructed (3 deferred as documented stubs:
    # csmplmemmanagerstartup, and the 3-arg adjuststereomsno/
    # adjuststereosampleno cores -- see smpl_mem_manager.h's class comment).
    "08d624b0",  # CSmplMemManager::CSmplMemManager()
    "08d624e0",  # CSmplMemManager::~CSmplMemManager()
    "08d62840",  # CSmplMemManager::setramsize()
    "08d62870",  # CSmplMemManager::updateramsize(unsigned long)
    "08d62890",  # CSmplMemManager::getnewmsno(int, short*, short*)
    "08d62d20",  # CSmplMemManager::getnewmsnodec(int, short*, short*)
    "08d63130",  # CSmplMemManager::incms(short)
    "08d63150",  # CSmplMemManager::decms(short)
    "08d631c0",  # CSmplMemManager::addms(short)
    "08d634f0",  # CSmplMemManager::clearms()
    "08d63500",  # CSmplMemManager::getfreemsnum(unsigned char*)
    "08d63540",  # CSmplMemManager::getslidermsno(short)
    "08d63710",  # CSmplMemManager::multisamplecompare(CUsrMultisample*, char*, CUsrMultisample*)
    "08d63b50",  # CSmplMemManager::searchstereonoless(char*, short, CUsrMultisample*, short, short)
    "08d64170",  # CSmplMemManager::searchstereonomore(char*, short, CUsrMultisample*, short, short)
    "08d64ff0",  # CSmplMemManager::adjuststereomsnosub(short, short)
    "08d65260",  # CSmplMemManager::adjuststereomsno()
    "08d65380",  # CSmplMemManager::getnewsmplno(int, short*, short*, short)
    "08d65860",  # CSmplMemManager::getnewsmplnodec(int, short*, short*, short)
    "08d65e70",  # CSmplMemManager::incsmpl(short)
    "08d65e90",  # CSmplMemManager::decsmpl(short)
    "08d65f10",  # CSmplMemManager::addsmpl(short)
    "08d66250",  # CSmplMemManager::clearsmpl()
    "08d66270",  # CSmplMemManager::getfreesmplnum(unsigned char*)
    "08d662b0",  # CSmplMemManager::getslidersampleno(short)
    "08d66480",  # CSmplMemManager::searchstereonomore(char*, short, CUsrSample*, short, short)
    "08d66620",  # CSmplMemManager::searchstereonoless(char*, short, CUsrSample*, short, short)
    "08d66b00",  # CSmplMemManager::adjuststereosamplenosub(short, short)
    "08d66d70",  # CSmplMemManager::adjuststereosampleno()
    "08d66e90",  # CSmplMemManager::samplecompare(CUsrSample*, char*, CUsrSample*)
    "08d66fa0",  # CSmplMemManager::isexistbank(unsigned char)
    "08d66fc0",  # CSmplMemManager::getfreetop(unsigned char)
    "08d66fd0",  # CSmplMemManager::setfreetop(unsigned char, unsigned long)
    "08d67000",  # CSmplMemManager::writedata(unsigned char, char*, char*, unsigned long)
    "08d67030",  # CSmplMemManager::readdata(unsigned char, char*, char*, unsigned long)
    "08d67060",  # CSmplMemManager::cutdata(unsigned char, unsigned long, unsigned long)
    "08d67150",  # CSmplMemManager::insertdata(unsigned char, unsigned long, unsigned long)
    "08d67380",  # CSmplMemManager::dataclear(unsigned char, unsigned long, unsigned long)
    "08d67530",  # CSmplMemManager::copydata(unsigned char, unsigned long, unsigned char, unsigned long, unsigned long)
    "08d67610",  # CSmplMemManager::addrltv(short)
    "08d67620",  # CSmplMemManager::decrltv(short)
    "08d67630",  # CSmplMemManager::clearrltv()
    "08d67640",  # CSmplMemManager::getuserltvnum()
    "08d67650",  # CSmplMemManager::getfreerltvnum(unsigned char*)
    "08d67690",  # CSmplMemManager::getremainsize(unsigned char)
    "08d676c0",  # CSmplMemManager::getremainsize(unsigned long*)
    "08d676e0",  # CSmplMemManager::getremainsmpltimems(unsigned char, int)
    "08d67740",  # CSmplMemManager::getremainsmpltimeandsize(unsigned char, unsigned long*, unsigned char*)
    "08d67820",  # CSmplMemManager::getusedsampleno(int, int*, int&)
    "08d67960",  # CSmplMemManager::refreshhdfreesize(EDevice_Id)
    "08d67990",  # CSmplMemManager::gethdfreesize()
    "08d679c0",  # CSmplMemManager::dechdfreesize(unsigned long)

    # 2026-07-29: CChunkRootBase/CChunkRootWithSeek/CChunkRootWithSeekWithCRC +
    # CCrc32 (chunk_root_family.h/.cpp) -- the "index/seek/CRC on top of
    # chunked I/O" layer chunk_family.h's own header comment flagged as the
    # natural next batch. CChunkRootWithSeek::BuildSubChunkIndex() (08b2c60)
    # stays "pending" -- only its fast-path guards are real, the deep
    # eRead-mode body is deferred (needs CImageStr, out of scope this batch).
    "080ad6f0",  # CChunkRootBase::PreClose()
    "080ad700",  # CChunkRootBase::PostClose()
    "080ad720",  # CChunkRootBase::MediaCheck()
    "080ad830",  # CChunkRootBase::Close()
    "080ad8c0",  # CChunkRootBase::OnWriteLenAndFlags(unsigned long, unsigned long, unsigned long, unsigned char)
    "080ad980",  # CChunkRootBase::Init()
    "080ada60",  # CChunkRootBase::~CChunkRootBase()
    "080adba0",  # CChunkRootBase::~CChunkRootBase()
    "080b1740",  # CChunkRootBase::CChunkRootBase(SChkHeader const&)
    "080b1800",  # CChunkRootBase::CChunkRootBase()
    "080b1850",  # CChunkRootBase::CloseStream()
    "080b1890",  # CChunkRootBase::ResetPath()
    "080b18c0",  # CChunkRootBase::SetPath(char const*)
    "080b1970",  # CChunkRootBase::OpenStreamInWrite()
    "080b1b30",  # CChunkRootBase::OpenStreamInRead()
    "08185200",  # CChunkRootBase::GetRelSonNumber() const
    "08185220",  # CChunkRootBase::GetFather()
    "08185260",  # CChunkRootBase::SetFather(CChunkBase*)
    "081852a0",  # CChunkRootBase::SetRankNumber(unsigned int)
    "081852e0",  # CChunkRootBase::OnSetInfo(CChunkInfoItem*)
    "080b2720",  # CChunkRootWithSeek::ExcludeFromIndex(SIdVRF)
    "080b2730",  # CChunkRootWithSeek::GetNumByteAfterIndex() const
    "080b2740",  # CChunkRootWithSeek::GetNextSubChunk(CChunk*&)
    "080b27d0",  # CChunkRootWithSeek::Init()
    "080b27e0",  # CChunkRootWithSeek::PostClose()
    "080b27f0",  # CChunkRootWithSeek::~CChunkRootWithSeek()
    "080b2850",  # CChunkRootWithSeek::~CChunkRootWithSeek()
    "080b28c0",  # CChunkRootWithSeek::CChunkRootWithSeek(SChkHeader const&)
    "080b2960",  # CChunkRootWithSeek::CChunkRootWithSeek()
    "080b29f0",  # CChunkRootWithSeek::ComputeIndexDataSize(unsigned long)
    "080b2a30",  # CChunkRootWithSeek::AddSubChunk(CChunk*&, SIdVRF)
    "080b2af0",  # CChunkRootWithSeek::CopySubChunkIndex(TObjArray<unsigned long> const&)
    "080b34e0",  # CChunkRootWithSeek::GetSizeWhenRewrite()
    "080b3550",  # CChunkRootWithSeek::SeekToSubChunk(unsigned int)
    "080b3650",  # CChunkRootWithSeek::PreClose()
    "080b39a0",  # CChunkRootWithSeekWithCRC::ExcludeFromIndex(SIdVRF)
    "080b3a00",  # CChunkRootWithSeekWithCRC::PreClose()
    "080b3b00",  # CChunkRootWithSeekWithCRC::GetNumByteAfterIndex() const
    "080b3b40",  # CChunkRootWithSeekWithCRC::~CChunkRootWithSeekWithCRC()
    "080b3b60",  # CChunkRootWithSeekWithCRC::~CChunkRootWithSeekWithCRC()
    "080b3bb0",  # CChunkRootWithSeekWithCRC::CChunkRootWithSeekWithCRC(SChkHeader const&, int)
    "080b3c20",  # CChunkRootWithSeekWithCRC::CChunkRootWithSeekWithCRC()
    "080b3c50",  # CChunkRootWithSeekWithCRC::GetCRC(unsigned long&, unsigned long&, int&, CChunkCallback const*)
    "080b4280",  # CChunkRootWithSeekWithCRC::PostClose()
    "080b43c0",  # CChunkRootWithSeekWithCRC::CheckCRC(CChunkCallback const*)
    "080b4460",  # CChunkRootWithSeekWithCRC::HasCRCSubChunk() const
    "080b4890",  # CCrc32::CCrc32(unsigned long, unsigned long)
    "080b4930",  # CCrc32::PutBuffer(unsigned char*, unsigned long)

    # 2026-07-29 (solo, no subagents): CFileKscList (file_ksc_list.h/.cpp) --
    # KSC-list config-record reader/writer, sibling of CKscSampleManager.
    # 18/26 methods (ctor/dtor + the "string"/"byte" field accessors +
    # record framing) -- ReadFilePath/SaveFilePath/RefreshFilePath/
    # GetDeviceInfo/Load()/Save() deferred (see file_ksc_list.h).
    #
    # round 46 follow-up (2026-07-29, solo): ReadFilePath/SaveFilePath's
    # real length-prefixed-string protocol reconstructed -- 20/26 methods
    # now real. RefreshFilePath/GetDeviceInfo/Load()/Save() still deferred.
    "084205d0",  # CFileKscList::ReadFilePath(char*, unsigned short*)
    "08420eb0",  # CFileKscList::SaveFilePath(char const*)
    "08420350",  # CFileKscList::CFileKscList()
    "08420360",  # CFileKscList::~CFileKscList()
    "084203f0",  # CFileKscList::ReadVendorId(char*)
    "08420440",  # CFileKscList::ReadProductId(char*)
    "08420490",  # CFileKscList::ReadSerialNumber(char*)
    "084204e0",  # CFileKscList::ReadAutoLoad(char*)
    "08420530",  # CFileKscList::ReadBitDepth(char*)
    "08420580",  # CFileKscList::ReadLoadMethod(char*)
    "08420370",  # CFileKscList::ReadHeaderId()
    "08420fc0",  # CFileKscList::ReadDot()
    "08421030",  # CFileKscList::WriteDot()
    "08420c50",  # CFileKscList::SaveHeaderId()
    "08420ca0",  # CFileKscList::SaveVendorId(char const*)
    "08420cf0",  # CFileKscList::SaveProductId(char const*)
    "08420d40",  # CFileKscList::SaveSerialNumber(char const*)
    "08420d90",  # CFileKscList::SaveAutoLoad(char const*)
    "08420df0",  # CFileKscList::SaveBitDepth(char const*)
    "08420e50",  # CFileKscList::SaveLoadMethod(char const*)

    # 2026-07-29 (solo, no subagents): CDirCD (dir_cd.h/.cpp) -- Akai/ISO
    # CD-ROM directory driver. 10/~40 methods -- the smallest, self-
    # contained ones (ctor/dtor + IsMixedCD/FindVolume/ReadNextEntry/etc
    # deferred, see dir_cd.h).
    "080d7e10",  # CDirCD::GetCurrEntry()
    "080d7e20",  # CDirCD::GetRootHandle() const
    "080d7e70",  # CDirCD::GetClusterSizeInSect() const
    "080d7fe0",  # CDirCD::GetMaxDirEntrySize()
    "080dbab0",  # CDirCD::GetNumAkaiPartition() const
    "080db420",  # CDirCD::SetError(EDrvNotify)
    "080daf80",  # CDirCD::ResetBufferedEntries()
    "080db190",  # CDirCD::GetTotalSectors() const
    "080dbad0",  # CDirCD::FindPartition(unsigned long, CDirCD::SAkaiPartition const*&) const
    "080db790",  # CDirCD::GetPTRecord(unsigned int) const

    # 2026-07-29 (solo, no subagents): CSysEx*Name/CSysExSetListSlotComment
    # family (sysex_object_names.h/.cpp) -- 8 classes x 4 trivial constant
    # accessors each (GetStorageId/GetVersion/GetObjectSize/
    # GetObjectSizeForExport). Destructors deliberately NOT credited here
    # (same "D0's free(this) not reproduced" convention as
    # long_binary_file.h -- see this header's own note).
    "089ecd10",  # CSysExSetListSlotComment::GetStorageId() const
    "089ecd20",  # CSysExSetListSlotComment::GetVersion() const
    "089ecd30",  # CSysExSetListSlotComment::GetObjectSize() const
    "089ecd40",  # CSysExSetListSlotComment::GetObjectSizeForExport() const
    "089ecd50",  # CSysExSetListSlotName::GetStorageId() const
    "089ecd60",  # CSysExSetListSlotName::GetVersion() const
    "089ecd70",  # CSysExSetListSlotName::GetObjectSize() const
    "089ecd80",  # CSysExSetListSlotName::GetObjectSizeForExport() const
    "089ecd90",  # CSysExCombiName::GetStorageId() const
    "089ecda0",  # CSysExCombiName::GetVersion() const
    "089ecdb0",  # CSysExCombiName::GetObjectSize() const
    "089ecdc0",  # CSysExCombiName::GetObjectSizeForExport() const
    "089ecdd0",  # CSysExProgName::GetVersion() const
    "089ecde0",  # CSysExProgName::GetStorageId() const
    "089ecdf0",  # CSysExProgName::GetObjectSize() const
    "089ece00",  # CSysExProgName::GetObjectSizeForExport() const
    "089ece10",  # CSysExSongName::GetStorageId() const
    "089ece20",  # CSysExSongName::GetVersion() const
    "089ece30",  # CSysExSongName::GetObjectSize() const
    "089ece40",  # CSysExSongName::GetObjectSizeForExport() const
    "089ece50",  # CSysExWaveSeqName::GetStorageId() const
    "089ece60",  # CSysExWaveSeqName::GetVersion() const
    "089ece70",  # CSysExWaveSeqName::GetObjectSize() const
    "089ece80",  # CSysExWaveSeqName::GetObjectSizeForExport() const
    "089ece90",  # CSysExDrumKitName::GetStorageId() const
    "089ecea0",  # CSysExDrumKitName::GetVersion() const
    "089eceb0",  # CSysExDrumKitName::GetObjectSize() const
    "089ecec0",  # CSysExDrumKitName::GetObjectSizeForExport() const
    "089eced0",  # CSysExSetListName::GetStorageId() const
    "089ecee0",  # CSysExSetListName::GetVersion() const
    "089ecef0",  # CSysExSetListName::GetObjectSize() const
    "089ecf00",  # CSysExSetListName::GetObjectSizeForExport() const

    # 2026-07-29 (solo, no subagents): CSysExSong/CSysExDrumKit/
    # CSysExCombi/CSysExWaveSeq/CSysExSetList/CSysExSongTimbreSet family
    # (sysex_objects.h/.cpp), sibling of the CSysEx*Name family above.
    # GetNumObjectsForDigest() deliberately NOT credited for DrumKit/
    # Combi/WaveSeq/SetList (real body is a genuinely unresolved indirect
    # call, see sysex_objects.h's own header comment); Song's own trivial
    # literal version IS credited. Dtors intentionally excluded (same
    # "D0's free(this) not reproduced" convention as CLongBinaryFile).
    "089ecf10",  # CSysExSong::GetStorageId() const
    "089ecf20",  # CSysExSong::HasDigests() const
    "089ecf30",  # CSysExSong::GetVersion() const
    "089ecf40",  # CSysExSong::GetObjectSize() const
    "089ecf50",  # CSysExSong::GetObjectSizeForExport() const
    "089ecf60",  # CSysExSong::GetNumObjectsForDigest(int) const
    "089ec810",  # CSysExDrumKit::GetStorageId() const
    "089ec820",  # CSysExDrumKit::HasDigests() const
    "089ec830",  # CSysExDrumKit::GetVersion() const
    "089ec840",  # CSysExDrumKit::GetObjectSize() const
    "089ec850",  # CSysExDrumKit::GetObjectSizeForExport() const
    "089eca60",  # CSysExCombi::GetStorageId() const
    "089eca70",  # CSysExCombi::HasDigests() const
    "089eca80",  # CSysExCombi::GetVersion() const
    "089eca90",  # CSysExCombi::GetObjectSize() const
    "089ecaa0",  # CSysExCombi::GetObjectSizeForExport() const
    "089ecb80",  # CSysExWaveSeq::GetStorageId() const
    "089ecb90",  # CSysExWaveSeq::HasDigests() const
    "089ecba0",  # CSysExWaveSeq::GetVersion() const
    "089ecbb0",  # CSysExWaveSeq::GetObjectSize() const
    "089ecbc0",  # CSysExWaveSeq::GetObjectSizeForExport() const
    "089ecc10",  # CSysExSetList::GetStorageId() const
    "089ecc20",  # CSysExSetList::HasDigests() const
    "089ecc30",  # CSysExSetList::GetVersion() const
    "089ecc40",  # CSysExSetList::GetObjectSize() const
    "089ecc50",  # CSysExSetList::GetObjectSizeForExport() const
    "089ecb30",  # CSysExSongTimbreSet::GetStorageId() const
    "089ecb40",  # CSysExSongTimbreSet::GetVersion() const
    "089ecb50",  # CSysExSongTimbreSet::GetObjectSize() const
    "089ecb60",  # CSysExSongTimbreSet::GetObjectSizeForExport() const

    # 2026-07-29 (solo, no subagents): CSysExKarmaGEInfo/CSysExSongControl
    # (sysex_control_objects.h/.cpp), 2 more siblings of the CSysEx*
    # family, picked up as a round-40-flagged follow-up.
    "089ebac0",  # CSysExKarmaGEInfo::GetObjectPointer(int, int) const
    "089edcf0",  # CSysExKarmaGEInfo::GetStorageId() const
    "089edd00",  # CSysExKarmaGEInfo::GetNumBanks() const
    "089edd10",  # CSysExKarmaGEInfo::GetVersion() const
    "089edd20",  # CSysExKarmaGEInfo::GetObjectSize() const
    "089edd30",  # CSysExKarmaGEInfo::GetObjectSizeForExport() const
    "089edd40",  # CSysExKarmaGEInfo::GetSysExBankId(int) const
    "089edd50",  # CSysExKarmaGEInfo::GetNumOfObject(int) const
    "089edd60",  # CSysExKarmaGEInfo::GetTotalSizeForExport(int, int) const
    "089ebcc0",  # CSysExSongControl::GetObjectPointer(int, int) const
    "089edf00",  # CSysExSongControl::GetStorageId() const
    "089edf10",  # CSysExSongControl::GetVersion() const
    "089edf20",  # CSysExSongControl::GetObjectSize() const
    "089edf30",  # CSysExSongControl::GetObjectSizeForExport() const

    # 2026-07-29 (round 42, solo, no subagents): CSysExGlobal/CSysExKarmaGE/
    # CSysExGETemplate/CSysExRegion (sysex_objects_ge_region.h/.cpp), 4 more
    # siblings of the CSysEx* family (sysex_objects_ge_region.h).
    "089ec9a0",  # CSysExGlobal::GetStorageId() const
    "089ec9b0",  # CSysExGlobal::GetNumBanks() const
    "089ec9c0",  # CSysExGlobal::HasDigests() const
    "089ec9d0",  # CSysExGlobal::GetVersion() const
    "089ec9e0",  # CSysExGlobal::GetObjectSize() const
    "089ec9f0",  # CSysExGlobal::GetObjectSizeForExport() const
    "089eca00",  # CSysExGlobal::GetSysExBankId(int) const
    "089eca10",  # CSysExGlobal::GetNumOfObject(int) const
    "089eca20",  # CSysExGlobal::GetNumObjectsForDigest(int) const
    "089ed3c0",  # CSysExGlobal::GetObjectPointer(int, int) const
    "089ed050",  # CSysExGlobal::~CSysExGlobal() (D2)
    "089ed620",  # CSysExGlobal::~CSysExGlobal() (D0)
    "089edc00",  # CSysExKarmaGE::GetStorageId() const
    "089edc10",  # CSysExKarmaGE::GetNumBanks() const
    "089edc20",  # CSysExKarmaGE::HasDigests() const
    "089edc30",  # CSysExKarmaGE::GetVersion() const
    "089edc40",  # CSysExKarmaGE::GetObjectSize() const
    "089edc50",  # CSysExKarmaGE::GetObjectSizeForExport() const
    "089edc60",  # CSysExKarmaGE::GetSysExBankId(int) const
    "089edc70",  # CSysExKarmaGE::GetNumOfObject(int) const
    "089ebaa0",  # CSysExKarmaGE::GetObjectPointer(int, int) const
    "089edca0",  # CSysExKarmaGE::~CSysExKarmaGE() (D2)
    "089edcb0",  # CSysExKarmaGE::~CSysExKarmaGE() (D0)
    "089eddc0",  # CSysExGETemplate::GetStorageId() const
    "089eddd0",  # CSysExGETemplate::GetNumBanks() const
    "089edde0",  # CSysExGETemplate::HasDigests() const
    "089eddf0",  # CSysExGETemplate::GetVersion() const
    "089ede00",  # CSysExGETemplate::GetObjectSize() const
    "089ede10",  # CSysExGETemplate::GetObjectSizeForExport() const
    "089ede20",  # CSysExGETemplate::GetSysExBankId(int) const
    "089ede30",  # CSysExGETemplate::GetNumOfObject(int) const
    "089ebc50",  # CSysExGETemplate::GetObjectPointer(int, int) const
    "089ede60",  # CSysExGETemplate::~CSysExGETemplate() (D2)
    "089ede70",  # CSysExGETemplate::~CSysExGETemplate() (D0)
    "089ee0e0",  # CSysExRegion::GetStorageId() const
    "089ee0f0",  # CSysExRegion::GetNumBanks() const
    "089ee100",  # CSysExRegion::HasDigests() const
    "089ee110",  # CSysExRegion::GetVersion() const
    "089ee120",  # CSysExRegion::GetObjectSize() const
    "089ee130",  # CSysExRegion::GetObjectSizeForExport() const
    "089ee140",  # CSysExRegion::GetSysExBankId(int) const
    "089ee150",  # CSysExRegion::GetNumOfObject(int) const
    "089ebe00",  # CSysExRegion::GetObjectPointer(int, int) const
    "089ebe20",  # CSysExRegion::GetTotalSizeForExport(int, int) const
    "089ee180",  # CSysExRegion::~CSysExRegion() (D2)
    "089ee190",  # CSysExRegion::~CSysExRegion() (D0)

    # 2026-07-29 (round 43, solo, no subagents): CMemoryAccessor's full
    # 12-method Read/Write{Big,Little}{16,24,32}Bit family
    # (storage_converter_ext_stubs.h). 3 of these (WriteBig32Bit/
    # ReadLittle16Bit/WriteLittle16Bit) were ALREADY real+committed
    # since round 46 but never added here -- a genuine manifest-crediting
    # gap, fixed alongside crediting this round's 9 new siblings.
    "0838dbc0",  # CMemoryAccessor::ReadBig32Bit(unsigned char const*)
    "0838dbf0",  # CMemoryAccessor::WriteBig32Bit(unsigned char*, unsigned long) [gap fix, round 46]
    "0838dc20",  # CMemoryAccessor::ReadBig24Bit(unsigned char const*)
    "0838dc40",  # CMemoryAccessor::WriteBig24Bit(unsigned char*, unsigned long)
    "0838dc60",  # CMemoryAccessor::ReadBig16Bit(unsigned char const*)
    "0838dc80",  # CMemoryAccessor::WriteBig16Bit(unsigned char*, unsigned short)
    "0838dca0",  # CMemoryAccessor::ReadLittle32Bit(unsigned char const*)
    "0838dcd0",  # CMemoryAccessor::WriteLittle32Bit(unsigned char*, unsigned long)
    "0838dd00",  # CMemoryAccessor::ReadLittle24Bit(unsigned char const*)
    "0838dd20",  # CMemoryAccessor::WriteLittle24Bit(unsigned char*, unsigned long)
    "0838dd40",  # CMemoryAccessor::ReadLittle16Bit(unsigned char const*) [gap fix, round 46]
    "0838dd60",  # CMemoryAccessor::WriteLittle16Bit(unsigned char*, unsigned short) [gap fix, round 46]

    # 2026-07-29 (round 44, solo, no subagents): CESDiskCommandTask's 84
    # "command trampoline" methods, all sharing one exact 44-byte shape
    # (es_disk_command_task.h/stg_disk_command_task.cpp). The other 11
    # CESDiskCommandTask methods (ctors/dtors/ExecuteXxxCommand/Exec) remain
    # deferred -- see header comment for why.
    "08ddddc0",  # CESDiskCommandTask::LoadMultiFile(unsigned char)
    "08ddddf0",  # CESDiskCommandTask::Blank(unsigned char)
    "08ddde20",  # CESDiskCommandTask::Finalize(unsigned char)
    "08ddde50",  # CESDiskCommandTask::BurnAudio(unsigned char)
    "08ddde80",  # CESDiskCommandTask::StartMIDIReceiver(unsigned char)
    "08dddeb0",  # CESDiskCommandTask::StartSetDate(unsigned char)
    "08dddee0",  # CESDiskCommandTask::FileUnprotect(unsigned char)
    "08dddf10",  # CESDiskCommandTask::FileProtect(unsigned char)
    "08dddf40",  # CESDiskCommandTask::OptimizeMedium(unsigned char)
    "08dddf70",  # CESDiskCommandTask::CheckMedium(unsigned char)
    "08dddfa0",  # CESDiskCommandTask::RateConvert(unsigned char)
    "08dddfd0",  # CESDiskCommandTask::ConvertToIso(unsigned char)
    "08dde000",  # CESDiskCommandTask::Format(unsigned char)
    "08dde030",  # CESDiskCommandTask::CreateDir(unsigned char)
    "08dde060",  # CESDiskCommandTask::DeleteUnusedWav(unsigned char)
    "08dde090",  # CESDiskCommandTask::Delete(unsigned char)
    "08dde0c0",  # CESDiskCommandTask::Copy(unsigned char)
    "08dde0f0",  # CESDiskCommandTask::Rename(unsigned char)
    "08dde120",  # CESDiskCommandTask::SaveKfx(unsigned char)
    "08dde150",  # CESDiskCommandTask::Save1Song(unsigned char)
    "08dde180",  # CESDiskCommandTask::SaveKcd(unsigned char)
    "08dde1b0",  # CESDiskCommandTask::SaveAifWav(unsigned char)
    "08dde1e0",  # CESDiskCommandTask::SaveExclusive(unsigned char)
    "08dde210",  # CESDiskCommandTask::SaveSMF(unsigned char)
    "08dde240",  # CESDiskCommandTask::SaveKge(unsigned char)
    "08dde270",  # CESDiskCommandTask::SaveSample(unsigned char)
    "08dde2a0",  # CESDiskCommandTask::SaveSeq(unsigned char)
    "08dde2d0",  # CESDiskCommandTask::SavePcg(unsigned char)
    "08dde300",  # CESDiskCommandTask::SavePcgSeq(unsigned char)
    "08dde330",  # CESDiskCommandTask::SaveAll(unsigned char)
    "08dde360",  # CESDiskCommandTask::LoadKscItem(unsigned char)
    "08dde390",  # CESDiskCommandTask::LoadKontaktSample(unsigned char)
    "08dde3c0",  # CESDiskCommandTask::LoadKontaktInstrument(unsigned char)
    "08dde3f0",  # CESDiskCommandTask::LoadKontaktMulti(unsigned char)
    "08dde420",  # CESDiskCommandTask::LoadKontaktBank(unsigned char)
    "08dde450",  # CESDiskCommandTask::LoadSF2(unsigned char)
    "08dde480",  # CESDiskCommandTask::Load1Fx(unsigned char)
    "08dde4b0",  # CESDiskCommandTask::LoadFxBank(unsigned char)
    "08dde4e0",  # CESDiskCommandTask::LoadFxs(unsigned char)
    "08dde510",  # CESDiskCommandTask::LoadKfx(unsigned char)
    "08dde540",  # CESDiskCommandTask::LoadKcd(unsigned char)
    "08dde570",  # CESDiskCommandTask::LoadAkaiVolume(unsigned char)
    "08dde5a0",  # CESDiskCommandTask::LoadAkaiProg(unsigned char)
    "08dde5d0",  # CESDiskCommandTask::LoadAkaiSample(unsigned char)
    "08dde600",  # CESDiskCommandTask::LoadWav(unsigned char)
    "08dde630",  # CESDiskCommandTask::LoadAif(unsigned char)
    "08dde660",  # CESDiskCommandTask::LoadKsf(unsigned char)
    "08dde690",  # CESDiskCommandTask::LoadKmp(unsigned char)
    "08dde6c0",  # CESDiskCommandTask::LoadExclusive(unsigned char)
    "08dde6f0",  # CESDiskCommandTask::LoadSMF(unsigned char)
    "08dde720",  # CESDiskCommandTask::Load1Pattern(unsigned char)
    "08dde750",  # CESDiskCommandTask::LoadTracks(unsigned char)
    "08dde780",  # CESDiskCommandTask::Load1Song(unsigned char)
    "08dde7b0",  # CESDiskCommandTask::Load1Region(unsigned char)
    "08dde7e0",  # CESDiskCommandTask::LoadRegionBank(unsigned char)
    "08dde810",  # CESDiskCommandTask::LoadCueLists(unsigned char)
    "08dde840",  # CESDiskCommandTask::LoadTemplateBank(unsigned char)
    "08dde870",  # CESDiskCommandTask::LoadTemplates(unsigned char)
    "08dde8a0",  # CESDiskCommandTask::Load1GE(unsigned char)
    "08dde8d0",  # CESDiskCommandTask::LoadGEBank(unsigned char)
    "08dde900",  # CESDiskCommandTask::LoadGEs(unsigned char)
    "08dde930",  # CESDiskCommandTask::Load1SetListSlot(unsigned char)
    "08dde960",  # CESDiskCommandTask::Load1SetList(unsigned char)
    "08dde990",  # CESDiskCommandTask::LoadSetLists(unsigned char)
    "08dde9c0",  # CESDiskCommandTask::LoadGlobal(unsigned char)
    "08dde9f0",  # CESDiskCommandTask::Load1DrumTrackPattern(unsigned char)
    "08ddea20",  # CESDiskCommandTask::LoadDrumTrackPatterns(unsigned char)
    "08ddea50",  # CESDiskCommandTask::Load1Wseq(unsigned char)
    "08ddea80",  # CESDiskCommandTask::LoadWseqBank(unsigned char)
    "08ddeab0",  # CESDiskCommandTask::LoadWaveSeqs(unsigned char)
    "08ddeae0",  # CESDiskCommandTask::Load1Dkit(unsigned char)
    "08ddeb10",  # CESDiskCommandTask::LoadDkitBank(unsigned char)
    "08ddeb40",  # CESDiskCommandTask::LoadDkits(unsigned char)
    "08ddeb70",  # CESDiskCommandTask::Load1Combi(unsigned char)
    "08ddeba0",  # CESDiskCommandTask::LoadCombiBank(unsigned char)
    "08ddebd0",  # CESDiskCommandTask::LoadCombis(unsigned char)
    "08ddec00",  # CESDiskCommandTask::Load1Prog(unsigned char)
    "08ddec30",  # CESDiskCommandTask::LoadSyx(unsigned char)
    "08ddec60",  # CESDiskCommandTask::LoadProgBank(unsigned char)
    "08ddec90",  # CESDiskCommandTask::LoadPrograms(unsigned char)
    "08ddecc0",  # CESDiskCommandTask::LoadPcgRamSmpl(unsigned char)
    "08ddecf0",  # CESDiskCommandTask::LoadAll(unsigned char)
    "08de01a0",  # CESDiskCommandTask::LoadMossBank(unsigned char)
    "08de01d0",  # CESDiskCommandTask::Load1MossProg(unsigned char)

    # 2026-07-29 (round 45, solo, no subagents): CESDiskTask, 39/143
    # tractable methods (es_disk_task.h/.cpp). 3 shapes: plain static-
    # global accessors, "this-offset" accessors (GET side only), and
    # single-branch index-gated dialog accessors. The other 104 methods
    # remain deferred -- see es_disk_task.cpp's own header comment.
    "08ddc8b0",  # CESDiskTask::GetFilerMsg(unsigned char, unsigned char*)
    "08ddc8d0",  # CESDiskTask::SetFilerMsg(unsigned char, unsigned char const*)
    "08ddc8f0",  # CESDiskTask::GetResultWriteExcl(unsigned char, unsigned char*)
    "08ddc910",  # CESDiskTask::GetProgress(unsigned char, unsigned char*)
    "08ddc930",  # CESDiskTask::SetProgress(unsigned char, unsigned char const*)
    "08ddc950",  # CESDiskTask::GetMultipleSelect(unsigned char, unsigned char*)
    "08ddc970",  # CESDiskTask::SetMultipleSelect(unsigned char, unsigned char const*)
    "08ddca60",  # CESDiskTask::GetNotifyFileSelected(unsigned char, unsigned char*)
    "08ddca80",  # CESDiskTask::SetNotifyFileSelected(unsigned char, unsigned char const*)
    "08ddcc10",  # CESDiskTask::SetWriteExcl(unsigned char, unsigned char const*)
    "08ddd800",  # CESDiskTask::GetDefaultFileName()
    "08ddc990",  # CESDiskTask::GetBankProgToWrite(unsigned char, char*) const
    "08ddc9c0",  # CESDiskTask::GetBankProgToWriteFullRange(unsigned char, unsigned char*) const
    "08ddca30",  # CESDiskTask::GetBankCombiToWrite(unsigned char, char*) const
    "08ddc9f0",  # CESDiskTask::GetOscTypeToWrite(unsigned char, unsigned char*) const
    "08ddca10",  # CESDiskTask::SetOscTypeToWrite(unsigned char, unsigned char const*)
    "08de3880",  # CESDiskTask::GetLdCombiBankDialog(unsigned char, unsigned char*)
    "08de38a0",  # CESDiskTask::SetLdCombiBankDialog(unsigned char, unsigned char const*)
    "08de38c0",  # CESDiskTask::GetLdDkitBankDialog(unsigned char, unsigned char*)
    "08de38e0",  # CESDiskTask::SetLdDkitBankDialog(unsigned char, unsigned char const*)
    "08de3900",  # CESDiskTask::GetLdKarmaGEBankDialog(unsigned char, unsigned char*)
    "08de3920",  # CESDiskTask::SetLdKarmaGEBankDialog(unsigned char, unsigned char const*)
    "08de3940",  # CESDiskTask::GetLdTemplateBankDialog(unsigned char, unsigned char*)
    "08de3960",  # CESDiskTask::SetLdTemplateBankDialog(unsigned char, unsigned char const*)
    "08de3980",  # CESDiskTask::GetLdProgBankDialog(unsigned char, unsigned char*)
    "08de39a0",  # CESDiskTask::SetLdProgBankDialog(unsigned char, unsigned char const*)
    "08de39c0",  # CESDiskTask::GetLdWseqBankDialog(unsigned char, unsigned char*)
    "08de39e0",  # CESDiskTask::SetLdWseqBankDialog(unsigned char, unsigned char const*)
    "08de4040",  # CESDiskTask::SetLoadSampleDialog(unsigned char, unsigned char const*)
    "08de4080",  # CESDiskTask::GetLoadSampleDialog(unsigned char, unsigned char*)
    "08de4260",  # CESDiskTask::GetLoadRegionsDialog(unsigned char, unsigned char*)
    "08de4280",  # CESDiskTask::SetLoadRegionsDialog(unsigned char, unsigned char const*)
    "08de4a40",  # CESDiskTask::GetEraseCDRWDialog(unsigned char, unsigned char*)
    "08de4a60",  # CESDiskTask::SetEraseCDRWDialog(unsigned char, unsigned char const*)
    "08de72b0",  # CESDiskTask::SetDeleteDialog(unsigned char, unsigned char const*)
    "08de72f0",  # CESDiskTask::GetDeleteDialog(unsigned char, unsigned char*)
    "08de7530",  # CESDiskTask::SetNewDirDialog(unsigned char, unsigned char const*)
    "08de7570",  # CESDiskTask::GetNewDirDialog(unsigned char, unsigned char*)
    "08ddd810",  # CESDiskTask::CopyBytes(unsigned char*, unsigned char const*, int)
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
