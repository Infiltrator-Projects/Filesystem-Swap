/* SPDX-License-Identifier: GPL-3.0-or-later
 * SWAP.so exposes only the canonical, freestanding Common V2 contract.
 * Swap has no mountable namespace or file-defragmentation service.
 */
#include <infiltratr/filesystem_module_v2.h>
#include <infiltratr/endian.h>
#include <stdint.h>
#include <errno.h>
#include <sys/random.h>
#include <stdlib.h>
#include <string.h>

enum { SWAP_PAGE = 4096, SWAP_MAX_PAGE = 65536, SWAP_SIGNATURE = 10,
       SWAP_HEADER_BYTES = 1536 };
static const uint32_t pages[] = {4096u,8192u,16384u,32768u,65536u};
static const uint8_t hibernate_magic[8] =
    {0xedu,0xc3u,0x02u,0xe9u,0x98u,0x56u,0xe5u,0x0cu};

/* The on-disk bad-page table must not count a page twice. Sorting a
 * bounded scratch table avoids quadratic checks on 64 KiB swap headers. */
static int compare_page(const void *left, const void *right)
{
    const uint32_t a = *(const uint32_t *)left;
    const uint32_t b = *(const uint32_t *)right;
    return a < b ? -1 : a > b ? 1 : 0;
}

static int transfer(const IfsFsHostRuntimeV2 *host, uint64_t at,
                    uint8_t *buffer, uint32_t bytes, int write)
{
    uint32_t index, sector = host->logical_sector_bytes;
    if (bytes % sector || at > host->volume_bytes ||
        bytes > host->volume_bytes - at)
        return IFS_FS_INVALID;
    for (index = 0; index < bytes; index += sector) {
        int result = write ?
            host->write_at(host->context, at + index, buffer + index, sector) :
            host->read_at(host->context, at + index, buffer + index, sector);
        if (result)
            return IFS_FS_IO_ERROR;
    }
    return IFS_FS_OK;
}

static int probe(const IfsFsHostRuntimeV2 *host, uint32_t *variant)
{
    uint8_t *buffer;
    uint32_t i;
    int result = IFS_FS_NOT_FOUND;
    if (!variant || !ifs_fs_host_v2_valid(host))
        return IFS_FS_INVALID;
    *variant = 0u;
    buffer = host->allocate(host->context, SWAP_MAX_PAGE);
    if (!buffer)
        return IFS_FS_NO_MEMORY;
    for (i = 0; i < sizeof(pages)/sizeof(pages[0]); ++i) {
        uint32_t size = pages[i], last, bad_count, j, le, be;
        int little;
        if (host->volume_bytes < size ||
            host->logical_sector_bytes > size ||
            size % host->logical_sector_bytes)
            continue;
        result = transfer(host, 0u, buffer, size, 0);
        if (result != IFS_FS_OK)
            break;
        if (!memcmp(buffer, hibernate_magic, sizeof(hibernate_magic))) {
            result = IFS_FS_CORRUPT;
            break;
        }
        if (memcmp(buffer + size - SWAP_SIGNATURE, "SWAPSPACE2",
                   SWAP_SIGNATURE)) {
            result = IFS_FS_NOT_FOUND;
            continue;
        }
        le = infiltratr_load_le32(buffer + 1024u);
        be = infiltratr_load_be32(buffer + 1024u);
        little = le == 1u;
        if (!little && be != 1u) {
            result = IFS_FS_CORRUPT;
            break;
        }
        last = little ? infiltratr_load_le32(buffer + 1028u) :
                        infiltratr_load_be32(buffer + 1028u);
        bad_count = little ? infiltratr_load_le32(buffer + 1032u) :
                             infiltratr_load_be32(buffer + 1032u);
        if (!last || (uint64_t)last + 1u > host->volume_bytes / size ||
            bad_count > (size - SWAP_HEADER_BYTES - SWAP_SIGNATURE)/4u) {
            result = IFS_FS_CORRUPT;
            break;
        }
        if (bad_count) {
            uint32_t *bad_pages = host->allocate(
                host->context, (size_t)bad_count * sizeof(*bad_pages));
            int duplicate = 0;
            if (!bad_pages) {
                result = IFS_FS_NO_MEMORY;
                break;
            }
            for (j = 0; j < bad_count; ++j) {
                const uint32_t bad = little ?
                    infiltratr_load_le32(buffer + SWAP_HEADER_BYTES + 4u*j) :
                    infiltratr_load_be32(buffer + SWAP_HEADER_BYTES + 4u*j);
                if (!bad || bad > last)
                    break;
                bad_pages[j] = bad;
            }
            if (j == bad_count) {
                qsort(bad_pages, bad_count, sizeof(*bad_pages), compare_page);
                for (j = 1u; j < bad_count; ++j)
                    if (bad_pages[j] == bad_pages[j - 1u]) {
                        duplicate = 1;
                        break;
                    }
            }
            host->release(host->context, bad_pages);
            if (j != bad_count || duplicate) {
                result = IFS_FS_CORRUPT;
                break;
            }
        }
        *variant = size;
        result = IFS_FS_OK;
        break;
    }
    host->release(host->context, buffer);
    return result;
}

/* SWAP UUIDs are on-media identities; never reproduce a constant UUID on
 * every disk. getrandom() blocks for real kernel entropy. Any unavailable
 * RNG fails BEFORE the first media write, preserving the original volume. */
static int swap_random_uuid(uint8_t uuid[16])
{
    size_t done=0u;
    while(done<16u) {
        const ssize_t got=getrandom(uuid+done,16u-done,0u);
        if(got<0 && errno==EINTR)continue;
        if(got<=0)return IFS_FS_IO_ERROR;
        done+=(size_t)got;
    }
    uuid[6]=(uint8_t)((uuid[6]&0x0fu)|0x40u); /* RFC 4122 version 4 */
    uuid[8]=(uint8_t)((uuid[8]&0x3fu)|0x80u); /* variant 10 */
    return IFS_FS_OK;
}

static int format(const IfsFsHostRuntimeV2 *host, uint32_t variant,
                  const char *label, uint32_t flags)
{
    uint8_t header[SWAP_PAGE] = {0};
    uint32_t observed = 0u;
    size_t len;
    int result;
    if (!ifs_fs_host_v2_valid(host))
        return IFS_FS_INVALID;
    if (flags || variant != SWAP_PAGE)
        return IFS_FS_UNSUPPORTED;
    if ((host->capabilities & IFS_FS_HOST_READ_ONLY) ||
        !(host->capabilities & IFS_FS_HOST_EXCLUSIVE) ||
        !(host->capabilities & IFS_FS_HOST_DURABLE))
        return IFS_FS_ACCESS;
    if (host->logical_sector_bytes > SWAP_PAGE ||
        SWAP_PAGE % host->logical_sector_bytes ||
        host->volume_bytes < 2u*SWAP_PAGE ||
        host->volume_bytes % SWAP_PAGE ||
        host->volume_bytes/SWAP_PAGE > UINT32_MAX)
        return IFS_FS_UNSUPPORTED;
    if (!label)
        return IFS_FS_INVALID;
    len = strlen(label);
    if (len > 16u)
        return IFS_FS_INVALID;
    infiltratr_store_le32(header + 1024u, 1u);
    infiltratr_store_le32(header + 1028u,
        (uint32_t)(host->volume_bytes/SWAP_PAGE - 1u));
    result=swap_random_uuid(header + 1036u);
    if(result!=IFS_FS_OK)return result;
    memcpy(header + 1052u, label, len);
    memcpy(header + 4086u, "SWAPSPACE2", SWAP_SIGNATURE);
    result = transfer(host, 0u, header, SWAP_PAGE, 1);
    if (result != IFS_FS_OK || host->flush(host->context))
        return IFS_FS_IO_ERROR;
    result = probe(host, &observed);
    return result == IFS_FS_OK && observed == SWAP_PAGE ?
        IFS_FS_OK : IFS_FS_IO_ERROR;
}

static const IfsFsCoreOpsV2 core = {
    .header = {IFS_FS_GROUP_ABI_V2, sizeof(IfsFsCoreOpsV2),
               IFS_FS_GROUP_CORE, 0u, IFS_FS_CORE_PROBE},
    .probe = probe
};
static const IfsFsVolumeOpsV2 volume = {
    .header = {IFS_FS_GROUP_ABI_V2, sizeof(IfsFsVolumeOpsV2),
               IFS_FS_GROUP_VOLUME, 0u, IFS_FS_VOLUME_FORMAT},
    .format = format
};
static const IfsFsGroupHeaderV2 *group(uint32_t id,
                                       uint32_t minimum, uint32_t maximum)
{
    if (minimum > IFS_FS_GROUP_ABI_V2 ||
        maximum < IFS_FS_GROUP_ABI_V2)
        return NULL;
    if (id == IFS_FS_GROUP_CORE) return &core.header;
    if (id == IFS_FS_GROUP_VOLUME) return &volume.header;
    return NULL;
}
static const IfsFsModuleApiV2 api = {
    IFS_FS_COMMON_ABI_V2, sizeof(IfsFsModuleApiV2), "swap",
    IFS_FS_PROFILE_HOSTED,
    IFS_FS_GROUP_BIT(IFS_FS_GROUP_CORE) |
        IFS_FS_GROUP_BIT(IFS_FS_GROUP_VOLUME),
    group
};
__attribute__((visibility("default")))
const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void) { return &api; }
