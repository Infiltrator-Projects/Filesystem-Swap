#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Resolve one published consumer per job, confirm its rebuilt source lineage,
# then authenticate the downloaded asset against the digest supplied by GitHub.
set -euo pipefail
require_rebuilt_boot() {
    local work api tag version package sha root efi lineage release
    local -a validators headers
    work="${RUNNER_TEMP:?}/infiltrator-boot-consumer"
    mkdir -p "$work"
    api=https://api.github.com/repos/Infiltrator-Projects/Boot
    headers=(-H 'Accept: application/vnd.github+json')
    if [[ -n ${GH_TOKEN:-} ]]; then headers+=(-H "Authorization: Bearer $GH_TOKEN"); fi
    curl -fsSL --retry 3 "${headers[@]}" "$api/releases?per_page=100" > "$work/releases.json"
    # This project publishes its BOOT builds as prereleases. /releases/latest
    # excludes all of them. Select the most recently published versioned build.
    release=$(jq -ce '[.[] | select(.draft == false and (.tag_name | test("^v[0-9]+\\.[0-9]+\\.[0-9]+$")))] | max_by(.published_at) | select(. != null)' "$work/releases.json")
    tag=$(jq -er .tag_name <<< "$release")
    version=${tag#v}
    package="infiltrator-boot_${version}_amd64.deb"
    sha=$(jq -er --arg name "$package" '[.assets[] | select(.name==$name)] | select(length==1) | .[0].digest | capture("^sha256:(?<digest>[0-9a-f]{64})$").digest' <<< "$release")
    lineage=$(curl -fsSL --retry 3 "${headers[@]}" "$api/compare/c98b1ca4622228b5e7a3ae5e3205eca0e0282f7e...$tag" | jq -er .status)
    case "$lineage" in ahead|identical) ;; *) echo "::error::BOOT $tag is outside the rebuilt source lineage" >&2; return 1 ;; esac
    curl -fsSL --retry 3 "https://github.com/Infiltrator-Projects/Boot/releases/download/$tag/$package" -o "$work/$package"
    printf '%s  %s\n' "$sha" "$work/$package" | sha256sum -c -
    [[ $(dpkg-deb -f "$work/$package" Package) == infiltrator-boot ]]
    [[ $(dpkg-deb -f "$work/$package" Version) == "$version" ]]
    root=$(mktemp -d "$work/package.XXXXXXXX")
    dpkg-deb -x "$work/$package" "$root"
    efi="$root/usr/share/infiltrator-boot/boot/boot_x64.efi"
    test -s "$efi"
    mapfile -d '' validators < <(find "$root/usr" -type f -name validate-module -executable -print0)
    if (( ${#validators[@]} != 1 )); then
        echo "::error::Rebuilt BOOT $tag provides ${#validators[@]} validate-module executables. The provider has no BOOT validation result." >&2
        return 1
    fi
    printf 'IFS_BOOT_VALIDATOR=%s\nIFS_BOOT_EFI=%s\n' "${validators[0]}" "$efi" >> "${GITHUB_ENV:?}"
    export IFS_BOOT_VALIDATOR="${validators[0]}" IFS_BOOT_EFI="$efi"
}
require_rebuilt_boot
