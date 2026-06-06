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
