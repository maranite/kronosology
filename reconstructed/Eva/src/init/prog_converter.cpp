/*
 * prog_converter.cpp  -  see include/prog_converter.h for the full derivation.
 */

#include "prog_converter.h"

#include <cstdlib>

// Real ground-truth symbols `_Z21HAL_DisableInterruptsv`/`_Z20HAL_EnableInterruptsv`
// (plain C++ free functions, no `extern "C"` -- the mangled names confirm that).
// Referenced (in comments only) throughout this project's other ctor/dtor
// reconstructions as the real critical-section idiom around malloc/free;
// CProgConverter::DeletingDtor() below is the first to actually call them, so
// it needs real linkable definitions. Same "opaque HAL sink, no real target
// reconstructed, no-op here" convention as HAL_LLDebugTrace()
// (alpha_keyb_ctrl_task.cpp) -- real hardware interrupt-mask control, out of
// scope for a host build.
void HAL_DisableInterrupts()
{
}

void HAL_EnableInterrupts()
{
}

namespace {

// Ground truth's own `&vtable_for_CStorageConverterBase + 8` (0x08fcc9c8), the
// real relocated value D1/D0 both write into all 3 of this object's raw
// pointer-sized slots. Recorded as a literal address per this project's
// LESSON_missing_vtable_write.md convention for documenting a real relocation
// target byte-for-byte -- not meaningfully dereferenceable on the host (this
// reconstruction never dispatches through m_vptr0/m_base4/m_base8).
void *const kBaseVtablePlus8 = reinterpret_cast<void *>(0x08fcc9c8);

} // namespace

// .text+0x08e07d90, 25B. Ground truth order: +0x8, then +0x4, then +0x0
// (reproduced exactly, though observably order-independent here).
CProgConverter::~CProgConverter()
{
	m_base8 = kBaseVtablePlus8;
	m_base4 = kBaseVtablePlus8;
	m_vptr0 = kBaseVtablePlus8;
}

// .text+0x08e07db0, 74B. Same 3 resets as ~CProgConverter() above, then a
// real HAL_DisableInterrupts()/free(this)/HAL_EnableInterrupts() sequence.
// Ground truth also has an exception-unwind landing pad after this
// (_Unwind_Resume/__cxa_call_unexpected) for the case free() itself throws --
// not modeled, matching this project's existing convention of reconstructing
// primary control flow only and leaving compiler-generated EH landing pads
// out (they have no observable effect unless an exception is actually in
// flight during this call, which nothing in this project's own call graph
// currently triggers).
void CProgConverter::DeletingDtor()
{
	m_base8 = kBaseVtablePlus8;
	m_base4 = kBaseVtablePlus8;
	m_vptr0 = kBaseVtablePlus8;

	HAL_DisableInterrupts();
	std::free(this);
	HAL_EnableInterrupts();
}

// Load()/Save() are NOT implemented here -- see prog_converter.h's own
// declaration comment for why (fully understood forwards, but their real
// target is itself a dispatcher into the still-deferred ExttoIntXXXX family).

// .text+0x08df7590, 37B. Real: forwards to m_pFormatConverter->Close(), then
// (unlike Load/Save) nulls the member out.
void CProgConverter::Close()
{
	if (m_pFormatConverter) {
		m_pFormatConverter->Close();
		m_pFormatConverter = 0;
	}
}
