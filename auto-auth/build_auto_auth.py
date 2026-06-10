#!/usr/bin/env python3
"""
build_auto_auth.py — produce the auto-auth USB-stick updater package.

Output: ./output/auto-auth/
  Copy the contents of that directory to the root of a FAT-formatted USB
  stick.  Insert into the Kronos and trigger an OS update from the front
  panel.

What happens on the Kronos:
  pretar.sh  — display message, pre-flight check
  tar        — extracts only /tmp/auth.stamp (harmless)
  posttar.sh — insmod oa_authgen.ko FROM THE USB STICK (never writes to disk),
               generates and submits auth strings for every installed EX library,
               then rmmod oa_authgen to leave no trace.

Stock-system compatible: no prior installation required.  The module is loaded
from /mnt/updaterSource/ (the mounted USB stick) and removed after use.

Requires: python3.  oa_authgen.ko must be built first (see oa_authgen/Makefile).
"""

import hashlib
import io
import shutil
import stat
import subprocess
import sys
import tarfile
from pathlib import Path

HERE    = Path(__file__).resolve().parent
STAGING = HERE / "output" / "auto-auth"
KO_SRC  = HERE / "oa_authgen" / "oa_authgen.ko"
DUM_SRC = HERE / "updater_msg" / "DisplayUpdaterMessage"

# UpdateOS package signing key (from UpdateOS .data section VMA 0x0813bac8)
_UPDATER_KEY = bytes([0x13, 0xd0, 0xaf, 0xef, 0xe0, 0x3c, 0x9b, 0x92,
                      0x16, 0x2f, 0xae, 0xff, 0x77, 0x53, 0x55, 0xe1])

# ── Kronos-side shell scripts (BusyBox sh, run during UpdateOS) ───────────────
# These execute on the Kronos, not the host.  They are written verbatim to the
# USB package.  Use raw strings so backslashes and $-variables are literal.

PRETAR_SH = r"""#!/bin/sh
# auto-auth pretar: pre-flight check.

LOG=/tmp/auth.log
exec >> "$LOG" 2>&1
echo "--- pretar start $(date) ---"

DUM=/mnt/updaterSource/DisplayUpdaterMessage
[ -x "$DUM" ] || DUM=true

"$DUM" "kronosology: EX authorisation..." || true
"""

POSTTAR_SH = r"""#!/bin/sh
# Authorise all installed EX libraries.
#
# Generates auth strings via GEN:Sxxx -> /proc/.oaauth for each installed EX
# and writes them directly to /korg/rw/Startup/AuthorizationStrings.
# OA.ko reads the file on the next normal boot (VerifyAuthorizationString).

LOG=/korg/rw/auth.log
exec >> "$LOG" 2>&1
echo "--- posttar start $(date) ---"

OAAUTH=/proc/.oaauth
AUTHFILE=/korg/rw/Startup/AuthorizationStrings
OPTIONS=/korg/rw/Options
MODULE=/mnt/updaterSource/oa_authgen.ko
DUM=/mnt/updaterSource/DisplayUpdaterMessage
[ -x "$DUM" ] || DUM=true

echo "MODULE=$([ -f "$MODULE" ] && echo yes || echo NO)"
echo "OPTIONS=$([ -d "$OPTIONS" ] && echo yes || echo no)"
echo "AUTHFILE=$([ -f "$AUTHFILE" ] && echo yes || echo no)"

rm -f /tmp/auth.stamp

mkdir -p "$(dirname "$AUTHFILE")"
AUTHTMP="${AUTHFILE}.tmp$$"
authorised=0
failed=0

# Load oa_authgen.ko from USB (native GPA — no OA.ko addresses needed).
WE_LOADED=0
if [ ! -e "$OAAUTH" ]; then
    if [ ! -f "$MODULE" ]; then
        echo "ERROR: $MODULE not found on USB"
        "$DUM" "ERROR: oa_authgen.ko missing" || true
        exit 1
    fi
    "$DUM" "Loading EX auth module..." || true
    /sbin/insmod "$MODULE"
    echo "insmod exit=$?"
    sleep 1
    WE_LOADED=1
fi

if [ ! -e "$OAAUTH" ]; then
    echo "ERROR: /proc/.oaauth unavailable after insmod"
    "$DUM" "ERROR: auth module failed" || true
    exit 1
fi

echo "OAAUTH=yes WE_LOADED=$WE_LOADED"

total=0
for opt_file in "$OPTIONS"/S*; do
    [ -f "$opt_file" ] || continue
    uid=$(awk -F',' 'NR==4{ gsub(/[[:space:]]/, "", $2); print $2; exit }' \
          "$opt_file" 2>/dev/null)
    [ -z "$uid" ] || [ "$uid" = "0" ] && continue
    total=$((total + 1))
done

echo "found $total EX libraries to authorise"
"$DUM" "Authorising EX libraries..." || true
echo "set 0" > /proc/OmapNKS4ProgressBar 2>/dev/null || true

> "$AUTHTMP"
_step=$(( total > 0 ? 100 / total : 100 ))
_pct=0

for opt_file in "$OPTIONS"/S*; do
    [ -f "$opt_file" ] || continue
    opt_id="${opt_file##*/}"
    uid=$(awk -F',' 'NR==4{ gsub(/[[:space:]]/, "", $2); print $2; exit }' \
          "$opt_file" 2>/dev/null)
    [ -z "$uid" ] || [ "$uid" = "0" ] && continue

    if printf 'GEN:%s' "$opt_id" > "$OAAUTH" 2>/dev/null; then
        auth=$(cat "$OAAUTH" 2>/dev/null)
        if [ "${#auth}" = "24" ]; then
            printf '%s\n' "$auth" >> "$AUTHTMP"
            echo "$opt_id ok (len=${#auth})"
            authorised=$((authorised + 1))
            "$DUM" "EXs $opt_id" || true
        else
            echo "FAIL $opt_id (len=${#auth}, expected 24)"
            failed=$((failed + 1))
        fi
    else
        echo "FAIL $opt_id (GEN write failed)"
        failed=$((failed + 1))
    fi

    _pct=$(( _pct + _step ))
    echo "set $_pct" > /proc/OmapNKS4ProgressBar 2>/dev/null || true
done

mv "$AUTHTMP" "$AUTHFILE"
chown pocky:pocky "$AUTHFILE" 2>/dev/null || true
echo "wrote $(awk 'END{print NR}' "$AUTHFILE") lines to AuthorizationStrings"

[ "$WE_LOADED" = "1" ] && /sbin/rmmod oa_authgen 2>/dev/null

echo "set 100" > /proc/OmapNKS4ProgressBar 2>/dev/null || true
echo "done: authorised=$authorised failed=$failed"
if [ "$failed" -gt 0 ]; then
    "$DUM" "Done: $authorised ok, $failed FAILED" || true
else
    "$DUM" "Done: all $authorised EXs authorised" || true
fi
echo "--- posttar end $(date) ---"
"""


# ── Helpers ───────────────────────────────────────────────────────────────────

def die(msg: str) -> None:
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def run(cmd: list, **kwargs) -> None:
    result = subprocess.run(cmd, check=False, **kwargs)
    if result.returncode != 0:
        die(f"command failed (rc={result.returncode}): {' '.join(str(c) for c in cmd)}")


def sign_package(staging: Path, version: str, source: str,
                 pretar: str, posttar: str) -> str:
    """Write install.info with SHA1(pretar + posttar + UPDATER_KEY) signature."""
    h = hashlib.sha1()
    for name in [pretar, posttar]:
        if name:
            h.update((staging / name).read_bytes())
    h.update(_UPDATER_KEY)
    sig = h.hexdigest()
    lines = [f"VERSION={version}", f"SOURCE={source}",
             f"PRETARSCRIPT={pretar}", f"POSTTARSCRIPT={posttar}",
             f"SIGNATURE={sig}"]
    (staging / "install.info").write_text("\n".join(lines) + "\n")
    return sig


def write_script(path: Path, content: str) -> None:
    path.write_text(content)
    path.chmod(path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def make_stamp_tar(dest: Path) -> None:
    """Create auth.tar.gz containing only /tmp/auth.stamp (UpdateOS needs a SOURCE file)."""
    stamp = b"kronosology auto-auth\n"
    with tarfile.open(dest, "w:gz") as tf:
        for name, ftype, content in [
            ("tmp",            tarfile.DIRTYPE, None),
            ("tmp/auth.stamp", tarfile.REGTYPE, stamp),
        ]:
            info           = tarfile.TarInfo(name=name)
            info.type      = ftype
            info.mode      = 0o755 if ftype == tarfile.DIRTYPE else 0o644
            info.uid       = info.gid   = 0
            info.uname     = info.gname = "root"
            info.size      = len(content) if content else 0
            tf.addfile(info, io.BytesIO(content) if content else None)


# ── Main ──────────────────────────────────────────────────────────────────────

def main() -> None:
    # Build oa_authgen.ko if not already present
    if not KO_SRC.exists():
        print("[build] building oa_authgen.ko ...")
        run(["make", "KDIR=/tmp/linux-kronos", "-s"], cwd=HERE / "oa_authgen")
    if not KO_SRC.exists():
        die(f"{KO_SRC} not found — build it first: cd oa_authgen && make KDIR=/path/to/linux-kronos")

    # Clean and recreate staging directory
    print(f"[build] cleaning staging {STAGING}")
    if STAGING.exists():
        shutil.rmtree(STAGING)
    STAGING.mkdir(parents=True)
    (STAGING / "mnt").mkdir()

    # Minimal source tar (stamp only — nothing real lands on Kronos disk)
    print("[build] building auth.tar.gz ...")
    make_stamp_tar(STAGING / "auth.tar.gz")
    print(f"  {(STAGING / 'auth.tar.gz').stat().st_size:,} bytes")

    # oa_authgen.ko onto the USB root (loaded at runtime, never installed)
    print("[build] copying oa_authgen.ko ...")
    shutil.copy2(KO_SRC, STAGING / "oa_authgen.ko")
    print(f"  {(STAGING / 'oa_authgen.ko').stat().st_size:,} bytes")

    # DisplayUpdaterMessage (optional — on-screen progress messages)
    if DUM_SRC.exists():
        print("[build] copying DisplayUpdaterMessage ...")
        dst = STAGING / "DisplayUpdaterMessage"
        shutil.copy2(DUM_SRC, dst)
        dst.chmod(dst.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    else:
        print("[build] NOTE: DisplayUpdaterMessage not built — no on-screen messages.")
        print("        Run 'make' in updater_msg/  (needs gcc-multilib + libfreetype6-dev:i386)")

    # Write Kronos-side shell scripts
    write_script(STAGING / "pretar.sh",  PRETAR_SH)
    write_script(STAGING / "posttar.sh", POSTTAR_SH)

    # Generate signed install.info
    print("[build] generating signed install.info ...")
    sig = sign_package(STAGING, "Authorize all EXs", "auth.tar.gz",
                       "pretar.sh", "posttar.sh")
    print(f"  SIGNATURE: {sig}")

    # Summary
    print()
    print("=" * 64)
    print(f"Build complete.  Contents of {STAGING}:")
    print("=" * 64)
    for p in sorted(STAGING.iterdir()):
        print(f"  {p.stat().st_size:>10,}  {p.name}")
    print()
    print("To use (stock Kronos — no prior installation required):")
    print("  1. Format a USB stick as FAT (or ext2)")
    print("  2. Copy the contents of output/auto-auth/ to the USB root")
    print("  3. Insert into the Kronos and trigger OS Update from the front panel")
    print("  4. Module is loaded from USB, all EXs are authorised, module is removed")
    print("     Nothing is written to the Kronos internal storage")


if __name__ == "__main__":
    main()
