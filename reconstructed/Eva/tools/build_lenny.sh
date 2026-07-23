#!/bin/sh
# build_lenny.sh - compile + real-link Eva's reconstruction against the real, on-image
# target shared libraries, using a period-correct (glibc 2.7, GCC 4.3.2) cross-build
# chroot instead of this dev host's own toolchain.
#
# Why this exists: this host's own g++ (glibc 2.34-based) cannot target the real Kronos
# ABI at all -- a trivial test link requires GLIBC_2.34 symbols the real target's libc.so.6
# (glibc 2.7 exactly) doesn't export. See README.md's "Linking / build-ABI status" for the
# full story. Debian Lenny (5.0) shipped glibc 2.7 exactly, period-matching the real
# target, so a Lenny i386 chroot is a working substitute cross-toolchain -- confirmed by a
# real link against the actual on-image libs (RestoreDVD_SystemMNT) succeeding with ZERO
# unresolved libc/libstdc++/OS-level symbols; every remaining unresolved symbol is one of
# Eva's own not-yet-reconstructed internal classes.
#
# One-time setup (already done as of 2026-07-22, documented here for reproducibility /
# in case the chroot needs rebuilding):
#   debootstrap --arch=i386 --no-check-gpg lenny \
#       /home/build/eva-toolchain/lenny-i386-root http://archive.debian.org/debian
#   (Lenny is long EOL -- only available via archive.debian.org; --no-check-gpg because
#   its signing keys predate this host's default keyring)
#   echo 'deb [trusted=yes] http://archive.debian.org/debian lenny main' \
#       > $CHROOT/etc/apt/sources.list
#   chroot $CHROOT apt-get update
#   chroot $CHROOT apt-get install -y --allow-unauthenticated \
#       g++ build-essential libssl-dev libxml2-dev uuid-dev zlib1g-dev
#
# This script does the per-invocation part: mount, build, link, unmount.

set -e

CHROOT=/home/build/eva-toolchain/lenny-i386-root
EVA_SRC=/home/share/kronosology/reconstructed/Eva
TARGET_LIBS=/home/share/RestoreDVD_SystemMNT/mnt/lib

mkdir -p "$CHROOT/mnt/eva" "$CHROOT/mnt/target-libs"

mountpoint -q "$CHROOT/proc" || mount --bind /proc "$CHROOT/proc"
mountpoint -q "$CHROOT/sys" || mount --bind /sys "$CHROOT/sys"
mountpoint -q "$CHROOT/dev" || mount --bind /dev "$CHROOT/dev"
mountpoint -q "$CHROOT/mnt/eva" || mount --bind "$EVA_SRC" "$CHROOT/mnt/eva"
mountpoint -q "$CHROOT/mnt/target-libs" || mount --bind "$TARGET_LIBS" "$CHROOT/mnt/target-libs"

echo "--- make objs (chroot g++ 4.3.2, target glibc 2.7 headers) ---"
chroot "$CHROOT" /bin/sh -c "cd /mnt/eva && rm -rf objs && CXX=g++ CXXFLAGS='-Wall -Wextra -g -Iinclude -std=gnu++98' make objs"

echo "--- link attempt against REAL on-image libs (RestoreDVD_SystemMNT) ---"
chroot "$CHROOT" /bin/sh -c "cd /mnt/eva && g++ -o /tmp/eva_boot_test \$(find objs -name '*.o') -L/mnt/target-libs -Wl,-rpath-link,/mnt/target-libs -lpthread" \
	&& echo "LINK OK" \
	|| echo "LINK INCOMPLETE (expected until Stage 4+ classes are reconstructed -- see unresolved symbols above)"

echo "--- done. Binary (if link succeeded) is at $CHROOT/tmp/eva_boot_test ---"
