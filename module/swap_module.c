/* SPDX-License-Identifier: GPL-3.0-or-later
 * The existing native SWAP analyser/worker and the test-media formatter
 * live in SWAP.so. No other application writes SWAP headers.
 */
#include "ifs_swap_module_v1.h"
#include "infiltratr/filesystem_module.h"
#include "infiltratr/endian.h"
#include <limits.h>
#include <stdint.h>
#include <string.h>
int ifs_swap_worker_main(int, char **);
#define SWAP_PAGE 4096u
#define SWAP_UUID "LDTM-SWAP-UUID-1"
static int valid_io(const IfsSwapIoV1 *io) {
    return io && io->bytes >= SWAP_PAGE * 2u &&
        io->bytes % SWAP_PAGE == 0 && io->bytes / SWAP_PAGE <= UINT32_MAX &&
        io->read_at && io->flush;
}
static int swap_format(const IfsSwapIoV1 *io, const char *label) {
    if (!valid_io(io) || !io->write_at || !label || strlen(label) > 16u) return -1;
    uint8_t page[SWAP_PAGE] = {0};
    infiltratr_store_le32(page + 1024u, 1u);
    infiltratr_store_le32(page + 1028u, (uint32_t)(io->bytes / SWAP_PAGE - 1u));
    memcpy(page + 1036u, SWAP_UUID, 16u);
    memcpy(page + 1052u, label, strlen(label));
    memcpy(page + 4086u, "SWAPSPACE2", 10u);
    return io->write_at(io->context, 0, page, SWAP_PAGE) == 0 &&
           io->flush(io->context) == 0 ? 0 : -1;
}
static int swap_verify(const IfsSwapIoV1 *io) {
    if (!valid_io(io)) return -1;
    uint8_t page[SWAP_PAGE];
    return io->read_at(io->context, 0, page, SWAP_PAGE) == 0 &&
        memcmp(page + 4086u, "SWAPSPACE2", 10u) == 0 &&
        infiltratr_load_le32(page + 1024u) == 1u &&
        infiltratr_load_le32(page + 1028u) == io->bytes / SWAP_PAGE - 1u &&
        infiltratr_load_le32(page + 1032u) == 0u ? 0 : -1;
}
static const IfsSwapModuleV1 swap_module = {
    IFS_SWAP_MODULE_V1, sizeof(IfsSwapModuleV1), "swap",
    IFS_SWAP_CAP_RUN | IFS_SWAP_CAP_FORMAT | IFS_SWAP_CAP_VERIFY,
    ifs_swap_worker_main, swap_format, swap_verify
};
const IfsSwapModuleV1 *infiltrator_filesystem_swap_v1(void) {
    return &swap_module;
}
/* Common's generic format-neutral ABI: swap exposes only probe and format.
 * Linux swap does not support file operations or meaningful file defrag. */
static int common_read(void *p, uint64_t at, void *dest, uint32_t count) {
    const IfsFsHostIoV1 *io = (const IfsFsHostIoV1 *)p;
    return io->read_at(io->context, at, dest, count);
}
static int common_write(void *p, uint64_t at, const void *src, uint32_t count) {
    const IfsFsHostIoV1 *io = (const IfsFsHostIoV1 *)p;
    return io->write_at(io->context, at, src, count);
}
static int common_flush(void *p) {
    const IfsFsHostIoV1 *io = (const IfsFsHostIoV1 *)p;
    return io->flush(io->context);
}
static IfsSwapIoV1 adapt(const IfsFsHostIoV1 *io) {
    IfsSwapIoV1 out = {0};
    if (io) {
        out.context = (void *)io;
        out.bytes = io->volume_bytes;
        out.read_at = common_read;
        out.write_at = io->write_at ? common_write : NULL;
        out.flush = io->flush ? common_flush : NULL;
    }
    return out;
}
static int common_probe(const IfsFsHostIoV1 *io, uint32_t *variant) {
    if (!io || io->structure_bytes < sizeof(*io) || !variant) return IFS_FS_INVALID;
    IfsSwapIoV1 a = adapt(io);
    if (swap_verify(&a) != 0) return IFS_FS_CORRUPT;
    *variant = 1u;
    return IFS_FS_OK;
}
static int common_format(const IfsFsHostIoV1 *io, uint32_t variant, const char *label) {
    if (!io || io->structure_bytes < sizeof(*io) ||
        !(io->flags & IFS_FS_IO_EXCLUSIVE) || variant != 1u)
        return IFS_FS_INVALID;
    IfsSwapIoV1 a = adapt(io);
    return swap_format(&a, label) == 0 ? IFS_FS_OK : IFS_FS_IO_ERROR;
}
static const IfsFsModuleApiV1 common_api = {
    .abi_version = IFS_FS_COMMON_ABI_V1,
    .structure_bytes = sizeof(IfsFsModuleApiV1),
    .format_id = "swap",
    .capabilities = IFS_FS_CAP_PROBE | IFS_FS_CAP_FORMAT,
    .probe = common_probe,
    .format = common_format
};
const IfsFsModuleApiV1 *infiltrator_filesystem_api_v1(void) {
    return &common_api;
}
