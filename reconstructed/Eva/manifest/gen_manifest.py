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
}

# CTask::RegisterIfc (0807ec90, 472 bytes) stays NOT in RECONSTRUCTED -- Tier B link-
# stub only (real signature, empty body): genuinely deep dedup-scan +
# TVector<SRegisteredIfc,1>::MakeCapacity()-driven append, a TVector<T,1> growth
# routine this project has never generalized anywhere it appears (ckernel.h's own
# note). CPoller (29 methods, .text+0x089ef740 ctor) surveyed but NOT pursued --
# genuinely deeper (~1900-byte ctor, pulls in a new not-yet-reconstructed
# CTask::SetMask(EMask) dependency) than any Tier-A candidate this batch, correctly
# out of scope. CEditor::CPanelIfcTask's own ctor (.text+0x0824b7e0) also stays out of
# scope -- its post-CTask::CTask() tail is real multiple-inheritance work
# (COutLinkMono sub-object + adjustment thunk) that is Peg/UI-editor-toolkit depth,
# not CModule/CTask/CLevelManagerArray/CPoller family depth; see task.h/
# panel_ifc_task.h. CScheduler::InsertTask(CTask const&) (.text+0x08062d80) -- the
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
