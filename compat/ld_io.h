/* SPDX-License-Identifier: GPL-3.0-or-later
 * Thin adapter to the canonical Common exact-I/O primitive.
 * swap_native.c is the unchanged code imported from Defragmenter.
 */
#ifndef IFS_SWAP_LD_IO_ADAPTER_H
#define IFS_SWAP_LD_IO_ADAPTER_H
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include "infiltratr/posix_io.h"
static inline ssize_t ld_pread_full(int fd, void *buffer, size_t length, uint64_t at) {
    if (length > (size_t)SSIZE_MAX) { errno = EOVERFLOW; return -1; }
    return infiltratr_pread_full(fd, buffer, length, at) == 0 ? (ssize_t)length : -1;
}
#endif
