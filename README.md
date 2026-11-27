# Filesystem-Swap

## 1. Purpose and format scope

Linux paging area, not a directory-bearing filesystem. This repository owns the format-specific implementation of `SWAP.so` for InfiltratorOS.

## 2. On-disk architecture and invariants

Interpret swap signatures, version, page geometry, UUID and unusable-page metadata. Recognise native suspend/hibernation signatures without overwriting them during probe; reject unusable geometry.

## 3. Common V2 implementation and capabilities

Hosted Common V2 exports genuine probe and format; it deliberately has no mount, stat, list or file read namespace. BOOT must reject it as a boot volume, not manufacture directory entries or treat absent validation as success. The canonical entry point is `infiltrator_filesystem_api_v2`. Consumers must negotiate the actual profile and operation masks and use bounded, partition-relative host I/O. A source-level callback does not by itself establish that every format variant or host profile is qualified.

## 4. Integrity, mutation, allocation and recovery

Formatting needs exclusive writable media. There is no file allocation, defragmentation, journal or ordinary filesystem recovery contract. Hibernation data must not be mistaken for disposable free pages. For destructive operations, require the applicable exclusive writable host, durability contract and format-specific validation. Never silently substitute a kernel or FUSE driver or treat a rejected or unavailable operation as success. Preserve original disk-format semantics; relocation and recovery are separate from ordinary file I/O.

## 5. Build, installation and release

The target installation is `/System/Filesystems/SWAP.so` and `/System/Resources/Documentation/Filesystem-Swap.pdf`. The project release policy is a portable amd64 DEB and a CPU-native source-first RUN, with GitHub's automatic source archive; do not treat this policy statement as verification of the current installer payload. The release workflow is the authoritative executable build and package specification. Historical adapters and test artifacts are not additional production filesystem authorities.

## 6. Qualification and limitations

[Recorded qualification workflow](https://github.com/Infiltrator-Projects/Filesystem-Swap/actions/runs/45520126664) · [Current workflows](https://github.com/Infiltrator-Projects/Filesystem-Swap/actions?query=branch%3Amain). A successful workflow establishes the tested scope only, not complete original-format parity, physical-media crash safety, or production BOOT/kernel integration. Use disposable images for destructive qualification.

## 7. Technical references and provenance

- [Provider implementation](module/swap_module.c)
- [Native design or source provenance](docs/COMMON_V2_BOOT_STATUS.md)
- [Migration and source history](MIGRATION_STATUS.md)
- [Original-format reference source](docs/Swap.pdfstreams) and [PDF](docs/Swap.pdf)
- [Project philosophy](docs/PHILOSOPHY.md)
- [Common ABI](https://github.com/Infiltrator-Projects/Libraries)

The original-format reference and historical migration notes describe different evidence from the current production provider. When descriptions disagree, the current exported descriptor, implementation, executable tests and exact-revision release evidence take precedence.

The [engineering reference](docs/Swap-TECHNICAL-REFERENCE.md) adds field-level original-format guidance. The PDF and editable reference contain one integrated manual.
