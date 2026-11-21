# Filesystem-Swap

SWAP.so is an independent Linux swap-area provider using the canonical
Infiltratr Common V2 filesystem ABI. It has real, bounded on-disk probing
and disposable-media formatting. The source-owned checksum, geometry,
swap version, UUID and unusable-page validation are exercised in C tests.

## BOOT eligibility

A Linux swap area is a paging store, not a filesystem directory tree.
It cannot legitimately contain named boot files, kernels or initramfs
images. BOOT **must reject SWAP.so as a boot volume**.

This is an intentional capability distinction, not a failed V2 migration:

- CORE advertises real PROBE, but not MOUNT, STAT, LIST or READ.
- VOLUME advertises the format operation; the host must provide the
  necessary exclusive and writable capabilities.
- The provider advertises HOSTED, not a freestanding BOOT-reader profile.
- No fake directory entries, invented file reads, EFI driver, Linux
  filesystem-specific `.ko` or FUSE substitute is supplied.

The production BOOT-validator test must prove a policy rejection using an
**executable production validator**. A missing validator, failed download,
checksum mismatch or crashed command cannot count as a successful check.
The current BOOT 0.1.77 release package no longer contains the expected
`/usr/lib/infiltrator-boot/validate-module`; the associated integration
test deliberately fails closed until this BOOT-side contract is restored.
Hosted Common V2 media and packaging tests are separate qualifications.

## Installation

The Debian package installs exactly:

- `/System/Filesystems/SWAP.so`
- `/System/Resources/Documentation/Filesystem-Swap.pdf`

The native `.run` builds and installs the source-owned CPU-native variant
through the same Common V2 interface. Build-time tooling is not a
filesystem runtime dependency. Neither installer supplies a kernel
module as an alternative filesystem implementation.
