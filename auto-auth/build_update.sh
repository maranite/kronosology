#!/bin/sh
# build_update.sh — produce the auto-auth USB-stick updater package.
#
# Output: ./output/auto-auth-installer/
#   Copy the contents of that directory to the root of a FAT-formatted USB
#   stick.  Insert into the Kronos and trigger an OS update from the front
#   panel.  No power-cycle required after the update.
#
# What the package does on the Kronos:
#   pretar.sh  — one-time: rename stock InstallEXs → InstallEXs.real
#   tar        — drop in oa_authgen.ko + our InstallEXs wrapper at /sbin/
#   posttar.sh — load oa_authgen.ko and authorise every installed EXs
#
# Requires: make, gcc (i386 support), python3, /tmp/linux-kronos kernel tree
#           (see oa_authgen/Makefile for details)

set -e
cd "$(dirname "$0")"
HERE="$(pwd)"

STAGING="$HERE/output/auto-auth-installer"
BUILDER="$HERE/../update-builder/update_builder.py"
KO_SRC="$HERE/oa_authgen/oa_authgen.ko"
WRAPPER_SRC="$HERE/wrapper_c/InstallEXs"

[ -x "$BUILDER" ] || { echo "ERROR: update_builder.py not found at $BUILDER"; exit 1; }

# ── Build binaries if needed ─────────────────────────────────────────────────
echo "[build] building oa_authgen.ko ..."
( cd "$HERE/oa_authgen" && make KDIR=/tmp/linux-kronos -s )

echo "[build] building InstallEXs wrapper ..."
( cd "$HERE/wrapper_c" && make -s )

[ -f "$KO_SRC" ]      || { echo "ERROR: $KO_SRC not found after build"; exit 1; }
[ -f "$WRAPPER_SRC" ] || { echo "ERROR: $WRAPPER_SRC not found after build"; exit 1; }

# ── Staging ──────────────────────────────────────────────────────────────────
echo "[build] cleaning staging $STAGING"
rm -rf "$STAGING"
mkdir -p "$STAGING"

# ── pretar.sh ────────────────────────────────────────────────────────────────
# Runs BEFORE the tar is extracted.
# • If /sbin/InstallEXs.real already exists → already installed; exit 0 so
#   the tar still lands (updating the binaries is safe and idempotent).
# • Otherwise rename the stock /sbin/InstallEXs → /sbin/InstallEXs.real so
#   the tar can drop our wrapper in its place.
cat > "$STAGING/pretar.sh" << 'PRETAR_EOF'
#!/bin/sh
# auto-auth pretar: protect stock InstallEXs before our wrapper is extracted.
set -e

if [ -f /sbin/InstallEXs.real ]; then
    echo "auto-auth: already installed (InstallEXs.real exists) — updating binaries"
    exit 0
fi

if [ ! -f /sbin/InstallEXs ]; then
    echo "auto-auth: WARNING: /sbin/InstallEXs not found — nothing to rename"
    exit 0
fi

mv /sbin/InstallEXs /sbin/InstallEXs.real
echo "auto-auth: renamed /sbin/InstallEXs → /sbin/InstallEXs.real"
PRETAR_EOF
chmod +x "$STAGING/pretar.sh"

# ── posttar.sh ───────────────────────────────────────────────────────────────
# Runs AFTER the tar is extracted (/sbin/oa_authgen.ko and /sbin/InstallEXs
# are now in place).
# • Fixes permissions on the wrapper (setuid, like the stock binary).
# • Loads oa_authgen.ko if not already loaded.
# • Iterates every /korg/rw/Options/S* option file; for each that has a
#   non-zero UID (line 4, second comma-separated field), generates and
#   submits the auth string via OA.ko's /proc/.oacmd interface.
cat > "$STAGING/posttar.sh" << 'POSTTAR_EOF'
#!/bin/sh
# auto-auth posttar: load module and authorise all installed EXs.

OAAUTH=/proc/.oaauth
OACMD=/proc/.oacmd
OPTIONS=/korg/rw/Options

# ── Fix wrapper permissions to match stock InstallEXs (setuid root) ──────────
if [ -f /sbin/InstallEXs ]; then
    chmod 4755 /sbin/InstallEXs
fi

# ── Load oa_authgen.ko if not already present ─────────────────────────────────
if [ ! -e "$OAAUTH" ]; then
    echo "auto-auth: loading oa_authgen.ko ..."
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
    echo "auto-auth: ERROR: /proc/.oaauth not available after insmod — skipping EX authorisation"
    exit 0
fi

# ── Authorise each installed EXs that has a non-zero UID ─────────────────────
# Option file format (example /korg/rw/Options/S023):
#   Line 1: EXs23
#   Line 2: 2 Church Pianos
#   Line 3: 23
#   Line 4: 2,24,EXs23 2 Church Pianos
#              ^  ^-- UID (non-zero = needs authorisation)
#              \-- bank type
authorised=0
skipped=0

for opt_file in "$OPTIONS"/S*; do
    [ -f "$opt_file" ] || continue
    opt_id=$(basename "$opt_file")

    # Extract UID: second comma-separated field on line 4
    uid=$(awk -F',' 'NR==4{ gsub(/[[:space:]]/, "", $2); print $2; exit }' \
          "$opt_file" 2>/dev/null)

    # Skip if no UID or UID is zero
    if [ -z "$uid" ] || [ "$uid" = "0" ]; then
        skipped=$((skipped + 1))
        continue
    fi

    # Generate auth string (oaauth_write returns error if option file missing)
    if printf 'GEN:%s' "$opt_id" > "$OAAUTH" 2>/dev/null; then
        auth=$(cat "$OAAUTH" 2>/dev/null)
        if [ -n "$auth" ]; then
            printf 'AU:%s' "$auth" > "$OACMD" 2>/dev/null
            sleep 1
            echo "auto-auth: authorised $opt_id (uid=$uid)"
            authorised=$((authorised + 1))
        else
            echo "auto-auth: WARNING: GEN:$opt_id returned empty string"
        fi
    else
        echo "auto-auth: WARNING: GEN:$opt_id failed (option file may be unreadable)"
    fi
done

echo "auto-auth: done — authorised=$authorised skipped=$skipped"
POSTTAR_EOF
chmod +x "$STAGING/posttar.sh"

# ── Payload tar ──────────────────────────────────────────────────────────────
# Package oa_authgen.ko and the InstallEXs wrapper into /sbin/ on the Kronos.
echo "[build] building auto-auth.tar.gz ..."
PAYLOAD=$(mktemp -d)
mkdir -p "$PAYLOAD/sbin"
cp "$KO_SRC"      "$PAYLOAD/sbin/oa_authgen.ko"
cp "$WRAPPER_SRC" "$PAYLOAD/sbin/InstallEXs"
chmod 0644 "$PAYLOAD/sbin/oa_authgen.ko"
chmod 4755 "$PAYLOAD/sbin/InstallEXs"   # setuid root, like the stock binary

( cd "$PAYLOAD" && tar --owner=root --group=root -czf "$STAGING/auto-auth.tar.gz" sbin/ )
rm -rf "$PAYLOAD"
ls -la "$STAGING/auto-auth.tar.gz"

# ── Signed install.info ───────────────────────────────────────────────────────
echo "[build] generating signed install.info ..."
python3 "$BUILDER" "$STAGING" \
    --version "auto-auth-1.0" \
    --source  "auto-auth.tar.gz" \
    --pretar  "pretar.sh" \
    --posttar "posttar.sh"

echo
echo "================================================================"
echo "Build complete.  Contents of $STAGING:"
echo "================================================================"
ls -la "$STAGING"
echo
echo "To install:"
echo "  1. Format a USB stick as FAT (or ext2)"
echo "  2. Copy the contents of output/auto-auth-installer/ to the USB root"
echo "  3. Insert into the Kronos and trigger OS Update from the front panel"
