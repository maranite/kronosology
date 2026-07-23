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
}

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
