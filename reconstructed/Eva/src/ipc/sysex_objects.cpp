// SPDX-License-Identifier: GPL-2.0
#include "sysex_objects.h"

unsigned int CSysExSong::GetStorageId() const { return 0x25; }
unsigned int CSysExSong::HasDigests() const { return 1; }
unsigned int CSysExSong::GetVersion() const { return 3; }
unsigned int CSysExSong::GetObjectSize() const { return 0x3314; }
unsigned int CSysExSong::GetObjectSizeForExport() const { return 0x3314; }
unsigned int CSysExSong::GetNumObjectsForDigest(int) const { return 200; }

unsigned int CSysExDrumKit::GetStorageId() const { return 3; }
unsigned int CSysExDrumKit::HasDigests() const { return 1; }
unsigned int CSysExDrumKit::GetVersion() const { return 3; }
unsigned int CSysExDrumKit::GetObjectSize() const { return 0x9618; }
unsigned int CSysExDrumKit::GetObjectSizeForExport() const { return 0x9618; }

unsigned int CSysExCombi::GetStorageId() const { return 2; }
unsigned int CSysExCombi::HasDigests() const { return 1; }
unsigned int CSysExCombi::GetVersion() const { return 3; }
unsigned int CSysExCombi::GetObjectSize() const { return 0x1e82; }
unsigned int CSysExCombi::GetObjectSizeForExport() const { return 0x1e82; }

unsigned int CSysExWaveSeq::GetStorageId() const { return 4; }
unsigned int CSysExWaveSeq::HasDigests() const { return 1; }
unsigned int CSysExWaveSeq::GetVersion() const { return 1; }
unsigned int CSysExWaveSeq::GetObjectSize() const { return 0x8a8; }
unsigned int CSysExWaveSeq::GetObjectSizeForExport() const { return 0x8a8; }

unsigned int CSysExSetList::GetStorageId() const { return 0x1b; }
unsigned int CSysExSetList::HasDigests() const { return 1; }
unsigned int CSysExSetList::GetVersion() const { return 0; }
unsigned int CSysExSetList::GetObjectSize() const { return 0x10f28; }
unsigned int CSysExSetList::GetObjectSizeForExport() const { return 0x10f28; }

unsigned int CSysExSongTimbreSet::GetStorageId() const { return 0xd; }
unsigned int CSysExSongTimbreSet::GetVersion() const { return 3; }
unsigned int CSysExSongTimbreSet::GetObjectSize() const { return 0x1e82; }
unsigned int CSysExSongTimbreSet::GetObjectSizeForExport() const { return 0x1e82; }
