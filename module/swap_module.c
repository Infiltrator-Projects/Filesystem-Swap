/* SPDX-License-Identifier: GPL-3.0-or-later
 * SWAP.so exposes the Common V2 hosted probe/format contract only.
 * Swap space has no directory namespace, cannot contain boot payloads and
 * must deliberately fail BOOT's freestanding V2 module validation.
 * Random identifiers require the hosted OS entropy service.
 */
#include <infiltratr/filesystem_module_v2.h>
#include <infiltratr/endian.h>
#include <stdint.h>
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
    if (at % sector || bytes % sector || at > host->volume_bytes ||
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
    uint32_t i, loaded = 0u;
    int result = IFS_FS_NOT_FOUND;
    if (!variant)
        return IFS_FS_INVALID;
    *variant = 0u;
    if (!ifs_fs_host_v2_valid(host))
        return IFS_FS_INVALID;
    buffer = host->allocate(host->context, SWAP_MAX_PAGE);
    if (!buffer)
        return IFS_FS_NO_MEMORY;
    for (i = 0; i < sizeof(pages)/sizeof(pages[0]); ++i) {
        uint32_t size = pages[i], last, bad_count, j, le, be;
        int little;
        if (host->volume_bytes < size)
            break;
        /* The swap page belongs to the image. A host that exposes larger
         * logical sectors can still inspect it through a bounded sector read. */
        result = ifs_fs_host_read_bytes_v2(host, loaded, buffer + loaded,
                                           size - loaded);
        if (result != IFS_FS_OK)
            break;
        loaded = size;
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

/* SWAP UUIDs are on-media identities. Randomness comes from Common
 * V2 host entropy, never an OS-specific import or repeated static UUID.
 * An unavailable entropy provider fails BEFORE the first media write. */
static int swap_random_uuid(const IfsFsHostRuntimeV2 *host, uint8_t uuid[16])
{
    int rc=ifs_fs_host_random_bytes_v2(host,uuid,16u);
    if(rc!=IFS_FS_OK)return rc;
    uuid[6]=(uint8_t)((uuid[6]&0x0fu)|0x40u); /* RFC 4122 version 4 */
    uuid[8]=(uint8_t)((uuid[8]&0x3fu)|0x80u); /* variant 10 */
    return IFS_FS_OK;
}

/* A prior swap signature must not survive a failed destructive reformat.
 * Detect candidate page-end signatures using the same on-disk page sizes
 * as PROBE; clear only a matching signature, including one beyond the
 * newly selected page. The host supplies the exclusive durable lease. */
static int retire_signatures(const IfsFsHostRuntimeV2 *host)
{
    const uint8_t zero[SWAP_SIGNATURE]={0};
    uint8_t magic[SWAP_SIGNATURE];
    for (size_t i=0;i<sizeof(pages)/sizeof(pages[0]);++i) {
        if (pages[i]>host->volume_bytes) break;
        const uint64_t at=pages[i]-SWAP_SIGNATURE;
        int rc=ifs_fs_host_read_bytes_v2(host,at,magic,sizeof(magic));
        if (rc!=IFS_FS_OK) return rc;
        if (!memcmp(magic,"SWAPSPACE2",sizeof(magic))) {
            rc=ifs_fs_host_write_bytes_v2(host,at,zero,sizeof(zero));
            if (rc!=IFS_FS_OK) return rc;
        }
    }
    return host->flush(host->context) ? IFS_FS_IO_ERROR : IFS_FS_OK;
}

static int format(const IfsFsHostRuntimeV2 *host, uint32_t variant,
                  const char *label, uint32_t flags)
{
    uint32_t observed=0u;
    int supported=0;
    if (!ifs_fs_host_v2_valid(host)) return IFS_FS_INVALID;
    for (size_t i=0;i<sizeof(pages)/sizeof(pages[0]);++i)
        if (pages[i]==variant) supported=1;
    if (flags || !supported) return IFS_FS_UNSUPPORTED;
    if ((host->capabilities & IFS_FS_HOST_READ_ONLY) ||
        !(host->capabilities & IFS_FS_HOST_EXCLUSIVE) ||
        !(host->capabilities & IFS_FS_HOST_DURABLE))
        return IFS_FS_ACCESS;
    if (host->logical_sector_bytes>variant ||
        variant%host->logical_sector_bytes ||
        host->volume_bytes<2u*(uint64_t)variant ||
        host->volume_bytes%variant ||
        host->volume_bytes/variant>UINT32_MAX)
        return IFS_FS_UNSUPPORTED;
    if (!label) return IFS_FS_INVALID;
    size_t len=0;
    while (len<=16u && label[len]) ++len;
    if (len>16u) return IFS_FS_INVALID;
    uint8_t *header=host->allocate(host->context,variant);
    if (!header) return IFS_FS_NO_MEMORY;
    memset(header,0,variant);
    infiltratr_store_le32(header+1024u,1u);
    infiltratr_store_le32(header+1028u,
        (uint32_t)(host->volume_bytes/variant-1u));
    int result=swap_random_uuid(host,header+1036u);
    if (result!=IFS_FS_OK) goto done;
    memcpy(header+1052u,label,len);

    /* Publish the magic only after the unsigned header has reached
     * durable storage. With a 4 KiB host sector this remains valid even
     * when the entire swap page is one indivisible host transfer.
     * A failed final flush is not success; retire the magic best effort. */
    result=retire_signatures(host);
    if (result!=IFS_FS_OK) goto done;
    result=transfer(host,0u,header,variant,1);
    if (result!=IFS_FS_OK) goto done;
    if (host->flush(host->context)) {
        result=IFS_FS_IO_ERROR;
        goto done;
    }
    memcpy(header+variant-SWAP_SIGNATURE,"SWAPSPACE2",SWAP_SIGNATURE);
    const uint32_t sector=host->logical_sector_bytes;
    result=transfer(host,variant-sector,header+variant-sector,sector,1);
    if (result!=IFS_FS_OK || host->flush(host->context)) {
        const uint8_t zero[SWAP_SIGNATURE]={0};
        (void)ifs_fs_host_write_bytes_v2(host,variant-SWAP_SIGNATURE,
                                         zero,sizeof(zero));
        (void)host->flush(host->context);
        result=IFS_FS_IO_ERROR;
        goto done;
    }
    result=probe(host,&observed);
    if (result!=IFS_FS_OK || observed!=variant) result=IFS_FS_IO_ERROR;
done:
    host->release(host->context,header);
    return result;
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
