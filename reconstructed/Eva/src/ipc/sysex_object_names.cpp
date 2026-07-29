// SPDX-License-Identifier: GPL-2.0
#include "sysex_object_names.h"

unsigned int CSysExSetListSlotComment::GetStorageId() const { return 0x1c; }
unsigned int CSysExSetListSlotComment::GetVersion() const { return 0; }
unsigned int CSysExSetListSlotComment::GetObjectSize() const { return 0x200; }
unsigned int CSysExSetListSlotComment::GetObjectSizeForExport() const { return 0x200; }

unsigned int CSysExSetListSlotName::GetStorageId() const { return 0x1d; }
unsigned int CSysExSetListSlotName::GetVersion() const { return 0; }
unsigned int CSysExSetListSlotName::GetObjectSize() const { return 0x18; }
unsigned int CSysExSetListSlotName::GetObjectSizeForExport() const { return 0x18; }

unsigned int CSysExCombiName::GetStorageId() const { return 0x1e; }
unsigned int CSysExCombiName::GetVersion() const { return 0; }
unsigned int CSysExCombiName::GetObjectSize() const { return 0x18; }
unsigned int CSysExCombiName::GetObjectSizeForExport() const { return 0x18; }

unsigned int CSysExProgName::GetStorageId() const { return 0x1f; }
unsigned int CSysExProgName::GetVersion() const { return 0; }
unsigned int CSysExProgName::GetObjectSize() const { return 0x18; }
unsigned int CSysExProgName::GetObjectSizeForExport() const { return 0x18; }

unsigned int CSysExSongName::GetStorageId() const { return 0x20; }
unsigned int CSysExSongName::GetVersion() const { return 0; }
unsigned int CSysExSongName::GetObjectSize() const { return 0x18; }
unsigned int CSysExSongName::GetObjectSizeForExport() const { return 0x18; }

unsigned int CSysExWaveSeqName::GetStorageId() const { return 0x21; }
unsigned int CSysExWaveSeqName::GetVersion() const { return 0; }
unsigned int CSysExWaveSeqName::GetObjectSize() const { return 0x18; }
unsigned int CSysExWaveSeqName::GetObjectSizeForExport() const { return 0x18; }

unsigned int CSysExDrumKitName::GetStorageId() const { return 0x22; }
unsigned int CSysExDrumKitName::GetVersion() const { return 0; }
unsigned int CSysExDrumKitName::GetObjectSize() const { return 0x18; }
unsigned int CSysExDrumKitName::GetObjectSizeForExport() const { return 0x18; }

unsigned int CSysExSetListName::GetStorageId() const { return 0x23; }
unsigned int CSysExSetListName::GetVersion() const { return 0; }
unsigned int CSysExSetListName::GetObjectSize() const { return 0x18; }
unsigned int CSysExSetListName::GetObjectSizeForExport() const { return 0x18; }
