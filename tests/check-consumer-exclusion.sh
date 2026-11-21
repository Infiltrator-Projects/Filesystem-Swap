#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# A missing or crashing validator cannot qualify a negative-only case.
set -euo pipefail
validator=${1:?production validator}
shift
(( $# > 0 ))
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
asset=infiltrator-filesystem-fat32_0.1.17_amd64.deb
curl -fsSL --retry 3 "https://github.com/Infiltrator-Projects/Filesystem-FAT32/releases/download/v0.1.17/$asset" -o "$work/$asset"
printf '%s  %s\n' f8db80caa384755028d2eb1d1c9b4a704bda82fc700d3f155119a3a77769c16f "$work/$asset" | sha256sum -c -
dpkg-deb -x "$work/$asset" "$work/fat32"
"$validator" "$work/fat32/System/Filesystems/FAT32.so"
for module in "$@"; do
    test -s "$module"
    status=0
    "$validator" "$module" || status=$?
    if ((status == 0 || status >= 126)); then
        echo "SWAP exclusion could not be established, validator exit $status" >&2
        exit 1
    fi
done
