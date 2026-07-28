# Memory Index

- [x86 magic-divide interpreter technique](x86_magic_divide_interpreter_technique.md) — dynamic/execution-based decoder (host-side x86-32 interpreter as ground-truth oracle) for dense branchy magic-divide functions; built for Eva's CWaveformTemplate, complements the static decoder below
- [CKG seq-backup decoder technique](ckg_seq_backup_technique.md) — scripted instruction-pattern decoder for dense tiny-accessor clusters; reuse for similar Set*/Get* runs
- [CKGBankManager/CSPREngine/CKGSeqBackup* facts](ckg_bankmanager_class_facts.md) — struct layout + open GetValue() case-mapping gap, current as of commit efa0926; case-order != Set* declaration order CONFIRMED not simple, see stg_value_getter_family.md follow-up note
- [STG value-getter family (~2300 methods, ~180 classes)](stg_value_getter_family.md) — huge codebase-wide Get*(ctx)->STGConvertedParam& pattern; 29 classes done (939 methods), manifest 2384/21689, CSTGEGBase's 14 non-family methods confirmed a different mechanism, ~139 classes pending
- [Shared-repo commit hygiene](shared_repo_commit_hygiene.md) — always `git diff --cached --stat` immediately before committing; concurrent sessions on /home/share can stage files between your `add` and `commit`
