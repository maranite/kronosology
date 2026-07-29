/*
 * stg_disk_command_task.cpp  -  CESDiskCommandTask's 84 command trampolines
 * (round 44, solo). See include/es_disk_command_task.h for the shared shape
 * derivation. Each opcode literal below was read individually from its own
 * ground-truth decompile (LoadMultiFile@08ddddc0.c .. Load1MossProg@08de01d0.c,
 * /home/share/Decomp/EVA_Decomp/eva_export/functions/) -- not inferred from a
 * pattern.
 */
#include "es_disk_command_task.h"

unsigned int CESDiskCommandTask::LoadMultiFile(unsigned char) { SetOpcode(0x52); return 1; }
unsigned int CESDiskCommandTask::Blank(unsigned char) { SetOpcode(0x51); return 1; }
unsigned int CESDiskCommandTask::Finalize(unsigned char) { SetOpcode(0x50); return 1; }
unsigned int CESDiskCommandTask::BurnAudio(unsigned char) { SetOpcode(0x4f); return 1; }
unsigned int CESDiskCommandTask::StartMIDIReceiver(unsigned char) { SetOpcode(0x57); return 1; }
unsigned int CESDiskCommandTask::StartSetDate(unsigned char) { SetOpcode(0x56); return 1; }
unsigned int CESDiskCommandTask::FileUnprotect(unsigned char) { SetOpcode(0x4e); return 1; }
unsigned int CESDiskCommandTask::FileProtect(unsigned char) { SetOpcode(0x4d); return 1; }
unsigned int CESDiskCommandTask::OptimizeMedium(unsigned char) { SetOpcode(0x4c); return 1; }
unsigned int CESDiskCommandTask::CheckMedium(unsigned char) { SetOpcode(0x4b); return 1; }
unsigned int CESDiskCommandTask::RateConvert(unsigned char) { SetOpcode(0x4a); return 1; }
unsigned int CESDiskCommandTask::ConvertToIso(unsigned char) { SetOpcode(0x49); return 1; }
unsigned int CESDiskCommandTask::Format(unsigned char) { SetOpcode(0x48); return 1; }
unsigned int CESDiskCommandTask::CreateDir(unsigned char) { SetOpcode(0x47); return 1; }
unsigned int CESDiskCommandTask::DeleteUnusedWav(unsigned char) { SetOpcode(0x46); return 1; }
unsigned int CESDiskCommandTask::Delete(unsigned char) { SetOpcode(0x45); return 1; }
unsigned int CESDiskCommandTask::Copy(unsigned char) { SetOpcode(0x44); return 1; }
unsigned int CESDiskCommandTask::Rename(unsigned char) { SetOpcode(0x43); return 1; }
unsigned int CESDiskCommandTask::SaveKfx(unsigned char) { SetOpcode(0x42); return 1; }
unsigned int CESDiskCommandTask::Save1Song(unsigned char) { SetOpcode(0x41); return 1; }
unsigned int CESDiskCommandTask::SaveKcd(unsigned char) { SetOpcode(0x40); return 1; }
unsigned int CESDiskCommandTask::SaveAifWav(unsigned char) { SetOpcode(0x3f); return 1; }
unsigned int CESDiskCommandTask::SaveExclusive(unsigned char) { SetOpcode(0x3e); return 1; }
unsigned int CESDiskCommandTask::SaveSMF(unsigned char) { SetOpcode(0x3d); return 1; }
unsigned int CESDiskCommandTask::SaveKge(unsigned char) { SetOpcode(0x3c); return 1; }
unsigned int CESDiskCommandTask::SaveSample(unsigned char) { SetOpcode(0x3b); return 1; }
unsigned int CESDiskCommandTask::SaveSeq(unsigned char) { SetOpcode(0x3a); return 1; }
unsigned int CESDiskCommandTask::SavePcg(unsigned char) { SetOpcode(0x39); return 1; }
unsigned int CESDiskCommandTask::SavePcgSeq(unsigned char) { SetOpcode(0x38); return 1; }
unsigned int CESDiskCommandTask::SaveAll(unsigned char) { SetOpcode(0x37); return 1; }
unsigned int CESDiskCommandTask::LoadKscItem(unsigned char) { SetOpcode(0x35); return 1; }
unsigned int CESDiskCommandTask::LoadKontaktSample(unsigned char) { SetOpcode(0x34); return 1; }
unsigned int CESDiskCommandTask::LoadKontaktInstrument(unsigned char) { SetOpcode(0x33); return 1; }
unsigned int CESDiskCommandTask::LoadKontaktMulti(unsigned char) { SetOpcode(0x32); return 1; }
unsigned int CESDiskCommandTask::LoadKontaktBank(unsigned char) { SetOpcode(0x31); return 1; }
unsigned int CESDiskCommandTask::LoadSF2(unsigned char) { SetOpcode(0x30); return 1; }
unsigned int CESDiskCommandTask::Load1Fx(unsigned char) { SetOpcode(0x2f); return 1; }
unsigned int CESDiskCommandTask::LoadFxBank(unsigned char) { SetOpcode(0x2e); return 1; }
unsigned int CESDiskCommandTask::LoadFxs(unsigned char) { SetOpcode(0x2d); return 1; }
unsigned int CESDiskCommandTask::LoadKfx(unsigned char) { SetOpcode(0x2c); return 1; }
unsigned int CESDiskCommandTask::LoadKcd(unsigned char) { SetOpcode(0x2b); return 1; }
unsigned int CESDiskCommandTask::LoadAkaiVolume(unsigned char) { SetOpcode(0x2a); return 1; }
unsigned int CESDiskCommandTask::LoadAkaiProg(unsigned char) { SetOpcode(0x29); return 1; }
unsigned int CESDiskCommandTask::LoadAkaiSample(unsigned char) { SetOpcode(0x28); return 1; }
unsigned int CESDiskCommandTask::LoadWav(unsigned char) { SetOpcode(0x27); return 1; }
unsigned int CESDiskCommandTask::LoadAif(unsigned char) { SetOpcode(0x26); return 1; }
unsigned int CESDiskCommandTask::LoadKsf(unsigned char) { SetOpcode(0x25); return 1; }
unsigned int CESDiskCommandTask::LoadKmp(unsigned char) { SetOpcode(0x24); return 1; }
unsigned int CESDiskCommandTask::LoadExclusive(unsigned char) { SetOpcode(0x23); return 1; }
unsigned int CESDiskCommandTask::LoadSMF(unsigned char) { SetOpcode(0x22); return 1; }
unsigned int CESDiskCommandTask::Load1Pattern(unsigned char) { SetOpcode(0x21); return 1; }
unsigned int CESDiskCommandTask::LoadTracks(unsigned char) { SetOpcode(0x20); return 1; }
unsigned int CESDiskCommandTask::Load1Song(unsigned char) { SetOpcode(0x1f); return 1; }
unsigned int CESDiskCommandTask::Load1Region(unsigned char) { SetOpcode(0x1e); return 1; }
unsigned int CESDiskCommandTask::LoadRegionBank(unsigned char) { SetOpcode(0x1d); return 1; }
unsigned int CESDiskCommandTask::LoadCueLists(unsigned char) { SetOpcode(0x1c); return 1; }
unsigned int CESDiskCommandTask::LoadTemplateBank(unsigned char) { SetOpcode(0x1a); return 1; }
unsigned int CESDiskCommandTask::LoadTemplates(unsigned char) { SetOpcode(0x19); return 1; }
unsigned int CESDiskCommandTask::Load1GE(unsigned char) { SetOpcode(0x18); return 1; }
unsigned int CESDiskCommandTask::LoadGEBank(unsigned char) { SetOpcode(0x17); return 1; }
unsigned int CESDiskCommandTask::LoadGEs(unsigned char) { SetOpcode(0x16); return 1; }
unsigned int CESDiskCommandTask::Load1SetListSlot(unsigned char) { SetOpcode(0x15); return 1; }
unsigned int CESDiskCommandTask::Load1SetList(unsigned char) { SetOpcode(0x14); return 1; }
unsigned int CESDiskCommandTask::LoadSetLists(unsigned char) { SetOpcode(0x13); return 1; }
unsigned int CESDiskCommandTask::LoadGlobal(unsigned char) { SetOpcode(0x10); return 1; }
unsigned int CESDiskCommandTask::Load1DrumTrackPattern(unsigned char) { SetOpcode(0x12); return 1; }
unsigned int CESDiskCommandTask::LoadDrumTrackPatterns(unsigned char) { SetOpcode(0x11); return 1; }
unsigned int CESDiskCommandTask::Load1Wseq(unsigned char) { SetOpcode(0x0f); return 1; }
unsigned int CESDiskCommandTask::LoadWseqBank(unsigned char) { SetOpcode(0x0e); return 1; }
unsigned int CESDiskCommandTask::LoadWaveSeqs(unsigned char) { SetOpcode(0x0d); return 1; }
unsigned int CESDiskCommandTask::Load1Dkit(unsigned char) { SetOpcode(0x0c); return 1; }
unsigned int CESDiskCommandTask::LoadDkitBank(unsigned char) { SetOpcode(0x0b); return 1; }
unsigned int CESDiskCommandTask::LoadDkits(unsigned char) { SetOpcode(0x0a); return 1; }
unsigned int CESDiskCommandTask::Load1Combi(unsigned char) { SetOpcode(0x09); return 1; }
unsigned int CESDiskCommandTask::LoadCombiBank(unsigned char) { SetOpcode(0x08); return 1; }
unsigned int CESDiskCommandTask::LoadCombis(unsigned char) { SetOpcode(0x07); return 1; }
unsigned int CESDiskCommandTask::Load1Prog(unsigned char) { SetOpcode(0x04); return 1; }
unsigned int CESDiskCommandTask::LoadSyx(unsigned char) { SetOpcode(0x36); return 1; }
unsigned int CESDiskCommandTask::LoadProgBank(unsigned char) { SetOpcode(0x03); return 1; }
unsigned int CESDiskCommandTask::LoadPrograms(unsigned char) { SetOpcode(0x02); return 1; }
unsigned int CESDiskCommandTask::LoadPcgRamSmpl(unsigned char) { SetOpcode(0x01); return 1; }
unsigned int CESDiskCommandTask::LoadAll(unsigned char) { SetOpcode(0x00); return 1; }
unsigned int CESDiskCommandTask::LoadMossBank(unsigned char) { SetOpcode(0x05); return 1; }
unsigned int CESDiskCommandTask::Load1MossProg(unsigned char) { SetOpcode(0x06); return 1; }
