# Memory Index

- [CKG seq-backup decoder technique](ckg_seq_backup_technique.md) — scripted instruction-pattern decoder for dense tiny-accessor clusters; reuse for similar Set*/Get* runs
- [CKGBankManager/CSPREngine/CKGSeqBackup* facts](ckg_bankmanager_class_facts.md) — struct layout + open GetValue() case-mapping gap, current as of commit efa0926; case-order != Set* declaration order CONFIRMED not simple, see stg_value_getter_family.md follow-up note
- [STG value-getter family (~2300 methods, ~180 classes)](stg_value_getter_family.md) — huge codebase-wide Get*(ctx)->STGConvertedParam& pattern; 17 classes done (818 methods), bare-SIB-scale-no-lea ctx-index + unsigned-movzx-byte + new DEF_RE "word(paren-prose)" trigger, ~151 classes pending
- [Shared-repo commit hygiene](shared_repo_commit_hygiene.md) — always `git diff --cached --stat` immediately before committing; concurrent sessions on /home/share can stage files between your `add` and `commit`
