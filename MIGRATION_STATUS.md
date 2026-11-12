# SWAP Common V2 migration — 11 November 2026

SWAP.so has one public filesystem entry point:
`infiltrator_filesystem_api_v2`. Its Common V2 CORE group
implements bounded read-only SWAPSPACE2 probing, and its VOLUME group
implements exclusive, durably flushed 4 KiB-page SWAPSPACE2 formatting.

The retired filesystem exports, header and
old host adaptation are removed. No previous-ABI fallback is built.
The independently owned native analyser remains in this repository,
but its historical CLI worker is not linked into the shipped SO;
it is not advertised as a Common V2 file or defrag capability.

The V2 contract test exercises missing media, read-only refusal,
format, probe, unsupported variants and corrupt signatures. On-media
qualifications beyond these tests, firmware, kernel, and the other
filesystems require separate evidence.

## Strict Common V2 SWAP formatting UUID safety (1.0.10)
Corrected a significant formatting bug: the source-owned `SWAP.so`
previously embedded the same constant 16-byte UUID in every formatted swap
area. The C-only formatter now obtains 128 bits of kernel entropy with
`getrandom()`, sets RFC 4122 v4/variant bits and writes no media if the
entropy request fails. The independent V2 regression formats disposable
media twice and asserts valid, distinct UUIDs. The Common V2 ABI is
unchanged. Linux-only entropy is intentionally provided by the kernel,
not a Python helper or external formatter. SWAP still has no mounted file
namespace or DEFRAG MAP/PLAN/EXECUTE/VERIFY/RECOVER capability.
