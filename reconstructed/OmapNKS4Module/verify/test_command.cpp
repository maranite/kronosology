// SPDX-License-Identifier: GPL-2.0
/*
 * test_command.cpp - known-answer tests for command.cpp's COmapNKS4Command setter/
 * query word encodings, most of which were fixed this session (2026-07-15) after a
 * fresh Ghidra decompile + disassembly of OmapNKS4Module.ko 3.2.2 found the original
 * reconstruction's "not fully disassembled" guesses were wrong for 5 of 6 setters
 * plus ReadPortConfiguration's response-routing. See KronosNKS4/docs/gaps.md
 * "Setter command word encodings - RESOLVED" and "Command-word response routing -
 * RESOLVED" for the full derivation each expected value here comes from.
 *
 * Uses host_stubs.cpp's recording SubmitNKS4CommandWrite/WaitForNKS4CommandWrite to
 * observe exactly what word command.cpp sends, without needing a real kernel.
 */

#include "../omapnks4_internal.h"
#include <cstdio>

extern unsigned int host_stub_last_cmd_word;
extern int host_stub_cmd_word_call_count;
extern bool host_stub_fail_write;
extern unsigned int host_stub_read_event_response;

namespace COmapNKS4Command {
bool CommunicationCheck(void);
bool ReadPortConfiguration(bool *is88, unsigned char *hwVer);
bool SetAllAnalogInputFilter(unsigned char a, unsigned char b);
bool SetRotaryEncoderSampleSpeed(unsigned int n);
bool ConfigureRotaryEncoders(unsigned int n, bool a, bool b);
bool SetLCDBrightness(unsigned char level);
bool ResetModule(unsigned char mode);
}

static int g_fail;

static void check_eq(const char *label, unsigned int got, unsigned int want)
{
	if (got == want) {
		std::printf("  ok    %-55s 0x%08x\n", label, got);
		return;
	}
	std::printf("  FAIL  %-55s got=0x%08x want=0x%08x\n", label, got, want);
	g_fail++;
}

static void check_true(const char *label, bool got)
{
	if (got) {
		std::printf("  ok    %s\n", label);
		return;
	}
	std::printf("  FAIL  %s\n", label);
	g_fail++;
}

int main(void)
{
	std::printf("OmapNKS4Module command.cpp known-answer test\n");
	std::printf("=============================================\n");

	// SetAllAnalogInputFilter@0x12ec0: word = 0x01B00000 | ((a<<4)|b)
	COmapNKS4Command::SetAllAnalogInputFilter(0xa, 0x5);
	check_eq("SetAllAnalogInputFilter(0xa,0x5)", host_stub_last_cmd_word, 0x01B000A5u);
	check_true("SetAllAnalogInputFilter(0x10,0) rejects a>=0x10",
	           !COmapNKS4Command::SetAllAnalogInputFilter(0x10, 0));

	// SetRotaryEncoderSampleSpeed@0x12fc0: word = 0x00800000 | (n<<8)
	COmapNKS4Command::SetRotaryEncoderSampleSpeed(0x12);
	check_eq("SetRotaryEncoderSampleSpeed(0x12)", host_stub_last_cmd_word, 0x00801200u);

	// SetLCDBrightness@0x12ff0: word = 0xC7000000 | (level<<16) - level in the REG byte
	COmapNKS4Command::SetLCDBrightness(0x7f);
	check_eq("SetLCDBrightness(0x7f)", host_stub_last_cmd_word, 0xC77F0000u);

	// ResetModule@0x13010: word = 0x06000000 | (mode<<16)
	COmapNKS4Command::ResetModule(0x02);
	check_eq("ResetModule(0x02)", host_stub_last_cmd_word, 0x06020000u);

	// ConfigureRotaryEncoders@0x12f40: n==0 is a valid no-op (ground truth: previously
	// assumed to be an error). No command word should be sent.
	host_stub_cmd_word_call_count = 0;
	check_true("ConfigureRotaryEncoders(0,_,_) returns true (no-op)",
	           COmapNKS4Command::ConfigureRotaryEncoders(0, false, false));
	check_eq("ConfigureRotaryEncoders(0,_,_) sends nothing",
	         (unsigned int)host_stub_cmd_word_call_count, 0u);

	// n in [1,4]: 3-word sequence word1=0x01810000|(((n-1)|flags)<<8), then the two
	// fixed words 0x01830000 and 0x01820100 (last word observed via the stub).
	host_stub_cmd_word_call_count = 0;
	check_true("ConfigureRotaryEncoders(3,true,false) succeeds",
	           COmapNKS4Command::ConfigureRotaryEncoders(3, true, false));
	check_eq("ConfigureRotaryEncoders(3,true,false) sends 3 words",
	         (unsigned int)host_stub_cmd_word_call_count, 3u);
	check_eq("ConfigureRotaryEncoders(3,true,false) last word = word3",
	         host_stub_last_cmd_word, 0x01820100u);

	check_true("ConfigureRotaryEncoders(5,_,_) rejects n>4",
	           !COmapNKS4Command::ConfigureRotaryEncoders(5, false, false));

	// ReadPortConfiguration: reg echo 0x0171 in the response's top 16 bits.
	host_stub_read_event_response = 0x01710503u; // hwVer=0x03, is88key bit set
	bool is88 = false; unsigned char hw = 0;
	check_true("ReadPortConfiguration succeeds on 0x0171 echo",
	           COmapNKS4Command::ReadPortConfiguration(&is88, &hw));
	check_eq("ReadPortConfiguration decodes hwVer", hw, 0x03u);
	check_true("ReadPortConfiguration decodes is88key", is88);

	host_stub_read_event_response = 0x00990503u; // wrong echo
	check_true("ReadPortConfiguration fails on mismatched echo",
	           !COmapNKS4Command::ReadPortConfiguration(&is88, &hw));

	std::printf("%s: %d failure(s)\n", g_fail ? "FAILED" : "PASSED", g_fail);
	return g_fail ? 1 : 0;
}
