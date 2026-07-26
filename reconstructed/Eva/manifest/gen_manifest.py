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
    "08160db0",  # MMainResMan (wrapper faithful; CResMan::CResMan itself is Tier-B)

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
}

# CBDApiInstance re-checked (Stage 6 breadth sweep, 2026-07-25) -- its 6 methods
# (IsPreloadRunning x2/IsBusy/RegisterLoader/dtor x2, .text+0x08243830..0x082455e0)
# stay NOT in RECONSTRUCTED. RegisterLoader(CBatchDiskMan*) (08243980) -- the one
# method with a plausible real caller -- was checked directly: zero call sites found
# anywhere in the 37,795-function export (confirmed by grepping every decompile for
# the mangled symbol, not just BDApiInstance's own file), and CBatchDiskMan's own
# constructor is itself never invoked on this project's traced boot path either
# (same "registered as a module descriptor, never actually instantiated" status as
# 13 of Mains()'s other 15 registration-shim modules -- see mains.cpp's own Stage 3
# note). A confirmed, thorough negative result, not a gap in this search.

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
# NEWLY FOUND this batch: CPoller's own real, definitive ground-truth caller is
# `CPanel::Setup()` (.text+0x089ee6e0) -- `CPanel` itself is a real, not-yet-
# reconstructed per-module class mains.cpp's own `PTR__CPanelConstructor_08f7c2f0`
# currently routes through the shared `ModuleFactoryCreateStub` (returns NULL) for
# exactly the reason `CEditor` once did before `CEditorConstructorCreate()` was
# added -- reconstructing `CPanel` would be the natural next step to make CPoller
# live on this reconstruction's own wired boot path. 9 of CPoller's 29 methods (ctor,
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
