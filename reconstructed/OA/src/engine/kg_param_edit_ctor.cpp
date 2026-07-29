// SPDX-License-Identifier: GPL-2.0
/*
 * kg_param_edit_ctor.cpp  -  CKGParamEdit::CKGParamEdit() (round 52, solo),
 * split out of kg_param_edit.cpp into its own translation unit: the ctor is
 * the ONLY CKGParamEdit method that needs zero of the ~35 new RT_ / KS_
 * externs (see include/oa_kg_param_edit_rt_externs.h), so existing test
 * binaries that only need CKGParamEdit's ctor to link (test_ckg_engine,
 * test_rtparm_ckgparamedit -- neither calls any SendXxx() method) can link
 * this tiny file instead of dragging in kg_param_edit.cpp's full ~35-symbol
 * mock surface.
 */
#include "oa_ckg_module_param_msg_handler.h"

CKGParamEdit::CKGParamEdit()
{
	mSoloStatus[0] = mSoloStatus[1] = mSoloStatus[2] = mSoloStatus[3] = 0;
}
