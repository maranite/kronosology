// SPDX-License-Identifier: GPL-2.0
/*
 * rtparm_ckgparamedit.cpp  -  CKGParamEdit::GetRTParmBufferSelectId(int),
 * .text+0x3ab550, 15 bytes. One member of the RTParm free-function family
 * reconstruction batch (see include/oa_rtparm_family.h for the full
 * scope), kept in its own translation unit rather than
 * src/engine/rtparm_family.cpp: including oa_ckg_module_param_msg_handler.h
 * (needed for CKGParamEdit's full declaration) together with
 * oa_rtparm_pe_table.h (needed for the rest of the family's
 * gRTParmFunctionTable_PE) hits a genuine, pre-existing latent conflict --
 * both headers declare `RT_run(unsigned char, unsigned char)` with
 * different linkage (`extern "C"` in oa_ckg_module_param_msg_handler.h,
 * deliberately, per that header's own comment on enum-widened KARMA
 * externs; `extern "C++"` in oa_rtparm_pe_table.h, the real mangled
 * linkage this project's own GE/PE table work independently verified).
 * Never triggered before this pass because no prior file needed both
 * headers together. Not "fixed" in either header under this pass's time
 * budget -- this file sidesteps it by only ever including
 * oa_ckg_module_param_msg_handler.h.
 */

#include "oa_ckg_module_param_msg_handler.h"

unsigned int CKGParamEdit::GetRTParmBufferSelectId(int deviceIndex)
{
	/* real body ignores `this`; table @.rodata+0xaa6c8, 4 entries {1,2,3,4} */
	if ((unsigned int)deviceIndex > 3)
		return 0;
	static const unsigned int kTable[4] = { 1, 2, 3, 4 };
	return kTable[deviceIndex];
}
