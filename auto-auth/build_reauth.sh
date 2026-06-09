#!/bin/bash
# build_reauth.sh — produce the standalone EX re-authorisation USB package.
#
# Output: ./output/auto-reauth/
#   Copy the contents of that directory to the root of a FAT-formatted USB
#   stick.  Insert into the Kronos and trigger an OS update from the front
#   panel.  No files are installed on the Kronos; the package purely reads
#   the chip, generates auth strings, and submits them for every EX library
#   that has a non-zero UID in /korg/rw/Options.
#
# Prerequisites on the Kronos (the auto-auth installer must have been run
# at least once to put these in place):
#   /sbin/oa_authgen.ko   — auth-string generator kernel module
#   /sbin/InstallEXs      — kronosology InstallEXs wrapper (for future installs)
#
# Requires: python3, update_builder.py

set -e
cd "$(dirname "$0")"
HERE="$(pwd)"

STAGING="$HERE/output/auto-reauth"
BUILDER="$HERE/../update-builder/update_builder.py"
DUM_SRC="$HERE/updater_msg/DisplayUpdaterMessage"

[ -x "$BUILDER" ] || { echo "ERROR: update_builder.py not found at $BUILDER"; exit 1; }

# ── Staging ──────────────────────────────────────────────────────────────────
echo "[build] cleaning staging $STAGING"
rm -rf "$STAGING"
mkdir -p "$STAGING" "$STAGING/mnt"

# ── Minimal source tar ────────────────────────────────────────────────────────
# UpdateOS requires a SOURCE tar to exist; we extract a harmless stamp file
# to /tmp so nothing on the Kronos filesystem is modified.
echo "[build] building reauth.tar.gz ..."
PAYLOAD=$(mktemp -d)
mkdir -p "$PAYLOAD/tmp"
printf 'kronosology auto-reauth\n' > "$PAYLOAD/tmp/auto-reauth.stamp"
( cd "$PAYLOAD" && tar --owner=root --group=root -czf "$STAGING/reauth.tar.gz" tmp/ )
rm -rf "$PAYLOAD"
ls -la "$STAGING/reauth.tar.gz"

# ── DisplayUpdaterMessage ─────────────────────────────────────────────────────
if [ -f "$DUM_SRC" ]; then
    echo "[build] copying DisplayUpdaterMessage ..."
    cp "$DUM_SRC" "$STAGING/DisplayUpdaterMessage"
    chmod +x "$STAGING/DisplayUpdaterMessage"
else
    echo "[build] NOTE: DisplayUpdaterMessage not built — no on-screen messages."
    echo "        Run 'make' in updater_msg/ (needs gcc-multilib + libfreetype6-dev:i386)"
fi

# ── pretar.sh ────────────────────────────────────────────────────────────────
# Runs BEFORE the tar is extracted (our tar only writes to /tmp, so this
# is purely a pre-flight check and display update).
cat > "$STAGING/pretar.sh" << 'PRETAR_EOF'
#!/bin/sh
# auto-reauth pretar: pre-flight check before re-authorising EX libraries.

DUM=/mnt/updaterSource/DisplayUpdaterMessage
[ -x "$DUM" ] || DUM=true

"$DUM" "kronosology: EX re-authorisation..."

# Warn if oa_authgen.ko is missing — the posttar will still attempt insmod,
# but it will fail.  Surface the warning here while we can still show it.
if [ ! -f /sbin/oa_authgen.ko ]; then
    echo "auto-reauth: WARNING: /sbin/oa_authgen.ko not found."
    echo "  Run the auto-auth installer first to deploy the module."
    "$DUM" "WARNING: auto-auth not installed"
fi
PRETAR_EOF
chmod +x "$STAGING/pretar.sh"

# ── posttar.sh ───────────────────────────────────────────────────────────────
# Runs AFTER the tar is extracted.  The tar only dropped /tmp/auto-reauth.stamp,
# so the Kronos filesystem is otherwise untouched.  This script:
#   1. Loads oa_authgen.ko if not already resident
#   2. Iterates /korg/rw/Options/S* and authorises every EX with a non-zero UID
cat > "$STAGING/posttar.sh" << 'POSTTAR_EOF'
#!/bin/sh
# auto-reauth posttar: re-authorise all installed EX libraries.

OAAUTH=/proc/.oaauth
OACMD=/proc/.oacmd
OPTIONS=/korg/rw/Options
DUM=/mnt/updaterSource/DisplayUpdaterMessage
[ -x "$DUM" ] || DUM=true

# ── Load oa_authgen.ko if not already present ─────────────────────────────────
if [ ! -e "$OAAUTH" ]; then
    if [ ! -f /sbin/oa_authgen.ko ]; then
        echo "auto-reauth: ERROR: /sbin/oa_authgen.ko not found — cannot authorise."
        "$DUM" "ERROR: oa_authgen.ko missing — run auto-auth installer first"
        exit 1
    fi

    echo "auto-reauth: loading oa_authgen.ko ..."
    "$DUM" "Loading EX auth module..."

    S=$(grep ' SetupAtmelForAuthorizations' /proc/kallsyms 2>/dev/null | cut -d' ' -f1)
    R=$(grep ' fFfFfFfFfFfF13' /proc/kallsyms 2>/dev/null | grep '\[OA\]' | cut -d' ' -f1)
    if [ -n "$S" ] && [ -n "$R" ]; then
        /sbin/insmod /sbin/oa_authgen.ko \
            setup_atmel_addr=0x${S} chip_read_addr=0x${R}
    else
        /sbin/insmod /sbin/oa_authgen.ko
    fi
    sleep 1
fi

if [ ! -e "$OAAUTH" ]; then
    echo "auto-reauth: ERROR: /proc/.oaauth not available after insmod."
    "$DUM" "ERROR: auth module failed to load"
    exit 1
fi

# ── Count how many EXs need authorisation ────────────────────────────────────
total=0
for opt_file in "$OPTIONS"/S*; do
    [ -f "$opt_file" ] || continue
    uid=$(awk -F',' 'NR==4{ gsub(/[[:space:]]/, "", $2); print $2; exit }' \
          "$opt_file" 2>/dev/null)
    [ -z "$uid" ] || [ "$uid" = "0" ] && continue
    total=$((total + 1))
done

echo "auto-reauth: found $total EX libraries to authorise"
"$DUM" "Authorising $total EX libraries..."
echo "set 0" > /proc/OmapNKS4ProgressBar 2>/dev/null || true

# ── Authorise each EX ─────────────────────────────────────────────────────────
authorised=0
skipped=0
failed=0

_progress_step=$(( total > 0 ? 100 / total : 100 ))
_progress=0

for opt_file in "$OPTIONS"/S*; do
    [ -f "$opt_file" ] || continue
    opt_id="${opt_file##*/}"

    uid=$(awk -F',' 'NR==4{ gsub(/[[:space:]]/, "", $2); print $2; exit }' \
          "$opt_file" 2>/dev/null)

    if [ -z "$uid" ] || [ "$uid" = "0" ]; then
        skipped=$((skipped + 1))
        continue
    fi

    if printf 'GEN:%s' "$opt_id" > "$OAAUTH" 2>/dev/null; then
        auth=$(cat "$OAAUTH" 2>/dev/null)
        if [ -n "$auth" ]; then
            printf 'AU:%s' "$auth" > "$OACMD" 2>/dev/null
            sleep 1
            echo "auto-reauth: authorised $opt_id (uid=$uid)"
            authorised=$((authorised + 1))
        else
            echo "auto-reauth: WARNING: GEN:$opt_id returned empty string"
            failed=$((failed + 1))
        fi
    else
        echo "auto-reauth: WARNING: GEN:$opt_id failed"
        failed=$((failed + 1))
    fi

    _progress=$(( _progress + _progress_step ))
    echo "set $_progress" > /proc/OmapNKS4ProgressBar 2>/dev/null || true
done

echo "set 100" > /proc/OmapNKS4ProgressBar 2>/dev/null || true
echo "auto-reauth: done — authorised=$authorised skipped=$skipped failed=$failed"

if [ "$failed" -gt 0 ]; then
    "$DUM" "Done: $authorised authorised, $failed failed"
else
    "$DUM" "Done: all $authorised EX libraries authorised"
fi
POSTTAR_EOF
chmod +x "$STAGING/posttar.sh"

# ── Signed install.info ───────────────────────────────────────────────────────
echo "[build] generating signed install.info ..."
python3 "$BUILDER" "$STAGING" \
    --version "auto-reauth-1.0" \
    --source  "reauth.tar.gz" \
    --pretar  "pretar.sh" \
    --posttar "posttar.sh"

echo
echo "================================================================"
echo "Build complete.  Contents of $STAGING:"
echo "================================================================"
ls -la "$STAGING"
echo
echo "To use:"
echo "  1. Format a USB stick as FAT (or ext2)"
echo "  2. Copy the contents of output/auto-reauth/ to the USB root"
echo "  3. Insert into the Kronos and trigger OS Update from the front panel"
echo "  4. No reboot required — auth strings are applied immediately"
