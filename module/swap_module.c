/* SPDX-License-Identifier: GPL-3.0-or-later
 * The existing native SWAP analyser/worker and the test-media formatter
 * live in SWAP.so. No other application writes SWAP headers.
 */
#include "ifs_swap_module_v1.h"
#include "infiltratr/filesystem_module_v2.h"
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
        io->read_at;
}
static int swap_format(const IfsSwapIoV1 *io, const char *label) {
    if (!valid_io(io) || !io->write_at || !io->flush ||
        !label || strlen(label) > 16u) return -1;
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
        (io->flags & IFS_FS_IO_READ_ONLY) ||
        !(io->flags & IFS_FS_IO_EXCLUSIVE) || variant != 1u ||
        !io->write_at || !io->flush)
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

/* One Common V2 contract: operate directly on verified V2 hosts.
 * The legacy Common V1 export above is transitional only; V2 never calls
 * it and does not contain a base_v1 pointer. The on-media Swap engine
 * remains unique in swap_format()/swap_verify().
 */
static int v2_read(void *ctx, uint64_t offset, void *p, uint32_t n)
{
    const IfsFsHostRuntimeV2 *h = ctx;
    return h->read_at(h->context, offset, p, n);
}
static int v2_write(void *ctx, uint64_t offset, const void *p, uint32_t n)
{
    const IfsFsHostRuntimeV2 *h = ctx;
    return h->write_at(h->context, offset, p, n);
}
static int v2_flush(void *ctx)
{
    const IfsFsHostRuntimeV2 *h = ctx;
    return h->flush(h->context);
}
static IfsSwapIoV1 native_v2_io(const IfsFsHostRuntimeV2 *h)
{
    IfsSwapIoV1 result = {0};
    result.context = (void *)h;
    result.bytes = h->volume_bytes;
    result.read_at = v2_read;
    result.write_at = h->write_at ? v2_write : NULL;
    result.flush = h->flush ? v2_flush : NULL;
    return result;
}
static int v2_probe(const IfsFsHostRuntimeV2 *h, uint32_t *variant)
{
    IfsSwapIoV1 io;
    if (!variant || !ifs_fs_host_v2_valid(h)) return IFS_FS_INVALID;
    io = native_v2_io(h);
    if (swap_verify(&io) != 0) return IFS_FS_CORRUPT;
    *variant = 1u;
    return IFS_FS_OK;
}
static int v2_format(const IfsFsHostRuntimeV2 *host,
                     uint32_t variant, const char *label, uint32_t flags)
{
    IfsSwapIoV1 io;
    if (!ifs_fs_host_v2_valid(host)) return IFS_FS_INVALID;
    if (flags) return IFS_FS_UNSUPPORTED;
    if (host->capabilities & IFS_FS_HOST_READ_ONLY) return IFS_FS_ACCESS;
    if (variant != 1u) return IFS_FS_UNSUPPORTED;
    if (host->logical_sector_bytes > SWAP_PAGE ||
        SWAP_PAGE % host->logical_sector_bytes != 0u)
        return IFS_FS_UNSUPPORTED;
    io = native_v2_io(host);
    return swap_format(&io, label) == 0 ? IFS_FS_OK : IFS_FS_IO_ERROR;
}
static const IfsFsCoreOpsV1 core_ops = {
    .header = {IFS_FS_GROUP_ABI_V1, sizeof(IfsFsCoreOpsV1),
               IFS_FS_GROUP_CORE, 0u, IFS_FS_CORE_PROBE},
    .probe = v2_probe
};
static const IfsFsVolumeOpsV1 volume_ops = {
    .header = {IFS_FS_GROUP_ABI_V1, sizeof(IfsFsVolumeOpsV1),
               IFS_FS_GROUP_VOLUME, 0u, IFS_FS_VOLUME_FORMAT},
    .format = v2_format
};
static const IfsFsGroupHeaderV2 *query_group(
    uint32_t id, uint32_t minimum, uint32_t maximum)
{
    if (minimum > IFS_FS_GROUP_ABI_V1 ||
        maximum < IFS_FS_GROUP_ABI_V1) return NULL;
    if (id == IFS_FS_GROUP_CORE) return &core_ops.header;
    if (id == IFS_FS_GROUP_VOLUME) return &volume_ops.header;
    return NULL;
}
static const IfsFsModuleApiV2 common_api_v2 = {
    IFS_FS_COMMON_ABI_V2, sizeof(IfsFsModuleApiV2), "swap",
    IFS_FS_PROFILE_HOSTED,
    IFS_FS_GROUP_BIT(IFS_FS_GROUP_CORE) |
    IFS_FS_GROUP_BIT(IFS_FS_GROUP_VOLUME), NULL, query_group
};
__attribute__((visibility("default")))
const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void)
{
    return &common_api_v2;
}
