# SWAP Common V2 and BOOT policy

The production `/System/Filesystems/SWAP.so` implements the **hosted-only**
Common V2 CORE probe and VOLUME formatter. It has no mountable namespace,
directories, files, paths, or file reads: `SWAPSPACE2` is a page-allocation
format, not a filesystem capable of carrying a kernel or initramfs.

Do **not** set `IFS_FS_PROFILE_FREESTANDING` or
`IFS_FS_PROFILE_FIRMWARE` for this module. Do not implement fake
`mount`/`stat`/`list`/`read` callbacks, generate a firmware ELF, or alter
BOOT's validator to accept a nonbootable disk image.

The release pipeline must prove three distinct facts: Common V2 validates as
`swap` under a hosted client, native SWAP media probing/formatting reject
corrupt headers and do not modify media without a writable exclusive lease,
and the unchanged BOOT validator **rejects** the actual installed `SWAP.so`.

The native formatter obtains fresh UUID entropy from the hosted OS. Production
firmware must never gain an entropy dependency solely to misrepresent SWAP as
a bootable format. The older source implementation is retained for legitimate
analysis and formatting; no filesystem-specific kernel driver substitutes for
the Common V2 module.
