/*
 * kontakt_parameter_family.h  -  the first batch of concrete
 * "CKontaktXxxParameter" leaf classes built on kontakt_parameter_base.h,
 * 2026-07-28. Direct follow-up to the standing lead in kontakt_xml.h's file
 * header ("CKontaktGroupParameter dispatches through CKontaktXml's value
 * parsers, out of scope that pass").
 *
 * METHOD: each concrete class's ctor passes a literal `const char **list` to
 * its base class (see kontakt_parameter_base.h) -- that list's own .data
 * bytes are the class's REAL XML child-element "name=" values, dumped
 * directly (objdump -s) rather than guessed. Each class's own AddParameter/
 * AddIndexedParameter/AddDynamicParameter override is a plain switch on the
 * matched list index; the real jump table for each was read directly out of
 * .rodata (not decompiled/guessed) and cross-referenced against the value-
 * list strings positionally, e.g. CKontaktGroupParameter's list[11] ==
 * "outRouting_" and its jump table's slot 11 really is the one special case
 * that calls out to CKontaktGroup::SetOutputRouting() instead of a plain
 * field store -- confirmed alignment, not assumed.
 *
 * 10 classes this pass (all confirmed self-contained -- zero calls to any
 * class outside {CKontaktXml, libc, this class's own tiny owner-struct
 * setters below} via a full per-function objdump -dr call-target sweep):
 *   CKontaktGroupParameter    (CKontaktIndexedParameter family -- the named
 *                              lead target; 32-entry list, 1 special case)
 *   CKontaktZoneParameter     (CKontaktParameter family; 33-entry list, only
 *                              indices 0-15 dispatched -- see below)
 *   CKontaktEffectParameter, CKontaktFilterParameter, CKontaktOutputParameter,
 *   CKontaktLfoParameter, CKontaktLoopParameter, CKontaktEnvelopeParameter,
 *   CKontaktPlaybackModeParameter, CKontaktStartCriteriaParameter
 *     (all CKontaktParameter family, small closed lists, no owner-setter
 *     dependency at all -- every field is a direct struct write)
 *   CKontaktScriptParameter   (CKontaktDynamicParameter family -- the 3rd
 *                              dispatch shape, text-suffix based)
 *
 * DELIBERATELY NOT in this pass (each needs the still-deferred
 * CKontaktXml::UnpackPath, or a materially bigger owner class):
 * CKontaktSampleParameter, CKontaktProgramParameter,
 * CKontaktContainerParameter. Also not in this pass: CKontaktBankParameter,
 * CKontaktOutputsParameter, CKontaktSendLevelsParameter,
 * CKontaktIntModulatorParameter, CKontaktExtModulatorParameter,
 * CKontaktVoiceGroupParameter, CKontaktTargetParameter -- each is equally
 * self-contained and mechanical (confirmed via the same call-target sweep)
 * but needs 1-5 additional tiny owner-class setters not yet reconstructed
 * (CKontaktBank::SetSlotMute/SetSlotSolo/SetSlotMidiChannel/
 * SetSlotReorderIndex/SetSlotAuxSendLevel, CKontaktOutputs::
 * SetPhysicalOutputMapping, CKontaktSendLevels::SetLevel, and 4 plain
 * CKontaktXxx::SetName(const char*) strncpy setters) -- real, traced,
 * flagged as a clean, low-risk follow-up batch, just deferred for scope.
 *
 * OWNER STRUCTS: every concrete class stores a raw pointer to its owning
 * "container" object (CKontaktGroup*, CKontaktZone*, ...) at offset +0x10
 * (confirmed identical across every ctor checked -- base class's own 16
 * bytes end at +0x10, this new field is the first byte the concrete
 * subclass's own ctor adds). That owner class is itself a much larger,
 * still-100%-unreconstructed real class (CKontaktGroup/CKontaktZone/... are
 * the "container" family with their own AddObject/AddAttribute overrides,
 * out of scope -- see kontakt_xml.h's file header). Rather than model any of
 * them as a real C++ class (which would need their real ctor/dtor/vtable,
 * none of which this pass touches), each is declared below as a minimal,
 * NEVER-constructed-by-this-code raw-offset struct: an opaque
 * `_unknown_prefix` byte blob covering every byte before the first field
 * this pass's own field-write traces confirmed, then named fields at their
 * real confirmed offsets (small internal gaps between confirmed fields are
 * explicit `_pad` blobs, not guessed at). Same discipline as
 * storage_converter_ext_stubs.h / prog_converter.h's own raw-offset members.
 * These structs are ONLY ever accessed through an existing pointer (this
 * class's own mOwner) -- never sizeof()'d, newed, or destroyed by any code
 * in this pass, so their real total size being larger than declared here is
 * expected and harmless.
 */

#ifndef KONTAKT_PARAMETER_FAMILY_H
#define KONTAKT_PARAMETER_FAMILY_H

#include "kontakt_parameter_base.h"

/* ===================== owner "container" raw-offset structs ============= */

/* CKontaktGroup -- fields confirmed via CKontaktGroupParameter's own 32-case
 * jump table (offsets 0x30-0xcc) plus CKontaktGroup::SetOutputRouting's own
 * real body (.text+0x089bda60: `this[outputIndex*4 + 0x50] = value`, a
 * 16-entry array inferred from the 0x50..0x8f gap between voiceGroup and
 * fxIdxAmpSplitPoint -- 16 x 4 bytes = 0x40 exactly fills it). `interpQuality`
 * (+0xa4) is a StringIndex() result against a list this build's own .data
 * contains exactly ONE real entry for ("normal") -- confirmed via direct
 * .data dump (not a truncation artifact), any other interpQuality string is
 * silently rejected/ignored in this ground truth. */
class CKontaktGroup {
public:
	unsigned char _unknown_prefix[0x30]; /* vptr, CKontaktXml state, name, and other still-unmodeled fields */

	float volume;                              /* +0x30 */
	float pan;                                 /* +0x34 */
	int tune;                                  /* +0x38 */
	bool keyTracking;                          /* +0x3c */
	bool reverse;                              /* +0x3d */
	bool releaseTrigger;                       /* +0x3e */
	bool releaseTriggerNoteMonophonic;         /* +0x3f */
	unsigned int rlsTrigCounter;                /* +0x40 */
	int outputMask;                            /* +0x44 */
	unsigned int midiChannel;                   /* +0x48 */
	int voiceGroup;                            /* +0x4c */
	unsigned int outRouting[16];                /* +0x50..+0x8f, see SetOutputRouting() */
	unsigned int fxIdxAmpSplitPoint;            /* +0x90 */
	bool selectedForEdit;                      /* +0x94 */
	bool selectedInGroupEditor;                /* +0x95 */
	bool zonesVisibleInMapListView;            /* +0x96 */
	unsigned char _pad_97[1];
	unsigned int batteryCellColor;              /* +0x98 */
	bool fxBeingEdited;                        /* +0x9c */
	unsigned char _pad_9d[3];
	unsigned int idxFXBeingEdited;              /* +0xa0 */
	int interpQuality;                         /* +0xa4, see file header */
	bool muted;                                /* +0xa8 */
	bool soloed;                               /* +0xa9 */
	bool groupPurged;                          /* +0xaa */
	unsigned char _pad_ab[1];
	unsigned int batteryLowKey;                 /* +0xac */
	unsigned int batteryHighKey;                /* +0xb0 */
	int grooveBoxID;                           /* +0xb4 */
	unsigned int lowVelocity;                   /* +0xb8 */
	unsigned int highVelocity;                  /* +0xbc */
	unsigned int fadeLowVelo;                   /* +0xc0 */
	unsigned int fadeHighVelo;                  /* +0xc4 */
	unsigned int fadeLowKey;                    /* +0xc8 */
	unsigned int fadeHighKey;                   /* +0xcc */

	/* .text+0x089bda60. `outputIndex` is (suffix-1) from the "outRouting_N"
	 * name (see CKontaktGroupParameter::AddIndexedParameter's own case 11
	 * below) -- real body does no bounds check at all, reproduced as-is. */
	void SetOutputRouting(unsigned int outputIndex, unsigned int value);
};

/* CKontaktZone -- fields confirmed via CKontaktZoneParameter's own jump
 * table. IMPORTANT, confirmed real finding: the guard is `cmp eax,0xf; ja
 * <fallback>` -- only list indices 0-15 are dispatched at all, even though
 * the real attribute-name list this class's ctor passes has 33 entries.
 * Indices 7-10 (fadeLowVelo/fadeHighVelo/fadeLowKey/fadeHighKey) sit WITHIN
 * the dispatched 0-15 range yet their own jump table slots point straight at
 * the shared fallback/no-op path (confirmed via direct .rodata jump-table
 * dump, not a disassembly-parsing artifact) -- i.e. those 4 names are
 * recognized (case-insensitively matched) but their values are silently
 * discarded in this ground truth. Indices 16-32 (fixedUnitNum, gridMode's
 * neighbours, grooveBoxID, ...) are entirely unreachable through this
 * dispatch (the `ja` guard rejects them before the jump table is even
 * consulted) -- their real storage, if any, is undetermined this pass. */
class CKontaktZone {
public:
	unsigned char _unknown_prefix[0x14];

	unsigned int sampleStart;             /* +0x14 */
	unsigned int sampleEnd;               /* +0x18 */
	unsigned int sampleStartModRange;     /* +0x1c */
	unsigned int lowVelocity;             /* +0x20 */
	unsigned int highVelocity;            /* +0x24 */
	unsigned int lowKey;                  /* +0x28 */
	unsigned int highKey;                 /* +0x2c */
	/* +0x30..+0x3f: not written by any dispatched case (includes the 4
	 * "recognized but discarded" fade fields noted above); undetermined. */
	unsigned char _gap_30[0x10];
	unsigned int rootKey;                 /* +0x40 */
	float zoneVolume;                     /* +0x44 */
	float zonePan;                        /* +0x48 */
	float zoneTune;                       /* +0x4c */
	int gridMode;                         /* +0x50, StringIndex() result -- this
	                                        * build's list has exactly ONE real
	                                        * entry, "grid_mode_none" (confirmed
	                                        * via direct .data dump, same
	                                        * situation as CKontaktGroup's
	                                        * interpQuality above). */
};

/* CKontaktEffect -- all 6 fields confirmed, small closed jump table. */
class CKontaktEffect {
public:
	unsigned char _unknown_prefix[0x10];

	bool routersOpen;                     /* +0x10 */
	unsigned char _pad_11[3];
	unsigned int classID;                  /* +0x14 */
	bool bypass;                          /* +0x18 */
	unsigned char _pad_19[3];
	float outLevel;                       /* +0x1c */
	float outLevelDry;                    /* +0x20 */
	int sendFXOutPartition;               /* +0x24 */
};

/* CKontaktFilter -- 19 fields, EVERY case is a plain float store (confirmed
 * by direct disassembly -- even the "typeA/typeB/typeC"/"bypA/bypB/bypC"
 * fields, which read as booleans/enums by name, are genuinely floats in
 * ground truth; not a misread, the real code path is identical `FloatValue()
 * -> fstp` for all 19). Perfectly contiguous, no gaps. */
class CKontaktFilter {
public:
	unsigned char _unknown_prefix[0x10];

	float cutoff;         /* +0x10 */
	float resonance;      /* +0x14 */
	float shiftB;         /* +0x18 */
	float shiftC;         /* +0x1c */
	float resB;           /* +0x20 */
	float resC;           /* +0x24 */
	float typeA;          /* +0x28 */
	float typeB;          /* +0x2c */
	float typeC;          /* +0x30 */
	float bypA;           /* +0x34 */
	float bypB;           /* +0x38 */
	float bypC;           /* +0x3c */
	float gain;           /* +0x40 */
	float freq_1;         /* +0x44 */
	float bandwidth_1;    /* +0x48 */
	float gain_1;         /* +0x4c */
	float freq_2;         /* +0x50 */
	float bandwidth_2;    /* +0x54 */
	float gain_2;         /* +0x58 */
};

/* CKontaktOutput -- no jump table (only 3 cases, GCC used a plain compare
 * chain instead), all 3 fields confirmed. */
class CKontaktOutput {
public:
	unsigned char _unknown_prefix[0x30];

	unsigned int numChannels;  /* +0x30 */
	unsigned int auxIdx;       /* +0x34 */
	float volume;              /* +0x38 */
};

/* CKontaktLfo -- 5 fields, contiguous. */
class CKontaktLfo {
public:
	unsigned char _unknown_prefix[0x10];

	float frequency;           /* +0x10 */
	float pulseWidth;          /* +0x14 */
	float startPhase;          /* +0x18 */
	float delay;               /* +0x1c */
	bool normalizeMultiLFO;    /* +0x20 */
};

/* CKontaktLoop -- 7 fields. "mode" is a StringIndex() enum result against a
 * real, complete 3-entry list ("oneshot"/"until_release"/"until_end"). */
class CKontaktLoop {
public:
	unsigned char _unknown_prefix[0x10];

	unsigned int loopStart;      /* +0x10 */
	unsigned int loopLength;     /* +0x14 */
	unsigned int loopCount;      /* +0x18 */
	int mode;                    /* +0x1c, StringIndex() result, see above */
	bool alternatingLoop;        /* +0x20 */
	unsigned char _pad_21[3];
	float loopTuning;            /* +0x24 */
	float xfadeLength;           /* +0x28 */
};

/* CKontaktEnvelope -- 10 fields. Confirmed real quirk: "attack" (+0x14) is
 * parsed via SignedValue() (sscanf "%d", an integer) and then converted with
 * a real `fild`/`fstp` pair before being stored -- i.e. the XML value is
 * integer text but the field itself is a float, unlike every sibling float
 * field here which is parsed directly via FloatValue(). Reproduced as-is
 * (int-parse then convert-to-float), not simplified to a plain float parse. */
class CKontaktEnvelope {
public:
	unsigned char _unknown_prefix[0x10];

	float atkCurving;      /* +0x10 */
	float attack;          /* +0x14, see note above: parsed as int, stored as float */
	float decay;           /* +0x18 */
	float hold;            /* +0x1c */
	float release;         /* +0x20 */
	float sustain;         /* +0x24 */
	bool noteOffLessMode;  /* +0x28 */
	unsigned char _pad_29[3];
	float decay1;          /* +0x2c */
	float breakLevel;      /* +0x30, real XML name is "break" (C++ keyword, renamed) */
	float decay2;          /* +0x34 */
};

/* CKontaktPlaybackMode -- 10 fields. "type" is a StringIndex() enum result
 * against a real, complete 3-entry list ("streaming"/"time_machine"/
 * "tone_machine"). */
class CKontaktPlaybackMode {
public:
	unsigned char _unknown_prefix[0xc];

	int type;                  /* +0xc, StringIndex() result, see above */
	float speed;                /* +0x10 */
	bool legato;                /* +0x14 */
	bool zoneLockedSpeed;       /* +0x15 */
	unsigned char _pad_16[2];
	unsigned int sp1200Filter;   /* +0x18 */
	float smooth;                /* +0x1c */
	float grainLength;           /* +0x20 */
	bool hiQuality;              /* +0x24 */
	unsigned char _pad_25[3];
	float formantShift;          /* +0x28 */
	bool dcFilter;                /* +0x2c */
};

/* CKontaktStartCriteria -- 6 fields. "mode" is a StringIndex() enum result
 * against a real, complete 2-entry list ("cycle_round_robin"/"on_controller");
 * "nextCriteria" against a separate real 2-entry list ("and"/"and_not"). */
class CKontaktStartCriteria {
public:
	unsigned char _unknown_prefix[0x10];

	int mode;                   /* +0x10, see above */
	unsigned int cycleClass;     /* +0x14 */
	int nextCriteria;           /* +0x18, see above */
	unsigned int controller;     /* +0x1c */
	unsigned int cc_min;         /* +0x20 */
	unsigned int cc_max;         /* +0x24 */
};

/* CKontaktScript -- 3 owner setters confirmed (SetDescription/SetPassword/
 * SetSourceText), all self-contained (strncpy/new[]/delete/memcpy/strlen,
 * no calls into any other still-unmodeled class). Confirmed real allocator
 * MISMATCH in SetSourceText: the buffer is allocated with `operator new[]`
 * but freed with plain scalar `operator delete` (not `operator delete[]`) --
 * verified via direct disassembly (call targets `_Znaj` vs `_ZdlPv`), not
 * a transcription slip; reproduced exactly since this project's convention
 * is real-binary-behavior over "corrected" code. Also confirmed: the
 * allocated buffer is `strlen(text)+1` bytes but memcpy only copies
 * `strlen(text)` bytes -- the reserved NUL-terminator byte is left
 * uninitialized by the real code (no defensive `buf[len]=0` write exists in
 * the disassembly); reproduced as-is. */
class CKontaktScript {
public:
	unsigned char _unknown_prefix[0x10];

	char *sourceText;          /* +0x10, heap-owned, see note above */
	bool sourceEditorOpen;     /* +0x14 */
	bool touchedButNotApplied; /* +0x15 */
	bool bypass;               /* +0x16 */
	char description[0x40];    /* +0x17, strncpy'd, not guaranteed NUL-terminated */
	char password[0x20];       /* +0x57, strncpy'd, not guaranteed NUL-terminated */

	/* .text+0x089c32b0. NULL text -> description[0]=0; else strncpy(description, text, 0x40). */
	void SetDescription(const char *text);
	/* .text+0x089c32f0. Same idea, password[0x20]. */
	void SetPassword(const char *text);
	/* .text+0x089c3240. See class header note above for the allocator/NUL quirks. */
	void SetSourceText(const char *text);
};

/* ============================ concrete Parameter classes ================= */

/* .text+0x089bdd80 (AddIndexedParameter) / +0x089be160 (ctor) /
 * +0x089be110,+0x089be130 (dtor D1/D0). List: 32 entries @0x91fbe00 (dumped
 * verbatim in kontakt_parameter_family.cpp). Every case is a direct
 * CKontaktXml value-parser call funneled straight into a CKontaktGroup
 * field write EXCEPT index 11 ("outRouting_") which instead calls
 * CKontaktGroup::SetOutputRouting(suffix-1, value) -- confirmed via a real
 * tail-jmp in the disassembly, not a plain field store. */
class CKontaktGroupParameter : public CKontaktIndexedParameter {
public:
	CKontaktGroupParameter(CKontaktGroup *owner);
	virtual void AddIndexedParameter(unsigned int index, unsigned int suffix, const unsigned char *value);

protected:
	CKontaktGroup *mOwner;
};

/* .text+0x089c9230 (AddParameter) / +0x089c93a0 (ctor) / +0x089c9350,
 * +0x089c9370 (dtor). List: 33 entries @0x91fc460, but only indices 0-15 are
 * actually dispatched -- see CKontaktZone's own class header above. */
class CKontaktZoneParameter : public CKontaktParameter {
public:
	CKontaktZoneParameter(CKontaktZone *owner);
	virtual void AddParameter(unsigned int index, const unsigned char *value);

protected:
	CKontaktZone *mOwner;
};

/* .text+0x089bc2c0 / +0x089bc3e0 / +0x089bc390,+0x089bc3b0. List: 6 entries @0x91fbb68. */
class CKontaktEffectParameter : public CKontaktParameter {
public:
	CKontaktEffectParameter(CKontaktEffect *owner);
	virtual void AddParameter(unsigned int index, const unsigned char *value);

protected:
	CKontaktEffect *mOwner;
};

/* .text+0x089bd100 / +0x089bd320 / +0x089bd2d0,+0x089bd2f0. List: 19 entries @0x91fbd20. */
class CKontaktFilterParameter : public CKontaktParameter {
public:
	CKontaktFilterParameter(CKontaktFilter *owner);
	virtual void AddParameter(unsigned int index, const unsigned char *value);

protected:
	CKontaktFilter *mOwner;
};

/* .text+0x089c0090 / +0x089c0160 / +0x089c0110,+0x089c0130. List: 3 entries @0x91fbfe4. */
class CKontaktOutputParameter : public CKontaktParameter {
public:
	CKontaktOutputParameter(CKontaktOutput *owner);
	virtual void AddParameter(unsigned int index, const unsigned char *value);

protected:
	CKontaktOutput *mOwner;
};

/* .text+0x089bf370 / +0x089bf470 / +0x089bf420,+0x089bf440. List: 5 entries @0x91fbf3c. */
class CKontaktLfoParameter : public CKontaktParameter {
public:
	CKontaktLfoParameter(CKontaktLfo *owner);
	virtual void AddParameter(unsigned int index, const unsigned char *value);

protected:
	CKontaktLfo *mOwner;
};

/* .text+0x089bf7f0 / +0x089bf930 / +0x089bf8e0,+0x089bf900. List: 7 entries @0x91fbfa0. */
class CKontaktLoopParameter : public CKontaktParameter {
public:
	CKontaktLoopParameter(CKontaktLoop *owner);
	virtual void AddParameter(unsigned int index, const unsigned char *value);

protected:
	CKontaktLoop *mOwner;
};

/* .text+0x089bc600 / +0x089bc750 / +0x089bc700,+0x089bc720. List: 10 entries @0x91fbbc0. */
class CKontaktEnvelopeParameter : public CKontaktParameter {
public:
	CKontaktEnvelopeParameter(CKontaktEnvelope *owner);
	virtual void AddParameter(unsigned int index, const unsigned char *value);

protected:
	CKontaktEnvelope *mOwner;
};

/* .text+0x089c0f60 / +0x089c10c0 / +0x089c1070,+0x089c1090. List: 10 entries @0x91fc060. */
class CKontaktPlaybackModeParameter : public CKontaktParameter {
public:
	CKontaktPlaybackModeParameter(CKontaktPlaybackMode *owner);
	virtual void AddParameter(unsigned int index, const unsigned char *value);

protected:
	CKontaktPlaybackMode *mOwner;
};

/* .text+0x089c3fd0 / +0x089c4100 / +0x089c40b0,+0x089c40d0. List: 6 entries @0x91fc2f4. */
class CKontaktStartCriteriaParameter : public CKontaktParameter {
public:
	CKontaktStartCriteriaParameter(CKontaktStartCriteria *owner);
	virtual void AddParameter(unsigned int index, const unsigned char *value);

protected:
	CKontaktStartCriteria *mOwner;
};

/* .text+0x089c3330 (AddDynamicParameter) / +0x089c3450 (ctor) / +0x089c3400,
 * +0x089c3420 (dtor). List: 7 entries @0x91fc280. Case 6 ("persistent_var_",
 * a TagN-shaped name expecting a numeric/text suffix per the file header's
 * dispatch-family note) is confirmed UNHANDLED -- its own jump-table slot
 * lands inside another case's shared epilogue rather than any real handler
 * code, i.e. this Kontakt build silently drops persistent script variables
 * through this path. Reproduced as a genuine no-op case, not omitted. */
class CKontaktScriptParameter : public CKontaktDynamicParameter {
public:
	CKontaktScriptParameter(CKontaktScript *owner);
	virtual void AddDynamicParameter(unsigned int index, const char *suffix, const unsigned char *value);

protected:
	CKontaktScript *mOwner;
};

#endif /* KONTAKT_PARAMETER_FAMILY_H */
