#!/usr/bin/env bash
# Mount a Kronos Mod.img, Eva.img, or WaveMotion.img without the Kronos hardware.
#
# Key format (from loadmod.ko binary analysis + LOOP_GET_STATUS64 extraction):
#   - Cipher  : aes → kernel resolves to cbc(aes) = AES-256-CBC
#   - Key     : 32 bytes — 31 printable ASCII hex chars + a single trailing null byte.
#               Korg's HexEncode writes 31 chars + relies on a pre-zeroed 32nd byte.
#   - IV mode : plain — sector number as 32-bit LE, padded to 16 bytes
#
# Keys (extracted 2026-06-04 from live Kronos at 192.168.0.3 via getloopkey):
#   Mod        a336a15cd841ec8926b99e7c3884eaa  + \x00
#   Eva        342ee59d549c7d329d835537be0540d  + \x00
#   WaveMotion 3e72c0e59fc017a9eb7d7e1168a4cdb  + \x00
#
# Universal across all Kronos units (proven by identical ciphertext across
# 2014 / 3.2.1 / 3.2.2 firmware versions and dump-from-device).
#
# Prerequisites:
#   sudo apt-get install cryptsetup
#
# Usage:
#   sudo ./mount_kronos_img.sh <type> <path/to/img> <mountpoint>
#   sudo ./mount_kronos_img.sh <type> <path/to/img> <mountpoint> unmount
#
#   <type> = mod | eva | wm
#
# Auto-detect convenience: if <type> is "auto", the type is inferred from the
# filename (Mod.img / Eva.img / WaveMotion.img, case-insensitive).
#
# Examples:
#   sudo ./mount_kronos_img.sh mod KRONOS_Update_3_2_2/mnt/korg/ro/Mod.img /mnt/mod322
#   sudo ./mount_kronos_img.sh auto KRONOS_Update_3_2_2/mnt/korg/ro/Eva.img /mnt/eva322

# 31-char keys (null byte appended automatically when written to keyfile)
KEY_MOD="a336a15cd841ec8926b99e7c3884eaa"
KEY_EVA="342ee59d549c7d329d835537be0540d"
KEY_WM="3e72c0e59fc017a9eb7d7e1168a4cdb"

usage() {
    sed -n '2,30p' "$0" >&2
    exit 1
}

[ $# -lt 3 ] && usage
TYPE="$1"
IMG="$2"
MNT="$3"
ACTION="${4:-mount}"

# Auto-detect type
if [ "$TYPE" = "auto" ]; then
    BASENAME=$(basename "$IMG" | tr 'A-Z' 'a-z')
    case "$BASENAME" in
        mod.img)        TYPE="mod" ;;
        eva.img)        TYPE="eva" ;;
        wavemotion.img) TYPE="wm" ;;
        *) echo "ERROR: cannot auto-detect type from '$BASENAME'." >&2; exit 1 ;;
    esac
fi

case "$TYPE" in
    mod) KEY="$KEY_MOD"; MAPPER="kronos_mod" ;;
    eva) KEY="$KEY_EVA"; MAPPER="kronos_eva" ;;
    wm)  KEY="$KEY_WM";  MAPPER="kronos_wm"  ;;
    *) echo "ERROR: type must be mod|eva|wm|auto (got '$TYPE')" >&2; exit 1 ;;
esac

if [ "$ACTION" = "unmount" ]; then
    umount "$MNT" 2>/dev/null
    cryptsetup close "$MAPPER" 2>/dev/null
    echo "Unmounted $MAPPER."
    exit 0
fi

[ -e "$IMG" ] || { echo "ERROR: image not found: $IMG" >&2; exit 1; }
mkdir -p "$MNT"

# Write the 31-char key + trailing null byte (= 32 bytes total) to a temp keyfile
KEYFILE=$(mktemp)
trap "rm -f '$KEYFILE'" EXIT
printf '%s' "$KEY" > "$KEYFILE"
printf '\x00'      >> "$KEYFILE"

cryptsetup open --type plain \
    --cipher aes-cbc-plain \
    --key-size 256 \
    --key-file "$KEYFILE" \
    "$IMG" "$MAPPER" || { echo "ERROR: cryptsetup failed" >&2; exit 1; }

mount -o ro "/dev/mapper/$MAPPER" "$MNT" || {
    cryptsetup close "$MAPPER"
    echo "ERROR: mount failed" >&2
    exit 1
}

echo "Mounted $IMG ($TYPE) at $MNT via /dev/mapper/$MAPPER"
echo "When done: sudo $0 $TYPE $IMG $MNT unmount"
