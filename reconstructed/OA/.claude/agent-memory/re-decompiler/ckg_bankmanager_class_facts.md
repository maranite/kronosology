---
name: ckg-bankmanager-class-facts
description: Struct layout and open gaps for CKGSeqBackupCommonParam/ModuleParam, CKGBankManager, CSPREngine (Karma sequencer backup cluster, OA.ko)
type: project
---

Reconstructed 2026-07-28 (commit `efa0926`, OA.ko manifest 1238->1441).
Facts below are current as of that commit -- verify still-current before
reusing (grep for the symbols/files named).

**CKGSeqBackupCommonParam / CKGSeqBackupModuleParam**
(`include/oa_karma_seq_backup.h`, `src/engine/karma_seq_backup.cpp`):
identical 4-field layout, `+0x0 m_source`/`+0x4 m_default` (void*, live vs.
default KarmaPerf record pointers), `+0x8 m_index` (int sub-index), `+0xc
m_value` (long, the computed backup output). All Set* methods are pure
"read one field via m_source or m_default (sometimes index-scaled) into
m_value" -- no side effects on m_source/m_default/m_index themselves.
Both classes' real ctors are genuinely empty (`ret`, no member init) --
a real caller must populate m_source/m_default/m_index via GetValue()
before using any Set* accessor; this project's reconstruction preserves
that (does not add defensive init).

**Open gap, not yet reconstructed**: `GetValue(int paramIndex, int
subIndex, long *out)` on both classes -- a real, fully self-contained
69-case (CommonParam) / 128-case (ModuleParam) jump-table dispatcher whose
case bodies DUPLICATE (not call) the same field logic as the
correspondingly-named Set* method. To finish this: need to confirm the
exact case-index -> Set*-method mapping (do NOT assume case order ==
declaration/address order without checking -- a wrong mapping is a silent
behavioral bug). See `HARDWARE_REVIEW_LOG.md`'s
"CKGSeqBackupCommonParam / CKGSeqBackupModuleParam" entry for the full
derivation notes (including where the jump table's `.rodata` base sits:
CommonParam's is `.text+0x3d12b6` relocation `R_386_32 .rodata`, byte
offset `0xacb94` into `.rodata`, entry per paramIndex 0..0x44).

**CKGBankManager** (`include/oa_engine_init.h`): was already a bare
opaque stand-in (just `ms_poInstance`) before this batch, from an earlier
`CTimerManager::ShouldSyncExternalClock()` discovery
(`src/engine/sk_stg_gate.cpp`). This batch ADDED 3 more real method
declarations (`GetSeqKarmaPerfCommon(unsigned int)`,
`GetSeqKarmaPerfModule(unsigned int)`, `GetSeqDefaultKarmaPerfCommon()`)
-- declared only, bodies still genuinely out of scope/unresolved (real
"Unknown symbol" at `make ko`, expected). `GetSeqDefaultKarmaPerfCommon`
is declared but not yet CALLED by anything reconstructed (it's only used
inside the still-deferred `GetValue()` bodies) -- don't be surprised it
doesn't show up as `U` in `nm OA.ko` yet.

**CSPREngine** (`include/oa_engine_init.h`): brand-new class this batch,
NOT previously declared anywhere in this project. Minimal opaque stand-in
(`static unsigned char *ms_poInstance`, storage defined in
`karma_seq_backup.cpp`). Byte `+0xa` is a gate flag (0 = feature/backup
path disabled, tested by both GetKarmaPerf*ForSeqBackup() helpers before
touching CKGBankManager at all) -- name "CSPREngine" suggests
Sequencer/Performance-Record engine but this is a guess from context, not
confirmed.

See [[ckg_seq_backup_technique]] for the decoder methodology that
produced this cluster.
