#!/bin/sh
# Kronos OA-bypass patcher
# ========================
# Applies the patches that:
#   - Bypass OA.ko's EX-bank authorization (so any EX install works without keys)
#   - Bypass loadmod's MD5 integrity check (so the modified loadoa is accepted)
#   - Bypass loadmod's inner MD5 check inside RetrieveSecurityICKey (so the
#     cryptoloop key still gets set up despite the modified loadoa)
#   - Redirect loadoa to load patched OA from /sbin/ instead of /korg/Mod/
#
# Targets:  Korg Kronos 2.6.32.11-korg kernel, OS v3.1+ (binaries with the MD5s
#           configured below; if MD5s don't match, script aborts before touching
#           anything).
# Runtime:  busybox-only — no python/perl/awk extensions. Uses dd, md5sum,
#           printf, mount, cp, mkdir, rm, sync, cut, cat, grep.
# Safety:   verifies stock MD5s BEFORE patching, backs up originals,
#           verifies patched MD5s AFTER patching, restores on any failure.
# Idempotent: re-running on an already-patched system detects it and exits 0.
#
# Usage:    sh kronos_patcher.sh           # apply
#           sh kronos_patcher.sh --verify  # check current state, no changes
#           sh kronos_patcher.sh --revert  # restore from backups
#
# After successful apply: power-cycle the Kronos.
# (A soft reboot may wedge the OmapNKS4 panel chip; only a full power-off
# of ~60 s reliably resets it.)

set -e
umask 022

# ----------------------------------------------------------------------------
# Configuration: known good MD5s
# ----------------------------------------------------------------------------
MD5_LOADMOD_STOCK="d1697c9b1c478c0dcdfaef71516fe5f2"
MD5_LOADMOD_PATCHED="28d1cec16f1d893f1d78241b62a989d9"
MD5_LOADOA_STOCK="8a3d61f3332d7bcf694e8c05845b4754"
MD5_LOADOA_PATCHED="d17c26036fa0f51f5f4c0ef2f6f424bf"
MD5_USBMIDI_STOCK="fae9ff96711b86791a83272e5affb334"      # /sbin/ V1 variant (102741 bytes)
MD5_USBMIDI_CRYPTOLOOP="e6b16f79b4216d4f7e734fd1d8bacdfd" # /korg/Mod/ V2 (102931 bytes) — WRONG for /sbin/
MD5_OA_STOCK="955636c2b11a70a1dbecefaaa7bd4f80"           # from /korg/Mod/
MD5_OA_PATCHED="163550b60b7508b2c0ba1fd314b0b944"
MD5_KORGUSB_STOCK="29fbd20cf729980e1cffd670391256b5"      # from /korg/Mod/

# All filesystem paths can be prefixed with ${KRONOS_ROOT}. On the Kronos this is
# unset (empty), so /sbin/foo means /sbin/foo. In an offline test sandbox, point
# it at the sandbox root: `KRONOS_ROOT=/tmp/test sh kronos_patcher.sh`.
# The script also skips the cryptoloop mount when KRONOS_ROOT is set, since
# /korg/Mod is assumed to be a real directory pre-populated with the stock OA.ko
# and KorgUsbAudioDriver.ko in the sandbox.
: "${KRONOS_ROOT:=}"
BACKUP_DIR="${KRONOS_ROOT}/korg/rw/kronos_patcher_backup"
MOD_IMG="${KRONOS_ROOT}/korg/ro/Mod.img"
MOUNT_MOD="${KRONOS_ROOT}/korg/Mod"

# All other paths use these prefixed forms.
SBIN_LOADMOD="${KRONOS_ROOT}/sbin/loadmod.ko"
SBIN_LOADOA="${KRONOS_ROOT}/sbin/loadoa"
SBIN_USBMIDI="${KRONOS_ROOT}/sbin/USBMidiAccessory.ko"
SBIN_OA="${KRONOS_ROOT}/sbin/OA.ko"
SBIN_KORGUSB="${KRONOS_ROOT}/sbin/KorgUsbAudioDriver.ko"

# ----------------------------------------------------------------------------
# Helpers
# ----------------------------------------------------------------------------
log() { echo "[patcher] $*"; }
die() { echo "[patcher] FATAL: $*" >&2; exit 1; }

md5_of() {
    [ -f "$1" ] || { echo "_no_file_"; return; }
    md5sum "$1" 2>/dev/null | cut -d' ' -f1
}

assert_md5() {
    # assert_md5 <file> <expected_md5> <description>
    local file="$1" want="$2" desc="$3" got
    [ -f "$file" ] || die "$desc: missing file $file"
    got=$(md5_of "$file")
    [ "$got" = "$want" ] || die "$desc: $file md5=$got, want $want"
}

# Convert a hex string (e.g. "9090e8") on stdin to raw bytes on stdout.
# Uses only POSIX `printf` features (no `\xNN` — dash's `printf %b` doesn't decode
# those; only `\NNN` octal escapes work portably across dash and bash). `od`,
# `xxd`, `hexdump` are not available on the Kronos.
hex_to_bin() {
    local h i pair dec fmt=""
    h=$(cat)
    i=1
    while [ $i -le ${#h} ]; do
        pair=$(printf '%s' "$h" | cut -c$i-$((i+1)))
        dec=$(printf '%d' "0x$pair")
        fmt="${fmt}\\$(printf '%03o' "$dec")"
        i=$((i + 2))
    done
    printf '%b' "$fmt"
}

# Compute md5 of an arbitrary hex string (without writing to a temp file).
md5_of_hex() {
    printf '%s' "$1" | hex_to_bin | md5sum | cut -d' ' -f1
}

# md5 of a byte range within a file.
md5_of_range() {
    # md5_of_range <file> <offset> <count>
    dd if="$1" bs=1 skip="$2" count="$3" 2>/dev/null | md5sum | cut -d' ' -f1
}

# Apply a single patch: <file> <offset_decimal> <orig_hex> <new_hex>.
# Verifies the original bytes match (via md5 of range vs md5 of expected hex),
# writes the new bytes, then re-verifies. No reliance on od/xxd/hexdump.
apply_patch() {
    local file="$1" off="$2" want="$3" new="$4"
    local len got_md5 want_md5 new_md5
    len=$(( ${#want} / 2 ))
    want_md5=$(md5_of_hex "$want")
    got_md5=$(md5_of_range "$file" "$off" "$len")
    if [ "$got_md5" != "$want_md5" ]; then
        die "patch site: $file offset $off has bytes md5=$got_md5, expected md5=$want_md5 ($want)"
    fi
    printf '%s' "$new" | hex_to_bin \
        | dd of="$file" bs=1 seek="$off" count="$len" conv=notrunc 2>/dev/null
    new_md5=$(md5_of_hex "$new")
    got_md5=$(md5_of_range "$file" "$off" "$len")
    if [ "$got_md5" != "$new_md5" ]; then
        die "patch site: $file offset $off post-write md5=$got_md5, wanted $new_md5 ($new)"
    fi
}

# ----------------------------------------------------------------------------
# Mode dispatch
# ----------------------------------------------------------------------------
MODE="apply"
case "${1:-}" in
    --verify) MODE="verify" ;;
    --revert) MODE="revert" ;;
    "")       MODE="apply" ;;
    -h|--help|*) echo "usage: $0 [--verify | --revert]"; exit 1 ;;
esac

# Detect already-patched state
ALREADY_PATCHED=0
if [ "$(md5_of $SBIN_LOADMOD)" = "$MD5_LOADMOD_PATCHED" ] \
&& [ "$(md5_of $SBIN_LOADOA)"    = "$MD5_LOADOA_PATCHED" ] \
&& [ "$(md5_of $SBIN_OA)"     = "$MD5_OA_PATCHED" ]; then
    ALREADY_PATCHED=1
fi

case "$MODE" in
verify)
    log "Current state:"
    log "  loadmod.ko          md5=$(md5_of $SBIN_LOADMOD)  (stock=$MD5_LOADMOD_STOCK patched=$MD5_LOADMOD_PATCHED)"
    log "  loadoa              md5=$(md5_of $SBIN_LOADOA)  (stock=$MD5_LOADOA_STOCK patched=$MD5_LOADOA_PATCHED)"
    log "  USBMidiAccessory.ko md5=$(md5_of $SBIN_USBMIDI)  (V1-stock=$MD5_USBMIDI_STOCK V2-wrong=$MD5_USBMIDI_CRYPTOLOOP)"
    log "  OA.ko               md5=$(md5_of $SBIN_OA)  (stock=$MD5_OA_STOCK patched=$MD5_OA_PATCHED)"
    log "  KorgUsbAudio.ko     md5=$(md5_of $SBIN_KORGUSB)  (stock=$MD5_KORGUSB_STOCK)"
    log "patched=$ALREADY_PATCHED"
    exit 0
    ;;
revert)
    [ -d "$BACKUP_DIR" ] || die "no backup at $BACKUP_DIR — nothing to revert"
    log "restoring originals from $BACKUP_DIR"
    cp -p "$BACKUP_DIR/loadmod.ko"          $SBIN_LOADMOD
    cp -p "$BACKUP_DIR/loadoa"              $SBIN_LOADOA
    cp -p "$BACKUP_DIR/USBMidiAccessory.ko" $SBIN_USBMIDI
    rm -f $SBIN_OA $SBIN_KORGUSB
    sync
    log "done. Power-cycle the Kronos to return to stock behaviour."
    exit 0
    ;;
apply)
    if [ "$ALREADY_PATCHED" = "1" ]; then
        log "already patched — nothing to do"
        exit 0
    fi
    ;;
esac

# ----------------------------------------------------------------------------
# Verify stock binaries (no changes yet)
# ----------------------------------------------------------------------------
log "verifying stock binaries"
assert_md5 $SBIN_LOADMOD          "$MD5_LOADMOD_STOCK" "stock loadmod.ko"
assert_md5 $SBIN_LOADOA              "$MD5_LOADOA_STOCK"  "stock loadoa"
assert_md5 $SBIN_USBMIDI "$MD5_USBMIDI_STOCK" "stock USBMidiAccessory.ko (V1)"

# Common pitfall: if /sbin/USBMidiAccessory.ko is the /korg/Mod/ variant, the
# patched loadmod is fine (MD5 check is bypassed) but the user might have
# clobbered the wrong variant. Warn loudly.
if [ "$(md5_of $SBIN_USBMIDI)" = "$MD5_USBMIDI_CRYPTOLOOP" ]; then
    die "$SBIN_USBMIDI is the /korg/Mod/ V2 variant. Restore the V1 stock /sbin/ variant first."
fi

# We need /korg/Mod mounted to copy out the stock OA.ko and KorgUsbAudioDriver.ko.
# The currently-running (stock) loadmod has the cryptoloop hook installed, so a
# plain mount of Mod.img will be intercepted and decrypted correctly.
UNMOUNT_MOD_AFTER=0
if [ -n "$KRONOS_ROOT" ]; then
    log "KRONOS_ROOT set; skipping cryptoloop mount (assuming $MOUNT_MOD pre-populated)"
elif ! grep -q " $MOUNT_MOD " /proc/mounts; then
    log "mounting cryptoloop $MOD_IMG -> $MOUNT_MOD"
    mount -t ext2 "$MOD_IMG" "$MOUNT_MOD" || \
        die "failed to mount $MOUNT_MOD — loadmod may not be loaded; try after a clean boot"
    UNMOUNT_MOD_AFTER=1
fi

assert_md5 "$MOUNT_MOD/OA.ko"                 "$MD5_OA_STOCK"      "/korg/Mod/OA.ko"
assert_md5 "$MOUNT_MOD/KorgUsbAudioDriver.ko"  "$MD5_KORGUSB_STOCK" "/korg/Mod/KorgUsbAudioDriver.ko"

# ----------------------------------------------------------------------------
# Back up originals
# ----------------------------------------------------------------------------
log "backing up originals to $BACKUP_DIR"
mkdir -p "$BACKUP_DIR"
cp -p $SBIN_LOADMOD          "$BACKUP_DIR/loadmod.ko"
cp -p $SBIN_LOADOA              "$BACKUP_DIR/loadoa"
cp -p $SBIN_USBMIDI "$BACKUP_DIR/USBMidiAccessory.ko"
{
    echo "$MD5_LOADMOD_STOCK  loadmod.ko"
    echo "$MD5_LOADOA_STOCK   loadoa"
    echo "$MD5_USBMIDI_STOCK  USBMidiAccessory.ko"
} > "$BACKUP_DIR/originals.md5"
sync

# Rollback trap: on any error from here on, restore originals
ROLLBACK=1
on_failure() {
    [ "$ROLLBACK" = "1" ] || return
    log "ROLLING BACK to originals"
    cp -p "$BACKUP_DIR/loadmod.ko"          $SBIN_LOADMOD    2>/dev/null
    cp -p "$BACKUP_DIR/loadoa"              $SBIN_LOADOA        2>/dev/null
    cp -p "$BACKUP_DIR/USBMidiAccessory.ko" $SBIN_USBMIDI 2>/dev/null
    rm -f $SBIN_OA $SBIN_KORGUSB
    sync
}
trap on_failure EXIT

# ----------------------------------------------------------------------------
# Stage 1: copy OA.ko and KorgUsbAudioDriver.ko out of the cryptoloop into /sbin/
# ----------------------------------------------------------------------------
log "copying OA.ko and KorgUsbAudioDriver.ko from $MOUNT_MOD -> /sbin/"
cp -p "$MOUNT_MOD/OA.ko"                $SBIN_OA
cp -p "$MOUNT_MOD/KorgUsbAudioDriver.ko" $SBIN_KORGUSB
sync
assert_md5 $SBIN_OA                "$MD5_OA_STOCK"       "freshly-copied OA.ko"
assert_md5 $SBIN_KORGUSB "$MD5_KORGUSB_STOCK"  "freshly-copied KorgUsbAudioDriver.ko"

if [ "$UNMOUNT_MOD_AFTER" = "1" ]; then
    umount "$MOUNT_MOD" 2>/dev/null || log "warn: could not unmount $MOUNT_MOD"
fi

# ----------------------------------------------------------------------------
# Stage 2: patch loadmod.ko — 3 bypasses
# ----------------------------------------------------------------------------
log "patching $SBIN_LOADMOD (3 bypasses)"
# (1) NOP test+jne after VerifyCodeIntegrityMd5 (init_module check 1; error code 1)
apply_patch $SBIN_LOADMOD 22317 "85c00f85a3000000" "9090909090909090"
# (2) NOP JNE after RetrieveSecurityICKey (init_module check 4 dongle gate)
apply_patch $SBIN_LOADMOD 22449 "7547"             "9090"
# (3) JMP past inner 16-byte MD5 check inside RetrieveSecurityICKey.
#     Lands at start of success path (GetRandomBytesWrapper call); the Atmel/
#     RSA chain runs naturally and ends with BuildCdromCommandStruct() which
#     populates the cryptoloop-key globals. Without this, status=0 lies and
#     subsequent cryptoloop mounts fail with "ext2 not found on loopN".
apply_patch $SBIN_LOADMOD 16304 "0f85e7feffff"     "e91e01000090"
assert_md5 $SBIN_LOADMOD "$MD5_LOADMOD_PATCHED" "patched loadmod.ko"

# ----------------------------------------------------------------------------
# Stage 3: patch loadoa — redirect /korg/Mod/OA.ko and KorgUsbAudioDriver.ko to /sbin/
# ----------------------------------------------------------------------------
log "patching $SBIN_LOADOA (path redirects)"
apply_patch $SBIN_LOADOA 13975 "6b6f7267"                              "7362696e"
apply_patch $SBIN_LOADOA 13980 "4d6f642f4f412e6b6f"                    "4f412e6b6f00000000"
apply_patch $SBIN_LOADOA 14785 "6b6f7267"                              "7362696e"
apply_patch $SBIN_LOADOA 14790 "4d"                                    "4b"
apply_patch $SBIN_LOADOA 14792 "642f4b6f7267557362417564"              "7267557362417564696f4472"
apply_patch $SBIN_LOADOA 14805 "6f44"                                  "7665"
apply_patch $SBIN_LOADOA 14808 "697665722e6b6f"                        "2e6b6f00000000"
assert_md5 $SBIN_LOADOA "$MD5_LOADOA_PATCHED" "patched loadoa"

# ----------------------------------------------------------------------------
# Stage 4: patch OA.ko — EX-bank authorization bypass (56 byte runs)
# ----------------------------------------------------------------------------
log "patching $SBIN_OA (56 EX-auth bypass patches)"
# Patches in CSTGPatch::IsUsingAnyUnauthorizedMultisamples (and the 4 COMDAT
# overrides), plus the helper functions in main .text. Each patch turns a
# 'check-and-return-true' into 'return false' or NOPs the comparison entirely.
apply_patch $SBIN_OA 48196 "85c0745ac705"                              "eb5c90909090"
apply_patch $SBIN_OA 48206 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 48216 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 48226 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 48236 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 48246 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 48256 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 48266 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 48276 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 48286 "1f000000"                                  "90909090"
apply_patch $SBIN_OA 48707 "c705"                                      "9090"
apply_patch $SBIN_OA 48713 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 48723 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 48733 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 48743 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 48753 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 48763 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 48773 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 48783 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 48793 "1f000000"                                  "90909090"
apply_patch $SBIN_OA 65859 "7409817805cc39fb22745ac705"                "eb639090909090909090909090"
apply_patch $SBIN_OA 65876 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 65886 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 65896 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 65906 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 65916 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 65926 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 65936 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 65946 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 65956 "1f000000"                                  "90909090"
apply_patch $SBIN_OA 70222 "7409817805cc39fb22745ac705"                "eb639090909090909090909090"
apply_patch $SBIN_OA 70239 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 70249 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 70259 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 70269 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 70279 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 70289 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 70299 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 70309 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 70319 "1f000000"                                  "90909090"
apply_patch $SBIN_OA 71736 "7409817805cc39fb22745ac705"                "eb639090909090909090909090"
apply_patch $SBIN_OA 71753 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 71763 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 71773 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 71783 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 71793 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 71803 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 71813 "3333333fc705"                              "909090909090"
apply_patch $SBIN_OA 71823 "cdcc4cbec705"                              "909090909090"
apply_patch $SBIN_OA 71833 "1f000000"                                  "90909090"
apply_patch $SBIN_OA 1342304 "8b40088b52280fbf40088b44023485c00f95c0"  "31c0c390909090909090909090909090909090"
apply_patch $SBIN_OA 1446080 "8b40088b52280fbf40088b44020485c00f95c0"  "31c0c390909090909090909090909090909090"
apply_patch $SBIN_OA 5961536 "8b92d0010000"                            "31c0c3909090"
apply_patch $SBIN_OA 5969600 "8b9278030000"                            "31c0c3909090"
apply_patch $SBIN_OA 5979984 "8b9225040000"                            "31c0c3909090"
apply_patch $SBIN_OA 5985008 "8b92ac000000"                            "31c0c3909090"
assert_md5 $SBIN_OA "$MD5_OA_PATCHED" "patched OA.ko"

# ----------------------------------------------------------------------------
# All patches verified
# ----------------------------------------------------------------------------
ROLLBACK=0
trap - EXIT
sync
log ""
log "=============================================="
log "All patches applied and verified successfully."
log "=============================================="
log "  $SBIN_LOADMOD          = patched ($MD5_LOADMOD_PATCHED)"
log "  $SBIN_LOADOA              = patched ($MD5_LOADOA_PATCHED)"
log "  $SBIN_OA               = patched ($MD5_OA_PATCHED)  (NEW, copied from cryptoloop)"
log "  $SBIN_KORGUSB = stock  ($MD5_KORGUSB_STOCK)  (NEW, copied from cryptoloop)"
log "  $SBIN_USBMIDI = V1 stock ($MD5_USBMIDI_STOCK)  (unchanged)"
log ""
log "Backups: $BACKUP_DIR"
log ""
log "Next step: POWER-CYCLE the Kronos (full power-off, ~60 s, then on)."
log "  A soft 'reboot' may wedge the OmapNKS4 panel chip and leave the Kronos"
log "  stuck on 'system cannot start' until another power cycle. Don't use it."
log ""
log "If anything goes wrong on next boot, SSH back in and run:"
log "  sh kronos_patcher.sh --revert"
log "then power-cycle again."
exit 0
