# Swap migration — 2026-10-30

Native Swap analyser/worker source copied without modification from Infiltrator-Projects/Defragmenter commit 30af892d8897d3aeaeefeac1bfcdb9620a0a5c72. This is a **source snapshot**, not a separately functional shared object, kernel module or EFI driver. The worker still depends on Defragmenter's host interfaces; its existing implementation remains active there until a separately tested Common-ABI filesystem module is integrated.

Linux swap is an allocation-summary / analysis-only storage format and has no regular files to defragment; do not advertise file relocation or generic file create/read/write.
