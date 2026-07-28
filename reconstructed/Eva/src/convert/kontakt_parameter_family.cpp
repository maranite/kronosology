/*
 * kontakt_parameter_family.cpp  -  see include/kontakt_parameter_family.h for
 * the full derivation. Every attribute-name list and jump-table mapping below
 * was read directly out of the real binary's .rodata (objdump -s / -dr), not
 * inferred from behavior.
 */

#include "kontakt_parameter_family.h"

#include <cstring>

/* ================================ owner setters =========================== */

void CKontaktGroup::SetOutputRouting(unsigned int outputIndex, unsigned int value)
{
	outRouting[outputIndex] = value;
}

void CKontaktContainer::SetOriginalSubDirectory(const char *text)
{
	if (text) {
		strncpy(origSubDir, text, sizeof(origSubDir));
	} else {
		origSubDir[0] = 0;
	}
}

void CKontaktSample::SetFile(const char *text)
{
	strncpy(file_ex2, text, sizeof(file_ex2));
}

void CKontaktSample::SetFilePbn(const char *text)
{
	strncpy(file_pbn, text, sizeof(file_pbn));
}

void CKontaktOutputs::SetPhysicalOutputMapping(unsigned int outputIndex, unsigned int value)
{
	physicalOutputMapping[outputIndex] = value; /* real: no bounds check at all, reproduced as-is */
}

void CKontaktScript::SetDescription(const char *text)
{
	if (text)
		strncpy(description, text, sizeof(description));
	else
		description[0] = 0;
}

void CKontaktScript::SetPassword(const char *text)
{
	if (text)
		strncpy(password, text, sizeof(password));
	else
		password[0] = 0;
}

void CKontaktScript::SetSourceText(const char *text)
{
	if (sourceText) {
		delete sourceText; /* real: scalar `operator delete`, not delete[] -- see header note */
		sourceText = 0;
	}
	if (text) {
		unsigned int len = (unsigned int)strlen(text);
		sourceText = new char[len + 1];
		memcpy(sourceText, text, len); /* real: does NOT write sourceText[len] -- see header note */
	}
}

void CKontaktProgram::SetWallpaperFile(const char *text)
{
	if (wallpaperFile) {
		delete wallpaperFile; /* real: scalar `operator delete`, not delete[] -- see header note */
		wallpaperFile = 0;
	}
	if (text) {
		unsigned int len = (unsigned int)strlen(text);
		wallpaperFile = new char[len];
		memcpy(wallpaperFile, text, len); /* real: does NOT write a trailing NUL -- see header note */
	}
}

void CKontaktBank::SetSlotMidiChannel(unsigned int slotIndex, unsigned int value)
{
	*(unsigned int *)((unsigned char *)this + slotIndex * 0x20 + 0x3c) = value;
}

void CKontaktBank::SetSlotSolo(unsigned int slotIndex, bool value)
{
	*((unsigned char *)this + slotIndex * 0x20 + 0x40) = value;
}

void CKontaktBank::SetSlotMute(unsigned int slotIndex, bool value)
{
	*((unsigned char *)this + slotIndex * 0x20 + 0x41) = value;
}

void CKontaktBank::SetSlotAuxSendLevel(unsigned int slotIndex, unsigned int auxIndex, float value)
{
	*(float *)((unsigned char *)this + slotIndex * 0x20 + auxIndex * 4 + 0x44) = value;
}

void CKontaktBank::SetSlotReorderIndex(unsigned int slotIndex, unsigned int value)
{
	*(unsigned int *)((unsigned char *)this + slotIndex * 0x20 + 0x54) = value;
}

void CKontaktSendLevels::SetLevel(unsigned int index, float value)
{
	if (index <= 7) /* real bounds check, unlike most other owner setters in this file */
		level[index] = value;
}

void CKontaktIntModulator::SetName(const char *text)
{
	strncpy(name, text, sizeof(name));
}

void CKontaktExtModulator::SetName(const char *text)
{
	strncpy(name, text, sizeof(name));
}

void CKontaktVoiceGroup::SetName(const char *text)
{
	strncpy(name, text, sizeof(name));
}

void CKontaktTarget::SetName(const char *text)
{
	strncpy(name, text, sizeof(name));
}

/* ================================ CKontaktGroupParameter =================== */

static const char *kGroupParamList[] = {
	"volume", "pan", "tune", "keyTracking", "reverse", "releaseTrigger",
	"releaseTriggerNoteMonophonic", "rlsTrigCounter", "outputMask", "midiChannel",
	"voiceGroup", "outRouting_", "fxIdxAmpSplitPoint", "selectedForEdit",
	"selectedInGroupEditor", "zonesVisibleInMapListView", "batteryCellColor",
	"fxBeingEdited", "idxFXBeingEdited", "interpQuality", "muted", "soloed",
	"groupPurged", "batteryLowKey", "batteryHighKey", "grooveBoxID",
	"lowVelocity", "highVelocity", "fadeLowVelo", "fadeHighVelo",
	"fadeLowKey", "fadeHighKey", 0
};

static const char *kGroupInterpQualityList[] = { "normal", 0 };

CKontaktGroupParameter::CKontaktGroupParameter(CKontaktGroup *owner)
	: CKontaktIndexedParameter(kGroupParamList)
	, mOwner(owner)
{
}

void CKontaktGroupParameter::AddIndexedParameter(unsigned int index, unsigned int suffix, const unsigned char *value)
{
	switch (index) {
	case 0:  mOwner->volume = CKontaktXml::FloatValue(value); break;
	case 1:  mOwner->pan = CKontaktXml::FloatValue(value); break;
	case 2:  mOwner->tune = CKontaktXml::SignedValue(value); break;
	case 3:  mOwner->keyTracking = CKontaktXml::BooleanValue(value); break;
	case 4:  mOwner->reverse = CKontaktXml::BooleanValue(value); break;
	case 5:  mOwner->releaseTrigger = CKontaktXml::BooleanValue(value); break;
	case 6:  mOwner->releaseTriggerNoteMonophonic = CKontaktXml::BooleanValue(value); break;
	case 7:  mOwner->rlsTrigCounter = CKontaktXml::UnsignedValue(value); break;
	case 8:  mOwner->outputMask = CKontaktXml::SignedValue(value); break;
	case 9:  mOwner->midiChannel = CKontaktXml::UnsignedValue(value); break;
	case 10: mOwner->voiceGroup = CKontaktXml::SignedValue(value); break;
	case 11: mOwner->SetOutputRouting(suffix - 1, CKontaktXml::UnsignedValue(value)); break;
	case 12: mOwner->fxIdxAmpSplitPoint = CKontaktXml::UnsignedValue(value); break;
	case 13: mOwner->selectedForEdit = CKontaktXml::BooleanValue(value); break;
	case 14: mOwner->selectedInGroupEditor = CKontaktXml::BooleanValue(value); break;
	case 15: mOwner->zonesVisibleInMapListView = CKontaktXml::BooleanValue(value); break;
	case 16: mOwner->batteryCellColor = CKontaktXml::UnsignedValue(value); break;
	case 17: mOwner->fxBeingEdited = CKontaktXml::BooleanValue(value); break;
	case 18: mOwner->idxFXBeingEdited = CKontaktXml::UnsignedValue(value); break;
	case 19: {
		int q = CKontaktXml::StringIndex(kGroupInterpQualityList, value);
		if (q >= 0)
			mOwner->interpQuality = q;
		break;
	}
	case 20: mOwner->muted = CKontaktXml::BooleanValue(value); break;
	case 21: mOwner->soloed = CKontaktXml::BooleanValue(value); break;
	case 22: mOwner->groupPurged = CKontaktXml::BooleanValue(value); break;
	case 23: mOwner->batteryLowKey = CKontaktXml::UnsignedValue(value); break;
	case 24: mOwner->batteryHighKey = CKontaktXml::UnsignedValue(value); break;
	case 25: mOwner->grooveBoxID = CKontaktXml::SignedValue(value); break;
	case 26: mOwner->lowVelocity = CKontaktXml::UnsignedValue(value); break;
	case 27: mOwner->highVelocity = CKontaktXml::UnsignedValue(value); break;
	case 28: mOwner->fadeLowVelo = CKontaktXml::UnsignedValue(value); break;
	case 29: mOwner->fadeHighVelo = CKontaktXml::UnsignedValue(value); break;
	case 30: mOwner->fadeLowKey = CKontaktXml::UnsignedValue(value); break;
	case 31: mOwner->fadeHighKey = CKontaktXml::UnsignedValue(value); break;
	default: CKontaktIndexedParameter::AddIndexedParameter(index, suffix, value); break;
	}
}

/* ================================ CKontaktOutputsParameter =============== */

static const char *kOutputsParamList[] = { "physOutMapping_", 0 };

CKontaktOutputsParameter::CKontaktOutputsParameter(CKontaktOutputs *owner)
	: CKontaktIndexedParameter(kOutputsParamList)
	, mOwner(owner)
{
}

void CKontaktOutputsParameter::AddIndexedParameter(unsigned int index, unsigned int suffix, const unsigned char *value)
{
	switch (index) {
	case 0: mOwner->SetPhysicalOutputMapping(suffix, CKontaktXml::UnsignedValue(value)); break;
	default: CKontaktIndexedParameter::AddIndexedParameter(index, suffix, value); break;
	}
}

/* ================================ CKontaktZoneParameter ===================== */

static const char *kZoneParamList[] = {
	"sampleStart", "sampleEnd", "sampleStartModRange", "lowVelocity", "highVelocity",
	"lowKey", "highKey", "fadeLowVelo", "fadeHighVelo", "fadeLowKey", "fadeHighKey",
	"rootKey", "zoneVolume", "zonePan", "zoneTune", "gridMode", "fixedUnitNum",
	"fixedUnitDenom", "measureNum", "measureDenom", "tempoScaling", "slicerSens",
	"isReferenceZone", "selected", "zoomScaleX", "zoomScaleY", "selectedLoopIdx",
	"zleIdxDisplayed", "batteryMapPos", "batteryMapPosGrabStart", "curEditorTab",
	"morphGroupIdx", "grooveBoxID", 0
};

static const char *kZoneGridModeList[] = { "grid_mode_none", 0 };

CKontaktZoneParameter::CKontaktZoneParameter(CKontaktZone *owner)
	: CKontaktParameter(kZoneParamList)
	, mOwner(owner)
{
}

void CKontaktZoneParameter::AddParameter(unsigned int index, const unsigned char *value)
{
	switch (index) {
	case 0: mOwner->sampleStart = CKontaktXml::UnsignedValue(value); break;
	case 1: mOwner->sampleEnd = CKontaktXml::UnsignedValue(value); break;
	case 2: mOwner->sampleStartModRange = CKontaktXml::UnsignedValue(value); break;
	case 3: mOwner->lowVelocity = CKontaktXml::UnsignedValue(value); break;
	case 4: mOwner->highVelocity = CKontaktXml::UnsignedValue(value); break;
	case 5: mOwner->lowKey = CKontaktXml::UnsignedValue(value); break;
	case 6: mOwner->highKey = CKontaktXml::UnsignedValue(value); break;
	/* 7-10 (fadeLowVelo/fadeHighVelo/fadeLowKey/fadeHighKey): confirmed
	 * real no-op -- see class header in kontakt_parameter_family.h. */
	case 7: case 8: case 9: case 10: break;
	case 11: mOwner->rootKey = CKontaktXml::UnsignedValue(value); break;
	case 12: mOwner->zoneVolume = CKontaktXml::FloatValue(value); break;
	case 13: mOwner->zonePan = CKontaktXml::FloatValue(value); break;
	case 14: mOwner->zoneTune = CKontaktXml::FloatValue(value); break;
	case 15: {
		int m = CKontaktXml::StringIndex(kZoneGridModeList, value);
		if (m >= 0)
			mOwner->gridMode = m;
		break;
	}
	default: CKontaktParameter::AddParameter(index, value); break;
	}
}

/* ================================ CKontaktContainerParameter ============= */

static const char *kContainerParamList[] = {
	"loadPurged", "tableOpen", "smallRackUnit", "auxSendsVisible", "libraryID",
	"loadingFlags", "curProgramChangeNum", "outputMask", "volume", "pan",
	"origSaveMode", "origAbsolutePaths", "origCompressedSamples", "origSubDir",
	"hasBeenSaved", 0
};

CKontaktContainerParameter::CKontaktContainerParameter(CKontaktContainer *owner)
	: CKontaktParameter(kContainerParamList)
	, mOwner(owner)
{
}

void CKontaktContainerParameter::AddParameter(unsigned int index, const unsigned char *value)
{
	switch (index) {
	case 0:  mOwner->loadPurged = CKontaktXml::BooleanValue(value); break;
	case 1:  mOwner->tableOpen = CKontaktXml::BooleanValue(value); break;
	case 2:  mOwner->smallRackUnit = CKontaktXml::BooleanValue(value); break;
	case 3:  mOwner->auxSendsVisible = CKontaktXml::BooleanValue(value); break;
	case 4:  mOwner->libraryID = CKontaktXml::UnsignedValue(value); break;
	case 5:  mOwner->loadingFlags = CKontaktXml::SignedValue(value); break;
	case 6:  mOwner->curProgramChangeNum = CKontaktXml::UnsignedValue(value); break;
	case 7:  mOwner->outputMask = CKontaktXml::UnsignedValue(value); break;
	case 8:  mOwner->volume = CKontaktXml::FloatValue(value); break;
	case 9:  mOwner->pan = CKontaktXml::FloatValue(value); break;
	case 10: mOwner->origSaveMode = CKontaktXml::UnsignedValue(value); break;
	case 11: mOwner->origAbsolutePaths = CKontaktXml::BooleanValue(value); break;
	case 12: mOwner->origCompressedSamples = CKontaktXml::BooleanValue(value); break;
	case 13: {
		char buf[0x100];
		CKontaktXml::UnpackPath(value, buf, sizeof(buf));
		mOwner->SetOriginalSubDirectory(buf);
		break;
	}
	case 14: mOwner->hasBeenSaved = CKontaktXml::BooleanValue(value); break;
	default: CKontaktParameter::AddParameter(index, value); break;
	}
}

/* ================================ CKontaktSampleParameter ================= */

static const char *kSampleParamList[] = {
	"file_ex2", "file_pbn", "isVolatile", "purged", "lastFileModified", "uniqueId",
	"lastPlayed", "sampleDataType", "sampleRate", "numChannels", "numFrames",
	"fileOffsetAudio", "fileOffsetContainer", "rootNote", "tuning", "littleEndian",
	"expectedDataSize", 0
};

CKontaktSampleParameter::CKontaktSampleParameter(CKontaktSample *owner)
	: CKontaktParameter(kSampleParamList)
	, mOwner(owner)
{
}

void CKontaktSampleParameter::AddParameter(unsigned int index, const unsigned char *value)
{
	switch (index) {
	case 0: {
		char buf[0x100];
		CKontaktXml::UnpackPath(value, buf, sizeof(buf));
		mOwner->SetFile(buf);
		break;
	}
	case 1: {
		char buf[0x100];
		CKontaktXml::UnpackPath(value, buf, sizeof(buf));
		mOwner->SetFilePbn(buf);
		break;
	}
	case 2:  mOwner->isVolatile = CKontaktXml::BooleanValue(value); break;
	case 3:  mOwner->purged = CKontaktXml::BooleanValue(value); break;
	case 4:  mOwner->lastFileModified = CKontaktXml::UnsignedValue(value); break;
	case 5:  mOwner->uniqueId = CKontaktXml::UnsignedValue(value); break;
	case 6:  mOwner->lastPlayed = CKontaktXml::UnsignedValue(value); break;
	case 7:  mOwner->sampleDataType = CKontaktXml::UnsignedValue(value); break;
	case 8:  mOwner->sampleRate = CKontaktXml::UnsignedValue(value); break;
	case 9:  mOwner->numChannels = CKontaktXml::UnsignedValue(value); break;
	case 10: mOwner->numFrames = CKontaktXml::UnsignedValue(value); break;
	case 11: mOwner->fileOffsetAudio = CKontaktXml::UnsignedValue(value); break;
	case 12: mOwner->fileOffsetContainer = CKontaktXml::UnsignedValue(value); break;
	case 13: mOwner->rootNote = CKontaktXml::UnsignedValue(value); break;
	case 14: mOwner->tuning = CKontaktXml::FloatValue(value); break;
	case 15: mOwner->littleEndian = CKontaktXml::BooleanValue(value); break;
	case 16: mOwner->expectedDataSize = CKontaktXml::UnsignedValue(value); break;
	default: CKontaktParameter::AddParameter(index, value); break;
	}
}

/* ================================ CKontaktProgramParameter ================= */

static const char *kProgramParamList[] = {
	"numBytesSamplesTotal", "transpose", "volume", "pan", "tune",
	"lowVelocity", "highVelocity", "lowKey", "highKey", "defaultKeyKeySwitch",
	"dfdChannelPreloadSize", "libraryID", "loadingFlags", "autoLoadMode", "cc64Mode",
	"useStdCC_7_10", "useStdCC_120_123", "cc7MaxVolume", "activeGroupIdx", "scriptIdxEdited",
	"ifxBeingEdited", "idxIFXBeingEdited", "sfxBeingEdited", "mapEditListMode", "mapEditLocked",
	"idxSFXBeingEdited", "routersOpenSource", "routersOpenAmp", "upDownMixOpenGroup", "groupSolo",
	"editorOpenMap", "editorOpenSample", "editorOpenGroup", "editorOpenScript", "editorOpenProp",
	"mapEditSelGroupsOnly", "ifxVisible", "sfxVisible", "modVisible", "masterZoneIdx",
	"batteryNumRowsVisible", "batteryNumColumnsVisible", "batteryIRCVisible", "perfViewScriptIdx", "catIconIdx",
	"instrumentCredits", "instrumentAuthor", "instrumentURL", "instrumentCat1", "instrumentCat2",
	"instrumentCat3", "wallpaperFile", "batteryCellColor", "muted", "soloed", 0
};

static const char *kProgramCc64ModeList[] = { "cc64_mode_sustain_plus_controller", 0 };

CKontaktProgramParameter::CKontaktProgramParameter(CKontaktProgram *owner)
	: CKontaktParameter(kProgramParamList)
	, mOwner(owner)
{
}

void CKontaktProgramParameter::AddParameter(unsigned int index, const unsigned char *value)
{
	switch (index) {
	case 0: mOwner->numBytesSamplesTotal = CKontaktXml::UnsignedValue(value); break;
	case 1: mOwner->transpose = CKontaktXml::SignedValue(value); break;
	case 2: mOwner->volume = CKontaktXml::FloatValue(value); break;
	case 3: mOwner->pan = CKontaktXml::FloatValue(value); break;
	case 4: mOwner->tune = CKontaktXml::SignedValue(value); break;
	case 5: mOwner->lowVelocity = CKontaktXml::UnsignedValue(value); break;
	case 6: mOwner->highVelocity = CKontaktXml::UnsignedValue(value); break;
	case 7: mOwner->lowKey = CKontaktXml::UnsignedValue(value); break;
	case 8: mOwner->highKey = CKontaktXml::UnsignedValue(value); break;
	/* 9-13 (defaultKeyKeySwitch/dfdChannelPreloadSize/libraryID/loadingFlags/
	 * autoLoadMode): confirmed real no-ops -- see class header. */
	case 9: case 10: case 11: case 12: case 13: break;
	case 14: {
		int m = CKontaktXml::StringIndex(kProgramCc64ModeList, value);
		if (m >= 0)
			mOwner->cc64Mode = m;
		break;
	}
	/* 15-50 (useStdCC_7_10 .. instrumentCat3, 36 entries): confirmed real
	 * no-ops -- see class header. */
	case 15: case 16: case 17: case 18: case 19:
	case 20: case 21: case 22: case 23: case 24:
	case 25: case 26: case 27: case 28: case 29:
	case 30: case 31: case 32: case 33: case 34:
	case 35: case 36: case 37: case 38: case 39:
	case 40: case 41: case 42: case 43: case 44:
	case 45: case 46: case 47: case 48: case 49:
	case 50:
		break;
	case 51: {
		char buf[0x100];
		CKontaktXml::UnpackPath(value, buf, sizeof(buf));
		mOwner->SetWallpaperFile(buf);
		break;
	}
	default: CKontaktParameter::AddParameter(index, value); break;
	}
}

/* ================================ CKontaktBankParameter ==================== */

static const char *kBankParamList[] = {
	"loadPurged", "libraryID", "loadingFlags", "midiChannel_slot", "solo_slot",
	"mute_slot", "auxSendLevel0_slot", "auxSendLevel1_slot", "auxSendLevel2_slot",
	"auxSendLevel3_slot", "reorderIdx_", "origSaveMode", "origAbsolutePaths", 0
};

CKontaktBankParameter::CKontaktBankParameter(CKontaktBank *owner)
	: CKontaktIndexedParameter(kBankParamList)
	, mOwner(owner)
{
}

void CKontaktBankParameter::AddIndexedParameter(unsigned int index, unsigned int suffix, const unsigned char *value)
{
	switch (index) {
	case 0:  mOwner->loadPurged = CKontaktXml::BooleanValue(value); break;
	case 1:  mOwner->libraryID = CKontaktXml::UnsignedValue(value); break;
	case 2:  mOwner->loadingFlags = CKontaktXml::UnsignedValue(value); break;
	case 3:  mOwner->SetSlotMidiChannel(suffix, CKontaktXml::UnsignedValue(value)); break;
	case 4:  mOwner->SetSlotSolo(suffix, CKontaktXml::BooleanValue(value)); break;
	case 5:  mOwner->SetSlotMute(suffix, CKontaktXml::BooleanValue(value)); break;
	/* 6-9 (auxSendLevel0_slot..auxSendLevel3_slot): one real shared jump-
	 * table target computing auxIndex = index-6 -- see class header. */
	case 6: case 7: case 8: case 9:
		mOwner->SetSlotAuxSendLevel(suffix, index - 6, (float)CKontaktXml::UnsignedValue(value));
		break;
	case 10: mOwner->SetSlotReorderIndex(suffix, CKontaktXml::UnsignedValue(value)); break;
	case 11: mOwner->origSaveMode = CKontaktXml::UnsignedValue(value); break;
	case 12: mOwner->origAbsolutePaths = CKontaktXml::BooleanValue(value); break;
	default: CKontaktIndexedParameter::AddIndexedParameter(index, suffix, value); break;
	}
}

/* ================================ CKontaktSendLevelsParameter ============== */

static const char *kSendLevelsParamList[] = { "level_", 0 };

CKontaktSendLevelsParameter::CKontaktSendLevelsParameter(CKontaktSendLevels *owner)
	: CKontaktIndexedParameter(kSendLevelsParamList)
	, mOwner(owner)
{
}

void CKontaktSendLevelsParameter::AddIndexedParameter(unsigned int index, unsigned int suffix, const unsigned char *value)
{
	switch (index) {
	case 0: mOwner->SetLevel(suffix, CKontaktXml::FloatValue(value)); break;
	default: CKontaktIndexedParameter::AddIndexedParameter(index, suffix, value); break;
	}
}

/* ================================ CKontaktIntModulatorParameter ============ */

static const char *kIntModulatorParamList[] = {
	"routersOpen", "bypass", "retrigger", "simulateK2DFDVolModulation",
	"legacyVolumeEnvBehaviour", "classID", "name", "frequency", "pulseWidth",
	"noteValue_Frequency", 0
};

CKontaktIntModulatorParameter::CKontaktIntModulatorParameter(CKontaktIntModulator *owner)
	: CKontaktParameter(kIntModulatorParamList)
	, mOwner(owner)
{
}

void CKontaktIntModulatorParameter::AddParameter(unsigned int index, const unsigned char *value)
{
	switch (index) {
	case 0: mOwner->routersOpen = CKontaktXml::BooleanValue(value); break;
	case 1: mOwner->bypass = CKontaktXml::BooleanValue(value); break;
	case 2: mOwner->retrigger = CKontaktXml::BooleanValue(value); break;
	case 3: mOwner->simulateK2DFDVolModulation = CKontaktXml::BooleanValue(value); break;
	case 4: mOwner->legacyVolumeEnvBehaviour = CKontaktXml::BooleanValue(value); break;
	case 5: mOwner->classID = CKontaktXml::UnsignedValue(value); break;
	case 6: mOwner->SetName((const char *)value); break;
	case 7: mOwner->frequency = CKontaktXml::FloatValue(value); break;
	case 8: mOwner->pulseWidth = CKontaktXml::FloatValue(value); break;
	case 9: mOwner->noteValue_Frequency = CKontaktXml::FloatValue(value); break;
	default: CKontaktParameter::AddParameter(index, value); break;
	}
}

/* ================================ CKontaktExtModulatorParameter ============ */

static const char *kExtModulatorParamList[] = {
	"type", "source", "delay", "initialValue", "bypass",
	"simulateK2EnvTimeModulation", "legacyStatModBehaviour", "classID", "name",
	"tableOpen", "ccNumber", 0
};

static const char *kExtModulatorTypeList[] = { "extMod", 0 };

static const char *kExtModulatorSourceList[] = {
	"", "keyPos", "velocity", "pitchBend", "rlsTrigCounter", "randomValBi",
	"midiCC", "monoAfterTouch", "randomValUni", 0
};

CKontaktExtModulatorParameter::CKontaktExtModulatorParameter(CKontaktExtModulator *owner)
	: CKontaktParameter(kExtModulatorParamList)
	, mOwner(owner)
{
}

void CKontaktExtModulatorParameter::AddParameter(unsigned int index, const unsigned char *value)
{
	switch (index) {
	case 0: {
		int t = CKontaktXml::StringIndex(kExtModulatorTypeList, value);
		if (t >= 0)
			mOwner->type = t;
		break;
	}
	case 1: {
		int s = CKontaktXml::StringIndex(kExtModulatorSourceList, value);
		if (s >= 0)
			mOwner->source = s;
		break;
	}
	/* real code round-trips through the FPU (SignedValue -> fild -> fisttp)
	 * before this store; mathematically a no-op for any in-range 32-bit
	 * int, reproduced as a plain assignment -- see class header. */
	case 2: mOwner->delay = CKontaktXml::SignedValue(value); break;
	case 3: mOwner->initialValue = CKontaktXml::SignedValue(value); break;
	case 4: mOwner->bypass = CKontaktXml::BooleanValue(value); break;
	case 5: mOwner->simulateK2EnvTimeModulation = CKontaktXml::BooleanValue(value); break;
	case 6: mOwner->legacyStatModBehaviour = CKontaktXml::BooleanValue(value); break;
	case 7: mOwner->classID = CKontaktXml::UnsignedValue(value); break;
	case 8: mOwner->SetName((const char *)value); break;
	case 9: mOwner->tableOpen = CKontaktXml::BooleanValue(value); break;
	case 10: mOwner->ccNumber = CKontaktXml::UnsignedValue(value); break;
	default: CKontaktParameter::AddParameter(index, value); break;
	}
}

/* ================================ CKontaktVoiceGroupParameter ============== */

static const char *kVoiceGroupParamList[] = {
	"name", "mode", "preferReleased", "maxNumVoices", "msFadeTime", "exclusionGroup", 0
};

static const char *kVoiceGroupModeList[] = { "kill_oldest", 0 };

CKontaktVoiceGroupParameter::CKontaktVoiceGroupParameter(CKontaktVoiceGroup *owner)
	: CKontaktParameter(kVoiceGroupParamList)
	, mOwner(owner)
{
}

void CKontaktVoiceGroupParameter::AddParameter(unsigned int index, const unsigned char *value)
{
	switch (index) {
	case 0: mOwner->SetName((const char *)value); break;
	case 1: {
		int m = CKontaktXml::StringIndex(kVoiceGroupModeList, value);
		if (m >= 0)
			mOwner->mode = m;
		break;
	}
	case 2: mOwner->preferReleased = CKontaktXml::BooleanValue(value); break;
	case 3: mOwner->maxNumVoices = CKontaktXml::UnsignedValue(value); break;
	case 4: mOwner->msFadeTime = CKontaktXml::UnsignedValue(value); break;
	case 5: mOwner->exclusionGroup = CKontaktXml::SignedValue(value); break;
	default: CKontaktParameter::AddParameter(index, value); break;
	}
}

/* ================================ CKontaktTargetParameter =================== */

static const char *kTargetParamList[] = {
	"target", "intensity", "slotIdx", "flags", "smoothingCoef", "name",
	"tableOpen", "targetObjIdx", 0
};

static const char *kTargetEnumList[] = {
	"pitch", "volume", "pan", "filterCutoff", "filterQ", "intensity",
	"intensity_upper", "ahdsr_attack", 0
};

CKontaktTargetParameter::CKontaktTargetParameter(CKontaktTarget *owner)
	: CKontaktParameter(kTargetParamList)
	, mOwner(owner)
{
}

void CKontaktTargetParameter::AddParameter(unsigned int index, const unsigned char *value)
{
	switch (index) {
	case 0: {
		int t = CKontaktXml::StringIndex(kTargetEnumList, value);
		if (t >= 0)
			mOwner->target = t;
		break;
	}
	case 1: mOwner->intensity = CKontaktXml::FloatValue(value); break;
	case 2: mOwner->slotIdx = CKontaktXml::SignedValue(value); break;
	case 3: mOwner->flags = CKontaktXml::UnsignedValue(value); break;
	case 4: mOwner->smoothingCoef = CKontaktXml::FloatValue(value); break;
	case 5: mOwner->SetName((const char *)value); break;
	case 6: mOwner->tableOpen = CKontaktXml::BooleanValue(value); break;
	case 7: mOwner->targetObjIdx = CKontaktXml::UnsignedValue(value); break;
	default: CKontaktParameter::AddParameter(index, value); break;
	}
}

/* ================================ CKontaktEffectParameter ==================== */

static const char *kEffectParamList[] = {
	"routersOpen", "classID", "bypass", "outLevel", "outLevelDry", "sendFXOutPartition", 0
};

CKontaktEffectParameter::CKontaktEffectParameter(CKontaktEffect *owner)
	: CKontaktParameter(kEffectParamList)
	, mOwner(owner)
{
}

void CKontaktEffectParameter::AddParameter(unsigned int index, const unsigned char *value)
{
	switch (index) {
	case 0: mOwner->routersOpen = CKontaktXml::BooleanValue(value); break;
	case 1: mOwner->classID = CKontaktXml::UnsignedValue(value); break;
	case 2: mOwner->bypass = CKontaktXml::BooleanValue(value); break;
	case 3: mOwner->outLevel = CKontaktXml::FloatValue(value); break;
	case 4: mOwner->outLevelDry = CKontaktXml::FloatValue(value); break;
	case 5: mOwner->sendFXOutPartition = CKontaktXml::SignedValue(value); break;
	default: CKontaktParameter::AddParameter(index, value); break;
	}
}

/* ================================ CKontaktFilterParameter ==================== */

static const char *kFilterParamList[] = {
	"cutoff", "resonance", "shiftB", "shiftC", "resB", "resC", "typeA", "typeB",
	"typeC", "bypA", "bypB", "bypC", "gain", "freq_1", "bandwidth_1", "gain_1",
	"freq_2", "bandwidth_2", "gain_2", 0
};

CKontaktFilterParameter::CKontaktFilterParameter(CKontaktFilter *owner)
	: CKontaktParameter(kFilterParamList)
	, mOwner(owner)
{
}

void CKontaktFilterParameter::AddParameter(unsigned int index, const unsigned char *value)
{
	switch (index) {
	case 0:  mOwner->cutoff = CKontaktXml::FloatValue(value); break;
	case 1:  mOwner->resonance = CKontaktXml::FloatValue(value); break;
	case 2:  mOwner->shiftB = CKontaktXml::FloatValue(value); break;
	case 3:  mOwner->shiftC = CKontaktXml::FloatValue(value); break;
	case 4:  mOwner->resB = CKontaktXml::FloatValue(value); break;
	case 5:  mOwner->resC = CKontaktXml::FloatValue(value); break;
	case 6:  mOwner->typeA = CKontaktXml::FloatValue(value); break;
	case 7:  mOwner->typeB = CKontaktXml::FloatValue(value); break;
	case 8:  mOwner->typeC = CKontaktXml::FloatValue(value); break;
	case 9:  mOwner->bypA = CKontaktXml::FloatValue(value); break;
	case 10: mOwner->bypB = CKontaktXml::FloatValue(value); break;
	case 11: mOwner->bypC = CKontaktXml::FloatValue(value); break;
	case 12: mOwner->gain = CKontaktXml::FloatValue(value); break;
	case 13: mOwner->freq_1 = CKontaktXml::FloatValue(value); break;
	case 14: mOwner->bandwidth_1 = CKontaktXml::FloatValue(value); break;
	case 15: mOwner->gain_1 = CKontaktXml::FloatValue(value); break;
	case 16: mOwner->freq_2 = CKontaktXml::FloatValue(value); break;
	case 17: mOwner->bandwidth_2 = CKontaktXml::FloatValue(value); break;
	case 18: mOwner->gain_2 = CKontaktXml::FloatValue(value); break;
	default: CKontaktParameter::AddParameter(index, value); break;
	}
}

/* ================================ CKontaktOutputParameter ==================== */

static const char *kOutputParamList[] = { "numChannels", "auxIdx", "volume", 0 };

CKontaktOutputParameter::CKontaktOutputParameter(CKontaktOutput *owner)
	: CKontaktParameter(kOutputParamList)
	, mOwner(owner)
{
}

void CKontaktOutputParameter::AddParameter(unsigned int index, const unsigned char *value)
{
	switch (index) {
	case 0: mOwner->numChannels = CKontaktXml::UnsignedValue(value); break;
	case 1: mOwner->auxIdx = CKontaktXml::UnsignedValue(value); break;
	case 2: mOwner->volume = CKontaktXml::FloatValue(value); break;
	default: CKontaktParameter::AddParameter(index, value); break;
	}
}

/* ================================ CKontaktLfoParameter ======================= */

static const char *kLfoParamList[] = {
	"frequency", "pulseWidth", "startPhase", "delay", "normalizeMultiLFO", 0
};

CKontaktLfoParameter::CKontaktLfoParameter(CKontaktLfo *owner)
	: CKontaktParameter(kLfoParamList)
	, mOwner(owner)
{
}

void CKontaktLfoParameter::AddParameter(unsigned int index, const unsigned char *value)
{
	switch (index) {
	case 0: mOwner->frequency = CKontaktXml::FloatValue(value); break;
	case 1: mOwner->pulseWidth = CKontaktXml::FloatValue(value); break;
	case 2: mOwner->startPhase = CKontaktXml::FloatValue(value); break;
	case 3: mOwner->delay = CKontaktXml::FloatValue(value); break;
	case 4: mOwner->normalizeMultiLFO = CKontaktXml::BooleanValue(value); break;
	default: CKontaktParameter::AddParameter(index, value); break;
	}
}

/* ================================ CKontaktLoopParameter ======================= */

static const char *kLoopParamList[] = {
	"loopStart", "loopLength", "loopCount", "mode", "alternatingLoop",
	"loopTuning", "xfadeLength", 0
};

static const char *kLoopModeList[] = { "oneshot", "until_release", "until_end", 0 };

CKontaktLoopParameter::CKontaktLoopParameter(CKontaktLoop *owner)
	: CKontaktParameter(kLoopParamList)
	, mOwner(owner)
{
}

void CKontaktLoopParameter::AddParameter(unsigned int index, const unsigned char *value)
{
	switch (index) {
	case 0: mOwner->loopStart = CKontaktXml::UnsignedValue(value); break;
	case 1: mOwner->loopLength = CKontaktXml::UnsignedValue(value); break;
	case 2: mOwner->loopCount = CKontaktXml::UnsignedValue(value); break;
	case 3: {
		int m = CKontaktXml::StringIndex(kLoopModeList, value);
		if (m >= 0)
			mOwner->mode = m;
		break;
	}
	case 4: mOwner->alternatingLoop = CKontaktXml::BooleanValue(value); break;
	case 5: mOwner->loopTuning = CKontaktXml::FloatValue(value); break;
	case 6: mOwner->xfadeLength = CKontaktXml::FloatValue(value); break;
	default: CKontaktParameter::AddParameter(index, value); break;
	}
}

/* ================================ CKontaktEnvelopeParameter =================== */

static const char *kEnvelopeParamList[] = {
	"atkCurving", "attack", "decay", "hold", "release", "sustain",
	"noteOffLessMode", "decay1", "break", "decay2", 0
};

CKontaktEnvelopeParameter::CKontaktEnvelopeParameter(CKontaktEnvelope *owner)
	: CKontaktParameter(kEnvelopeParamList)
	, mOwner(owner)
{
}

void CKontaktEnvelopeParameter::AddParameter(unsigned int index, const unsigned char *value)
{
	switch (index) {
	case 0: mOwner->atkCurving = CKontaktXml::FloatValue(value); break;
	/* real: parsed as a signed int, then int-to-float converted (`fild`) --
	 * NOT a direct FloatValue() call. See class header note. */
	case 1: mOwner->attack = (float)CKontaktXml::SignedValue(value); break;
	case 2: mOwner->decay = CKontaktXml::FloatValue(value); break;
	case 3: mOwner->hold = CKontaktXml::FloatValue(value); break;
	case 4: mOwner->release = CKontaktXml::FloatValue(value); break;
	case 5: mOwner->sustain = CKontaktXml::FloatValue(value); break;
	case 6: mOwner->noteOffLessMode = CKontaktXml::BooleanValue(value); break;
	case 7: mOwner->decay1 = CKontaktXml::FloatValue(value); break;
	case 8: mOwner->breakLevel = CKontaktXml::FloatValue(value); break;
	case 9: mOwner->decay2 = CKontaktXml::FloatValue(value); break;
	default: CKontaktParameter::AddParameter(index, value); break;
	}
}

/* ================================ CKontaktPlaybackModeParameter =============== */

static const char *kPlaybackModeParamList[] = {
	"type", "speed", "legato", "zoneLockedSpeed", "sp1200Filter", "smooth",
	"grainLength", "hiQuality", "formantShift", "dcFilter", 0
};

static const char *kPlaybackModeTypeList[] = {
	"streaming", "time_machine", "tone_machine", 0
};

CKontaktPlaybackModeParameter::CKontaktPlaybackModeParameter(CKontaktPlaybackMode *owner)
	: CKontaktParameter(kPlaybackModeParamList)
	, mOwner(owner)
{
}

void CKontaktPlaybackModeParameter::AddParameter(unsigned int index, const unsigned char *value)
{
	switch (index) {
	case 0: {
		int t = CKontaktXml::StringIndex(kPlaybackModeTypeList, value);
		if (t >= 0)
			mOwner->type = t;
		break;
	}
	case 1: mOwner->speed = CKontaktXml::FloatValue(value); break;
	case 2: mOwner->legato = CKontaktXml::BooleanValue(value); break;
	case 3: mOwner->zoneLockedSpeed = CKontaktXml::BooleanValue(value); break;
	case 4: mOwner->sp1200Filter = CKontaktXml::UnsignedValue(value); break;
	case 5: mOwner->smooth = CKontaktXml::FloatValue(value); break;
	case 6: mOwner->grainLength = CKontaktXml::FloatValue(value); break;
	case 7: mOwner->hiQuality = CKontaktXml::BooleanValue(value); break;
	case 8: mOwner->formantShift = CKontaktXml::FloatValue(value); break;
	case 9: mOwner->dcFilter = CKontaktXml::BooleanValue(value); break;
	default: CKontaktParameter::AddParameter(index, value); break;
	}
}

/* ================================ CKontaktStartCriteriaParameter ============== */

static const char *kStartCriteriaParamList[] = {
	"mode", "cycleClass", "nextCriteria", "controller", "cc_min", "cc_max", 0
};

static const char *kStartCriteriaModeList[] = { "cycle_round_robin", "on_controller", 0 };
static const char *kStartCriteriaNextCriteriaList[] = { "and", "and_not", 0 };

CKontaktStartCriteriaParameter::CKontaktStartCriteriaParameter(CKontaktStartCriteria *owner)
	: CKontaktParameter(kStartCriteriaParamList)
	, mOwner(owner)
{
}

void CKontaktStartCriteriaParameter::AddParameter(unsigned int index, const unsigned char *value)
{
	switch (index) {
	case 0: {
		int m = CKontaktXml::StringIndex(kStartCriteriaModeList, value);
		if (m >= 0)
			mOwner->mode = m;
		break;
	}
	case 1: mOwner->cycleClass = CKontaktXml::UnsignedValue(value); break;
	case 2: {
		int n = CKontaktXml::StringIndex(kStartCriteriaNextCriteriaList, value);
		if (n >= 0)
			mOwner->nextCriteria = n;
		break;
	}
	case 3: mOwner->controller = CKontaktXml::UnsignedValue(value); break;
	case 4: mOwner->cc_min = CKontaktXml::UnsignedValue(value); break;
	case 5: mOwner->cc_max = CKontaktXml::UnsignedValue(value); break;
	default: CKontaktParameter::AddParameter(index, value); break;
	}
}

/* ================================ CKontaktScriptParameter ===================== */

static const char *kScriptParamList[] = {
	"sourceText", "sourceEditorOpen", "touchedButNotApplied", "bypass",
	"description", "password", "persistent_var_", 0
};

CKontaktScriptParameter::CKontaktScriptParameter(CKontaktScript *owner)
	: CKontaktDynamicParameter(kScriptParamList)
	, mOwner(owner)
{
}

void CKontaktScriptParameter::AddDynamicParameter(unsigned int index, const char * /*suffix*/, const unsigned char *value)
{
	switch (index) {
	case 0: mOwner->SetSourceText((const char *)value); break;
	case 1: mOwner->sourceEditorOpen = CKontaktXml::BooleanValue(value); break;
	case 2: mOwner->touchedButNotApplied = CKontaktXml::BooleanValue(value); break;
	case 3: mOwner->bypass = CKontaktXml::BooleanValue(value); break;
	case 4: mOwner->SetDescription((const char *)value); break;
	case 5: mOwner->SetPassword((const char *)value); break;
	/* 6 ("persistent_var_"): confirmed real no-op -- see class header. */
	case 6: break;
	default: CKontaktDynamicParameter::AddDynamicParameter(index, "", value); break;
	}
}

/* ====================== concrete plural "Parameters" wrappers ============
 * See kontakt_parameter_family.h's own file-header note for how these tie
 * into the resolved "V"/"Parameters" factory-family finding. */

CKontaktGroupParameters::CKontaktGroupParameters(CKontaktGroup *owner)
	: mOwner(owner)
{
}

CKontaktIndexedParameter *CKontaktGroupParameters::MakeIndexedParameter()
{
	return new CKontaktGroupParameter(mOwner);
}

CKontaktOutputParameters::CKontaktOutputParameters(CKontaktOutput *owner)
	: mOwner(owner)
{
}

CKontaktParameter *CKontaktOutputParameters::MakeParameter()
{
	return new CKontaktOutputParameter(mOwner);
}

CKontaktZoneParameters::CKontaktZoneParameters(CKontaktZone *owner)
	: mOwner(owner)
{
}

CKontaktParameter *CKontaktZoneParameters::MakeParameter()
{
	return new CKontaktZoneParameter(mOwner);
}

CKontaktContainerParameters::CKontaktContainerParameters(CKontaktContainer *owner)
	: mOwner(owner)
{
}

CKontaktParameter *CKontaktContainerParameters::MakeParameter()
{
	return new CKontaktContainerParameter(mOwner);
}

CKontaktOutputsParameters::CKontaktOutputsParameters(CKontaktOutputs *owner)
	: mOwner(owner)
{
}

CKontaktIndexedParameter *CKontaktOutputsParameters::MakeIndexedParameter()
{
	return new CKontaktOutputsParameter(mOwner);
}

CKontaktBankParameters::CKontaktBankParameters(CKontaktBank *owner)
	: mOwner(owner)
{
}

CKontaktIndexedParameter *CKontaktBankParameters::MakeIndexedParameter()
{
	return new CKontaktBankParameter(mOwner);
}

CKontaktProgramParameters::CKontaktProgramParameters(CKontaktProgram *owner)
	: mOwner(owner)
{
}

CKontaktParameter *CKontaktProgramParameters::MakeParameter()
{
	return new CKontaktProgramParameter(mOwner);
}
