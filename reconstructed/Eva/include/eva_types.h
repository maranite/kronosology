/*
 * eva_types.h  -  shared low-level types for the Eva boot-path reconstruction (Stage 0/1).
 *
 * Eva is a normal dynamically-linked x86-32 userspace ELF (GCC 4.5.0 era), not a
 * freestanding kernel module -- see PLAN.md's "What's different from OA.ko" section
 * before assuming any OA.ko convention carries over unchanged.
 *
 * Only the minimal faithful shapes needed to compile the Stage-1 boot-path translation
 * units are declared here; classes get filled in incrementally as later stages need more
 * of their real layout. Offsets/sizes recovered from Eva's own (unstripped) symbol table
 * and Ghidra decompile export (/home/share/Decomp/EVA_Decomp/eva_export/).
 */

#ifndef EVA_TYPES_H
#define EVA_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* CSTGHandle -- shared-memory handle wrapper (Stage 2, not yet reconstructed).
 * USTGUserAPI::Connect() constructs one on the stack (mode/id = 1) and calls
 * Access() on it to attach; the real Access() (_ZNK10CSTGHandle6AccessEv, a
 * genuine const member taking no arguments besides `this`, `.text+0x08e31e80`)
 * is declared here only as a call-contract stub, not implemented -- Eva's
 * Connect() path double-calls it (once on the stack handle, once on the
 * resulting pointer) in a way not yet fully decoded; faithfully preserved
 * rather than guessed at. Declared as a real member (not a free function) so
 * the mangled name matches the original when this is eventually implemented.
 */
struct CSTGHandle {
	uint32_t mode; /* local_10[0] = 1 in USTGUserAPI::Connect -- exact field name/meaning TBD */

	void *Access() const;
};

#endif /* EVA_TYPES_H */
