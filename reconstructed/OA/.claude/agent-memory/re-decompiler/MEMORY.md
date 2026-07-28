# Memory Index

- [CKG seq-backup decoder technique](ckg_seq_backup_technique.md) — scripted instruction-pattern decoder for dense tiny-accessor clusters; reuse for similar Set*/Get* runs
- [CKGBankManager/CSPREngine/CKGSeqBackup* facts](ckg_bankmanager_class_facts.md) — struct layout + open GetValue() case-mapping gap, current as of commit efa0926; case-order != Set* declaration order CONFIRMED not simple, see stg_value_getter_family.md follow-up note
- [STG value-getter family (~2300 methods, ~180 classes)](stg_value_getter_family.md) — huge codebase-wide Get*(ctx)->STGConvertedParam& pattern; 6 classes done (504 methods), gen_oa_manifest.py DEF_RE paren + literal-*/-in-prose gotchas documented, ~163 classes pending
