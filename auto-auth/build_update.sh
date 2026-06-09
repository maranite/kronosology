#!/bin/bash
# build_update.sh — produce the auto-auth USB-stick updater package.
#
# Output: ./output/auto-auth-installer/
#   Copy the contents of that directory to the root of a FAT-formatted USB
#   stick.  Insert into the Kronos and trigger an OS update from the front
#   panel.  A reboot is required after install (normal OS-update behaviour).
#
# What the package does on the Kronos:
#   pretar.sh  — backs up stock binaries; renames stock InstallEXs → InstallEXs.real
#   tar        — installs to /sbin/:
#                  loadmod.ko          (3-bypass: MD5 check + crypto-init)
#                  loadoa              (path-redirect: /korg/Mod/ → /sbin/)
#                  OA.ko               (56-run NOP patch: EX auth bypass)
#                  KorgUsbAudioDriver.ko  (stock copy; needed by patched loadoa)
#                  oa_authgen.ko       (our EX auth-string generator)
#                  InstallEXs          (our wrapper; calls real then auto-auths)
#   posttar.sh — loads oa_authgen.ko and immediately authorises every installed
#                EX library (no reboot needed for auth strings to take effect)
#
# Stock-system compatible: no prior installation required.
# Future Korg OTA overwrites patched files back to stock — system keeps working.
#
# Requires: bash, make, gcc (i386 support), python3, /tmp/linux-kronos kernel tree.

set -e
cd "$(dirname "$0")"
HERE="$(pwd)"

STAGING="$HERE/output/auto-auth-installer"
BUILDER="$HERE/../update-builder/update_builder.py"
PATCHER="$HERE/../patcher/kronos_patcher.sh"
KO_SRC="$HERE/oa_authgen/oa_authgen.ko"
WRAPPER_SRC="$HERE/wrapper_c/InstallEXs"
DUM_SRC="$HERE/updater_msg/DisplayUpdaterMessage"

# ── Stock binary sources ──────────────────────────────────────────────────────
KRONOS_SRC="/mnt/source/Kronos"
LOADMOD_STOCK="$KRONOS_SRC/KRONOS_Update_3_2_1/sbin/loadmod.ko"
LOADOA_STOCK="$KRONOS_SRC/KRONOS_Update_3_2_1/sbin/loadoa"
USBMIDI_STOCK="$KRONOS_SRC/KRONOS_Update_3_2_1/sbin/USBMidiAccessory.ko"
OA_STOCK="$KRONOS_SRC/dump from kronos/korg/Mod/OA.ko"
KORGUSB_STOCK="$KRONOS_SRC/dump from kronos/korg/Mod/KorgUsbAudioDriver.ko"

# Expected patched MD5s (from kronos_patcher.sh configuration block)
LOADMOD_PATCHED_MD5="28d1cec16f1d893f1d78241b62a989d9"
LOADOA_PATCHED_MD5="d17c26036fa0f51f5f4c0ef2f6f424bf"
OA_PATCHED_MD5="163550b60b7508b2c0ba1fd314b0b944"
KORGUSB_STOCK_MD5="29fbd20cf729980e1cffd670391256b5"   # not patched; just copied

[ -x "$BUILDER" ]  || { echo "ERROR: update_builder.py not found at $BUILDER"; exit 1; }
[ -f "$PATCHER" ]  || { echo "ERROR: kronos_patcher.sh not found at $PATCHER"; exit 1; }
for f in "$LOADMOD_STOCK" "$LOADOA_STOCK" "$USBMIDI_STOCK" "$OA_STOCK" "$KORGUSB_STOCK"; do
    [ -f "$f" ] || { echo "ERROR: stock source not found: $f"; exit 1; }
done

# ── Build our binaries ────────────────────────────────────────────────────────
echo "[build] building oa_authgen.ko ..."
( cd "$HERE/oa_authgen" && make KDIR=/tmp/linux-kronos -s )

echo "[build] building InstallEXs wrapper ..."
( cd "$HERE/wrapper_c" && make -s )

echo "[build] building DisplayUpdaterMessage ..."
if ! ( cd "$HERE/updater_msg" && make -s ) 2>/dev/null; then
    echo "[build] WARNING: DisplayUpdaterMessage build failed (missing 32-bit FreeType?)."
    echo "[build]   Install gcc-multilib + libfreetype6-dev:i386 and retry, or"
    echo "[build]   copy a pre-built binary to $HERE/updater_msg/DisplayUpdaterMessage"
fi

[ -f "$KO_SRC" ]      || { echo "ERROR: $KO_SRC not found after build"; exit 1; }
[ -f "$WRAPPER_SRC" ] || { echo "ERROR: $WRAPPER_SRC not found after build"; exit 1; }
[ -f "$DUM_SRC" ] || echo "WARNING: DisplayUpdaterMessage not built — no on-screen messages."

# ── Patch binaries via patcher sandbox ───────────────────────────────────────
# kronos_patcher.sh supports KRONOS_ROOT=/sandbox — it skips the live cryptoloop
# mount and instead reads from $SANDBOX/korg/Mod/ directly.  We populate that
# from our extracted stock sources, run the patcher, then collect the results.
echo "[build] preparing patcher sandbox ..."
SANDBOX=$(mktemp -d)
trap 'rm -rf "$SANDBOX"' EXIT

mkdir -p \
    "$SANDBOX/sbin" \
    "$SANDBOX/korg/Mod" \
    "$SANDBOX/korg/rw/kronos_patcher_backup"

cp "$LOADMOD_STOCK" "$SANDBOX/sbin/loadmod.ko"
cp "$LOADOA_STOCK"  "$SANDBOX/sbin/loadoa"
cp "$USBMIDI_STOCK" "$SANDBOX/sbin/USBMidiAccessory.ko"
cp "$OA_STOCK"      "$SANDBOX/korg/Mod/OA.ko"
cp "$KORGUSB_STOCK" "$SANDBOX/korg/Mod/KorgUsbAudioDriver.ko"

echo "[build] running patcher (loadmod + loadoa + OA.ko + KorgUsbAudioDriver.ko) ..."
KRONOS_ROOT="$SANDBOX" sh "$PATCHER" 2>&1 | sed 's/^/  [patcher] /'

# Verify all patched outputs
echo "[build] verifying patched binaries ..."
for pair in \
    "sbin/loadmod.ko:$LOADMOD_PATCHED_MD5" \
    "sbin/loadoa:$LOADOA_PATCHED_MD5" \
    "sbin/OA.ko:$OA_PATCHED_MD5" \
    "sbin/KorgUsbAudioDriver.ko:$KORGUSB_STOCK_MD5"
do
    relpath="${pair%%:*}"; want="${pair##*:}"
    got=$(md5sum "$SANDBOX/$relpath" | cut -d' ' -f1)
    if [ "$got" = "$want" ]; then
        echo "[build]   $relpath OK ($got)"
    else
        echo "ERROR: $relpath MD5 mismatch: got $got, want $want"
        exit 1
    fi
done

# ── Staging ──────────────────────────────────────────────────────────────────
echo "[build] cleaning staging $STAGING"
rm -rf "$STAGING"
mkdir -p "$STAGING" "$STAGING/mnt"

# ── pretar.sh ────────────────────────────────────────────────────────────────
cat > "$STAGING/pretar.sh" << 'PRETAR_EOF'
#!/bin/sh
# auto-auth pretar: back up stock files, rename InstallEXs for our wrapper.
set -e

DUM=/mnt/updaterSource/DisplayUpdaterMessage
[ -x "$DUM" ] || DUM=true

"$DUM" "Installing kronosology auto-auth..."

# Create .stock backups on first install so users can restore if needed.
for f in /sbin/loadmod.ko /sbin/loadoa /sbin/OA.ko; do
    [ -f "$f" ] && [ ! -f "${f}.stock" ] && cp "$f" "${f}.stock" && \
        echo "auto-auth: backed up ${f} → ${f}.stock"
done

if [ -f /sbin/InstallEXs.real ]; then
    echo "auto-auth: already installed (InstallEXs.real exists) — updating binaries"
    "$DUM" "Updating auto-auth binaries..."
    exit 0
fi

if [ ! -f /sbin/InstallEXs ]; then
    echo "auto-auth: WARNING: /sbin/InstallEXs not found — nothing to rename"
    "$DUM" "WARNING: InstallEXs not found"
    exit 0
fi

"$DUM" "Preparing EX authorization hook..."
mv /sbin/InstallEXs /sbin/InstallEXs.real
echo "auto-auth: renamed /sbin/InstallEXs → /sbin/InstallEXs.real"
PRETAR_EOF
chmod +x "$STAGING/pretar.sh"

# ── posttar.sh ───────────────────────────────────────────────────────────────
cat > "$STAGING/posttar.sh" << 'POSTTAR_EOF'
#!/bin/sh
# auto-auth posttar: load module and authorise all installed EXs.
#
# NOTE: patched loadmod.ko, loadoa, OA.ko, and KorgUsbAudioDriver.ko are now on
# disk; they take effect at next reboot.  EX auth strings are applied immediately
# (no reboot needed for the Kronos to recognise the installed EX libraries).

OAAUTH=/proc/.oaauth
OACMD=/proc/.oacmd
OPTIONS=/korg/rw/Options
DUM=/mnt/updaterSource/DisplayUpdaterMessage
[ -x "$DUM" ] || DUM=true

# Fix wrapper permissions to match stock InstallEXs (setuid root)
[ -f /sbin/InstallEXs ] && chmod 4755 /sbin/InstallEXs

# ── Load oa_authgen.ko if not already present ─────────────────────────────────
if [ ! -e "$OAAUTH" ]; then
    echo "auto-auth: loading oa_authgen.ko ..."
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
    echo "auto-auth: ERROR: /proc/.oaauth not available after insmod — skipping EX authorisation"
    "$DUM" "ERROR: auth module failed to load"
    exit 0
fi

# ── Authorise each installed EXs that has a non-zero UID ─────────────────────
# Option file format (example /korg/rw/Options/S023):
#   Line 4: 2,24,EXs23 2 Church Pianos
#              ^  ^-- UID (non-zero = needs authorisation)
authorised=0
skipped=0

"$DUM" "Authorising EX libraries..."
echo "set 0" > /proc/OmapNKS4ProgressBar 2>/dev/null || true

# Count first so progress bar is evenly distributed
_auth_total=0
for opt_file in "$OPTIONS"/S*; do
    [ -f "$opt_file" ] || continue
    _uid=$(awk -F',' 'NR==4{ gsub(/[[:space:]]/, "", $2); print $2; exit }' \
           "$opt_file" 2>/dev/null)
    [ -z "$_uid" ] || [ "$_uid" = "0" ] && continue
    _auth_total=$(( _auth_total + 1 ))
done
_progress_step=$(( _auth_total > 0 ? 100 / _auth_total : 100 ))
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
            echo "auto-auth: authorised $opt_id (uid=$uid)"
            authorised=$((authorised + 1))
        else
            echo "auto-auth: WARNING: GEN:$opt_id returned empty string"
        fi
    else
        echo "auto-auth: WARNING: GEN:$opt_id failed"
    fi

    _progress=$(( _progress + _progress_step ))
    echo "set $_progress" > /proc/OmapNKS4ProgressBar 2>/dev/null || true
done

echo "set 100" > /proc/OmapNKS4ProgressBar 2>/dev/null || true
echo "auto-auth: done — authorised=$authorised skipped=$skipped"
"$DUM" "auto-auth complete: ${authorised} EX libraries authorised"
echo "auto-auth: NOTE: patched loadmod/loadoa/OA installed — power-cycle to activate"
POSTTAR_EOF
chmod +x "$STAGING/posttar.sh"

# ── Payload tar ──────────────────────────────────────────────────────────────
# Installs into /sbin/ on the Kronos:
#   loadmod.ko          — 3-bypass: MD5 check + crypto-init
#   loadoa              — path-redirect: /korg/Mod/OA.ko → /sbin/OA.ko
#   OA.ko               — 56-run NOP patch: EX auth bypass
#   KorgUsbAudioDriver.ko — stock copy required by patched loadoa
#   oa_authgen.ko       — our EX auth-string generator
#   InstallEXs          — our wrapper (renamed stock is at InstallEXs.real)
echo "[build] building auto-auth.tar.gz ..."
PAYLOAD=$(mktemp -d)
mkdir -p "$PAYLOAD/sbin"
cp "$SANDBOX/sbin/loadmod.ko"             "$PAYLOAD/sbin/loadmod.ko"
cp "$SANDBOX/sbin/loadoa"                 "$PAYLOAD/sbin/loadoa"
cp "$SANDBOX/sbin/OA.ko"                  "$PAYLOAD/sbin/OA.ko"
cp "$SANDBOX/sbin/KorgUsbAudioDriver.ko"  "$PAYLOAD/sbin/KorgUsbAudioDriver.ko"
cp "$KO_SRC"                              "$PAYLOAD/sbin/oa_authgen.ko"
cp "$WRAPPER_SRC"                         "$PAYLOAD/sbin/InstallEXs"
chmod 0755 "$PAYLOAD/sbin/loadmod.ko"
chmod 0755 "$PAYLOAD/sbin/loadoa"
chmod 0755 "$PAYLOAD/sbin/OA.ko"
chmod 0755 "$PAYLOAD/sbin/KorgUsbAudioDriver.ko"
chmod 0644 "$PAYLOAD/sbin/oa_authgen.ko"
chmod 4755 "$PAYLOAD/sbin/InstallEXs"     # setuid root, like the stock binary

( cd "$PAYLOAD" && tar --owner=root --group=root -czf "$STAGING/auto-auth.tar.gz" sbin/ )
rm -rf "$PAYLOAD"
ls -la "$STAGING/auto-auth.tar.gz"

# ── DisplayUpdaterMessage ─────────────────────────────────────────────────────
if [ -f "$DUM_SRC" ]; then
    echo "[build] copying DisplayUpdaterMessage ..."
    cp "$DUM_SRC" "$STAGING/DisplayUpdaterMessage"
    chmod +x "$STAGING/DisplayUpdaterMessage"
else
    echo "[build] skipping DisplayUpdaterMessage (not built)"
fi

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
echo "Patched binary checksums:"
echo "  loadmod.ko         : $LOADMOD_PATCHED_MD5"
echo "  loadoa             : $LOADOA_PATCHED_MD5"
echo "  OA.ko              : $OA_PATCHED_MD5"
echo "  KorgUsbAudioDriver : $KORGUSB_STOCK_MD5  (stock copy)"
echo
echo "To install:"
echo "  1. Format a USB stick as FAT (or ext2)"
echo "  2. Copy the contents of output/auto-auth-installer/ to the USB root"
echo "  3. Insert into the Kronos and trigger OS Update from the front panel"
echo "  4. EX auth strings applied immediately; POWER-CYCLE to activate patched binaries"
echo "     (soft reboot may wedge the OmapNKS4 panel chip — use full power-off ~60s)"
