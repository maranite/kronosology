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
