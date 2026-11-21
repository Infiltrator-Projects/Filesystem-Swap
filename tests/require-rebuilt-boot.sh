#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Only the consumer's published package can establish BOOT acceptance.
set -euo pipefail
require_rebuilt_boot() {
    local work version package sha root efi
    local -a validators
    work="${RUNNER_TEMP:?}/infiltrator-boot-consumer"
    mkdir -p "$work"
    version=0.1.161
    package="infiltrator-boot_${version}_amd64.deb"
    sha=90044d40ce5f1307ca0ae1eb464aec751b72a8deb3991e566d4bc6c63d4d6c73
    curl -fsSL --retry 3 "https://github.com/Infiltrator-Projects/Boot/releases/download/v$version/$package" -o "$work/$package"
    printf '%s  %s\n' "$sha" "$work/$package" | sha256sum -c -
    [[ $(dpkg-deb -f "$work/$package" Package) == infiltrator-boot ]]
    [[ $(dpkg-deb -f "$work/$package" Version) == "$version" ]]
    root=$(mktemp -d "$work/package.XXXXXXXX")
    dpkg-deb -x "$work/$package" "$root"
    efi="$root/usr/share/infiltrator-boot/boot/boot_x64.efi"
    test -s "$efi"
    mapfile -d '' validators < <(find "$root/usr" -type f -name validate-module -executable -print0)
    if (( ${#validators[@]} != 1 )); then
        echo "::error::Rebuilt BOOT v$version provides ${#validators[@]} validate-module executables. The provider has no BOOT validation result." >&2
        exit 1
    fi
    printf 'IFS_BOOT_VALIDATOR=%s\nIFS_BOOT_EFI=%s\n' "${validators[0]}" "$efi" >> "${GITHUB_ENV:?}"
    export IFS_BOOT_VALIDATOR="${validators[0]}" IFS_BOOT_EFI="$efi"
}
require_rebuilt_boot
