# Swap module migration (2026-10-30)
Defragmenter's original native swap analyser, native data mapper and worker
are now source-owned by this repository under native/,
and built into SWAP.so. The original source files are deliberately unchanged;
a minimal compatibility adapter delegates exact I/O to the pinned, authoritative
Libraries Common implementation rather than copying Defragmenter's I/O engine.

SWAP.so exports a versioned format-specific worker/format/verify interface and
the Common V1 format-neutral PROBE/FORMAT interface. Its test-media SWAPSPACE2
writer and format verifier live in the module rather than in Defragmenter.
CI builds the actual SO and runs the imported original native regression plus
a module formatting/corruption test. The filesystem has no files to relocate,
so no SWAP defragmentation capability is advertised.

This statement covers SWAP only. It does not certify the other filesystem
drivers, Defragmenter's whole migration, EFI/KO, or release packaging.
