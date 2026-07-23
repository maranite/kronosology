/*
 * mains.h  -  Mains() and the 17 MMainXxx(CSystemApi*, ...) module-registration
 * functions it calls in sequence (Stage 3).
 *
 * Not to be confused with CKernel::InitSystemLayer()'s own, unrelated MMainXxx(void)
 * family (MMainEditMan/MMainViewer/MMainSeqTimer/... -- see ckernel.cpp) -- same
 * naming convention, genuinely different functions, different call shape.
 */

#ifndef MAINS_H
#define MAINS_H

#include "system_api.h"

void Mains();

void MMainPanelDriver(CSystemApi *api);
void MMainHIDDriver(CSystemApi *api, const char *eventsName, const char *commandsName);
void MMainAlphaKeybCtrl(CSystemApi *api);
void MMainLinuxDriver(CSystemApi *api);
void MMainEditor(CSystemApi *api);
void MMainPanel(CSystemApi *api);
void MMainBatchDiskMan(CSystemApi *api);
void MMainESCommon(CSystemApi *api);
void MMainESProg(CSystemApi *api);
void MMainESEffect(CSystemApi *api);
void MMainESCombi(CSystemApi *api);
void MMainESGlobal(CSystemApi *api);
void MMainESMOSS(CSystemApi *api);
void MMainESSampling(CSystemApi *api);
void MMainESSetList(CSystemApi *api);
void MMainESSong(CSystemApi *api);
void MMainESDisk(CSystemApi *api);

#endif /* MAINS_H */
