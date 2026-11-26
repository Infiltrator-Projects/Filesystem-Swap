# Swap: technical reference

## Scope: paging store rather than filesystem

Linux swap is a backing store for virtual-memory pages, not a hierarchical filesystem. It has no directories, file names, inodes, native extent tree, journal or persistent ordinary-file allocator. The historic SWAP-SPACE signature identifies the obsolete bitmap-based version; SWAPSPACE2 identifies the modern header with a bad-page list. A swap file inside a filesystem has an outer filesystem allocation mapping, distinct from the inner swap header and page offsets. The current reference uses Linux v6.12 include/linux/swap.h and mm/swapfile.c. A valid header alone is not permission to activate a swap area.

## Page geometry and signatures

The swap header occupies the first memory-page-sized region of the area. Its ten-byte ASCII signature sits at page_size-10, not always offset 4086. At 4096 bytes that offset is 4086; at 65536 it is 65526. Detect candidate page sizes using bounded reads and validate version and geometry; do not infer the host page size from an unrelated device sector size. Page 0 is reserved. Page n starts at n*page_size relative to the swap area. For a swap file the host mapping must translate this logical position to stable storage. Reject truncated headers and arithmetic overflow.

## Modern header byte inventory

The first 1024 bytes are reserved for boot/disklabel purposes. At 1024: 32-bit version (normally 1); 1028 last usable page index; 1032 number of bad pages; 1036 16-byte UUID; 1052 16-byte volume label; 1068..1535 reserved/padding; bad-page list begins at 1536 with 32-bit page indices. The magic is at page_size-10. Version 1 can be read in either byte order by detecting the swapped version and decoding all numerical fields consistently; UUID and label bytes are not swapped as integers. Bound bad-page count by available space, reject duplicates and indices outside 1..last_page. A declared last_page must fit the actual backing area.

## Worked capacity and page map

For an area of 256 MiB with 4096-byte pages, total page slots are 65536; last page index is 65535. With no bad pages the maximum usable storage is 65535 pages, 268431360 bytes. With valid bad-page indices 7 and 500, 65533 pages remain. Index 500 begins at byte 2048000. This is structural capacity, not live free capacity. The kernel may impose a lower maximum based on swap-entry encoding. The runtime swap_map tracks per-slot use, references and special states; ordinary page allocation state is not stored as a modern on-disk persistent bitmap. An offline utility cannot infer which residual bytes belong to currently live processes.

## Legacy bitmap format and hibernation

Old SWAP-SPACE stores a page-availability bitmap in the first page before its signature; available page bits identify usable slots and page 0 is excluded. Its representable capacity is constrained by the header bitmap. Do not confuse it with the modern version-1 bad-page list. Historical kernel/architecture bit ordering must be verified against authentic fixtures before writing that legacy format. Suspend-to-disk can replace the ordinary signature with a resume marker and store a memory image. Such an area may have recovery significance; do not overwrite or reformat it on the assumption that its ordinary swap signature is absent. Hibernation image layout is a separate versioned protocol.

## Formatting, activation and defragmentation boundary

Formatting writes coherent page geometry, version, UUID/label, bad-page list and signature; it is not allocation of files or a secure erase of prior pages. Verify exclusive inactive ownership before any write. Native swapon builds runtime mapping and may refuse unsuitable backing files, holes, changing extents or unsupported CoW layouts. A swapfile must be deactivated before relocating its outer filesystem extents. A raw swap partition has no persistent file fragmentation to repair and must not advertise ordinary defragmentation, file creation or directory enumeration. Active pages must never be moved by an offline formatter. Keep live kernel state, on-disk header geometry and host filesystem ownership separate.

## Validation and source references

Pinned definitions: https://github.com/torvalds/linux/blob/v6.12/include/linux/swap.h ; validation: https://github.com/torvalds/linux/blob/v6.12/mm/swapfile.c . Independent format fixtures: util-linux mkswap and swaplabel (https://github.com/util-linux/util-linux). Test multiple page sizes, swapped byte order, invalid version, truncated area, oversized last page, duplicate/out-of-range bad pages, arbitrary residual content and missing or nonstandard signature. Verify header bytes, capacity and no writes during probe. Do not activate test media containing unknown or sensitive memory images. Runtime allocation and hibernation recovery are separate kernel functions. The earlier manual follows for provenance.
