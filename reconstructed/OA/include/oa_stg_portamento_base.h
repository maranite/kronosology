// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_PORTAMENTO_BASE_H
#define OA_STG_PORTAMENTO_BASE_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_portamento_base.h  -  CSTGPortamentoBase's value-getter family:
 * all 6 real weak-symbol ctx-only candidates reconstructed, zero
 * outliers -- see include/oa_stg_string.h for the pilot class's full
 * derivation. CSTGPortamentoBase is the STG portamento (pitch-glide)
 * patch component -- glide time, three mode flags, and the AMS source/
 * intensity siblings for time -- confirmed genuinely fresh via a
 * word-boundary grep before starting, no pre-existing struct or ctor
 * anywhere in this project.
 *
 * Dialect: zero ctx-dynamic-index methods. Three independent single-bit
 * booleans -- Enabled, Fingered, ConstantTime -- pack into one byte at
 * +0x1d via the family's established shift-then-mask bitfield shape
 * (Enabled bit 0 has no shift instruction at all, Fingered bit 1 and
 * ConstantTime bit 2 use an explicit shr), same convention as
 * CSTGPolysixModelPatch's own Arpeggiator* group and
 * CSTGVPMOsc's four-bit-in-one-byte group. Time and AMSIntensity are
 * plain fixed-K dwords, dual-write. AMSSource is a plain fixed-K signed
 * byte, single-write.
 */

struct CSTGPortamentoBase {
	STGConvertedParam &GetPortamentoTime(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPortamentoEnabled(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPortamentoFingered(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPortamentoConstantTime(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPortamentoAMSSource(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetPortamentoAMSIntensity(CSTGPatchMessageContext &ctx);
};

#endif
