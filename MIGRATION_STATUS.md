# SWAP Common V2 migration — 11 November 2026

SWAP.so has one public filesystem entry point:
`infiltrator_filesystem_api_v2`. Its Common V2 CORE group
implements bounded read-only SWAPSPACE2 probing, and its VOLUME group
implements exclusive, durably flushed 4 KiB-page SWAPSPACE2 formatting.

The retired format-specific SWAP V1 and Common V1 exports, header and
old host adaptation are removed. No previous-ABI fallback is built.
The independently owned native analyser remains in this repository,
but its historical CLI worker is not linked into the shipped SO;
it is not advertised as a Common V2 file or defrag capability.

The V2 contract test exercises missing media, read-only refusal,
format, probe, unsupported variants and corrupt signatures. On-media
qualifications beyond these tests, firmware, kernel, and the other
filesystems require separate evidence.
