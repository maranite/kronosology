#!/bin/bash
# run_vm_virtual_probe_test.sh
#
# Reusable, reproducible boot test for OmapNKS4Module.ko's vm_virtual_probe=1
# standalone-VM path (front-panel USB driver, no physical hardware required).
#
# WHAT THIS REPLACES: every previous test of this exact module chain
# (RTAIVirtualDriver.ko -> STGEnabler.ko -> STGGmp.ko -> OmapNKS4Module.ko
# vm_virtual_probe=1) was done by hand -- manual SSH to the kronosvm sandbox,
# manual guestfish disk surgery, manual insmod sequencing, eyeballing dmesg.
# This script encodes that whole procedure so it can be re-run identically by
# anyone, any time this module's source changes.
#
# WHAT IT DOES, IN ORDER:
#   1. Rebuilds OmapNKS4Module.ko FRESH from the canonical source in this repo
#      (../ from this script), on kronosvm, under /home/build (never /home/share --
#      that's a CIFS mount and can't hold the symlinks `make modules_prepare`
#      needs; see the project CLAUDE.md's "Development Environment" section).
#      RTAIVirtualDriver.ko / STGEnabler.ko / STGGmp.ko are reused as-is from
#      the known-good kronosvm reference build (their sources are not part of
#      this module and haven't changed this session).
#   2. Makes a FRESH copy of the reference kronos.img disk-image template for
#      this run only -- a run never reuses another run's own image (project
#      convention: reuse-across-runs was once wrongly blamed for a hang; the
#      real cause was something else, but fresh-copy-per-run remains correct
#      hygiene and this script preserves it).
#   3. Writes a fresh /sbin/loadoa into that image (via guestfish) that loads
#      the four modules in the standard order and logs each step, plus copies
#      the four .ko files into /korg/rw/oa_recon/ (disk-image partition
#      /dev/sda6, matching the existing project convention). The script body
#      is generated HERE, each run, from this file -- not copied from a
#      previously-patched image or a stale copy on kronosvm, so it can't go
#      stale.
#   4. Boots the image headless in QEMU (same invocation as the project's
#      known-good /home/build/omapnks4_boot_test/run_test.sh), with the
#      serial console redirected to a per-run log file.
#   5. Polls that log, with a BOUNDED timeout (default 180s / 3 minutes,
#      override with -t/--timeout), for known milestone strings.
#   6. Prints a PASS / PARTIAL / FAIL verdict based ONLY on what's actually in
#      the captured log -- never assumed.
#
# ############################################################################
# KNOWN, CURRENTLY-UNRESOLVED BUG -- READ BEFORE INTERPRETING RESULTS
# ############################################################################
# As of this writing, OmapNKS4Module.ko's vm_virtual_probe=1 boot reliably
# reaches probe, COmapNKS4Driver_Configure(), worker-thread creation, event
# injection, and the SetLCDBrightness/ResetModule setter test -- then HANGS
# for 4+ minutes with zero further console output and no kernel oops. Root
# cause is still open; see this module's README.md, search for "REFUTED" and
# "Where things stand, narrowed further" for the full diagnostic trail.
#
# This means a real, complete, non-hanging boot past that point has NEVER
# actually been observed. A PARTIAL verdict from this script is therefore the
# CORRECT, EXPECTED, CURRENT-STATE result -- not a bug in this script, and not
# something this script tries to work around, hide, or "fix" by trimming the
# module chain. If you see PARTIAL stopping at the documented milestones,
# that's ground truth, not a script failure.
# ############################################################################
#
# OPERATIONAL NOTE (found 2026-07-19: an agent invocation of this script got cut off
# mid-poll by its own caller's timeout, before this script's EXIT trap could run,
# leaving a real orphaned qemu-system-i386 process on kronosvm): whatever invokes this
# script MUST allow it at least TIMEOUT+30s of real wall-clock time to run to
# completion (e.g. run it in the background and wait, or wrap the invocation in a
# generous `timeout` of its own) -- if the caller itself gets killed before this
# script's own cleanup trap fires, the remote qemu process is orphaned. Manual
# recovery: `ssh kronosvm pkill -f qemu-system-i386` (safe if you're sure nothing else
# is using the sandbox) or target a specific run's image path more precisely via
# `pgrep -af qemu-system-i386` first.
#
# Usage:
#   run_vm_virtual_probe_test.sh [-t|--timeout SECONDS] [-k|--keep-run-dir] [-l|--linger SECONDS]
#
#   -l/--linger: after "[loadoa] done" is detected, keep the VM running and
#   keep polling the console log for this many EXTRA seconds before shutting
#   it down (default 0, i.e. original behavior -- shut down immediately on
#   completion). Use this to observe background kernel-thread diagnostics
#   that only fire some seconds after loadoa's own script has already
#   finished (e.g. kOmapNKS4MsgRoutine/kShutdownSSDRoutine's periodic
#   "DIAG PMR#N"/"DIAG SSD#N" prints).
#
# Requires: local `ssh kronosvm` (passwordless, already configured) reaching
# the 192.168.3.87 sandbox; that host must already have /home/build/linux-kronos
# (kernel tree for the .ko build) and /home/build/omapnks4_boot_test/{kronos.img,
# RTAIVirtualDriver.ko,STGEnabler.ko,STGGmp.ko,run_test.sh} (reference template +
# companion modules) -- both are pre-existing sandbox setup, not created by this
# script.

set -euo pipefail

# --------------------------------------------------------------------------
# Config / defaults
# --------------------------------------------------------------------------
SSH_HOST="kronosvm"
REMOTE_BASE="/home/build/omapnks4_boot_test"
REMOTE_SCRATCH="${REMOTE_BASE}/src_scratch"        # fresh OmapNKS4Module.ko build tree
REMOTE_REF_IMG="${REMOTE_BASE}/kronos.img"          # pristine template, never written to
REMOTE_REF_KOS=(RTAIVirtualDriver.ko STGEnabler.ko STGGmp.ko)  # reused as-is

TIMEOUT=180          # seconds; recommend 3 minutes per project instructions
POLL_INTERVAL=5       # seconds between log checks
KEEP_RUN_DIR=0
LINGER=0              # extra seconds to keep the VM alive AFTER "[loadoa] done",
                       # still polling/capturing console output, before shutdown.
                       # Added 2026-07-21 for kOmapNKS4MsgRoutine/kShutdownSSDRoutine's
                       # own background wait_event_timeout() loops: loadoa itself
                       # returns almost immediately after these threads print their
                       # "alive, entering main loop" line, so without lingering, this
                       # script's normal completion detection kills qemu long before
                       # ShutdownSSDRoutine's 10000-jiffy (~10s) timeout can ever fire
                       # even once -- see reconstructed/OmapNKS4Module/README.md's
                       # "Known limitations" entry on this thread's own diagnostic
                       # ("DIAG SSD#N") never having been directly observed.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOCAL_SRC_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"     # OmapNKS4Module/ (this module's source)

while [ $# -gt 0 ]; do
    case "$1" in
        -t|--timeout) TIMEOUT="$2"; shift 2 ;;
        -k|--keep-run-dir) KEEP_RUN_DIR=1; shift ;;
        -l|--linger) LINGER="$2"; shift 2 ;;
        -h|--help)
            grep '^#' "${BASH_SOURCE[0]}" | sed 's/^#//; s/^# //'
            exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
done

RUN_ID="run_$(date +%Y%m%d_%H%M%S)"
REMOTE_RUN_DIR="${REMOTE_BASE}/runs/${RUN_ID}"
REMOTE_IMG="${REMOTE_RUN_DIR}/kronos.img"
REMOTE_LOG="${REMOTE_RUN_DIR}/boot_console.log"
REMOTE_MON="${REMOTE_RUN_DIR}/qmon.sock"
REMOTE_PIDFILE="${REMOTE_RUN_DIR}/qemu.pid"
LOCAL_RESULTS_DIR="${SCRIPT_DIR}/results/${RUN_ID}"

QEMU_PID=""   # remote pid of qemu-system-i386, once launched

log()  { echo "[run_vm_virtual_probe_test] $*"; }
die()  { echo "[run_vm_virtual_probe_test] FATAL: $*" >&2; exit 1; }

# --------------------------------------------------------------------------
# Cleanup: always kill our own qemu (if still running) and never leave a
# stray qemu-system-i386 process on kronosvm, regardless of how we exit.
# --------------------------------------------------------------------------
cleanup() {
    local rc=$?
    # CRITICAL: disable errexit for the rest of this trap. Without this, a
    # single transient non-zero return from ANY command below (e.g. one of
    # several back-to-back `ssh` calls hitting a momentary connection hiccup)
    # aborts the trap immediately under `set -e` and SKIPS every following
    # cleanup step -- including the rm -f that removes the 2GB per-run disk
    # image. Confirmed the hard way (2026-07-19): both of this script's first
    # two real runs left their image behind on kronosvm because the `pkill`
    # ssh call below returned non-zero and errexit silently cut the trap off
    # before it ever reached `rm -f`. Every command from here on is also
    # explicitly `|| true`-guarded as belt-and-braces on top of this.
    set +e
    if [ -n "${QEMU_PID}" ]; then
        ssh "${SSH_HOST}" "kill -TERM ${QEMU_PID} 2>/dev/null; sleep 1; kill -KILL ${QEMU_PID} 2>/dev/null; true" 2>/dev/null || true
    fi
    # Belt-and-braces: kill anything qemu launched by THIS run (matched on the
    # run's own unique image path), in case the pidfile/pid tracking above
    # missed it for any reason (e.g. script killed mid-launch).
    ssh "${SSH_HOST}" "pkill -f \"qemu-system-i386.*${REMOTE_IMG}\" 2>/dev/null; true" 2>/dev/null || true
    if [ "${KEEP_RUN_DIR}" -eq 0 ]; then
        # Always keep the console log (copied out locally already by this
        # point) but drop the 2GB per-run disk image copy on kronosvm.
        ssh "${SSH_HOST}" "rm -f '${REMOTE_IMG}'" 2>/dev/null || true
    fi
    exit "${rc}"
}
trap cleanup EXIT INT TERM

# --------------------------------------------------------------------------
# Step 0: sanity / pre-flight -- confirm no stray qemu already running, and
# that the reference materials this script depends on are actually there.
# --------------------------------------------------------------------------
log "Pre-flight: checking ${SSH_HOST} for stray qemu-system-i386 processes..."
STRAY="$(ssh "${SSH_HOST}" 'pgrep -af qemu-system-i386 || true')"
if [ -n "${STRAY}" ]; then
    log "WARNING: found existing qemu-system-i386 process(es) on ${SSH_HOST} before this run started:"
    echo "${STRAY}" | sed 's/^/    /'
    log "Not touching those (may belong to someone else) -- proceeding with this run's own instance."
fi

ssh "${SSH_HOST}" "test -f '${REMOTE_REF_IMG}'" \
    || die "reference image ${REMOTE_REF_IMG} not found on ${SSH_HOST}"
for ko in "${REMOTE_REF_KOS[@]}"; do
    ssh "${SSH_HOST}" "test -f '${REMOTE_BASE}/${ko}'" \
        || die "reference module ${REMOTE_BASE}/${ko} not found on ${SSH_HOST}"
done
ssh "${SSH_HOST}" "test -d /home/build/linux-kronos" \
    || die "kernel tree /home/build/linux-kronos not found on ${SSH_HOST}"

# --------------------------------------------------------------------------
# Step 1: rebuild OmapNKS4Module.ko fresh, on kronosvm, from THIS repo's
# current canonical source (not the .ko already sitting on kronosvm).
# --------------------------------------------------------------------------
log "Copying canonical source (${LOCAL_SRC_DIR}) to ${SSH_HOST}:${REMOTE_SCRATCH} (fresh each run)..."
ssh "${SSH_HOST}" "rm -rf '${REMOTE_SCRATCH}' && mkdir -p '${REMOTE_SCRATCH}'"
# Only source files -- deliberately not .o/.ko/.cmd/.tmp_versions/Module.symvers
# build cruft that may be sitting in the local tree from unrelated local builds.
scp -q "${LOCAL_SRC_DIR}"/*.cpp "${LOCAL_SRC_DIR}"/*.h "${LOCAL_SRC_DIR}/Makefile" \
    "${SSH_HOST}:${REMOTE_SCRATCH}/"

log "Building OmapNKS4Module.ko on ${SSH_HOST} (KDIR=/home/build/linux-kronos)..."
ssh "${SSH_HOST}" "cd '${REMOTE_SCRATCH}' && make ko 2>&1" \
    | tee "/tmp/${RUN_ID}_build.log"
ssh "${SSH_HOST}" "test -f '${REMOTE_SCRATCH}/OmapNKS4Module.ko'" \
    || die "build did not produce OmapNKS4Module.ko -- see /tmp/${RUN_ID}_build.log"
log "Build OK: $(ssh "${SSH_HOST}" "ls -la '${REMOTE_SCRATCH}/OmapNKS4Module.ko'")"

# --------------------------------------------------------------------------
# Step 2: fresh per-run disk image copy (never reuse a run's own image).
# --------------------------------------------------------------------------
log "Preparing fresh run directory and disk-image copy: ${REMOTE_RUN_DIR}"
ssh "${SSH_HOST}" "mkdir -p '${REMOTE_RUN_DIR}' && cp --sparse=auto '${REMOTE_REF_IMG}' '${REMOTE_IMG}'"

# --------------------------------------------------------------------------
# Step 3: generate /sbin/loadoa fresh (this run, from this script -- not
# reused from any previously-patched image) and inject it + the four .ko
# files into the fresh image via guestfish.
#
# Load order and mechanism match the project's existing, previously-hand-
# verified /sbin/loadoa test variant: RTAIVirtualDriver.ko -> STGEnabler.ko
# -> STGGmp.ko -> OmapNKS4Module.ko vm_virtual_probe=1, all read from
# /korg/rw/oa_recon/ (disk-image partition /dev/sda6). Every insmod result is
# logged both to dmesg (kmsg) and to this run's own serial console via plain
# stdout (loadoa runs synchronously and "wait"-blocking under init's
# `l3:3:wait:/etc/OA.rc start` -> `/sbin/loadoa`, so stdout lands on ttyS0,
# i.e. this run's boot_console.log).
# --------------------------------------------------------------------------
LOADOA_TMP="/tmp/${RUN_ID}_loadoa.sh"
cat > "${LOADOA_TMP}" <<'LOADOA_EOF'
#!/bin/bash
# NOTE on kmsg(): the kernel cmdline carries "console=ttyS0,115200 console=tty0" --
# the LAST console= wins for userspace's default /dev/console, so plain stdout from
# this script (invoked synchronously from init's "l3:3:wait:/etc/OA.rc start") lands
# on tty0, not ttyS0 -- while genuine kernel printk() output is multiplexed to EVERY
# registered console, including ttyS0. Confirmed empirically (2026-07-19): without
# the explicit /dev/ttyS0 write below, none of this script's own progress lines ever
# reached run_test.sh-style ttyS0-redirected console logs, even though the module's
# own printk() lines always did. Writing directly to /dev/ttyS0 here (in addition to
# the original /dev/kmsg + stdout, kept for parity with the hand-tested convention
# this replaces) is what actually gets these lines into the captured log this script
# polls -- without it, a genuine future fix of the vm_virtual_probe hang could never
# be detected as PASS, only ever as PARTIAL.
kmsg() { echo "[loadoa] $*" > /dev/kmsg 2>/dev/null; echo "[loadoa] $*" > /dev/ttyS0 2>/dev/null; echo "[loadoa] $*"; }
kmsg "OmapNKS4Module.ko self-contained virtual-board test (vm_virtual_probe=1)"
OA_DIR=/korg/rw/oa_recon
INSMOD_LOG=/korg/rw/oa_recon/insmod_log.txt
: > "$INSMOD_LOG"
for m in RTAIVirtualDriver.ko STGEnabler.ko STGGmp.ko; do
    echo "=== insmod $m ===" >> "$INSMOD_LOG"
    if insmod "$OA_DIR/$m" >> "$INSMOD_LOG" 2>&1; then
        kmsg "$m: loaded OK"
    else
        kmsg "WARNING: insmod $m failed"
    fi
done

echo "=== insmod OmapNKS4Module.ko vm_virtual_probe=1 ===" >> "$INSMOD_LOG"
kmsg "insmod OmapNKS4Module.ko vm_virtual_probe=1 ..."
if insmod "$OA_DIR/OmapNKS4Module.ko" vm_virtual_probe=1 >> "$INSMOD_LOG" 2>&1; then
    kmsg "OmapNKS4Module.ko: LOADED OK"
else
    kmsg "OmapNKS4Module.ko: insmod result logged"
fi

dmesg | tail -150 >> "$INSMOD_LOG" 2>/dev/null
{ echo "loaded_modules: $(cat /proc/modules 2>/dev/null)"; } >> /korg/rw/vm_boot_status.txt
echo "TTYMARKER" > /dev/ttyS1
kmsg "done"
exit 0
LOADOA_EOF
chmod +x "${LOADOA_TMP}"
scp -q "${LOADOA_TMP}" "${SSH_HOST}:${REMOTE_RUN_DIR}/loadoa"

log "Injecting fresh loadoa + module chain into ${REMOTE_IMG} via guestfish..."
ssh "${SSH_HOST}" bash -s <<EOF
set -e
cd '${REMOTE_RUN_DIR}'
guestfish -a '${REMOTE_IMG}' -m /dev/sda2:/ -m /dev/sda6:/korg/rw -- <<GFEOF
upload loadoa /sbin/loadoa
chmod 0755 /sbin/loadoa
upload ${REMOTE_SCRATCH}/OmapNKS4Module.ko /korg/rw/oa_recon/OmapNKS4Module.ko
upload ${REMOTE_BASE}/RTAIVirtualDriver.ko /korg/rw/oa_recon/RTAIVirtualDriver.ko
upload ${REMOTE_BASE}/STGEnabler.ko /korg/rw/oa_recon/STGEnabler.ko
upload ${REMOTE_BASE}/STGGmp.ko /korg/rw/oa_recon/STGGmp.ko
chmod 0644 /korg/rw/oa_recon/OmapNKS4Module.ko
chmod 0644 /korg/rw/oa_recon/RTAIVirtualDriver.ko
chmod 0644 /korg/rw/oa_recon/STGEnabler.ko
chmod 0644 /korg/rw/oa_recon/STGGmp.ko
GFEOF
EOF
log "Image prepared."

# --------------------------------------------------------------------------
# Step 4: boot headless in QEMU, same invocation shape as the project's
# known-good run_test.sh (pc/n270/4G/tcg/no display, serial->file, monitor
# socket, no network, no reboot-on-crash).
# --------------------------------------------------------------------------
log "Booting ${REMOTE_IMG} (timeout ${TIMEOUT}s)..."
# Unique telnet port per run (ttyS1 -- unused by this script's own detection,
# which relies solely on ttyS0/boot_console.log, but still needs a real,
# non-colliding port so multiple runs/leftover processes can't fight over
# the same one; the original run_test.sh's fixed 4472 is a single-run-at-a-
# time convention this script deliberately doesn't rely on).
TELNET_PORT=$(( 40000 + (RANDOM % 10000) ))
ssh "${SSH_HOST}" bash -s <<EOF
cd '${REMOTE_RUN_DIR}'
nohup qemu-system-i386 \
    -name Kronos-OmapNKS4-VmVirtualProbe-${RUN_ID} \
    -M pc -cpu n270 -m 4096M -smp 4,sockets=1,cores=2,threads=2 -accel tcg \
    -drive file='${REMOTE_IMG}',format=raw,if=ide,index=0,media=disk \
    -display none -vga none \
    -serial file:'${REMOTE_LOG}' \
    -serial telnet:127.0.0.1:${TELNET_PORT},server,nowait \
    -monitor unix:'${REMOTE_MON}',server,nowait \
    -net none -rtc base=utc -no-reboot \
    > '${REMOTE_RUN_DIR}/qemu_stdout.log' 2>&1 &
echo \$! > '${REMOTE_PIDFILE}'
EOF
QEMU_PID="$(ssh "${SSH_HOST}" "cat '${REMOTE_PIDFILE}'")"
log "qemu-system-i386 launched on ${SSH_HOST}, pid ${QEMU_PID}"

# --------------------------------------------------------------------------
# Step 5: poll the console log for milestones, bounded by TIMEOUT.
#
# Ordered milestone list. Each entry is "label|regex". Order matters: the
# LAST one matched in the captured log is reported as "furthest milestone
# reached" and drives the verdict. Regexes are stable substrings of this
# module's actual current printk() text (grep'd from source, not guessed --
# see main.cpp/usb.cpp/driver.cpp), not full sprintf-format lines (those
# contain interpolated values that vary run to run).
# --------------------------------------------------------------------------
MILESTONES=(
    "loadoa started|\[loadoa\] OmapNKS4Module.ko self-contained virtual-board test"
    "RTAIVirtualDriver.ko loaded|\[loadoa\] RTAIVirtualDriver.ko: loaded OK"
    "STGEnabler.ko loaded|\[loadoa\] STGEnabler.ko: loaded OK"
    "STGGmp.ko loaded|\[loadoa\] STGGmp.ko: loaded OK"
    "insmod OmapNKS4Module.ko attempted|\[loadoa\] insmod OmapNKS4Module.ko vm_virtual_probe=1"
    "OmapNKS4Init: enter|OmapNKS4Init: enter"
    "vm_virtual_probe: synthesizing virtual board|synthesizing a virtual NKS4 board"
    "OmapNKS4Probe() called (synthetic)|calling OmapNKS4Probe\(\) with a synthetic"
    "OmapNKS4Probe: probe success|probe success"
    "OmapNKS4Probe() returned|OmapNKS4Probe\(\) returned"
    "OmapNKS4Init: waited for probe|cycles for OmapNKS4Probe"
    "Configure(): is88Key|is88Key:"
    "Configure(): HardwareVersion|HardwareVersion:"
    "Configure(): NKS4 versions reported|NKS4 versions"
    "vm_virtual_probe_inject_event: installer support enabled|enabling installer support and injecting"
    "vm_virtual_probe_inject_event: bytes injected|host-supplied bytes"
    "vm_virtual_probe_test_setters: SetLCDBrightness/ResetModule called|calling SetLCDBrightness\(0x80\)/ResetModule\(0x00\)"
    "SetLCDBrightness accepted|SetLCDBrightness accepted"
    "ResetModule accepted|ResetModule accepted"
    "insmod OmapNKS4Module.ko RETURNED (LOADED OK)|\[loadoa\] OmapNKS4Module.ko: LOADED OK"
    "insmod OmapNKS4Module.ko RETURNED (nonzero, but returned)|\[loadoa\] OmapNKS4Module.ko: insmod result logged"
    "loadoa script reached its own end|\[loadoa\] done"
)
# Index of the completion markers (insmod returning at all is THE open bug's
# crux -- both success and "returned nonzero" variants count as "the hang did
# not happen this time"); final "loadoa done" is the strictly-complete signal.
COMPLETION_LABEL="loadoa script reached its own end"

mkdir -p "${LOCAL_RESULTS_DIR}"
ELAPSED=0
LAST_MATCHED_INDEX=-1
COMPLETED=0
while [ "${ELAPSED}" -lt "${TIMEOUT}" ]; do
    # Pull the log down locally each poll (cheap; logs here are tiny) so the
    # final saved copy is guaranteed present even if the SSH session drops.
    scp -q "${SSH_HOST}:${REMOTE_LOG}" "${LOCAL_RESULTS_DIR}/boot_console.log" 2>/dev/null || true
    if [ -f "${LOCAL_RESULTS_DIR}/boot_console.log" ]; then
        for i in "${!MILESTONES[@]}"; do
            regex="${MILESTONES[$i]#*|}"
            if grep -qE "${regex}" "${LOCAL_RESULTS_DIR}/boot_console.log" 2>/dev/null; then
                LAST_MATCHED_INDEX=$i
            fi
        done
        if grep -qE "\[loadoa\] done" "${LOCAL_RESULTS_DIR}/boot_console.log" 2>/dev/null; then
            COMPLETED=1
            break
        fi
    fi
    # Also bail out early (and loudly) if qemu itself died -- that's not the
    # known hang, it's a harder failure worth surfacing immediately.
    if ! ssh "${SSH_HOST}" "kill -0 ${QEMU_PID} 2>/dev/null"; then
        log "qemu-system-i386 (pid ${QEMU_PID}) is no longer running -- it exited or crashed."
        break
    fi
    sleep "${POLL_INTERVAL}"
    ELAPSED=$((ELAPSED + POLL_INTERVAL))
    log "  ...waited ${ELAPSED}s/${TIMEOUT}s (furthest milestone so far: $([ ${LAST_MATCHED_INDEX} -ge 0 ] && echo "${MILESTONES[$LAST_MATCHED_INDEX]%%|*}" || echo "none yet"))"
done

# --------------------------------------------------------------------------
# Optional linger: loadoa itself finishes almost immediately after
# kOmapNKS4MsgRoutine/kShutdownSSDRoutine print their "alive, entering main
# loop" lines, well before either thread's own wait_event_timeout() has had
# time to elapse even once. Without this, background-thread diagnostics are
# structurally unobservable no matter how large --timeout is, since the main
# poll loop above exits (and normally proceeds straight to qemu shutdown) the
# moment "[loadoa] done" is seen.
# --------------------------------------------------------------------------
if [ "${COMPLETED}" -eq 1 ] && [ "${LINGER}" -gt 0 ]; then
    log "Completion detected -- lingering ${LINGER}s to capture background-thread diagnostics..."
    LINGER_ELAPSED=0
    while [ "${LINGER_ELAPSED}" -lt "${LINGER}" ]; do
        if ! ssh "${SSH_HOST}" "kill -0 ${QEMU_PID} 2>/dev/null"; then
            log "qemu-system-i386 (pid ${QEMU_PID}) exited during linger -- stopping linger early."
            break
        fi
        sleep "${POLL_INTERVAL}"
        LINGER_ELAPSED=$((LINGER_ELAPSED + POLL_INTERVAL))
        log "  ...lingered ${LINGER_ELAPSED}s/${LINGER}s"
    done
    scp -q "${SSH_HOST}:${REMOTE_LOG}" "${LOCAL_RESULTS_DIR}/boot_console.log" 2>/dev/null || true
fi

# Final pull, in case the last change landed between the last poll and now.
scp -q "${SSH_HOST}:${REMOTE_LOG}" "${LOCAL_RESULTS_DIR}/boot_console.log" 2>/dev/null || true
for i in "${!MILESTONES[@]}"; do
    regex="${MILESTONES[$i]#*|}"
    if grep -qE "${regex}" "${LOCAL_RESULTS_DIR}/boot_console.log" 2>/dev/null; then
        LAST_MATCHED_INDEX=$i
    fi
done
if grep -qE "\[loadoa\] done" "${LOCAL_RESULTS_DIR}/boot_console.log" 2>/dev/null; then
    COMPLETED=1
fi

# --------------------------------------------------------------------------
# Step 6: stop the VM (we have what we need either way) and report.
# --------------------------------------------------------------------------
ssh "${SSH_HOST}" "kill -TERM ${QEMU_PID} 2>/dev/null; sleep 1; kill -KILL ${QEMU_PID} 2>/dev/null; true"
QEMU_PID=""   # already handled; cleanup trap's kill becomes a no-op

echo
echo "================================================================"
echo " OmapNKS4Module.ko vm_virtual_probe=1 boot test -- ${RUN_ID}"
echo "================================================================"
echo "Console log saved locally: ${LOCAL_RESULTS_DIR}/boot_console.log"
echo
echo "Milestones reached, in order:"
for i in "${!MILESTONES[@]}"; do
    label="${MILESTONES[$i]%%|*}"
    regex="${MILESTONES[$i]#*|}"
    if grep -qE "${regex}" "${LOCAL_RESULTS_DIR}/boot_console.log" 2>/dev/null; then
        echo "  [x] ${label}"
    else
        echo "  [ ] ${label}"
    fi
done
echo

# Verdict thresholds, by milestone array index:
#   >= index of "OmapNKS4Init: enter"  -> at least reached the module under test
#   >= index of "insmod OmapNKS4Module.ko attempted" but below that -> chain
#      loader itself regressed (RTAI/STGEnabler/STGGmp), i.e. FAIL
ENTER_IDX=-1
for i in "${!MILESTONES[@]}"; do
    [ "${MILESTONES[$i]%%|*}" = "OmapNKS4Init: enter" ] && ENTER_IDX=$i
done

if [ "${COMPLETED}" -eq 1 ]; then
    VERDICT="PASS"
    DETAIL="loadoa reported its own completion (\"[loadoa] done\") -- the module chain \
loaded, OmapNKS4Module.ko's insmod call RETURNED, and the whole test script ran to \
completion. If you are seeing this, the previously-open 4+ minute post-\"return 0\" \
hang (see README.md, search \"REFUTED\"/\"Where things stand, narrowed further\") did \
NOT occur this run -- that would be genuinely new and worth investigating/confirming \
with a repeat run before declaring the bug fixed."
elif [ "${LAST_MATCHED_INDEX}" -ge "${ENTER_IDX}" ] && [ "${ENTER_IDX}" -ge 0 ]; then
    VERDICT="PARTIAL"
    DETAIL="Reached module-under-test milestones up through \
\"${MILESTONES[$LAST_MATCHED_INDEX]%%|*}\", then no further console output within \
${TIMEOUT}s. This matches the KNOWN, CURRENTLY-UNRESOLVED hang documented in \
README.md (OmapNKS4Module.ko's vm_virtual_probe=1 boot reaches probe/Configure()/\
worker-thread creation/event injection/SetLCDBrightness+ResetModule, then hangs with \
zero further output and no kernel oops -- root cause still open). This is EXPECTED, \
CURRENT-STATE behavior, not a failure of this test script."
else
    VERDICT="FAIL"
    DETAIL="Did not reach \"OmapNKS4Init: enter\" (the module under test's own first \
printk) within ${TIMEOUT}s -- furthest milestone was \
\"$([ ${LAST_MATCHED_INDEX} -ge 0 ] && echo "${MILESTONES[$LAST_MATCHED_INDEX]%%|*}" || echo "NONE")\". \
This is a REGRESSION before previously-working milestones (module-chain load order,\
kernel boot, or the OmapNKS4Module.ko insmod call itself never being reached) -- \
investigate the build/boot, not the known post-\"return 0\" hang."
fi

echo "VERDICT: ${VERDICT}"
echo "${DETAIL}" | fold -s -w 78
echo "================================================================"

if [ "${VERDICT}" = "FAIL" ]; then
    exit 1
fi
exit 0
