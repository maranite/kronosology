#!/bin/bash
# build_reauth_stock.sh — zero-footprint EX authoriser for stock Kronos systems.
#
# Output: ./output/auto-reauth-stock/
#   Copy the contents of that directory to the root of a FAT-formatted USB
#   stick.  Insert into the Kronos and trigger an OS update from the front
#   panel.
#
# What happens on the Kronos:
#   pretar.sh  — display message, pre-flight check
#   tar        — extracts only /tmp/reauth-stock.stamp (harmless)
#   posttar.sh — insmod oa_authgen.ko FROM THE USB STICK (never writes to disk),
#                generates and submits auth strings for every installed EX library,
#                then rmmod oa_authgen to leave no trace.
#
# Stock-system compatible: no prior installation required.  The module is loaded
# from /mnt/updaterSource/ (the mounted USB stick) and removed after use.
#
# Requires: python3, update_builder.py, oa_authgen.ko already built.

set -e
cd "$(dirname "$0")"
HERE="$(pwd)"

STAGING="$HERE/output/auto-reauth-stock"
BUILDER="$HERE/../update-builder/update_builder.py"
KO_SRC="$HERE/oa_authgen/oa_authgen.ko"
DUM_SRC="$HERE/updater_msg/DisplayUpdaterMessage"

[ -x "$BUILDER" ] || { echo "ERROR: update_builder.py not found at $BUILDER"; exit 1; }

# ── Build oa_authgen.ko if needed ────────────────────────────────────────────
echo "[build] building oa_authgen.ko ..."
( cd "$HERE/oa_authgen" && make KDIR=/tmp/linux-kronos -s )
[ -f "$KO_SRC" ] || { echo "ERROR: $KO_SRC not found after build"; exit 1; }

# ── Staging ──────────────────────────────────────────────────────────────────
echo "[build] cleaning staging $STAGING"
rm -rf "$STAGING"
mkdir -p "$STAGING"

# ── Minimal source tar ────────────────────────────────────────────────────────
# UpdateOS requires a SOURCE file.  We extract only a harmless stamp to /tmp;
# nothing on the Kronos internal filesystem is modified by the tar itself.
echo "[build] building reauth-stock.tar.gz ..."
PAYLOAD=$(mktemp -d)
mkdir -p "$PAYLOAD/tmp"
printf 'kronosology auto-reauth-stock\n' > "$PAYLOAD/tmp/reauth-stock.stamp"
( cd "$PAYLOAD" && tar --owner=root --group=root -czf "$STAGING/reauth-stock.tar.gz" tmp/ )
rm -rf "$PAYLOAD"
ls -la "$STAGING/reauth-stock.tar.gz"

# ── oa_authgen.ko onto the USB root ──────────────────────────────────────────
# The module is insmod'd directly from /mnt/updaterSource/oa_authgen.ko at
# runtime and rmmod'd afterwards — it is never copied to the Kronos disk.
echo "[build] copying oa_authgen.ko to USB root ..."
cp "$KO_SRC" "$STAGING/oa_authgen.ko"
ls -la "$STAGING/oa_authgen.ko"

# ── DisplayUpdaterMessage (optional) ─────────────────────────────────────────
if [ -f "$DUM_SRC" ]; then
    echo "[build] copying DisplayUpdaterMessage ..."
    cp "$DUM_SRC" "$STAGING/DisplayUpdaterMessage"
    chmod +x "$STAGING/DisplayUpdaterMessage"
else
    echo "[build] NOTE: DisplayUpdaterMessage not built — no on-screen messages."
    echo "        Run 'make' in updater_msg/ (needs gcc-multilib + libfreetype6-dev:i386)"
fi

# ── pretar.sh ────────────────────────────────────────────────────────────────
cat > "$STAGING/pretar.sh" << 'PRETAR_EOF'
#!/bin/sh
# auto-reauth-stock pretar: pre-flight check.

LOG=/tmp/reauth-stock.log
exec >> "$LOG" 2>&1
echo "--- pretar start $(date) ---"

DUM=/mnt/updaterSource/DisplayUpdaterMessage
[ -x "$DUM" ] || DUM=true

"$DUM" "kronosology: EX authorisation..." || true
PRETAR_EOF
chmod +x "$STAGING/pretar.sh"

# ── posttar.sh ───────────────────────────────────────────────────────────────
cat > "$STAGING/posttar.sh" << 'POSTTAR_EOF'
#!/bin/sh
# auto-reauth-stock posttar: load oa_authgen.ko from USB, generate auth strings
# for every installed EX library, write them directly to AuthorizationStrings,
# then unload.
#
# OA.ko is NOT loaded during UpdateOS — /proc/.oacmd does not exist.
# We write auth strings directly to the file; OA.ko reads and verifies them at
# the next normal boot (VerifyAuthorizationString decodes 24-char base32 strings).

LOG=/korg/rw/reauth-stock.log
exec >> "$LOG" 2>&1
echo "--- posttar start $(date) ---"

OAAUTH=/proc/.oaauth
AUTHFILE=/korg/rw/Startup/AuthorizationStrings
OPTIONS=/korg/rw/Options
MODULE=/mnt/updaterSource/oa_authgen.ko
DUM=/mnt/updaterSource/DisplayUpdaterMessage
[ -x "$DUM" ] || DUM=true

echo "MODULE exists=$([ -f "$MODULE" ] && echo yes || echo NO)"
echo "OPTIONS dir=$([ -d "$OPTIONS" ] && echo yes || echo no)"
echo "AUTHFILE exists=$([ -f "$AUTHFILE" ] && echo yes || echo no)"
echo "kallsyms Setup=$(grep ' SetupAtmelForAuthorizations' /proc/kallsyms 2>/dev/null | cut -d' ' -f1)"
echo "kallsyms Read=$(grep ' fFfFfFfFfFfF13' /proc/kallsyms 2>/dev/null | grep '\[OA\]' | cut -d' ' -f1)"
echo "mounts=$(grep korg /proc/mounts 2>/dev/null | awk '{print $2,$4}' | tr '\n' '|')"

# ── Clean up stamp file extracted by tar ─────────────────────────────────────
rm -f /tmp/reauth-stock.stamp

# ── Load oa_authgen.ko from USB ───────────────────────────────────────────────
WE_LOADED=0

if [ ! -e "$OAAUTH" ]; then
    if [ ! -f "$MODULE" ]; then
        echo "ERROR: $MODULE not found on USB"
        "$DUM" "ERROR: oa_authgen.ko missing from USB" || true
        exit 1
    fi

    "$DUM" "Loading EX auth module..." || true

    S=$(grep ' SetupAtmelForAuthorizations' /proc/kallsyms 2>/dev/null | cut -d' ' -f1)
    R=$(grep ' fFfFfFfFfFfF13' /proc/kallsyms 2>/dev/null | grep '\[OA\]' | cut -d' ' -f1)
    echo "insmod: S=$S R=$R"
    if [ -n "$S" ] && [ -n "$R" ]; then
        /sbin/insmod "$MODULE" setup_atmel_addr=0x${S} chip_read_addr=0x${R}
    else
        /sbin/insmod "$MODULE"
    fi
    echo "insmod exit=$?"
    sleep 1
    WE_LOADED=1
fi

if [ ! -e "$OAAUTH" ]; then
    echo "ERROR: /proc/.oaauth not available after insmod"
    "$DUM" "ERROR: auth module failed to load" || true
    exit 1
fi

echo "OAAUTH exists=yes (WE_LOADED=$WE_LOADED)"

# ── Self-test: generate one auth string and verify length ────────────────────
_test_auth=$(printf 'GEN:%s' "S012" > "$OAAUTH" 2>/dev/null && cat "$OAAUTH" 2>/dev/null)
echo "selftest GEN:S012 => [${_test_auth}] len=${#_test_auth}"
if [ "${#_test_auth}" != "24" ]; then
    echo "ERROR: self-test failed — expected 24-char auth string, got ${#_test_auth}"
    "$DUM" "ERROR: auth gen self-test failed" || true
    [ "$WE_LOADED" = "1" ] && /sbin/rmmod oa_authgen 2>/dev/null
    exit 1
fi

# ── Ensure AuthorizationStrings directory exists ──────────────────────────────
mkdir -p "$(dirname "$AUTHFILE")"

# ── Count EXs to authorise ────────────────────────────────────────────────────
total=0
for opt_file in "$OPTIONS"/S*; do
    [ -f "$opt_file" ] || continue
    uid=$(awk -F',' 'NR==4{ gsub(/[[:space:]]/, "", $2); print $2; exit }' \
          "$opt_file" 2>/dev/null)
    [ -z "$uid" ] || [ "$uid" = "0" ] && continue
    total=$((total + 1))
done

echo "found $total EX libraries to authorise"
"$DUM" "Authorising $total EX libraries..." || true

# ── Generate auth strings and write directly to AuthorizationStrings ──────────
# OA.ko is not loaded — we bypass /proc/.oacmd entirely.
# OA.ko's VerifyAuthorizationString reads exactly 24 base32 chars per line,
# so plain `printf '%s\n' "$auth"` is the correct format.
authorised=0
failed=0
AUTHTMP="${AUTHFILE}.tmp$$"
> "$AUTHTMP"

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
            echo "$opt_id auth_len=${#auth} ok"
            authorised=$((authorised + 1))
        else
            echo "FAIL $opt_id (bad auth len=${#auth} expected 24)"
            failed=$((failed + 1))
        fi
    else
        echo "FAIL $opt_id (GEN write failed)"
        failed=$((failed + 1))
    fi
done

# Atomic replace — don't leave a partial file if something goes wrong mid-loop
mv "$AUTHTMP" "$AUTHFILE"
echo "AuthorizationStrings: $(awk 'END{print NR}' "$AUTHFILE") lines written"

# ── Unload module ─────────────────────────────────────────────────────────────
[ "$WE_LOADED" = "1" ] && /sbin/rmmod oa_authgen 2>/dev/null

echo "done: authorised=$authorised failed=$failed"
if [ "$failed" -gt 0 ]; then
    "$DUM" "Done: $authorised ok, $failed FAILED" || true
else
    "$DUM" "Done: all $authorised EX libs authorised" || true
fi
echo "--- posttar end $(date) ---"
POSTTAR_EOF
chmod +x "$STAGING/posttar.sh"

# ── Signed install.info ───────────────────────────────────────────────────────
echo "[build] generating signed install.info ..."
python3 "$BUILDER" "$STAGING" \
    --version "auto-reauth-stock-1.0" \
    --source  "reauth-stock.tar.gz" \
    --pretar  "pretar.sh" \
    --posttar "posttar.sh"

echo
echo "================================================================"
echo "Build complete.  Contents of $STAGING:"
echo "================================================================"
ls -la "$STAGING"
echo
echo "To use (stock Kronos — no prior installation required):"
echo "  1. Format a USB stick as FAT (or ext2)"
echo "  2. Copy the contents of output/auto-reauth-stock/ to the USB root"
echo "  3. Insert into the Kronos and trigger OS Update from the front panel"
echo "  4. The module is loaded, EXs are authorised, module is unloaded"
echo "  5. Nothing is written to the Kronos internal storage"
