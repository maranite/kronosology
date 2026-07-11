# OA.ko drop-in reconstruction — build + verify
#
# Three independent checks:
#   make objs    - compile each reconstructed TU to a target-ABI object (codegen check,
#                  faithful to OA_322.ko: x86-32, -mregparm=3, freestanding, no C++ runtime).
#   make verify  - build & run the host known-answer tests for the pure auth math.
#   make ko KDIR=... - the REAL kernel-module build via Kbuild, same dual-mode
#                  convention as AT88VirtualChip/Makefile and
#                  KorgUsbAudioVirtualDriver/Makefile. Requires KDIR pointing
#                  at a PREPARED (`make modules_prepare`) Kronos 2.6.32.11-korg
#                  kernel source tree ON A LOCAL FILESYSTEM (not /home/share,
#                  which is SMB and breaks the symlinks Kbuild needs --
#                  /home/build/linux-2.6.32.11 is the working local copy).
#                  This is EXPECTED to still show many "Unknown symbol"
#                  warnings from modpost for the ~150+ CSTGGlobal methods and
#                  manager bodies not yet reconstructed -- that's fine, those
#                  resolve at insmod time against other loaded modules /
#                  OA.ko's own not-yet-written code, same as any real driver's
#                  external dependencies. What this build step actually
#                  proves is that every reconstructed TU compiles cleanly
#                  against the REAL kernel headers with the REAL target
#                  toolchain flags -- the thing `make objs`'s freestanding
#                  host compile can't fully verify on its own.
#   make         - objs + verify (the two host-only checks).
#
# Needs a multilib g++ (for -m32) and a host cc.  Objects are throwaway proof-of-compile
# artifacts (gitignored); the real .ko link is a later stage once all TUs are reconstructed.

ifneq ($(KERNELRELEASE),)

# ---- Kbuild sub-make: building the real .ko -------------------------------
obj-m := OA.o
OA-objs := src/auth/klm_manager.o \
           src/auth/products.o \
           src/auth/atmel_setup.o \
           src/auth/cdrom_check.o \
           src/auth/parse_auth.o \
           src/auth/verify_auth_string.o \
           src/auth/process_oacmd.o \
           src/auth/oa_cmd_proc.o \
           src/crypto/blowfish.o \
           src/crypto/moancjsd82.o \
           src/crypto/cb32.o \
           src/crypto/md5.o \
           src/mem/bank_memory.o \
           src/mem/quad_list.o \
           src/mem/scale.o \
           src/mem/new_delete.o \
           src/mem/heap_manager.o \
           src/mem/heap_manager_alloc_static.o \
           src/engine/engine.o \
           src/engine/global.o \
           src/engine/cc_info_table.o \
           src/engine/global_ctor.o \
           src/engine/engine_startup_bits.o \
           src/engine/engine_startup_bits2.o \
           src/engine/engine_init.o \
           src/engine/voice_models.o \
           src/engine/managers.o \
           src/engine/cost_profile.o \
           src/engine/startup_file.o \
           src/engine/sk_stg_gate.o \
           src/engine/midi_clock_sync.o \
           src/engine/effect_manager_run_effects.o \
           src/engine/lfo_stepseq_quad.o \
           src/engine/wave_seq_manager.o \
           src/engine/midi_dispatcher.o \
           src/engine/midi_queue_writer.o \
           src/engine/midi_queue.o \
           src/engine/midi_port_manager.o \
           src/engine/audio_input_mixer.o \
           src/engine/waveseq_setlist_init.o \
           src/engine/alias_bank_init.o \
           src/engine/alias_bank_convert.o \
           src/engine/waveseq_generator.o \
           src/engine/smoother_init.o \
           src/engine/smoother_ctor.o \
           src/engine/smoother_cancel.o \
           src/engine/controller_rt_data_init.o \
           src/engine/controller_rt_data_ctor.o \
           src/engine/slot_voice_data_ctor.o \
           src/engine/vector_manager.o \
           src/engine/vector_manager_init.o \
           src/engine/vector_eg_ctors.o \
           src/engine/push_unsolicited_message.o \
           src/engine/front_panel_smoothers.o \
           src/engine/audio_bus_manager.o \
           src/engine/program_slot_ctor.o \
           src/engine/program_ctor.o \
           src/engine/combi_ctor.o \
           src/engine/sequence_ctor.o \
           src/engine/streaming_event_manager.o \
           src/engine/sampling_interface_ctor.o \
           src/engine/channel_values_set_controller_value.o \
           src/engine/channel_values_reset.o \
           src/engine/audio_input_use_settings.o \
           src/engine/message_processor.o \
           src/engine/hdr_record_track.o \
           src/engine/hdr_manager_init.o \
           src/engine/controller_rt_data_set_assignment.o \
           src/engine/slot_voice_data_midi_filters.o \
           src/engine/slot_voice_data_update_midi_filter_resend.o \
           src/engine/slot_voice_data_free.o \
           src/engine/load_balancer_static.o \
           src/engine/performance_vars_set_is_dying.o \
           src/engine/drum_kit_data.o \
           src/engine/playback_buffer_events.o \
           src/engine/playback_event_methods.o \
           src/engine/wave_sample_convert.o \
           src/engine/playback_subrate.o \
           src/engine/lfo_tables.o \
           src/engine/eq_coefficients.o \
           src/init/init_module.o \
           src/init/debug_marker.o \
           src/init/stgheap_init.o \
           src/init/shmemproc_init.o \
           src/init/setup_global_resources.o \
           src/init/keybed_init.o \
           src/init/comport.o \
           src/init/comport_init.o \
           src/init/audio_start.o \
           src/init/rtfifo_init.o \
           src/init/cpu_affinity.o \
           src/init/startup_helpers.o \
           src/init/file_io.o \
           src/init/stg_daemons.o \
           src/engine/tick_count.o \
           src/engine/controller_info_perf_deactivate.o \
           src/init/rtwrap.o \
           src/init/drumpad_init.o \
           src/init/keybed_debounce.o \
           src/init/calibration_data.o \
           src/auth/atmel_primitives.o \
           src/auth/atmel_deax.o \
           src/auth/atmel_zone_io.o \
           src/init/daemon_lifecycle.o \
           src/stub/bar2_stubs.o \
           src/stub/bar2_stubs_auth.o \
           src/stub/bar2_stubs_c.o

# Matches every other reconstructed C++ kernel module's confirmed build
# flags exactly (AT88VirtualChip, KorgUsbAudioVirtualDriver, OmapNKS4Module).
ccflags-y := -mregparm=3 -fno-exceptions -fno-rtti -fno-threadsafe-statics \
             -fno-use-cxa-atexit -fpermissive -I$(src)/include

# CXX is NOT one of Kbuild's own variables (only $(CC) is defined in this
# context) -- it expands to empty here unless set explicitly, which
# silently corrupts the compile command (the leading flag gets
# misinterpreted as the program name by the shell). Confirmed by actually
# running this build for the first time across all three reconstructed
# C++ kernel modules that use this pattern (AT88VirtualChip,
# KorgUsbAudioVirtualDriver, this one) -- none had been Kbuild-tested
# before, so this bug was latent in all three identically.
CXX ?= g++

# The kernel build's own inherited -msoft-float flag makes GCC emit
# calls to libgcc soft-float helpers (__divsf3 etc.) for this file's
# genuine floating-point arithmetic (CSTGCPUInfo's confirmed real
# fmul/fdiv/rcpss sequence) -- helpers that will never resolve on a
# real kernel (same class of issue InitializeSTGHeap's own double-math
# already hit, sec 10.45). Unlike that case, hand-rewriting this one
# in pure integer math isn't practical (IEEE-754 float semantics,
# including an approximate SSE reciprocal), and the REAL binary itself
# confirms real hardware FPU instructions are actually used here (raw
# fmul/fdiv/rcpss in the disassembly, not calls to helper functions) --
# so overriding this one file's own flags to allow real hardware FP
# codegen is the faithful fix, not a workaround.
CFLAGS_engine_startup_bits.o := -mhard-float -msse2 -mfpmath=sse

# Same reasoning as engine_startup_bits.o above: CLoadBalancer::
# Initialize()/CPowerOffTimer::Initialize()/CSTGDiskCostManager::
# Initialize() all do genuine float arithmetic confirmed real in the
# original binary (fmul/fisttp/rcpss sequences), which would otherwise
# pull in unresolvable libgcc soft-float helpers under the kernel's own
# -msoft-float default.
CFLAGS_engine_startup_bits2.o := -mhard-float -msse2 -mfpmath=sse

# Same reasoning again: CSTGGlobal::UpdatePerfChangeHoldTime() (sec
# 10.71) does genuine float arithmetic confirmed real in the original
# binary (fld/fmul/fisttp), which would otherwise pull in unresolvable
# libgcc soft-float helpers under the kernel's own -msoft-float default.
CFLAGS_global.o := -mhard-float -msse2 -mfpmath=sse

# Same reasoning again: scale.cpp's own linear-scale mapping does
# genuine double-precision arithmetic (confirmed real in the original
# binary), which would otherwise pull in unresolvable libgcc
# soft-float/double helpers under the kernel's own -msoft-float
# default. Bar-2 fix (2026-07-02).
CFLAGS_scale.o := -mhard-float -msse2 -mfpmath=sse

# Same reasoning again: smoother_init.cpp's own genuine double-precision
# 0.04*cyclesPerTick computation (confirmed real, sec 10.86) would
# otherwise pull in unresolvable libgcc soft-float/double helpers.
CFLAGS_smoother_init.o := -mhard-float -msse2 -mfpmath=sse

# Same reasoning again: midi_clock_sync.cpp's own genuine (but confirmed
# exact/unambiguous, batch 21) double-precision arithmetic in
# CSTGMIDIClockSyncBase::Initialize()/CSTGMIDIClockSync::
# CSTGMIDIClockSync()/RecomputeMIDIClockInterval() would otherwise pull
# in unresolvable libgcc soft-float/double helpers.
CFLAGS_midi_clock_sync.o := -mhard-float -msse2 -mfpmath=sse

# Same reasoning again: effect_manager_run_effects.cpp's own
# CSTGEffectManager::RunEffects() (batch 49) does a genuine plain-C float
# clamp (against confirmed real 40.0f/300.0f `.rodata.cst4` constants)
# on CSTGMIDIClockSync::GetFilteredTempoBPM()'s own return value -- would
# otherwise pull in unresolvable libgcc soft-float helpers under the
# kernel's own -msoft-float default.
CFLAGS_effect_manager_run_effects.o := -mhard-float -msse2 -mfpmath=sse

# Same reasoning again: wave_sample_convert.cpp's own scalar reimplementation
# of the real SSE2 sample-conversion leaf functions (int16->float, scale,
# mix-add) does genuine float arithmetic confirmed real in the original
# binary (real movaps/cvtdq2ps/mulps/addps SSE2 code there -- this project's
# scalar reimplementation is functionally, not byte-, equivalent), which
# would otherwise pull in unresolvable libgcc soft-float helpers
# (__mulsf3/__addsf3/__floatsisf) under the kernel's own -msoft-float
# default -- confirmed via a throwaway -m32 -mregparm=3 -msoft-float compile
# before this override was added (batch 26).
CFLAGS_wave_sample_convert.o := -mhard-float -msse2 -mfpmath=sse

# Same reasoning again: lfo_tables.cpp's ctor does genuine plain-C float
# arithmetic (phase-ramp multiplies/subtracts, sine-table negation) to
# populate several lookup tables -- would otherwise pull in unresolvable
# libgcc soft-float helpers under the kernel's own -msoft-float default
# (batch 28).
CFLAGS_lfo_tables.o := -mhard-float -msse2 -mfpmath=sse

# Same reasoning again: keybed_debounce.cpp's CSTGKeybedKeyDebounceFilter_
# Initialize() does one genuine plain-C float multiply chain (busGainScale
# * 50.0f * 0.001f) to compute a debounce-window sample count -- would
# otherwise pull in unresolvable libgcc soft-float/double helpers under the
# kernel's own -msoft-float default (batch 38). The result has no
# fractional part for this exact chain, so SSE2 vs the real x87 extended-
# precision sequence is not a fidelity concern here (see that file's own
# header comment).
CFLAGS_keybed_debounce.o := -mhard-float -msse2 -mfpmath=sse

$(obj)/%.o: $(src)/%.cpp
	$(CXX) $(c_flags) -x c++ -c -o $@ $<

else

TARGET_CXX      ?= g++
TARGET_CXXFLAGS := -m32 -mregparm=3 -fno-exceptions -fno-rtti -ffreestanding -fno-builtin \
                   -O2 -Wall -Iinclude

HOST_CC         ?= cc
HOST_CXX        ?= g++
HOST_CFLAGS     := -Wall -Wextra -O2 -Iinclude
HOST_CXXFLAGS   := -Wall -Wextra -O2 -Iinclude

SRC := src/auth/klm_manager.cpp \
       src/auth/products.cpp \
       src/auth/atmel_setup.cpp \
       src/auth/cdrom_check.cpp \
       src/auth/parse_auth.cpp \
       src/auth/verify_auth_string.cpp \
       src/auth/process_oacmd.cpp \
       src/auth/oa_cmd_proc.cpp \
       src/crypto/blowfish.cpp \
       src/crypto/moancjsd82.cpp \
       src/crypto/cb32.cpp \
       src/crypto/md5.cpp \
       src/mem/bank_memory.cpp \
       src/mem/quad_list.cpp \
       src/mem/scale.cpp \
       src/mem/new_delete.cpp \
       src/mem/heap_manager.cpp \
       src/mem/heap_manager_alloc_static.cpp \
       src/engine/engine.cpp \
       src/engine/global.cpp \
       src/engine/cc_info_table.cpp \
       src/engine/global_ctor.cpp \
       src/engine/engine_startup_bits.cpp \
       src/engine/engine_startup_bits2.cpp \
       src/engine/engine_init.cpp \
       src/engine/voice_models.cpp \
       src/engine/managers.cpp \
       src/engine/cost_profile.cpp \
       src/engine/startup_file.cpp \
       src/engine/sk_stg_gate.cpp \
       src/engine/midi_clock_sync.cpp \
       src/engine/effect_manager_run_effects.cpp \
       src/engine/lfo_stepseq_quad.cpp \
       src/engine/wave_seq_manager.cpp \
       src/engine/waveseq_generator.cpp \
       src/engine/smoother_ctor.cpp \
       src/engine/smoother_cancel.cpp \
       src/engine/controller_rt_data_ctor.cpp \
       src/engine/slot_voice_data_ctor.cpp \
       src/engine/alias_bank_convert.cpp \
       src/engine/midi_dispatcher.cpp \
       src/engine/midi_port_manager.cpp \
       src/engine/vector_manager.cpp \
       src/engine/vector_manager_init.cpp \
       src/engine/vector_eg_ctors.cpp \
       src/engine/front_panel_smoothers.cpp \
       src/engine/audio_bus_manager.cpp \
       src/engine/program_slot_ctor.cpp \
       src/engine/program_ctor.cpp \
       src/engine/combi_ctor.cpp \
       src/engine/sequence_ctor.cpp \
       src/engine/streaming_event_manager.cpp \
       src/engine/sampling_interface_ctor.cpp \
       src/engine/channel_values_set_controller_value.cpp \
       src/engine/channel_values_reset.cpp \
       src/engine/audio_input_use_settings.cpp \
       src/engine/message_processor.cpp \
       src/engine/hdr_record_track.cpp \
       src/engine/hdr_manager_init.cpp \
       src/engine/controller_rt_data_set_assignment.cpp \
       src/engine/slot_voice_data_midi_filters.cpp \
       src/engine/slot_voice_data_update_midi_filter_resend.cpp \
       src/engine/slot_voice_data_free.cpp \
       src/engine/load_balancer_static.cpp \
       src/engine/performance_vars_set_is_dying.cpp \
       src/engine/drum_kit_data.cpp \
       src/engine/playback_buffer_events.cpp \
       src/engine/playback_event_methods.cpp \
       src/engine/wave_sample_convert.cpp \
       src/engine/playback_subrate.cpp \
       src/engine/lfo_tables.cpp \
       src/engine/eq_coefficients.cpp \
       src/init/init_module.cpp \
       src/init/stgheap_init.cpp \
       src/init/shmemproc_init.cpp \
       src/init/setup_global_resources.cpp \
       src/init/keybed_init.cpp \
       src/init/comport.cpp \
       src/init/comport_init.cpp \
       src/init/audio_start.cpp \
       src/init/rtfifo_init.cpp \
       src/init/cpu_affinity.cpp \
       src/init/startup_helpers.cpp \
       src/init/file_io.cpp \
       src/init/stg_daemons.cpp \
       src/engine/tick_count.cpp \
       src/engine/controller_info_perf_deactivate.cpp \
       src/init/rtwrap.cpp \
       src/init/drumpad_init.cpp \
       src/init/keybed_debounce.cpp \
       src/init/calibration_data.cpp \
       src/auth/atmel_primitives.cpp \
       src/auth/atmel_deax.cpp \
       src/auth/atmel_zone_io.cpp \
       src/init/daemon_lifecycle.cpp

# `.hostobj`, NOT `.o` (sec 10.143): this rule's own outputs used to be
# plain `.o` files at the exact same paths Kbuild's `OA-objs` list uses
# below. Since `make`/`make objs` (this freestanding host codegen check)
# and `make ko` (the real Kbuild module build) both existed at file
# scope with no `clean` between them, running the host check BEFORE the
# Kbuild build left stale, host-flagged `.o` files that Kbuild's own
# make then saw as already up to date relative to their unchanged `.cpp`
# sources -- and silently reused for the final `.ko` link instead of
# recompiling with the real kernel flags (`-DMODULE`/`-D__KERNEL__`/
# `-fno-pie`/etc). Confirmed real: this is exactly how the sec 10.142
# `-fno-pie` fix appeared to "stop working" immediately after being
# fixed -- `make` (host) then `make ko` (Kbuild) without a `clean`
# silently linked pre-`-fno-pie` host objects into `OA.ko`, reintroducing
# the 761 `R_386_GOTPC` relocations sec 10.142 had just eliminated. A
# distinct suffix makes the two build modes' outputs unable to collide,
# so no ordering/staleness discipline is needed between them.
OBJ := $(SRC:.cpp=.hostobj)

TESTS := verify/test_klm_auth verify/test_crypto verify/test_bank_memory verify/test_quad_list \
         verify/test_scale verify/test_new_delete verify/test_engine verify/test_managers \
         verify/test_global verify/test_init_module verify/test_stgheap_init \
         verify/test_shmemproc_init verify/test_pcmmodproc_init verify/test_setup_global_resources \
         verify/test_keybed_init verify/test_audio_start verify/test_rtfifo_init \
         verify/test_cpu_affinity verify/test_comport verify/test_comport_init \
         verify/test_global_ctor verify/test_engine_startup_bits \
         verify/test_engine_init verify/test_heap_manager verify/test_engine_startup_bits2 \
         verify/test_cost_profile verify/test_lfo_stepseq_quad verify/test_wave_seq_manager \
         verify/test_midi_dispatcher verify/test_vector_manager verify/test_vector_manager_init \
         verify/test_vector_eg_ctors verify/test_midi_queue_writer \
         verify/test_midi_queue verify/test_midi_port_manager \
         verify/test_waveseq_setlist_init verify/test_alias_bank_init \
         verify/test_smoother_init verify/test_push_unsolicited_message \
         verify/test_startup_file verify/test_sk_stg_gate \
         verify/test_midi_clock_sync \
         verify/test_audio_input_mixer \
         verify/test_alias_bank_convert verify/test_waveseq_generator \
         verify/test_audio_bus_manager verify/test_program_slot_ctor \
         verify/test_sequence_ctor verify/test_smoother_cancel \
         verify/test_controller_rt_data_ctor verify/test_slot_voice_data_ctor \
         verify/test_sampling_interface_ctor \
         verify/test_channel_values verify/test_hdr_record_track \
         verify/test_smoother_ctor verify/test_hdr_manager_init \
         verify/test_controller_rt_data_set_assignment verify/test_slot_voice_data_midi_filters \
         verify/test_heap_manager_alloc_static verify/test_slot_voice_data_free \
         verify/test_channel_values_reset verify/test_audio_input_use_settings \
         verify/test_message_processor \
         verify/test_load_balancer_static verify/test_performance_vars_set_is_dying \
         verify/test_drum_kit_data verify/test_playback_buffer_events \
         verify/test_playback_event_methods \
         verify/test_wave_sample_convert \
         verify/test_playback_subrate \
         verify/test_lfo_tables verify/test_eq_coefficients \
         verify/test_startup_helpers \
         verify/test_file_io \
         verify/test_stg_daemons \
         verify/test_tick_count \
         verify/test_controller_info_perf_deactivate \
         verify/test_rtwrap \
         verify/test_drumpad_init \
         verify/test_keybed_debounce \
         verify/test_atmel_primitives \
         verify/test_calibration_data \
         verify/test_daemon_lifecycle \
         verify/test_voice_models \
         verify/test_atmel_deax verify/test_program_ctor \
         verify/test_combi_ctor \
         verify/test_atmel_zone_io \
         verify/test_effect_manager_run_effects

CRYPTO_SRC := src/crypto/blowfish.cpp src/crypto/moancjsd82.cpp src/crypto/cb32.cpp src/crypto/md5.cpp

KDIR ?= /home/build/linux-kronos
PWD  := $(shell pwd)

.PHONY: all objs verify clean ko ko-clean
all: objs verify

objs: $(OBJ)

%.hostobj: %.cpp
	$(TARGET_CXX) $(TARGET_CXXFLAGS) -c $< -o $@

verify: $(TESTS)
	@for t in $(TESTS); do echo "== $$t =="; ./$$t; done

verify/test_klm_auth: verify/test_klm_auth.c include/oa_authmath.h
	$(HOST_CC) $(HOST_CFLAGS) $< -o $@

verify/test_crypto: verify/test_crypto.cpp $(CRYPTO_SRC) \
                     include/oa_crypto.h include/oa_blowfish.h include/oa_md5.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_crypto.cpp $(CRYPTO_SRC) -o $@

verify/test_bank_memory: verify/test_bank_memory.cpp src/mem/bank_memory.cpp include/oa_bank_memory.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_bank_memory.cpp src/mem/bank_memory.cpp -o $@

verify/test_quad_list: verify/test_quad_list.cpp src/mem/quad_list.cpp include/oa_quad.h include/oa_types.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_quad_list.cpp src/mem/quad_list.cpp -o $@

verify/test_scale: verify/test_scale.cpp src/mem/scale.cpp include/oa_scale.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_scale.cpp src/mem/scale.cpp -o $@

verify/test_new_delete: verify/test_new_delete.cpp src/mem/new_delete.cpp include/oa_new_delete.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_new_delete.cpp src/mem/new_delete.cpp -o $@

verify/test_engine: verify/test_engine.cpp src/engine/engine.cpp src/engine/global.cpp src/engine/cc_info_table.cpp src/engine/managers.cpp src/mem/bank_memory.cpp include/oa_engine.h include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_engine.cpp src/engine/engine.cpp src/engine/global.cpp src/engine/cc_info_table.cpp src/engine/managers.cpp src/mem/bank_memory.cpp -o $@

verify/test_managers: verify/test_managers.cpp src/engine/managers.cpp src/engine/streaming_event_manager.cpp src/mem/bank_memory.cpp include/oa_engine.h include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_managers.cpp src/engine/managers.cpp src/engine/streaming_event_manager.cpp src/mem/bank_memory.cpp -o $@

verify/test_global: verify/test_global.cpp src/engine/global.cpp src/engine/cc_info_table.cpp src/engine/managers.cpp src/mem/bank_memory.cpp src/engine/controller_rt_data_init.cpp src/engine/midi_queue.cpp include/oa_global.h include/oa_engine.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_global.cpp src/engine/global.cpp src/engine/cc_info_table.cpp src/engine/managers.cpp src/mem/bank_memory.cpp src/engine/controller_rt_data_init.cpp src/engine/midi_queue.cpp -o $@

verify/test_init_module: verify/test_init_module.cpp src/init/init_module.cpp include/oa_init.h include/oa_atmel.h include/oa_stgheap_init.h include/oa_shmemproc_init.h include/oa_keybed_init.h include/oa_comport.h include/oa_audio_start.h include/oa_rtfifo_init.h include/oa_cpu_affinity.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_init_module.cpp src/init/init_module.cpp -o $@

verify/test_stgheap_init: verify/test_stgheap_init.cpp src/init/stgheap_init.cpp include/oa_stgheap_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_stgheap_init.cpp src/init/stgheap_init.cpp -o $@

verify/test_shmemproc_init: verify/test_shmemproc_init.cpp src/init/shmemproc_init.cpp include/oa_shmemproc_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_shmemproc_init.cpp src/init/shmemproc_init.cpp -o $@

verify/test_pcmmodproc_init: verify/test_pcmmodproc_init.cpp src/auth/oa_cmd_proc.cpp include/oa_cmd_proc.h include/process_oacmd.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_pcmmodproc_init.cpp src/auth/oa_cmd_proc.cpp -o $@

verify/test_setup_global_resources: verify/test_setup_global_resources.cpp src/init/setup_global_resources.cpp src/mem/bank_memory.cpp src/mem/new_delete.cpp include/oa_setup_global_resources.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_setup_global_resources.cpp src/init/setup_global_resources.cpp src/mem/bank_memory.cpp src/mem/new_delete.cpp -o $@

verify/test_keybed_init: verify/test_keybed_init.cpp src/init/keybed_init.cpp src/init/comport.cpp include/oa_keybed_init.h include/oa_comport.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_keybed_init.cpp src/init/keybed_init.cpp src/init/comport.cpp -o $@

verify/test_comport: verify/test_comport.cpp src/init/comport.cpp include/oa_comport.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_comport.cpp src/init/comport.cpp -o $@

verify/test_comport_init: verify/test_comport_init.cpp src/init/comport_init.cpp src/init/comport.cpp include/oa_comport.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_comport_init.cpp src/init/comport_init.cpp src/init/comport.cpp -o $@

verify/test_global_ctor: verify/test_global_ctor.cpp src/engine/global_ctor.cpp src/engine/global.cpp src/engine/cc_info_table.cpp src/engine/managers.cpp src/mem/bank_memory.cpp src/engine/controller_rt_data_init.cpp include/oa_global.h include/oa_engine.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_global_ctor.cpp src/engine/global_ctor.cpp src/engine/global.cpp src/engine/cc_info_table.cpp src/engine/managers.cpp src/mem/bank_memory.cpp src/engine/controller_rt_data_init.cpp -o $@

verify/test_engine_startup_bits: verify/test_engine_startup_bits.cpp src/engine/engine_startup_bits.cpp include/oa_setup_global_resources.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_engine_startup_bits.cpp src/engine/engine_startup_bits.cpp -o $@

verify/test_engine_init: verify/test_engine_init.cpp src/engine/engine_init.cpp src/engine/front_panel_smoothers.cpp src/mem/bank_memory.cpp include/oa_engine.h include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_engine_init.cpp src/engine/engine_init.cpp src/engine/front_panel_smoothers.cpp src/mem/bank_memory.cpp -o $@

verify/test_audio_start: verify/test_audio_start.cpp src/init/audio_start.cpp include/oa_audio_start.h include/oa_engine.h include/oa_cpu_affinity.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_audio_start.cpp src/init/audio_start.cpp -o $@

verify/test_rtfifo_init: verify/test_rtfifo_init.cpp src/init/rtfifo_init.cpp include/oa_rtfifo_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_rtfifo_init.cpp src/init/rtfifo_init.cpp -o $@

verify/test_heap_manager: verify/test_heap_manager.cpp src/mem/heap_manager.cpp include/oa_heapmanager.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_heap_manager.cpp src/mem/heap_manager.cpp -o $@

verify/test_engine_startup_bits2: verify/test_engine_startup_bits2.cpp src/engine/engine_startup_bits2.cpp src/engine/engine_startup_bits.cpp src/engine/managers.cpp src/mem/bank_memory.cpp include/oa_engine.h include/oa_engine_init.h include/oa_setup_global_resources.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_engine_startup_bits2.cpp src/engine/engine_startup_bits2.cpp src/engine/engine_startup_bits.cpp src/engine/managers.cpp src/mem/bank_memory.cpp -o $@

verify/test_cost_profile: verify/test_cost_profile.cpp src/engine/cost_profile.cpp include/oa_setup_global_resources.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_cost_profile.cpp src/engine/cost_profile.cpp -o $@

verify/test_startup_file: verify/test_startup_file.cpp src/engine/startup_file.cpp include/oa_setup_global_resources.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_startup_file.cpp src/engine/startup_file.cpp -o $@

verify/test_sk_stg_gate: verify/test_sk_stg_gate.cpp src/engine/sk_stg_gate.cpp include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_sk_stg_gate.cpp src/engine/sk_stg_gate.cpp -o $@

verify/test_midi_clock_sync: verify/test_midi_clock_sync.cpp src/engine/midi_clock_sync.cpp src/engine/sk_stg_gate.cpp include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) -mhard-float -msse2 -mfpmath=sse verify/test_midi_clock_sync.cpp src/engine/midi_clock_sync.cpp src/engine/sk_stg_gate.cpp -o $@

verify/test_effect_manager_run_effects: verify/test_effect_manager_run_effects.cpp src/engine/effect_manager_run_effects.cpp src/engine/midi_clock_sync.cpp src/engine/sk_stg_gate.cpp include/oa_engine.h include/oa_engine_init.h include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) -mhard-float -msse2 -mfpmath=sse verify/test_effect_manager_run_effects.cpp src/engine/effect_manager_run_effects.cpp src/engine/midi_clock_sync.cpp src/engine/sk_stg_gate.cpp -o $@

verify/test_lfo_stepseq_quad: verify/test_lfo_stepseq_quad.cpp src/engine/lfo_stepseq_quad.cpp include/oa_engine.h include/oa_engine_init.h include/oa_setup_global_resources.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_lfo_stepseq_quad.cpp src/engine/lfo_stepseq_quad.cpp -o $@

verify/test_wave_seq_manager: verify/test_wave_seq_manager.cpp src/engine/wave_seq_manager.cpp include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_wave_seq_manager.cpp src/engine/wave_seq_manager.cpp -o $@

verify/test_midi_dispatcher: verify/test_midi_dispatcher.cpp src/engine/midi_dispatcher.cpp include/oa_engine.h include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_midi_dispatcher.cpp src/engine/midi_dispatcher.cpp -o $@

verify/test_midi_queue_writer: verify/test_midi_queue_writer.cpp src/engine/midi_queue_writer.cpp include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_midi_queue_writer.cpp src/engine/midi_queue_writer.cpp -o $@

verify/test_midi_queue: verify/test_midi_queue.cpp src/engine/midi_queue.cpp include/oa_global.h include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_midi_queue.cpp src/engine/midi_queue.cpp -o $@

verify/test_midi_port_manager: verify/test_midi_port_manager.cpp src/engine/midi_port_manager.cpp src/engine/midi_queue.cpp src/engine/midi_queue_writer.cpp include/oa_global.h include/oa_engine.h include/oa_engine_init.h include/oa_heap.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_midi_port_manager.cpp src/engine/midi_port_manager.cpp src/engine/midi_queue.cpp src/engine/midi_queue_writer.cpp -o $@

verify/test_audio_input_mixer: verify/test_audio_input_mixer.cpp src/engine/audio_input_mixer.cpp src/mem/bank_memory.cpp include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_audio_input_mixer.cpp src/engine/audio_input_mixer.cpp src/mem/bank_memory.cpp -o $@

verify/test_waveseq_setlist_init: verify/test_waveseq_setlist_init.cpp src/engine/waveseq_setlist_init.cpp include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_waveseq_setlist_init.cpp src/engine/waveseq_setlist_init.cpp -o $@

verify/test_alias_bank_init: verify/test_alias_bank_init.cpp src/engine/alias_bank_init.cpp include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_alias_bank_init.cpp src/engine/alias_bank_init.cpp -o $@

verify/test_alias_bank_convert: verify/test_alias_bank_convert.cpp src/engine/alias_bank_convert.cpp include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_alias_bank_convert.cpp src/engine/alias_bank_convert.cpp -o $@

verify/test_waveseq_generator: verify/test_waveseq_generator.cpp src/engine/waveseq_generator.cpp include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_waveseq_generator.cpp src/engine/waveseq_generator.cpp -o $@

verify/test_audio_bus_manager: verify/test_audio_bus_manager.cpp src/engine/audio_bus_manager.cpp include/oa_engine.h include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_audio_bus_manager.cpp src/engine/audio_bus_manager.cpp -o $@

verify/test_program_slot_ctor: verify/test_program_slot_ctor.cpp src/engine/program_slot_ctor.cpp include/oa_global.h include/oa_internal.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_program_slot_ctor.cpp src/engine/program_slot_ctor.cpp -o $@

verify/test_sequence_ctor: verify/test_sequence_ctor.cpp src/engine/sequence_ctor.cpp include/oa_global.h include/oa_internal.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_sequence_ctor.cpp src/engine/sequence_ctor.cpp -o $@

verify/test_smoother_init: verify/test_smoother_init.cpp src/engine/smoother_init.cpp src/mem/bank_memory.cpp include/oa_engine_init.h include/oa_bank_memory.h include/oa_setup_global_resources.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_smoother_init.cpp src/engine/smoother_init.cpp src/mem/bank_memory.cpp -o $@

verify/test_smoother_cancel: verify/test_smoother_cancel.cpp src/engine/smoother_cancel.cpp include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_smoother_cancel.cpp src/engine/smoother_cancel.cpp -o $@

verify/test_controller_rt_data_ctor: verify/test_controller_rt_data_ctor.cpp src/engine/controller_rt_data_ctor.cpp include/oa_global.h include/oa_internal.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_controller_rt_data_ctor.cpp src/engine/controller_rt_data_ctor.cpp -o $@

verify/test_slot_voice_data_ctor: verify/test_slot_voice_data_ctor.cpp src/engine/slot_voice_data_ctor.cpp src/mem/bank_memory.cpp include/oa_global.h include/oa_bank_memory.h include/oa_internal.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_slot_voice_data_ctor.cpp src/engine/slot_voice_data_ctor.cpp src/mem/bank_memory.cpp -o $@

verify/test_sampling_interface_ctor: verify/test_sampling_interface_ctor.cpp src/engine/sampling_interface_ctor.cpp include/oa_global.h include/oa_internal.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_sampling_interface_ctor.cpp src/engine/sampling_interface_ctor.cpp -o $@

verify/test_channel_values: verify/test_channel_values.cpp src/engine/channel_values_set_controller_value.cpp src/engine/cc_info_table.cpp include/oa_global.h include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_channel_values.cpp src/engine/channel_values_set_controller_value.cpp src/engine/cc_info_table.cpp -o $@

verify/test_hdr_record_track: verify/test_hdr_record_track.cpp src/engine/hdr_record_track.cpp src/mem/bank_memory.cpp include/oa_global.h include/oa_engine.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_hdr_record_track.cpp src/engine/hdr_record_track.cpp src/mem/bank_memory.cpp -o $@

verify/test_smoother_ctor: verify/test_smoother_ctor.cpp src/engine/smoother_ctor.cpp include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_smoother_ctor.cpp src/engine/smoother_ctor.cpp -o $@

verify/test_hdr_manager_init: verify/test_hdr_manager_init.cpp src/engine/hdr_manager_init.cpp src/engine/hdr_record_track.cpp src/engine/audio_input_mixer.cpp src/mem/bank_memory.cpp include/oa_global.h include/oa_engine.h include/oa_bank_memory.h include/oa_setup_global_resources.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_hdr_manager_init.cpp src/engine/hdr_manager_init.cpp src/engine/hdr_record_track.cpp src/engine/audio_input_mixer.cpp src/mem/bank_memory.cpp -o $@

verify/test_controller_rt_data_set_assignment: verify/test_controller_rt_data_set_assignment.cpp src/engine/controller_rt_data_set_assignment.cpp src/engine/cc_info_table.cpp include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_controller_rt_data_set_assignment.cpp src/engine/controller_rt_data_set_assignment.cpp src/engine/cc_info_table.cpp -o $@

verify/test_slot_voice_data_midi_filters: verify/test_slot_voice_data_midi_filters.cpp src/engine/slot_voice_data_midi_filters.cpp include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_slot_voice_data_midi_filters.cpp src/engine/slot_voice_data_midi_filters.cpp -o $@

verify/test_heap_manager_alloc_static: verify/test_heap_manager_alloc_static.cpp src/mem/heap_manager_alloc_static.cpp include/oa_setup_global_resources.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_heap_manager_alloc_static.cpp src/mem/heap_manager_alloc_static.cpp -o $@

verify/test_slot_voice_data_free: verify/test_slot_voice_data_free.cpp src/engine/slot_voice_data_free.cpp include/oa_global.h include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_slot_voice_data_free.cpp src/engine/slot_voice_data_free.cpp -o $@

verify/test_channel_values_reset: verify/test_channel_values_reset.cpp src/engine/channel_values_reset.cpp src/engine/cc_info_table.cpp include/oa_global.h include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_channel_values_reset.cpp src/engine/channel_values_reset.cpp src/engine/cc_info_table.cpp -o $@

verify/test_audio_input_use_settings: verify/test_audio_input_use_settings.cpp src/engine/audio_input_use_settings.cpp src/engine/audio_input_mixer.cpp src/mem/bank_memory.cpp include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_audio_input_use_settings.cpp src/engine/audio_input_use_settings.cpp src/engine/audio_input_mixer.cpp src/mem/bank_memory.cpp -o $@

verify/test_message_processor: verify/test_message_processor.cpp src/engine/message_processor.cpp include/oa_engine.h include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_message_processor.cpp src/engine/message_processor.cpp -o $@

verify/test_push_unsolicited_message: verify/test_push_unsolicited_message.cpp src/engine/push_unsolicited_message.cpp include/oa_rtfifo_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_push_unsolicited_message.cpp src/engine/push_unsolicited_message.cpp -o $@

verify/test_vector_manager: verify/test_vector_manager.cpp src/engine/vector_manager.cpp include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_vector_manager.cpp src/engine/vector_manager.cpp -o $@

verify/test_vector_manager_init: verify/test_vector_manager_init.cpp src/engine/vector_manager_init.cpp src/engine/vector_manager.cpp include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_vector_manager_init.cpp src/engine/vector_manager_init.cpp src/engine/vector_manager.cpp -o $@

verify/test_vector_eg_ctors: verify/test_vector_eg_ctors.cpp src/engine/vector_eg_ctors.cpp include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_vector_eg_ctors.cpp src/engine/vector_eg_ctors.cpp -o $@

verify/test_cpu_affinity: verify/test_cpu_affinity.cpp src/init/cpu_affinity.cpp include/oa_cpu_affinity.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_cpu_affinity.cpp src/init/cpu_affinity.cpp -o $@

verify/test_load_balancer_static: verify/test_load_balancer_static.cpp src/engine/load_balancer_static.cpp include/oa_engine.h include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_load_balancer_static.cpp src/engine/load_balancer_static.cpp -o $@

verify/test_performance_vars_set_is_dying: verify/test_performance_vars_set_is_dying.cpp src/engine/performance_vars_set_is_dying.cpp include/oa_engine.h include/oa_engine_init.h include/oa_global.h include/oa_setup_global_resources.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_performance_vars_set_is_dying.cpp src/engine/performance_vars_set_is_dying.cpp -o $@

verify/test_drum_kit_data: verify/test_drum_kit_data.cpp src/engine/drum_kit_data.cpp include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_drum_kit_data.cpp src/engine/drum_kit_data.cpp -o $@

verify/test_playback_buffer_events: verify/test_playback_buffer_events.cpp src/engine/playback_buffer_events.cpp src/engine/playback_event_methods.cpp src/engine/managers.cpp src/engine/engine_startup_bits2.cpp src/engine/hdr_manager_init.cpp src/engine/audio_input_mixer.cpp src/engine/hdr_record_track.cpp src/mem/bank_memory.cpp include/oa_engine.h include/oa_engine_init.h include/oa_setup_global_resources.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_playback_buffer_events.cpp src/engine/playback_buffer_events.cpp src/engine/playback_event_methods.cpp src/engine/managers.cpp src/engine/engine_startup_bits2.cpp src/engine/hdr_manager_init.cpp src/engine/audio_input_mixer.cpp src/engine/hdr_record_track.cpp src/mem/bank_memory.cpp -o $@

verify/test_playback_event_methods: verify/test_playback_event_methods.cpp src/engine/playback_event_methods.cpp include/oa_engine.h include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_playback_event_methods.cpp src/engine/playback_event_methods.cpp -o $@

verify/test_wave_sample_convert: verify/test_wave_sample_convert.cpp src/engine/wave_sample_convert.cpp include/oa_engine.h include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_wave_sample_convert.cpp src/engine/wave_sample_convert.cpp -o $@

verify/test_playback_subrate: verify/test_playback_subrate.cpp src/engine/playback_subrate.cpp include/oa_engine.h include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_playback_subrate.cpp src/engine/playback_subrate.cpp -o $@

verify/test_lfo_tables: verify/test_lfo_tables.cpp src/engine/lfo_tables.cpp include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_lfo_tables.cpp src/engine/lfo_tables.cpp -o $@

verify/test_eq_coefficients: verify/test_eq_coefficients.cpp src/engine/eq_coefficients.cpp include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_eq_coefficients.cpp src/engine/eq_coefficients.cpp -o $@

verify/test_startup_helpers: verify/test_startup_helpers.cpp src/init/startup_helpers.cpp include/oa_setup_global_resources.h include/oa_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_startup_helpers.cpp src/init/startup_helpers.cpp -o $@

verify/test_file_io: verify/test_file_io.cpp src/init/file_io.cpp
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_file_io.cpp src/init/file_io.cpp -o $@

verify/test_stg_daemons: verify/test_stg_daemons.cpp src/init/stg_daemons.cpp include/oa_daemons.h include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_stg_daemons.cpp src/init/stg_daemons.cpp -o $@

verify/test_tick_count: verify/test_tick_count.cpp src/engine/tick_count.cpp include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_tick_count.cpp src/engine/tick_count.cpp -o $@

verify/test_controller_info_perf_deactivate: verify/test_controller_info_perf_deactivate.cpp src/engine/controller_info_perf_deactivate.cpp include/oa_global.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_controller_info_perf_deactivate.cpp src/engine/controller_info_perf_deactivate.cpp -o $@

verify/test_rtwrap: verify/test_rtwrap.cpp src/init/rtwrap.cpp
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_rtwrap.cpp src/init/rtwrap.cpp -o $@

verify/test_drumpad_init: verify/test_drumpad_init.cpp src/init/drumpad_init.cpp include/oa_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_drumpad_init.cpp src/init/drumpad_init.cpp -o $@

verify/test_keybed_debounce: verify/test_keybed_debounce.cpp src/init/keybed_debounce.cpp include/oa_keybed_init.h include/oa_engine.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_keybed_debounce.cpp src/init/keybed_debounce.cpp -o $@

verify/test_atmel_primitives: verify/test_atmel_primitives.cpp src/auth/atmel_primitives.cpp include/oa_atmel.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_atmel_primitives.cpp src/auth/atmel_primitives.cpp -o $@

verify/test_calibration_data: verify/test_calibration_data.cpp src/init/calibration_data.cpp
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_calibration_data.cpp src/init/calibration_data.cpp -o $@

verify/test_daemon_lifecycle: verify/test_daemon_lifecycle.cpp src/init/daemon_lifecycle.cpp include/oa_daemons.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_daemon_lifecycle.cpp src/init/daemon_lifecycle.cpp -o $@

verify/test_voice_models: verify/test_voice_models.cpp src/engine/voice_models.cpp src/mem/bank_memory.cpp include/oa_engine.h include/oa_engine_init.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_voice_models.cpp src/engine/voice_models.cpp src/mem/bank_memory.cpp -o $@

verify/test_atmel_deax: verify/test_atmel_deax.cpp src/auth/atmel_deax.cpp include/oa_atmel.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_atmel_deax.cpp src/auth/atmel_deax.cpp -o $@

verify/test_atmel_zone_io: verify/test_atmel_zone_io.cpp src/auth/atmel_zone_io.cpp src/auth/atmel_deax.cpp include/oa_atmel.h include/oa_crypto.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_atmel_zone_io.cpp src/auth/atmel_zone_io.cpp src/auth/atmel_deax.cpp -o $@

verify/test_program_ctor: verify/test_program_ctor.cpp src/engine/program_ctor.cpp include/oa_global.h include/oa_engine_init.h include/oa_internal.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_program_ctor.cpp src/engine/program_ctor.cpp -o $@

verify/test_combi_ctor: verify/test_combi_ctor.cpp src/engine/combi_ctor.cpp src/engine/program_ctor.cpp include/oa_global.h include/oa_engine_init.h include/oa_internal.h
	$(HOST_CXX) $(HOST_CXXFLAGS) verify/test_combi_ctor.cpp src/engine/combi_ctor.cpp src/engine/program_ctor.cpp -o $@

clean:
	rm -f $(OBJ) $(TESTS)

# The real kernel-module build. KDIR must point at a PREPARED, LOCAL-
# FILESYSTEM copy of the Kronos 2.6.32.11-korg kernel source tree (see
# the file header comment -- /home/share breaks the symlinks Kbuild
# needs, hence the /home/build default above).
#
# CORRECTED (2026-07-04): default KDIR switched from the vanilla
# linux-2.6.32.11 tree to /home/build/linux-kronos (cgudrian/linux-
# kronos, Korg's own korg_kronos_defconfig) -- confirmed via direct
# `.gnu.linkonce.this_module` comparison against a real Korg-shipped
# .ko that the vanilla tree produces a WRONG struct module layout
# (312 bytes, init reloc at 0xbc) vs the real one (328 bytes, init
# reloc at 0xd4); see /home/share/project_linux_kronos_kernel_tree.md.
# ARCH=i386 must be passed explicitly (this tree builds a 32-bit target
# on a 64-bit host -- Kbuild does NOT remember ARCH from a prior
# modules_prepare run):
#   make ko KDIR=/home/build/linux-kronos
ko:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) modules

ko-clean:
	$(MAKE) -C $(KDIR) ARCH=i386 M=$(PWD) clean

endif
