/*
 * storage_converter_base.cpp  -  see include/storage_converter_base.h for the
 * full derivation (scripted objdump -dr instruction-pattern decode + a direct
 * .rodata vtable dump cross-check, zero anomalies across all 256 methods).
 *
 * Every body below is one of exactly three shapes: the one real memcpy
 * (Ext0000toInt0000), a same-target-Y tail-call thunk, or an empty no-op --
 * see the header comment for the full X/Y rule and how it was verified.
 */

#include "storage_converter_base.h"

#include <cstring>

// ---- target Int0000 ----

/* .text+0x08dea8f0, 37B. Real: unconditional memcpy(dst, src, size) using
 * param's own dst/src/size fields -- Int0000 is byte-identical to Ext0000,
 * so this is the one genuinely non-trivial body in the whole 16x16 matrix.
 */
void CStorageConverterBase::Ext0000toInt0000(const CConvertStorageParam &param) const
{
	std::memcpy(param.m_internalBuf, param.m_externalBuf, param.m_size);
}

/* .text+0x08de9180, 19B. Real: tail-call (jmp, not call+ret) to Ext0000toInt0000
 * -- ground truth's vtable slot for "produce Int0000" is looked up via
 * this->vtbl[0x58] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0001toInt0000(const CConvertStorageParam &param) const
{
	Ext0000toInt0000(param);
}

/* .text+0x08de91a0, 19B. Real: tail-call (jmp, not call+ret) to Ext0000toInt0000
 * -- ground truth's vtable slot for "produce Int0000" is looked up via
 * this->vtbl[0x58] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0002toInt0000(const CConvertStorageParam &param) const
{
	Ext0000toInt0000(param);
}

/* .text+0x08de91c0, 19B. Real: tail-call (jmp, not call+ret) to Ext0000toInt0000
 * -- ground truth's vtable slot for "produce Int0000" is looked up via
 * this->vtbl[0x58] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0003toInt0000(const CConvertStorageParam &param) const
{
	Ext0000toInt0000(param);
}

/* .text+0x08de91e0, 19B. Real: tail-call (jmp, not call+ret) to Ext0000toInt0000
 * -- ground truth's vtable slot for "produce Int0000" is looked up via
 * this->vtbl[0x58] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0004toInt0000(const CConvertStorageParam &param) const
{
	Ext0000toInt0000(param);
}

/* .text+0x08de9200, 19B. Real: tail-call (jmp, not call+ret) to Ext0000toInt0000
 * -- ground truth's vtable slot for "produce Int0000" is looked up via
 * this->vtbl[0x58] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0005toInt0000(const CConvertStorageParam &param) const
{
	Ext0000toInt0000(param);
}

/* .text+0x08de9220, 19B. Real: tail-call (jmp, not call+ret) to Ext0000toInt0000
 * -- ground truth's vtable slot for "produce Int0000" is looked up via
 * this->vtbl[0x58] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0006toInt0000(const CConvertStorageParam &param) const
{
	Ext0000toInt0000(param);
}

/* .text+0x08de9240, 19B. Real: tail-call (jmp, not call+ret) to Ext0000toInt0000
 * -- ground truth's vtable slot for "produce Int0000" is looked up via
 * this->vtbl[0x58] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0007toInt0000(const CConvertStorageParam &param) const
{
	Ext0000toInt0000(param);
}

/* .text+0x08de9260, 19B. Real: tail-call (jmp, not call+ret) to Ext0000toInt0000
 * -- ground truth's vtable slot for "produce Int0000" is looked up via
 * this->vtbl[0x58] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0008toInt0000(const CConvertStorageParam &param) const
{
	Ext0000toInt0000(param);
}

/* .text+0x08de9280, 19B. Real: tail-call (jmp, not call+ret) to Ext0000toInt0000
 * -- ground truth's vtable slot for "produce Int0000" is looked up via
 * this->vtbl[0x58] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0009toInt0000(const CConvertStorageParam &param) const
{
	Ext0000toInt0000(param);
}

/* .text+0x08de92a0, 19B. Real: tail-call (jmp, not call+ret) to Ext0000toInt0000
 * -- ground truth's vtable slot for "produce Int0000" is looked up via
 * this->vtbl[0x58] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000AtoInt0000(const CConvertStorageParam &param) const
{
	Ext0000toInt0000(param);
}

/* .text+0x08de92c0, 19B. Real: tail-call (jmp, not call+ret) to Ext0000toInt0000
 * -- ground truth's vtable slot for "produce Int0000" is looked up via
 * this->vtbl[0x58] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000BtoInt0000(const CConvertStorageParam &param) const
{
	Ext0000toInt0000(param);
}

/* .text+0x08de92e0, 19B. Real: tail-call (jmp, not call+ret) to Ext0000toInt0000
 * -- ground truth's vtable slot for "produce Int0000" is looked up via
 * this->vtbl[0x58] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000CtoInt0000(const CConvertStorageParam &param) const
{
	Ext0000toInt0000(param);
}

/* .text+0x08de9300, 19B. Real: tail-call (jmp, not call+ret) to Ext0000toInt0000
 * -- ground truth's vtable slot for "produce Int0000" is looked up via
 * this->vtbl[0x58] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000DtoInt0000(const CConvertStorageParam &param) const
{
	Ext0000toInt0000(param);
}

/* .text+0x08de9320, 19B. Real: tail-call (jmp, not call+ret) to Ext0000toInt0000
 * -- ground truth's vtable slot for "produce Int0000" is looked up via
 * this->vtbl[0x58] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000EtoInt0000(const CConvertStorageParam &param) const
{
	Ext0000toInt0000(param);
}

/* .text+0x08de9340, 19B. Real: tail-call (jmp, not call+ret) to Ext0000toInt0000
 * -- ground truth's vtable slot for "produce Int0000" is looked up via
 * this->vtbl[0x58] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000FtoInt0000(const CConvertStorageParam &param) const
{
	Ext0000toInt0000(param);
}

// ---- target Int0001 ----

/* .text+0x08de9360, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0000toInt0001(const CConvertStorageParam &) const
{
}

/* .text+0x08de9370, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0001toInt0001(const CConvertStorageParam &) const
{
}

/* .text+0x08de9380, 22B. Real: tail-call (jmp, not call+ret) to Ext0001toInt0001
 * -- ground truth's vtable slot for "produce Int0001" is looked up via
 * this->vtbl[0x9c] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0002toInt0001(const CConvertStorageParam &param) const
{
	Ext0001toInt0001(param);
}

/* .text+0x08de93a0, 22B. Real: tail-call (jmp, not call+ret) to Ext0001toInt0001
 * -- ground truth's vtable slot for "produce Int0001" is looked up via
 * this->vtbl[0x9c] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0003toInt0001(const CConvertStorageParam &param) const
{
	Ext0001toInt0001(param);
}

/* .text+0x08de93c0, 22B. Real: tail-call (jmp, not call+ret) to Ext0001toInt0001
 * -- ground truth's vtable slot for "produce Int0001" is looked up via
 * this->vtbl[0x9c] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0004toInt0001(const CConvertStorageParam &param) const
{
	Ext0001toInt0001(param);
}

/* .text+0x08de93e0, 22B. Real: tail-call (jmp, not call+ret) to Ext0001toInt0001
 * -- ground truth's vtable slot for "produce Int0001" is looked up via
 * this->vtbl[0x9c] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0005toInt0001(const CConvertStorageParam &param) const
{
	Ext0001toInt0001(param);
}

/* .text+0x08de9400, 22B. Real: tail-call (jmp, not call+ret) to Ext0001toInt0001
 * -- ground truth's vtable slot for "produce Int0001" is looked up via
 * this->vtbl[0x9c] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0006toInt0001(const CConvertStorageParam &param) const
{
	Ext0001toInt0001(param);
}

/* .text+0x08de9420, 22B. Real: tail-call (jmp, not call+ret) to Ext0001toInt0001
 * -- ground truth's vtable slot for "produce Int0001" is looked up via
 * this->vtbl[0x9c] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0007toInt0001(const CConvertStorageParam &param) const
{
	Ext0001toInt0001(param);
}

/* .text+0x08de9440, 22B. Real: tail-call (jmp, not call+ret) to Ext0001toInt0001
 * -- ground truth's vtable slot for "produce Int0001" is looked up via
 * this->vtbl[0x9c] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0008toInt0001(const CConvertStorageParam &param) const
{
	Ext0001toInt0001(param);
}

/* .text+0x08de9460, 22B. Real: tail-call (jmp, not call+ret) to Ext0001toInt0001
 * -- ground truth's vtable slot for "produce Int0001" is looked up via
 * this->vtbl[0x9c] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0009toInt0001(const CConvertStorageParam &param) const
{
	Ext0001toInt0001(param);
}

/* .text+0x08de9480, 22B. Real: tail-call (jmp, not call+ret) to Ext0001toInt0001
 * -- ground truth's vtable slot for "produce Int0001" is looked up via
 * this->vtbl[0x9c] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000AtoInt0001(const CConvertStorageParam &param) const
{
	Ext0001toInt0001(param);
}

/* .text+0x08de94a0, 22B. Real: tail-call (jmp, not call+ret) to Ext0001toInt0001
 * -- ground truth's vtable slot for "produce Int0001" is looked up via
 * this->vtbl[0x9c] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000BtoInt0001(const CConvertStorageParam &param) const
{
	Ext0001toInt0001(param);
}

/* .text+0x08de94c0, 22B. Real: tail-call (jmp, not call+ret) to Ext0001toInt0001
 * -- ground truth's vtable slot for "produce Int0001" is looked up via
 * this->vtbl[0x9c] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000CtoInt0001(const CConvertStorageParam &param) const
{
	Ext0001toInt0001(param);
}

/* .text+0x08de94e0, 22B. Real: tail-call (jmp, not call+ret) to Ext0001toInt0001
 * -- ground truth's vtable slot for "produce Int0001" is looked up via
 * this->vtbl[0x9c] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000DtoInt0001(const CConvertStorageParam &param) const
{
	Ext0001toInt0001(param);
}

/* .text+0x08de9500, 22B. Real: tail-call (jmp, not call+ret) to Ext0001toInt0001
 * -- ground truth's vtable slot for "produce Int0001" is looked up via
 * this->vtbl[0x9c] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000EtoInt0001(const CConvertStorageParam &param) const
{
	Ext0001toInt0001(param);
}

/* .text+0x08de9520, 22B. Real: tail-call (jmp, not call+ret) to Ext0001toInt0001
 * -- ground truth's vtable slot for "produce Int0001" is looked up via
 * this->vtbl[0x9c] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000FtoInt0001(const CConvertStorageParam &param) const
{
	Ext0001toInt0001(param);
}

// ---- target Int0002 ----

/* .text+0x08de9540, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0000toInt0002(const CConvertStorageParam &) const
{
}

/* .text+0x08de9550, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0001toInt0002(const CConvertStorageParam &) const
{
}

/* .text+0x08de9560, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0002toInt0002(const CConvertStorageParam &) const
{
}

/* .text+0x08de9570, 22B. Real: tail-call (jmp, not call+ret) to Ext0002toInt0002
 * -- ground truth's vtable slot for "produce Int0002" is looked up via
 * this->vtbl[0xe0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0003toInt0002(const CConvertStorageParam &param) const
{
	Ext0002toInt0002(param);
}

/* .text+0x08de9590, 22B. Real: tail-call (jmp, not call+ret) to Ext0002toInt0002
 * -- ground truth's vtable slot for "produce Int0002" is looked up via
 * this->vtbl[0xe0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0004toInt0002(const CConvertStorageParam &param) const
{
	Ext0002toInt0002(param);
}

/* .text+0x08de95b0, 22B. Real: tail-call (jmp, not call+ret) to Ext0002toInt0002
 * -- ground truth's vtable slot for "produce Int0002" is looked up via
 * this->vtbl[0xe0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0005toInt0002(const CConvertStorageParam &param) const
{
	Ext0002toInt0002(param);
}

/* .text+0x08de95d0, 22B. Real: tail-call (jmp, not call+ret) to Ext0002toInt0002
 * -- ground truth's vtable slot for "produce Int0002" is looked up via
 * this->vtbl[0xe0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0006toInt0002(const CConvertStorageParam &param) const
{
	Ext0002toInt0002(param);
}

/* .text+0x08de95f0, 22B. Real: tail-call (jmp, not call+ret) to Ext0002toInt0002
 * -- ground truth's vtable slot for "produce Int0002" is looked up via
 * this->vtbl[0xe0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0007toInt0002(const CConvertStorageParam &param) const
{
	Ext0002toInt0002(param);
}

/* .text+0x08de9610, 22B. Real: tail-call (jmp, not call+ret) to Ext0002toInt0002
 * -- ground truth's vtable slot for "produce Int0002" is looked up via
 * this->vtbl[0xe0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0008toInt0002(const CConvertStorageParam &param) const
{
	Ext0002toInt0002(param);
}

/* .text+0x08de9630, 22B. Real: tail-call (jmp, not call+ret) to Ext0002toInt0002
 * -- ground truth's vtable slot for "produce Int0002" is looked up via
 * this->vtbl[0xe0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0009toInt0002(const CConvertStorageParam &param) const
{
	Ext0002toInt0002(param);
}

/* .text+0x08de9650, 22B. Real: tail-call (jmp, not call+ret) to Ext0002toInt0002
 * -- ground truth's vtable slot for "produce Int0002" is looked up via
 * this->vtbl[0xe0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000AtoInt0002(const CConvertStorageParam &param) const
{
	Ext0002toInt0002(param);
}

/* .text+0x08de9670, 22B. Real: tail-call (jmp, not call+ret) to Ext0002toInt0002
 * -- ground truth's vtable slot for "produce Int0002" is looked up via
 * this->vtbl[0xe0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000BtoInt0002(const CConvertStorageParam &param) const
{
	Ext0002toInt0002(param);
}

/* .text+0x08de9690, 22B. Real: tail-call (jmp, not call+ret) to Ext0002toInt0002
 * -- ground truth's vtable slot for "produce Int0002" is looked up via
 * this->vtbl[0xe0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000CtoInt0002(const CConvertStorageParam &param) const
{
	Ext0002toInt0002(param);
}

/* .text+0x08de96b0, 22B. Real: tail-call (jmp, not call+ret) to Ext0002toInt0002
 * -- ground truth's vtable slot for "produce Int0002" is looked up via
 * this->vtbl[0xe0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000DtoInt0002(const CConvertStorageParam &param) const
{
	Ext0002toInt0002(param);
}

/* .text+0x08de96d0, 22B. Real: tail-call (jmp, not call+ret) to Ext0002toInt0002
 * -- ground truth's vtable slot for "produce Int0002" is looked up via
 * this->vtbl[0xe0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000EtoInt0002(const CConvertStorageParam &param) const
{
	Ext0002toInt0002(param);
}

/* .text+0x08de96f0, 22B. Real: tail-call (jmp, not call+ret) to Ext0002toInt0002
 * -- ground truth's vtable slot for "produce Int0002" is looked up via
 * this->vtbl[0xe0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000FtoInt0002(const CConvertStorageParam &param) const
{
	Ext0002toInt0002(param);
}

// ---- target Int0003 ----

/* .text+0x08de9710, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0000toInt0003(const CConvertStorageParam &) const
{
}

/* .text+0x08de9720, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0001toInt0003(const CConvertStorageParam &) const
{
}

/* .text+0x08de9730, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0002toInt0003(const CConvertStorageParam &) const
{
}

/* .text+0x08de9740, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0003toInt0003(const CConvertStorageParam &) const
{
}

/* .text+0x08de9750, 22B. Real: tail-call (jmp, not call+ret) to Ext0003toInt0003
 * -- ground truth's vtable slot for "produce Int0003" is looked up via
 * this->vtbl[0x124] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0004toInt0003(const CConvertStorageParam &param) const
{
	Ext0003toInt0003(param);
}

/* .text+0x08de9770, 22B. Real: tail-call (jmp, not call+ret) to Ext0003toInt0003
 * -- ground truth's vtable slot for "produce Int0003" is looked up via
 * this->vtbl[0x124] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0005toInt0003(const CConvertStorageParam &param) const
{
	Ext0003toInt0003(param);
}

/* .text+0x08de9790, 22B. Real: tail-call (jmp, not call+ret) to Ext0003toInt0003
 * -- ground truth's vtable slot for "produce Int0003" is looked up via
 * this->vtbl[0x124] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0006toInt0003(const CConvertStorageParam &param) const
{
	Ext0003toInt0003(param);
}

/* .text+0x08de97b0, 22B. Real: tail-call (jmp, not call+ret) to Ext0003toInt0003
 * -- ground truth's vtable slot for "produce Int0003" is looked up via
 * this->vtbl[0x124] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0007toInt0003(const CConvertStorageParam &param) const
{
	Ext0003toInt0003(param);
}

/* .text+0x08de97d0, 22B. Real: tail-call (jmp, not call+ret) to Ext0003toInt0003
 * -- ground truth's vtable slot for "produce Int0003" is looked up via
 * this->vtbl[0x124] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0008toInt0003(const CConvertStorageParam &param) const
{
	Ext0003toInt0003(param);
}

/* .text+0x08de97f0, 22B. Real: tail-call (jmp, not call+ret) to Ext0003toInt0003
 * -- ground truth's vtable slot for "produce Int0003" is looked up via
 * this->vtbl[0x124] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0009toInt0003(const CConvertStorageParam &param) const
{
	Ext0003toInt0003(param);
}

/* .text+0x08de9810, 22B. Real: tail-call (jmp, not call+ret) to Ext0003toInt0003
 * -- ground truth's vtable slot for "produce Int0003" is looked up via
 * this->vtbl[0x124] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000AtoInt0003(const CConvertStorageParam &param) const
{
	Ext0003toInt0003(param);
}

/* .text+0x08de9830, 22B. Real: tail-call (jmp, not call+ret) to Ext0003toInt0003
 * -- ground truth's vtable slot for "produce Int0003" is looked up via
 * this->vtbl[0x124] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000BtoInt0003(const CConvertStorageParam &param) const
{
	Ext0003toInt0003(param);
}

/* .text+0x08de9850, 22B. Real: tail-call (jmp, not call+ret) to Ext0003toInt0003
 * -- ground truth's vtable slot for "produce Int0003" is looked up via
 * this->vtbl[0x124] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000CtoInt0003(const CConvertStorageParam &param) const
{
	Ext0003toInt0003(param);
}

/* .text+0x08de9870, 22B. Real: tail-call (jmp, not call+ret) to Ext0003toInt0003
 * -- ground truth's vtable slot for "produce Int0003" is looked up via
 * this->vtbl[0x124] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000DtoInt0003(const CConvertStorageParam &param) const
{
	Ext0003toInt0003(param);
}

/* .text+0x08de9890, 22B. Real: tail-call (jmp, not call+ret) to Ext0003toInt0003
 * -- ground truth's vtable slot for "produce Int0003" is looked up via
 * this->vtbl[0x124] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000EtoInt0003(const CConvertStorageParam &param) const
{
	Ext0003toInt0003(param);
}

/* .text+0x08de98b0, 22B. Real: tail-call (jmp, not call+ret) to Ext0003toInt0003
 * -- ground truth's vtable slot for "produce Int0003" is looked up via
 * this->vtbl[0x124] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000FtoInt0003(const CConvertStorageParam &param) const
{
	Ext0003toInt0003(param);
}

// ---- target Int0004 ----

/* .text+0x08de98d0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0000toInt0004(const CConvertStorageParam &) const
{
}

/* .text+0x08de98e0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0001toInt0004(const CConvertStorageParam &) const
{
}

/* .text+0x08de98f0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0002toInt0004(const CConvertStorageParam &) const
{
}

/* .text+0x08de9900, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0003toInt0004(const CConvertStorageParam &) const
{
}

/* .text+0x08de9910, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0004toInt0004(const CConvertStorageParam &) const
{
}

/* .text+0x08de9920, 22B. Real: tail-call (jmp, not call+ret) to Ext0004toInt0004
 * -- ground truth's vtable slot for "produce Int0004" is looked up via
 * this->vtbl[0x168] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0005toInt0004(const CConvertStorageParam &param) const
{
	Ext0004toInt0004(param);
}

/* .text+0x08de9940, 22B. Real: tail-call (jmp, not call+ret) to Ext0004toInt0004
 * -- ground truth's vtable slot for "produce Int0004" is looked up via
 * this->vtbl[0x168] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0006toInt0004(const CConvertStorageParam &param) const
{
	Ext0004toInt0004(param);
}

/* .text+0x08de9960, 22B. Real: tail-call (jmp, not call+ret) to Ext0004toInt0004
 * -- ground truth's vtable slot for "produce Int0004" is looked up via
 * this->vtbl[0x168] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0007toInt0004(const CConvertStorageParam &param) const
{
	Ext0004toInt0004(param);
}

/* .text+0x08de9980, 22B. Real: tail-call (jmp, not call+ret) to Ext0004toInt0004
 * -- ground truth's vtable slot for "produce Int0004" is looked up via
 * this->vtbl[0x168] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0008toInt0004(const CConvertStorageParam &param) const
{
	Ext0004toInt0004(param);
}

/* .text+0x08de99a0, 22B. Real: tail-call (jmp, not call+ret) to Ext0004toInt0004
 * -- ground truth's vtable slot for "produce Int0004" is looked up via
 * this->vtbl[0x168] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0009toInt0004(const CConvertStorageParam &param) const
{
	Ext0004toInt0004(param);
}

/* .text+0x08de99c0, 22B. Real: tail-call (jmp, not call+ret) to Ext0004toInt0004
 * -- ground truth's vtable slot for "produce Int0004" is looked up via
 * this->vtbl[0x168] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000AtoInt0004(const CConvertStorageParam &param) const
{
	Ext0004toInt0004(param);
}

/* .text+0x08de99e0, 22B. Real: tail-call (jmp, not call+ret) to Ext0004toInt0004
 * -- ground truth's vtable slot for "produce Int0004" is looked up via
 * this->vtbl[0x168] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000BtoInt0004(const CConvertStorageParam &param) const
{
	Ext0004toInt0004(param);
}

/* .text+0x08de9a00, 22B. Real: tail-call (jmp, not call+ret) to Ext0004toInt0004
 * -- ground truth's vtable slot for "produce Int0004" is looked up via
 * this->vtbl[0x168] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000CtoInt0004(const CConvertStorageParam &param) const
{
	Ext0004toInt0004(param);
}

/* .text+0x08de9a20, 22B. Real: tail-call (jmp, not call+ret) to Ext0004toInt0004
 * -- ground truth's vtable slot for "produce Int0004" is looked up via
 * this->vtbl[0x168] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000DtoInt0004(const CConvertStorageParam &param) const
{
	Ext0004toInt0004(param);
}

/* .text+0x08de9a40, 22B. Real: tail-call (jmp, not call+ret) to Ext0004toInt0004
 * -- ground truth's vtable slot for "produce Int0004" is looked up via
 * this->vtbl[0x168] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000EtoInt0004(const CConvertStorageParam &param) const
{
	Ext0004toInt0004(param);
}

/* .text+0x08de9a60, 22B. Real: tail-call (jmp, not call+ret) to Ext0004toInt0004
 * -- ground truth's vtable slot for "produce Int0004" is looked up via
 * this->vtbl[0x168] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000FtoInt0004(const CConvertStorageParam &param) const
{
	Ext0004toInt0004(param);
}

// ---- target Int0005 ----

/* .text+0x08de9a80, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0000toInt0005(const CConvertStorageParam &) const
{
}

/* .text+0x08de9a90, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0001toInt0005(const CConvertStorageParam &) const
{
}

/* .text+0x08de9aa0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0002toInt0005(const CConvertStorageParam &) const
{
}

/* .text+0x08de9ab0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0003toInt0005(const CConvertStorageParam &) const
{
}

/* .text+0x08de9ac0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0004toInt0005(const CConvertStorageParam &) const
{
}

/* .text+0x08de9ad0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0005toInt0005(const CConvertStorageParam &) const
{
}

/* .text+0x08de9ae0, 22B. Real: tail-call (jmp, not call+ret) to Ext0005toInt0005
 * -- ground truth's vtable slot for "produce Int0005" is looked up via
 * this->vtbl[0x1ac] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0006toInt0005(const CConvertStorageParam &param) const
{
	Ext0005toInt0005(param);
}

/* .text+0x08de9b00, 22B. Real: tail-call (jmp, not call+ret) to Ext0005toInt0005
 * -- ground truth's vtable slot for "produce Int0005" is looked up via
 * this->vtbl[0x1ac] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0007toInt0005(const CConvertStorageParam &param) const
{
	Ext0005toInt0005(param);
}

/* .text+0x08de9b20, 22B. Real: tail-call (jmp, not call+ret) to Ext0005toInt0005
 * -- ground truth's vtable slot for "produce Int0005" is looked up via
 * this->vtbl[0x1ac] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0008toInt0005(const CConvertStorageParam &param) const
{
	Ext0005toInt0005(param);
}

/* .text+0x08de9b40, 22B. Real: tail-call (jmp, not call+ret) to Ext0005toInt0005
 * -- ground truth's vtable slot for "produce Int0005" is looked up via
 * this->vtbl[0x1ac] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0009toInt0005(const CConvertStorageParam &param) const
{
	Ext0005toInt0005(param);
}

/* .text+0x08de9b60, 22B. Real: tail-call (jmp, not call+ret) to Ext0005toInt0005
 * -- ground truth's vtable slot for "produce Int0005" is looked up via
 * this->vtbl[0x1ac] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000AtoInt0005(const CConvertStorageParam &param) const
{
	Ext0005toInt0005(param);
}

/* .text+0x08de9b80, 22B. Real: tail-call (jmp, not call+ret) to Ext0005toInt0005
 * -- ground truth's vtable slot for "produce Int0005" is looked up via
 * this->vtbl[0x1ac] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000BtoInt0005(const CConvertStorageParam &param) const
{
	Ext0005toInt0005(param);
}

/* .text+0x08de9ba0, 22B. Real: tail-call (jmp, not call+ret) to Ext0005toInt0005
 * -- ground truth's vtable slot for "produce Int0005" is looked up via
 * this->vtbl[0x1ac] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000CtoInt0005(const CConvertStorageParam &param) const
{
	Ext0005toInt0005(param);
}

/* .text+0x08de9bc0, 22B. Real: tail-call (jmp, not call+ret) to Ext0005toInt0005
 * -- ground truth's vtable slot for "produce Int0005" is looked up via
 * this->vtbl[0x1ac] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000DtoInt0005(const CConvertStorageParam &param) const
{
	Ext0005toInt0005(param);
}

/* .text+0x08de9be0, 22B. Real: tail-call (jmp, not call+ret) to Ext0005toInt0005
 * -- ground truth's vtable slot for "produce Int0005" is looked up via
 * this->vtbl[0x1ac] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000EtoInt0005(const CConvertStorageParam &param) const
{
	Ext0005toInt0005(param);
}

/* .text+0x08de9c00, 22B. Real: tail-call (jmp, not call+ret) to Ext0005toInt0005
 * -- ground truth's vtable slot for "produce Int0005" is looked up via
 * this->vtbl[0x1ac] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000FtoInt0005(const CConvertStorageParam &param) const
{
	Ext0005toInt0005(param);
}

// ---- target Int0006 ----

/* .text+0x08de9c20, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0000toInt0006(const CConvertStorageParam &) const
{
}

/* .text+0x08de9c30, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0001toInt0006(const CConvertStorageParam &) const
{
}

/* .text+0x08de9c40, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0002toInt0006(const CConvertStorageParam &) const
{
}

/* .text+0x08de9c50, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0003toInt0006(const CConvertStorageParam &) const
{
}

/* .text+0x08de9c60, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0004toInt0006(const CConvertStorageParam &) const
{
}

/* .text+0x08de9c70, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0005toInt0006(const CConvertStorageParam &) const
{
}

/* .text+0x08de9c80, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0006toInt0006(const CConvertStorageParam &) const
{
}

/* .text+0x08de9c90, 22B. Real: tail-call (jmp, not call+ret) to Ext0006toInt0006
 * -- ground truth's vtable slot for "produce Int0006" is looked up via
 * this->vtbl[0x1f0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0007toInt0006(const CConvertStorageParam &param) const
{
	Ext0006toInt0006(param);
}

/* .text+0x08de9cb0, 22B. Real: tail-call (jmp, not call+ret) to Ext0006toInt0006
 * -- ground truth's vtable slot for "produce Int0006" is looked up via
 * this->vtbl[0x1f0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0008toInt0006(const CConvertStorageParam &param) const
{
	Ext0006toInt0006(param);
}

/* .text+0x08de9cd0, 22B. Real: tail-call (jmp, not call+ret) to Ext0006toInt0006
 * -- ground truth's vtable slot for "produce Int0006" is looked up via
 * this->vtbl[0x1f0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0009toInt0006(const CConvertStorageParam &param) const
{
	Ext0006toInt0006(param);
}

/* .text+0x08de9cf0, 22B. Real: tail-call (jmp, not call+ret) to Ext0006toInt0006
 * -- ground truth's vtable slot for "produce Int0006" is looked up via
 * this->vtbl[0x1f0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000AtoInt0006(const CConvertStorageParam &param) const
{
	Ext0006toInt0006(param);
}

/* .text+0x08de9d10, 22B. Real: tail-call (jmp, not call+ret) to Ext0006toInt0006
 * -- ground truth's vtable slot for "produce Int0006" is looked up via
 * this->vtbl[0x1f0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000BtoInt0006(const CConvertStorageParam &param) const
{
	Ext0006toInt0006(param);
}

/* .text+0x08de9d30, 22B. Real: tail-call (jmp, not call+ret) to Ext0006toInt0006
 * -- ground truth's vtable slot for "produce Int0006" is looked up via
 * this->vtbl[0x1f0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000CtoInt0006(const CConvertStorageParam &param) const
{
	Ext0006toInt0006(param);
}

/* .text+0x08de9d50, 22B. Real: tail-call (jmp, not call+ret) to Ext0006toInt0006
 * -- ground truth's vtable slot for "produce Int0006" is looked up via
 * this->vtbl[0x1f0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000DtoInt0006(const CConvertStorageParam &param) const
{
	Ext0006toInt0006(param);
}

/* .text+0x08de9d70, 22B. Real: tail-call (jmp, not call+ret) to Ext0006toInt0006
 * -- ground truth's vtable slot for "produce Int0006" is looked up via
 * this->vtbl[0x1f0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000EtoInt0006(const CConvertStorageParam &param) const
{
	Ext0006toInt0006(param);
}

/* .text+0x08de9d90, 22B. Real: tail-call (jmp, not call+ret) to Ext0006toInt0006
 * -- ground truth's vtable slot for "produce Int0006" is looked up via
 * this->vtbl[0x1f0] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000FtoInt0006(const CConvertStorageParam &param) const
{
	Ext0006toInt0006(param);
}

// ---- target Int0007 ----

/* .text+0x08de9db0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0000toInt0007(const CConvertStorageParam &) const
{
}

/* .text+0x08de9dc0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0001toInt0007(const CConvertStorageParam &) const
{
}

/* .text+0x08de9dd0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0002toInt0007(const CConvertStorageParam &) const
{
}

/* .text+0x08de9de0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0003toInt0007(const CConvertStorageParam &) const
{
}

/* .text+0x08de9df0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0004toInt0007(const CConvertStorageParam &) const
{
}

/* .text+0x08de9e00, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0005toInt0007(const CConvertStorageParam &) const
{
}

/* .text+0x08de9e10, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0006toInt0007(const CConvertStorageParam &) const
{
}

/* .text+0x08de9e20, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0007toInt0007(const CConvertStorageParam &) const
{
}

/* .text+0x08de9e30, 22B. Real: tail-call (jmp, not call+ret) to Ext0007toInt0007
 * -- ground truth's vtable slot for "produce Int0007" is looked up via
 * this->vtbl[0x234] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0008toInt0007(const CConvertStorageParam &param) const
{
	Ext0007toInt0007(param);
}

/* .text+0x08de9e50, 22B. Real: tail-call (jmp, not call+ret) to Ext0007toInt0007
 * -- ground truth's vtable slot for "produce Int0007" is looked up via
 * this->vtbl[0x234] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0009toInt0007(const CConvertStorageParam &param) const
{
	Ext0007toInt0007(param);
}

/* .text+0x08de9e70, 22B. Real: tail-call (jmp, not call+ret) to Ext0007toInt0007
 * -- ground truth's vtable slot for "produce Int0007" is looked up via
 * this->vtbl[0x234] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000AtoInt0007(const CConvertStorageParam &param) const
{
	Ext0007toInt0007(param);
}

/* .text+0x08de9e90, 22B. Real: tail-call (jmp, not call+ret) to Ext0007toInt0007
 * -- ground truth's vtable slot for "produce Int0007" is looked up via
 * this->vtbl[0x234] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000BtoInt0007(const CConvertStorageParam &param) const
{
	Ext0007toInt0007(param);
}

/* .text+0x08de9eb0, 22B. Real: tail-call (jmp, not call+ret) to Ext0007toInt0007
 * -- ground truth's vtable slot for "produce Int0007" is looked up via
 * this->vtbl[0x234] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000CtoInt0007(const CConvertStorageParam &param) const
{
	Ext0007toInt0007(param);
}

/* .text+0x08de9ed0, 22B. Real: tail-call (jmp, not call+ret) to Ext0007toInt0007
 * -- ground truth's vtable slot for "produce Int0007" is looked up via
 * this->vtbl[0x234] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000DtoInt0007(const CConvertStorageParam &param) const
{
	Ext0007toInt0007(param);
}

/* .text+0x08de9ef0, 22B. Real: tail-call (jmp, not call+ret) to Ext0007toInt0007
 * -- ground truth's vtable slot for "produce Int0007" is looked up via
 * this->vtbl[0x234] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000EtoInt0007(const CConvertStorageParam &param) const
{
	Ext0007toInt0007(param);
}

/* .text+0x08de9f10, 22B. Real: tail-call (jmp, not call+ret) to Ext0007toInt0007
 * -- ground truth's vtable slot for "produce Int0007" is looked up via
 * this->vtbl[0x234] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000FtoInt0007(const CConvertStorageParam &param) const
{
	Ext0007toInt0007(param);
}

// ---- target Int0008 ----

/* .text+0x08de9f30, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0000toInt0008(const CConvertStorageParam &) const
{
}

/* .text+0x08de9f40, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0001toInt0008(const CConvertStorageParam &) const
{
}

/* .text+0x08de9f50, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0002toInt0008(const CConvertStorageParam &) const
{
}

/* .text+0x08de9f60, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0003toInt0008(const CConvertStorageParam &) const
{
}

/* .text+0x08de9f70, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0004toInt0008(const CConvertStorageParam &) const
{
}

/* .text+0x08de9f80, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0005toInt0008(const CConvertStorageParam &) const
{
}

/* .text+0x08de9f90, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0006toInt0008(const CConvertStorageParam &) const
{
}

/* .text+0x08de9fa0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0007toInt0008(const CConvertStorageParam &) const
{
}

/* .text+0x08de9fb0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0008toInt0008(const CConvertStorageParam &) const
{
}

/* .text+0x08de9fc0, 22B. Real: tail-call (jmp, not call+ret) to Ext0008toInt0008
 * -- ground truth's vtable slot for "produce Int0008" is looked up via
 * this->vtbl[0x278] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext0009toInt0008(const CConvertStorageParam &param) const
{
	Ext0008toInt0008(param);
}

/* .text+0x08de9fe0, 22B. Real: tail-call (jmp, not call+ret) to Ext0008toInt0008
 * -- ground truth's vtable slot for "produce Int0008" is looked up via
 * this->vtbl[0x278] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000AtoInt0008(const CConvertStorageParam &param) const
{
	Ext0008toInt0008(param);
}

/* .text+0x08dea000, 22B. Real: tail-call (jmp, not call+ret) to Ext0008toInt0008
 * -- ground truth's vtable slot for "produce Int0008" is looked up via
 * this->vtbl[0x278] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000BtoInt0008(const CConvertStorageParam &param) const
{
	Ext0008toInt0008(param);
}

/* .text+0x08dea020, 22B. Real: tail-call (jmp, not call+ret) to Ext0008toInt0008
 * -- ground truth's vtable slot for "produce Int0008" is looked up via
 * this->vtbl[0x278] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000CtoInt0008(const CConvertStorageParam &param) const
{
	Ext0008toInt0008(param);
}

/* .text+0x08dea040, 22B. Real: tail-call (jmp, not call+ret) to Ext0008toInt0008
 * -- ground truth's vtable slot for "produce Int0008" is looked up via
 * this->vtbl[0x278] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000DtoInt0008(const CConvertStorageParam &param) const
{
	Ext0008toInt0008(param);
}

/* .text+0x08dea060, 22B. Real: tail-call (jmp, not call+ret) to Ext0008toInt0008
 * -- ground truth's vtable slot for "produce Int0008" is looked up via
 * this->vtbl[0x278] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000EtoInt0008(const CConvertStorageParam &param) const
{
	Ext0008toInt0008(param);
}

/* .text+0x08dea080, 22B. Real: tail-call (jmp, not call+ret) to Ext0008toInt0008
 * -- ground truth's vtable slot for "produce Int0008" is looked up via
 * this->vtbl[0x278] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000FtoInt0008(const CConvertStorageParam &param) const
{
	Ext0008toInt0008(param);
}

// ---- target Int0009 ----

/* .text+0x08dea0a0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0000toInt0009(const CConvertStorageParam &) const
{
}

/* .text+0x08dea0b0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0001toInt0009(const CConvertStorageParam &) const
{
}

/* .text+0x08dea0c0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0002toInt0009(const CConvertStorageParam &) const
{
}

/* .text+0x08dea0d0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0003toInt0009(const CConvertStorageParam &) const
{
}

/* .text+0x08dea0e0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0004toInt0009(const CConvertStorageParam &) const
{
}

/* .text+0x08dea0f0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0005toInt0009(const CConvertStorageParam &) const
{
}

/* .text+0x08dea100, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0006toInt0009(const CConvertStorageParam &) const
{
}

/* .text+0x08dea110, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0007toInt0009(const CConvertStorageParam &) const
{
}

/* .text+0x08dea120, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0008toInt0009(const CConvertStorageParam &) const
{
}

/* .text+0x08dea130, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0009toInt0009(const CConvertStorageParam &) const
{
}

/* .text+0x08dea140, 22B. Real: tail-call (jmp, not call+ret) to Ext0009toInt0009
 * -- ground truth's vtable slot for "produce Int0009" is looked up via
 * this->vtbl[0x2bc] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000AtoInt0009(const CConvertStorageParam &param) const
{
	Ext0009toInt0009(param);
}

/* .text+0x08dea160, 22B. Real: tail-call (jmp, not call+ret) to Ext0009toInt0009
 * -- ground truth's vtable slot for "produce Int0009" is looked up via
 * this->vtbl[0x2bc] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000BtoInt0009(const CConvertStorageParam &param) const
{
	Ext0009toInt0009(param);
}

/* .text+0x08dea180, 22B. Real: tail-call (jmp, not call+ret) to Ext0009toInt0009
 * -- ground truth's vtable slot for "produce Int0009" is looked up via
 * this->vtbl[0x2bc] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000CtoInt0009(const CConvertStorageParam &param) const
{
	Ext0009toInt0009(param);
}

/* .text+0x08dea1a0, 22B. Real: tail-call (jmp, not call+ret) to Ext0009toInt0009
 * -- ground truth's vtable slot for "produce Int0009" is looked up via
 * this->vtbl[0x2bc] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000DtoInt0009(const CConvertStorageParam &param) const
{
	Ext0009toInt0009(param);
}

/* .text+0x08dea1c0, 22B. Real: tail-call (jmp, not call+ret) to Ext0009toInt0009
 * -- ground truth's vtable slot for "produce Int0009" is looked up via
 * this->vtbl[0x2bc] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000EtoInt0009(const CConvertStorageParam &param) const
{
	Ext0009toInt0009(param);
}

/* .text+0x08dea1e0, 22B. Real: tail-call (jmp, not call+ret) to Ext0009toInt0009
 * -- ground truth's vtable slot for "produce Int0009" is looked up via
 * this->vtbl[0x2bc] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000FtoInt0009(const CConvertStorageParam &param) const
{
	Ext0009toInt0009(param);
}

// ---- target Int000A ----

/* .text+0x08dea200, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0000toInt000A(const CConvertStorageParam &) const
{
}

/* .text+0x08dea210, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0001toInt000A(const CConvertStorageParam &) const
{
}

/* .text+0x08dea220, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0002toInt000A(const CConvertStorageParam &) const
{
}

/* .text+0x08dea230, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0003toInt000A(const CConvertStorageParam &) const
{
}

/* .text+0x08dea240, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0004toInt000A(const CConvertStorageParam &) const
{
}

/* .text+0x08dea250, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0005toInt000A(const CConvertStorageParam &) const
{
}

/* .text+0x08dea260, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0006toInt000A(const CConvertStorageParam &) const
{
}

/* .text+0x08dea270, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0007toInt000A(const CConvertStorageParam &) const
{
}

/* .text+0x08dea280, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0008toInt000A(const CConvertStorageParam &) const
{
}

/* .text+0x08dea290, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0009toInt000A(const CConvertStorageParam &) const
{
}

/* .text+0x08dea2a0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000AtoInt000A(const CConvertStorageParam &) const
{
}

/* .text+0x08dea2b0, 22B. Real: tail-call (jmp, not call+ret) to Ext000AtoInt000A
 * -- ground truth's vtable slot for "produce Int000A" is looked up via
 * this->vtbl[0x300] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000BtoInt000A(const CConvertStorageParam &param) const
{
	Ext000AtoInt000A(param);
}

/* .text+0x08dea2d0, 22B. Real: tail-call (jmp, not call+ret) to Ext000AtoInt000A
 * -- ground truth's vtable slot for "produce Int000A" is looked up via
 * this->vtbl[0x300] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000CtoInt000A(const CConvertStorageParam &param) const
{
	Ext000AtoInt000A(param);
}

/* .text+0x08dea2f0, 22B. Real: tail-call (jmp, not call+ret) to Ext000AtoInt000A
 * -- ground truth's vtable slot for "produce Int000A" is looked up via
 * this->vtbl[0x300] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000DtoInt000A(const CConvertStorageParam &param) const
{
	Ext000AtoInt000A(param);
}

/* .text+0x08dea310, 22B. Real: tail-call (jmp, not call+ret) to Ext000AtoInt000A
 * -- ground truth's vtable slot for "produce Int000A" is looked up via
 * this->vtbl[0x300] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000EtoInt000A(const CConvertStorageParam &param) const
{
	Ext000AtoInt000A(param);
}

/* .text+0x08dea330, 22B. Real: tail-call (jmp, not call+ret) to Ext000AtoInt000A
 * -- ground truth's vtable slot for "produce Int000A" is looked up via
 * this->vtbl[0x300] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000FtoInt000A(const CConvertStorageParam &param) const
{
	Ext000AtoInt000A(param);
}

// ---- target Int000B ----

/* .text+0x08dea350, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0000toInt000B(const CConvertStorageParam &) const
{
}

/* .text+0x08dea360, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0001toInt000B(const CConvertStorageParam &) const
{
}

/* .text+0x08dea370, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0002toInt000B(const CConvertStorageParam &) const
{
}

/* .text+0x08dea380, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0003toInt000B(const CConvertStorageParam &) const
{
}

/* .text+0x08dea390, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0004toInt000B(const CConvertStorageParam &) const
{
}

/* .text+0x08dea3a0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0005toInt000B(const CConvertStorageParam &) const
{
}

/* .text+0x08dea3b0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0006toInt000B(const CConvertStorageParam &) const
{
}

/* .text+0x08dea3c0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0007toInt000B(const CConvertStorageParam &) const
{
}

/* .text+0x08dea3d0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0008toInt000B(const CConvertStorageParam &) const
{
}

/* .text+0x08dea3e0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0009toInt000B(const CConvertStorageParam &) const
{
}

/* .text+0x08dea3f0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000AtoInt000B(const CConvertStorageParam &) const
{
}

/* .text+0x08dea400, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000BtoInt000B(const CConvertStorageParam &) const
{
}

/* .text+0x08dea410, 22B. Real: tail-call (jmp, not call+ret) to Ext000BtoInt000B
 * -- ground truth's vtable slot for "produce Int000B" is looked up via
 * this->vtbl[0x344] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000CtoInt000B(const CConvertStorageParam &param) const
{
	Ext000BtoInt000B(param);
}

/* .text+0x08dea430, 22B. Real: tail-call (jmp, not call+ret) to Ext000BtoInt000B
 * -- ground truth's vtable slot for "produce Int000B" is looked up via
 * this->vtbl[0x344] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000DtoInt000B(const CConvertStorageParam &param) const
{
	Ext000BtoInt000B(param);
}

/* .text+0x08dea450, 22B. Real: tail-call (jmp, not call+ret) to Ext000BtoInt000B
 * -- ground truth's vtable slot for "produce Int000B" is looked up via
 * this->vtbl[0x344] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000EtoInt000B(const CConvertStorageParam &param) const
{
	Ext000BtoInt000B(param);
}

/* .text+0x08dea470, 22B. Real: tail-call (jmp, not call+ret) to Ext000BtoInt000B
 * -- ground truth's vtable slot for "produce Int000B" is looked up via
 * this->vtbl[0x344] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000FtoInt000B(const CConvertStorageParam &param) const
{
	Ext000BtoInt000B(param);
}

// ---- target Int000C ----

/* .text+0x08dea490, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0000toInt000C(const CConvertStorageParam &) const
{
}

/* .text+0x08dea4a0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0001toInt000C(const CConvertStorageParam &) const
{
}

/* .text+0x08dea4b0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0002toInt000C(const CConvertStorageParam &) const
{
}

/* .text+0x08dea4c0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0003toInt000C(const CConvertStorageParam &) const
{
}

/* .text+0x08dea4d0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0004toInt000C(const CConvertStorageParam &) const
{
}

/* .text+0x08dea4e0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0005toInt000C(const CConvertStorageParam &) const
{
}

/* .text+0x08dea4f0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0006toInt000C(const CConvertStorageParam &) const
{
}

/* .text+0x08dea500, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0007toInt000C(const CConvertStorageParam &) const
{
}

/* .text+0x08dea510, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0008toInt000C(const CConvertStorageParam &) const
{
}

/* .text+0x08dea520, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0009toInt000C(const CConvertStorageParam &) const
{
}

/* .text+0x08dea530, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000AtoInt000C(const CConvertStorageParam &) const
{
}

/* .text+0x08dea540, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000BtoInt000C(const CConvertStorageParam &) const
{
}

/* .text+0x08dea550, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000CtoInt000C(const CConvertStorageParam &) const
{
}

/* .text+0x08dea560, 22B. Real: tail-call (jmp, not call+ret) to Ext000CtoInt000C
 * -- ground truth's vtable slot for "produce Int000C" is looked up via
 * this->vtbl[0x388] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000DtoInt000C(const CConvertStorageParam &param) const
{
	Ext000CtoInt000C(param);
}

/* .text+0x08dea580, 22B. Real: tail-call (jmp, not call+ret) to Ext000CtoInt000C
 * -- ground truth's vtable slot for "produce Int000C" is looked up via
 * this->vtbl[0x388] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000EtoInt000C(const CConvertStorageParam &param) const
{
	Ext000CtoInt000C(param);
}

/* .text+0x08dea5a0, 22B. Real: tail-call (jmp, not call+ret) to Ext000CtoInt000C
 * -- ground truth's vtable slot for "produce Int000C" is looked up via
 * this->vtbl[0x388] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000FtoInt000C(const CConvertStorageParam &param) const
{
	Ext000CtoInt000C(param);
}

// ---- target Int000D ----

/* .text+0x08dea5c0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0000toInt000D(const CConvertStorageParam &) const
{
}

/* .text+0x08dea5d0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0001toInt000D(const CConvertStorageParam &) const
{
}

/* .text+0x08dea5e0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0002toInt000D(const CConvertStorageParam &) const
{
}

/* .text+0x08dea5f0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0003toInt000D(const CConvertStorageParam &) const
{
}

/* .text+0x08dea600, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0004toInt000D(const CConvertStorageParam &) const
{
}

/* .text+0x08dea610, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0005toInt000D(const CConvertStorageParam &) const
{
}

/* .text+0x08dea620, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0006toInt000D(const CConvertStorageParam &) const
{
}

/* .text+0x08dea630, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0007toInt000D(const CConvertStorageParam &) const
{
}

/* .text+0x08dea640, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0008toInt000D(const CConvertStorageParam &) const
{
}

/* .text+0x08dea650, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0009toInt000D(const CConvertStorageParam &) const
{
}

/* .text+0x08dea660, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000AtoInt000D(const CConvertStorageParam &) const
{
}

/* .text+0x08dea670, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000BtoInt000D(const CConvertStorageParam &) const
{
}

/* .text+0x08dea680, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000CtoInt000D(const CConvertStorageParam &) const
{
}

/* .text+0x08dea690, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000DtoInt000D(const CConvertStorageParam &) const
{
}

/* .text+0x08dea6a0, 22B. Real: tail-call (jmp, not call+ret) to Ext000DtoInt000D
 * -- ground truth's vtable slot for "produce Int000D" is looked up via
 * this->vtbl[0x3cc] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000EtoInt000D(const CConvertStorageParam &param) const
{
	Ext000DtoInt000D(param);
}

/* .text+0x08dea6c0, 22B. Real: tail-call (jmp, not call+ret) to Ext000DtoInt000D
 * -- ground truth's vtable slot for "produce Int000D" is looked up via
 * this->vtbl[0x3cc] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000FtoInt000D(const CConvertStorageParam &param) const
{
	Ext000DtoInt000D(param);
}

// ---- target Int000E ----

/* .text+0x08dea6e0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0000toInt000E(const CConvertStorageParam &) const
{
}

/* .text+0x08dea6f0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0001toInt000E(const CConvertStorageParam &) const
{
}

/* .text+0x08dea700, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0002toInt000E(const CConvertStorageParam &) const
{
}

/* .text+0x08dea710, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0003toInt000E(const CConvertStorageParam &) const
{
}

/* .text+0x08dea720, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0004toInt000E(const CConvertStorageParam &) const
{
}

/* .text+0x08dea730, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0005toInt000E(const CConvertStorageParam &) const
{
}

/* .text+0x08dea740, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0006toInt000E(const CConvertStorageParam &) const
{
}

/* .text+0x08dea750, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0007toInt000E(const CConvertStorageParam &) const
{
}

/* .text+0x08dea760, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0008toInt000E(const CConvertStorageParam &) const
{
}

/* .text+0x08dea770, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0009toInt000E(const CConvertStorageParam &) const
{
}

/* .text+0x08dea780, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000AtoInt000E(const CConvertStorageParam &) const
{
}

/* .text+0x08dea790, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000BtoInt000E(const CConvertStorageParam &) const
{
}

/* .text+0x08dea7a0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000CtoInt000E(const CConvertStorageParam &) const
{
}

/* .text+0x08dea7b0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000DtoInt000E(const CConvertStorageParam &) const
{
}

/* .text+0x08dea7c0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000EtoInt000E(const CConvertStorageParam &) const
{
}

/* .text+0x08dea7d0, 22B. Real: tail-call (jmp, not call+ret) to Ext000EtoInt000E
 * -- ground truth's vtable slot for "produce Int000E" is looked up via
 * this->vtbl[0x410] and jumped to directly, ignoring the source version X
 * entirely (confirmed for all 120 thunks in this matrix via a full .rodata
 * vtable-slot resolution, not just this one).
 */
void CStorageConverterBase::Ext000FtoInt000E(const CConvertStorageParam &param) const
{
	Ext000EtoInt000E(param);
}

// ---- target Int000F ----

/* .text+0x08dea7f0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0000toInt000F(const CConvertStorageParam &) const
{
}

/* .text+0x08dea800, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0001toInt000F(const CConvertStorageParam &) const
{
}

/* .text+0x08dea810, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0002toInt000F(const CConvertStorageParam &) const
{
}

/* .text+0x08dea820, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0003toInt000F(const CConvertStorageParam &) const
{
}

/* .text+0x08dea830, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0004toInt000F(const CConvertStorageParam &) const
{
}

/* .text+0x08dea840, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0005toInt000F(const CConvertStorageParam &) const
{
}

/* .text+0x08dea850, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0006toInt000F(const CConvertStorageParam &) const
{
}

/* .text+0x08dea860, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0007toInt000F(const CConvertStorageParam &) const
{
}

/* .text+0x08dea870, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0008toInt000F(const CConvertStorageParam &) const
{
}

/* .text+0x08dea880, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext0009toInt000F(const CConvertStorageParam &) const
{
}

/* .text+0x08dea890, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000AtoInt000F(const CConvertStorageParam &) const
{
}

/* .text+0x08dea8a0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000BtoInt000F(const CConvertStorageParam &) const
{
}

/* .text+0x08dea8b0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000CtoInt000F(const CConvertStorageParam &) const
{
}

/* .text+0x08dea8c0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000DtoInt000F(const CConvertStorageParam &) const
{
}

/* .text+0x08dea8d0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000EtoInt000F(const CConvertStorageParam &) const
{
}

/* .text+0x08dea8e0, 1B (bare `ret`). Real: unconditional no-op -- confirmed
 * genuine, not a decompiler artifact (see header comment's triangular-matrix
 * rule: X<=Y and Y!=0 is never implemented in this build).
 */
void CStorageConverterBase::Ext000FtoInt000F(const CConvertStorageParam &) const
{
}
