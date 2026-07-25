// SPDX-License-Identifier: GPL-2.0
/*
 * vm_test_accessors.cpp  -  NOT real OA_real.ko symbols. These three tiny
 * `extern "C"` functions exist purely as a VM-testing convenience so that
 * KronosScreenRemoteDaemon's nks4_inject.ko/midi_bridge.ko can resolve
 * CSTGFrontPanel::sInstance and CSTGMidiPortManager::sMidiInPorts/
 * sMidiOutPorts against THIS reconstruction's own compiled OA.ko without
 * needing the fragile fixed-byte-offset / opcode-pattern-scanning tricks
 * those modules use to find the same globals inside the real, differently
 * compiled (GCC 4.5.0) OA_real.ko on actual Kronos hardware. On real
 * hardware these functions simply don't exist and are never called; the
 * daemon modules only use them when an explicit `fn_*_get` module param is
 * passed in, which defaults to 0 (disabled) everywhere else.
 */

#include "oa_global.h"
#include "oa_setup_global_resources.h"
#include "oa_engine.h"

extern "C" void *CSTGFrontPanel_GetInstanceForTest(void)
{
	return CSTGFrontPanel::sInstance;
}

extern "C" void **CSTGMidiPortManager_GetInPortsArrayForTest(void)
{
	return CSTGMidiPortManager::sMidiInPorts;
}

extern "C" void **CSTGMidiPortManager_GetOutPortsArrayForTest(void)
{
	return CSTGMidiPortManager::sMidiOutPorts;
}
