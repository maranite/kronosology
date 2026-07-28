// SPDX-License-Identifier: GPL-2.0
#ifndef OA_STG_HDR_TRACK_H
#define OA_STG_HDR_TRACK_H

#include "oa_global.h"	/* STGConvertedParam, CSTGParamsOwner::sValueGetterTemp,
			 * CSTGControllerRTData::sInstance */

/*
 * oa_stg_hdr_track.h  -  CSTGHDRTrack's value-getter family: all 15 real
 * weak-symbol Getter*(CSTGHDRTrackMessageContext&) candidates
 * reconstructed, zero outliers.
 *
 * Discovered via the broader-discovery-method sweep (see
 * stg_value_getter_family.md's batch 18 recipe, re-run fresh this batch
 * -- the whole-binary sValueGetterTemp relocation cross-reference stayed
 * exhausted at its own 75-class count, but a follow-up sweep for the
 * OTHER sibling *MessageContext types the family uses beyond
 * CSTGPatchMessageContext/CSTGMessageContext turned this class up).
 *
 * CSTGHDRTrack was previously only known as an OPAQUE, raw-offset-only
 * embedded sub-object (16 instances at a confirmed 0x2c-byte stride
 * inside CSTGSequence, see oa_global.h's own CSTGSequence header
 * comment) -- no standalone struct existed anywhere in this project
 * before this batch. Genuinely fresh class.
 *
 * Field layout cross-checks cleanly against CSTGSequence's OWN
 * already-confirmed ctor: it zeros exactly 3 bytes per HDRTrack slot at
 * +0x4/+0x5/+0x6 -- this batch's own field discovery independently
 * lands OutputBus/FXCtrlBus/HDRBus (all signed bytes) at those SAME
 * three offsets, an independent cross-check, not a coincidence.
 *
 * Dialect: mostly the family's simplest -- fixed-K fields directly off
 * `this`, offsets 0x4..0x2a, EQBypass/Mute packed as two independent
 * single-bit booleans into byte 0x2b (bit 0/bit 1, the by-now-usual
 * shift-then-mask bitfield shape). One genuinely NEW combined shape:
 *
 *   GetValueSolo reads NEITHER `this` NOR the usual per-call ctx.index
 *     field -- it resolves the real, already-declared global singleton
 *     `CSTGControllerRTData::sInstance` (oa_global.h) and reads a WORD
 *     at that object's own +0x24, then shifts it right by a per-call
 *     bit-index read from `ctx`'s own +0x18 byte (masked to 5 bits,
 *     matching x86's own shift-count masking), `& 1`. This combines two
 *     shapes the family has each seen separately before -- a
 *     global-singleton indirection ignoring `this` entirely (first seen
 *     on CSTGEffectBalance, batch 18) and a per-call ctx-derived
 *     variable bit-shift amount (first seen on CSTGVPMModelPatch's own
 *     CtxShift shape) -- into one method, and is the first confirmed
 *     case of EITHER shape reading its shifted data from a REAL,
 *     already-declared external singleton rather than reimplementing a
 *     private raw selector lookup or reading a fixed field on `this`.
 */

struct CSTGHDRTrackMessageContext {
	unsigned char _unrecovered_head[4];	/* +0x00..+0x03, unconfirmed */
	unsigned int index;			/* +0x04, unconfirmed for this
						 * class -- kept only for
						 * structural consistency with
						 * every sibling context type;
						 * none of CSTGHDRTrack's own
						 * candidates read it */
	unsigned char _unrecovered_mid[0x10];	/* +0x08..+0x17, unconfirmed */
	unsigned char soloBitIndex;		/* +0x18, confirmed -- see
						 * GetValueSolo above */
};

struct CSTGHDRTrack {
	STGConvertedParam &GetValueEQBypass(CSTGHDRTrackMessageContext &ctx);
	STGConvertedParam &GetValueEQHigh(CSTGHDRTrackMessageContext &ctx);
	STGConvertedParam &GetValueEQInputTrim(CSTGHDRTrackMessageContext &ctx);
	STGConvertedParam &GetValueEQLow(CSTGHDRTrackMessageContext &ctx);
	STGConvertedParam &GetValueEQMid(CSTGHDRTrackMessageContext &ctx);
	STGConvertedParam &GetValueEQMidFreq(CSTGHDRTrackMessageContext &ctx);
	STGConvertedParam &GetValueFXCtrlBus(CSTGHDRTrackMessageContext &ctx);
	STGConvertedParam &GetValueHDRBus(CSTGHDRTrackMessageContext &ctx);
	STGConvertedParam &GetValueLevel(CSTGHDRTrackMessageContext &ctx);
	STGConvertedParam &GetValueMute(CSTGHDRTrackMessageContext &ctx);
	STGConvertedParam &GetValueOutputBus(CSTGHDRTrackMessageContext &ctx);
	STGConvertedParam &GetValuePan(CSTGHDRTrackMessageContext &ctx);
	STGConvertedParam &GetValueSend1Level(CSTGHDRTrackMessageContext &ctx);
	STGConvertedParam &GetValueSend2Level(CSTGHDRTrackMessageContext &ctx);
	STGConvertedParam &GetValueSolo(CSTGHDRTrackMessageContext &ctx);
};

#endif
