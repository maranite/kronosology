// SPDX-License-Identifier: GPL-2.0
/*
 * host_stubs.cpp - minimal host-side test doubles for the STG/RTAI/kernel externs
 * that command.cpp and driver.cpp depend on, so those two translation units can be
 * compiled and linked as plain host programs for verify/'s known-answer tests.
 *
 * These are deliberately NOT faithful reimplementations of the real kernel-side
 * behavior (that would just be re-deriving driver.cpp's own callers, which is out of
 * scope for a unit test of driver.cpp/command.cpp themselves) - they are minimal,
 * observable seams: recording stubs where a test needs to see what was sent, no-ops
 * everywhere else. Matches the "test double" approach already used elsewhere in this
 * project (AT88VirtualChip/verify).
 */

#include "../omapnks4_internal.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>

// ---- printk --------------------------------------------------------------------
extern "C" int printk(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int n = vfprintf(stderr, fmt, ap);
	va_end(ap);
	return n;
}

// ---- command.cpp's externs (host->panel command word path) ---------------------
// Recording stub: records the last word submitted so tests can assert on it, and
// always reports success (return 0) unless a test overrides host_stub_fail_write.
bool host_stub_fail_write = false;
unsigned int host_stub_last_cmd_word = 0;
int host_stub_cmd_word_call_count = 0;

int SubmitNKS4CommandWrite(unsigned int cmd)
{
	host_stub_last_cmd_word = cmd;
	host_stub_cmd_word_call_count++;
	return host_stub_fail_write ? -1 : 0;
}

int WaitForNKS4CommandWrite(unsigned int cmd)
{
	host_stub_last_cmd_word = cmd;
	host_stub_cmd_word_call_count++;
	return host_stub_fail_write ? -1 : 0;
}

unsigned int host_stub_read_event_response = 0;
int WaitForNKS4ReadEvent(unsigned int *resp)
{
	*resp = host_stub_read_event_response;
	return host_stub_fail_write ? -1 : 0;
}

int SubmitNKS4CommandMultipleWriteNONBLOCKING(unsigned int *, unsigned int) { return 0; }
int SubmitOmapNKS4CmdBulkWrite(unsigned char, unsigned char *, unsigned int) { return 0; }
int SubmitOmapNKS4BulkWrite(unsigned int *, unsigned int) { return 0; }
bool OmapNKS4WriteQueueNotFull(int) { return true; }
void SignalShutdownSSD(void) { }
void SetShutdownDelay(int) { }
void WaitOnAtmelRead(void) { }
int SubmitOmapNKS4VideoWrite(unsigned int *, unsigned int) { return 0; }
void SignalVideoMessageProcessor(void) { }

// ---- driver.cpp's event-delivery externs (recording stubs) ----------------------
unsigned char host_stub_last_proc_event = 0xff;
int host_stub_proc_event_call_count = 0;
void OmapNKS4ProcAddEvent(unsigned char ev)
{
	host_stub_last_proc_event = ev;
	host_stub_proc_event_call_count++;
}

unsigned int host_stub_last_reader_word = 0;
int host_stub_reader_call_count = 0;
void SendNKS4EventToLinuxReader(unsigned int cmd)
{
	host_stub_last_reader_word = cmd;
	host_stub_reader_call_count++;
}

int host_stub_atmel_read_complete_count = 0;
void SignalAtmelReadComplete(void) { host_stub_atmel_read_complete_count++; }

// ---- aftertouch calibration (driver.cpp) -----------------------------------------
// Ground truth for the real curve tables/scale factor was never recovered (they were
// only ever `extern`-declared, never defined, anywhere in this reconstruction - see
// verify/README.md). host_stub_calibration_passthrough == true (the default) makes
// ApplyNKS4Calibration an identity function, which is enough to test
// ReceiveEventBuffer's byte-PACKING logic (what this session's fixes were about)
// without depending on unknown curve data.
bool host_stub_calibration_passthrough = true;
short host_stub_calibration_return = 0;

extern "C" {
// driver.cpp declares all four of these inside its own `extern "C" { ... }` block, so
// the stub definitions must match that linkage exactly or the linker sees a different
// (C++-mangled) symbol name and reports "undefined reference" against driver.cpp's
// plain-C-linkage calls.
int ApplyNKS4Calibration(unsigned int, short raw)
{
	return host_stub_calibration_passthrough ? raw : host_stub_calibration_return;
}
// `extern` is required here even inside `extern "C"`: in C++, a `const` global has
// internal linkage by default unless explicitly declared `extern` (a separate rule
// from the extern "C" linkage-specification, which only affects name mangling) -
// without it, the linker doesn't see these as satisfying driver.cpp's references.
extern const unsigned char sAfterTouch1ConvertTable[256] = {0};
extern const unsigned char sAfterTouch2ConvertTable[256] = {0};
/* _DAT_0000af38 removed (re-verification pass, 2026-07-17): driver.cpp now
 * defines this itself as a real global (see its own comment there) - this
 * stub had become a stale duplicate definition, a link error the moment
 * both translation units were linked together (the same class of bug as
 * the project's already-documented sMaxWritePacketSize duplicate). */
}

// ---- CNKS4EventFilter::FilterEvent - always allow through (host_stub_filter_allow
// lets a test simulate suppression if it ever needs to) --------------------------
bool host_stub_filter_allow = true;
unsigned char CNKS4EventFilter::FilterEvent(unsigned int) { return host_stub_filter_allow ? 1 : 0; }

// ---- CSTGOmapNKS4Fifos::sInstance - real struct, zero-initialized by default; tests
// read inputFifo.pRing[]/dwWriteIndex directly to see what push_event() wrote. -----
struct CSTGOmapNKS4Fifos CSTGOmapNKS4Fifos::sInstance;
void CSTGOmapNKS4Fifos::Initialize(int) { }
void CSTGOmapNKS4Fifos::TriggerOutputInterrupt(void) { }

// ---- video.cpp externs referenced by driver.cpp's progress-bar code (not exercised
// by the ReceiveEventBuffer/command.cpp tests, stubbed only so the whole translation
// unit links) -----------------------------------------------------------------------
// COmapNKS4VideoAPI has a real (non-trivial) constructor defined in video.cpp, which
// this verify suite doesn't link (out of scope - see docs/gaps.md "Colour-LCD pixel
// streaming pipeline"). Supply a trivial stub definition instead of linking video.cpp,
// just so g_video's static initialization resolves.
COmapNKS4VideoAPI::COmapNKS4VideoAPI(void) { std::memset(this, 0, sizeof(*this)); }
struct COmapNKS4VideoAPI g_video;
extern "C" int OmapNKS4VideoAPI_SendFillData(struct COmapNKS4VideoAPI *, unsigned char, int, int, int) { return 0; }
