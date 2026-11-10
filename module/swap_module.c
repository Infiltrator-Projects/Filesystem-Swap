/* SPDX-License-Identifier: GPL-3.0-or-later
 * SWAP.so: one Common V2 ABI. The media engine and exported provider live
 * together. No historical module entry points or fallback paths.
 */
#include <infiltratr/filesystem_module_v2.h>
#include <infiltratr/endian.h>
#include <stdint.h>
#include <string.h>
#define SWAP_PAGE 4096u
#define SWAP_UUID "LDTM-SWAP-UUID-1"

static int swap_media_valid(const IfsFsHostRuntimeV2 *h)
{
    return ifs_fs_host_v2_valid(h) &&
        h->volume_bytes >= SWAP_PAGE * 2u &&
        h->volume_bytes % SWAP_PAGE == 0u &&
        h->volume_bytes / SWAP_PAGE <= UINT32_MAX;
}
static int swap_probe(const IfsFsHostRuntimeV2 *h, uint32_t *variant)
{
    uint8_t page[SWAP_PAGE];
    if (!variant || !swap_media_valid(h)) return IFS_FS_INVALID;
    if (h->read_at(h->context, 0u, page, sizeof(page)) != 0)
        return IFS_FS_IO_ERROR;
    if (memcmp(page + 4086u, "SWAPSPACE2", 10u) != 0 ||
        infiltratr_load_le32(page + 1024u) != 1u ||
        infiltratr_load_le32(page + 1028u) !=
            h->volume_bytes / SWAP_PAGE - 1u ||
        infiltratr_load_le32(page + 1032u) != 0u)
        return IFS_FS_NOT_FOUND;
    *variant = 1u;
    return IFS_FS_OK;
}
static int swap_format(const IfsFsHostRuntimeV2 *h, uint32_t variant,
                       const char *label, uint32_t flags)
{
    uint8_t page[SWAP_PAGE] = {0};
    size_t label_len;
    if (!swap_media_valid(h)) return IFS_FS_INVALID;
    if (flags) return IFS_FS_UNSUPPORTED;
    if (variant != 1u) return IFS_FS_UNSUPPORTED;
    if (h->capabilities & IFS_FS_HOST_READ_ONLY) return IFS_FS_ACCESS;
    if (!(h->capabilities & IFS_FS_HOST_EXCLUSIVE) ||
        !(h->capabilities & IFS_FS_HOST_DURABLE) ||
        !h->write_at || !h->flush) return IFS_FS_ACCESS;
    if (!label) return IFS_FS_INVALID;
    for (label_len=0u;label_len<=16u && label[label_len];++label_len) {}
    if (label_len > 16u) return IFS_FS_INVALID;
    infiltratr_store_le32(page + 1024u, 1u);
    infiltratr_store_le32(page + 1028u,
        (uint32_t)(h->volume_bytes / SWAP_PAGE - 1u));
    memcpy(page + 1036u, SWAP_UUID, 16u);
    memcpy(page + 1052u, label, label_len);
    memcpy(page + 4086u, "SWAPSPACE2", 10u);
    return h->write_at(h->context,0u,page,sizeof(page))==0 &&
        h->flush(h->context)==0 ? IFS_FS_OK : IFS_FS_IO_ERROR;
}
static const IfsFsCoreOpsV2 core_ops={
    .header={IFS_FS_GROUP_ABI_V2,sizeof(IfsFsCoreOpsV2),
        IFS_FS_GROUP_CORE,0u,IFS_FS_CORE_PROBE},
    .probe=swap_probe
};
static const IfsFsVolumeOpsV2 volume_ops={
    .header={IFS_FS_GROUP_ABI_V2,sizeof(IfsFsVolumeOpsV2),
        IFS_FS_GROUP_VOLUME,0u,IFS_FS_VOLUME_FORMAT},
    .format=swap_format
};
static const IfsFsGroupHeaderV2 *query_group(
    uint32_t id,uint32_t minimum,uint32_t maximum)
{
    if(minimum>IFS_FS_GROUP_ABI_V2 || maximum<IFS_FS_GROUP_ABI_V2)
        return NULL;
    if(id==IFS_FS_GROUP_CORE)return &core_ops.header;
    if(id==IFS_FS_GROUP_VOLUME)return &volume_ops.header;
    return NULL;
}
static const IfsFsModuleApiV2 module={
    .abi_version=IFS_FS_COMMON_ABI_V2,
    .structure_bytes=sizeof(IfsFsModuleApiV2),
    .format_id="swap",
    .runtime_profiles=IFS_FS_PROFILE_HOSTED,
    .group_mask=IFS_FS_GROUP_BIT(IFS_FS_GROUP_CORE)|
                IFS_FS_GROUP_BIT(IFS_FS_GROUP_VOLUME),
    .query_group=query_group
};
__attribute__((visibility("default")))
const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void)
{
    return &module;
}
