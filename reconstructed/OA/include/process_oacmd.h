// SPDX-License-Identifier: GPL-2.0
/*
 * process_oacmd.h  -  /proc/.oacmd command dispatcher.
 *
 * Ground-truthed by direct disassembly of the real OA.ko 3.2.1 binary
 * (.text+0xa0c0, 1773 bytes) and its .rel.text relocations -- see
 * MASTER_REFERENCE.md for the full derivation.
 *
 * `/proc/.oacmd` (hidden procfs entry, created by OA.ko) is the single
 * userspace<->kernel command channel every tool (Eva, InstallEXs) uses.
 * Pattern: userspace writes a command string, then reads back an integer
 * result. oa_cmd_write (.text+0x9f20) forwards the write payload (max 127
 * bytes) into ProcessOACmd(); oa_cmd_open/read/close
 * (.text+0x9e60/0x9eb0/0x9e80) manage the rest of the state machine --
 * none of the four proc fops handlers themselves are reconstructed in this
 * pass, only the dispatcher they call into.
 *
 * Command grammar: a 2-character prefix, a literal ':', then a
 * command-specific payload. Confirmed real prefixes (string bytes read
 * directly from the binary's rodata, not guessed): "LM", "LD", "CM", "CD",
 * "AU", "CL". docs/interfaces/proc_oacmd.md additionally claims "SO", "PT",
 * "PC", "KI", "LA", "PR" exist further into the function; this pass
 * confirmed via relocation that CSTGInstalledEXProducts::ReInitialize,
 * CSTGPianoModel::RescanPianoTypes, and CSTGPCMPrecacheManager::Reset/
 * AfterProcess are all real call targets somewhere in ProcessOACmd, but did
 * NOT disassemble their exact prefix strings/argument grammar -- left as an
 * open TODO (see process_oacmd.cpp) rather than guessed.
 *
 * LM/LD/CM/CD share a parsing preamble (confirmed): "<uuid>:<flags>:<n2>:<n3>"
 * -- a 36-character dashed-hex UUID (CUUID::ConvertFromText), then
 * ":%lu:%lu:%lu" via sscanf. AU takes a single 24-character auth string with
 * no further parsing (see VerifyAndSaveAuthString, products.cpp). The
 * LM-vs-LD-vs-CM-vs-CD post-parse dispatch itself is reconstructed as a
 * plain switch on the matched prefix here, not as the compiler's branchless
 * seta/setb comparison trick seen in the disassembly -- same routing,
 * different (clearer) generated code, consistent with this project's
 * existing convention (see klm_manager.cpp's AuthorizeBuiltins comment).
 */

#ifndef OA_PROCESS_OACMD_H
#define OA_PROCESS_OACMD_H

/*
 * Dispatch one command string written to /proc/.oacmd. Writes the integer
 * result of the operation to *outResult. Returns 0 if the command's prefix
 * was recognized (regardless of whether the operation itself succeeded --
 * see *outResult for that), -1 for an unrecognized or malformed command
 * (the real firmware also logs `printk("bad oa cmd %s\n", cmd)` in this
 * case, not reproduced here).
 */
int ProcessOACmd(const char *cmd, int *outResult);

#endif /* OA_PROCESS_OACMD_H */
