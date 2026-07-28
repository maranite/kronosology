// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_TG92_OSC_H
#define OA_STG_TG92_OSC_H

#include "oa_adsr_base.h"	/* CSTGPatchMessageContext, STGConvertedParam,
				 * CSTGParamsOwner::sValueGetterTemp */

/*
 * oa_stg_tg92_osc.h  -  CSTGTG92Osc's value-getter family: both real
 * weak-symbol ctx-only candidates reconstructed, zero outliers -- see
 * include/oa_stg_string.h for the pilot class's full derivation.
 * Multisample-velocity-zone patch component -- confirmed genuinely fresh
 * via a word-boundary grep before starting. Distinct from the
 * already-modeled CSTGVPMTG92Osc (VPM engine's own TG92 oscillator) and
 * from the still-open, pure-virtual-deferred CSTGTG92OscBase (see the
 * family memory's batch 12 note) -- CSTGTG92Osc is a small, separate
 * subclass with just these 2 own real candidates.
 *
 * Dialect: both candidates fixed-K fields read directly off this, zero
 * ctx-dynamic-index methods.
 *
 * Field-shape summary:
 *   - Plain unsigned 8-bit fields, no shift/mask (the family's
 *     established "unsigned non-bitfield byte" variant, first confirmed
 *     on CSTGPolysixMG::GetMIDITempoSyncTimes): single-write value only
 *     -- GetOscVelocityZoneLow, GetOscVelocityZoneHigh.
 * No dword fields in this class, so no dual-write instance either way.
 */

struct CSTGTG92Osc {
	STGConvertedParam &GetOscVelocityZoneLow(CSTGPatchMessageContext &ctx);
	STGConvertedParam &GetOscVelocityZoneHigh(CSTGPatchMessageContext &ctx);
};

#endif
