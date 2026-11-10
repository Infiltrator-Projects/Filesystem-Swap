/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ifs_swap_module_v1.h"
#include <infiltratr/filesystem_module_v2.h>
extern const IfsFsModuleApiV1 *infiltrator_filesystem_api_v1(void);
extern const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void);
#include <stdio.h>
#define CHECK(test) do { if (!(test)) { fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #test); exit(EXIT_FAILURE); } } while (0)
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int rd(void *p, uint64_t at, void *buf, uint32_t n) {
    return pread(*(int *)p, buf, n, (off_t)at) == (ssize_t)n ? 0 : -1;
}
static int wr(void *p, uint64_t at, const void *buf, uint32_t n) {
    return pwrite(*(int *)p, buf, n, (off_t)at) == (ssize_t)n ? 0 : -1;
}
static int fl(void *p) { return fsync(*(int *)p); }
static void *test_alloc(void *ctx, size_t bytes) { (void)ctx; return malloc(bytes); }
static void test_free(void *ctx, void *p) { (void)ctx; free(p); }
int main(void) {
    const IfsSwapModuleV1 *m = infiltrator_filesystem_swap_v1();
    CHECK(m && m->abi_version == 1 && m->structure_bytes >= sizeof(*m));
    CHECK(strcmp(m->format_id, "swap") == 0);
    CHECK(m->capabilities == (IFS_SWAP_CAP_RUN | IFS_SWAP_CAP_FORMAT | IFS_SWAP_CAP_VERIFY));
    CHECK(m->run && m->format && m->verify);
    char path[] = "/tmp/ifs-swap-module.XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0);
    CHECK(unlink(path) == 0);
    CHECK(ftruncate(fd, 16 * 4096) == 0);
    IfsSwapIoV1 io = {&fd, 16 * 4096, rd, wr, fl};
    CHECK(m->verify(&io) != 0);
    CHECK(m->format(&io, "LD_SWAP") == 0);
    CHECK(m->verify(&io) == 0);
    {
        const IfsFsModuleApiV1 *common = infiltrator_filesystem_api_v1();
        const IfsFsModuleApiV2 *v2 = infiltrator_filesystem_api_v2();
        const IfsFsVolumeOpsV1 *group;
        const IfsFsCoreOpsV1 *core;
        IfsFsHostIoV1 ro = {0};
        IfsFsHostRuntimeV2 writable = {0};
        uint32_t found = 0u;
        ro.structure_bytes = sizeof(ro);
        ro.flags = IFS_FS_IO_READ_ONLY;
        ro.context = &fd;
        ro.volume_bytes = 16u * 4096u;
        ro.read_at = rd;
        /* A read-only host has no flush callback, and must still probe. */
        CHECK(ifs_fs_common_api_valid(common, "swap"));
        CHECK(ifs_fs_api_v2_valid(v2, "swap"));
        CHECK(ifs_fs_api_v2_native_valid(v2, "swap"));
        CHECK(v2->base_v1 == NULL);
        CHECK(v2->runtime_profiles == IFS_FS_PROFILE_HOSTED);
        CHECK(common->probe(&ro, &found) == IFS_FS_OK && found == 1u);
        CHECK(!common->mount && !common->create && !common->read);
        core = (const IfsFsCoreOpsV1 *)ifs_fs_group_v2(v2, IFS_FS_GROUP_CORE);
        CHECK(core && core->header.capabilities == IFS_FS_CORE_PROBE);
        CHECK(core->probe && !core->mount && !core->read && !core->write);
        group = (const IfsFsVolumeOpsV1 *)ifs_fs_group_v2(v2, IFS_FS_GROUP_VOLUME);
        CHECK(group && group->header.capabilities == IFS_FS_VOLUME_FORMAT);
        writable.abi_version = IFS_FS_HOST_ABI_V2;
        writable.structure_bytes = sizeof(writable);
        writable.capabilities = IFS_FS_HOST_EXCLUSIVE | IFS_FS_HOST_DURABLE;
        writable.volume_bytes = ro.volume_bytes;
        writable.logical_sector_bytes = 512u;
        writable.physical_sector_bytes = 4096u;
        writable.maximum_transfer_bytes = 4096u;
        writable.context = &fd;
        writable.read_at = rd;
        writable.write_at = wr;
        writable.flush = fl;
        writable.allocate = test_alloc;
        writable.release = test_free;
        CHECK(ifs_fs_host_v2_valid(&writable));
        CHECK(core->probe(&writable, &found) == IFS_FS_OK && found == 1u);
        CHECK(group->format(&writable, 1u, "LD_SWAP", 0u) == IFS_FS_OK);
        CHECK(m->verify(&io) == 0);
        writable.capabilities = IFS_FS_HOST_READ_ONLY;
        writable.write_at = NULL;
        writable.flush = NULL;
        CHECK(ifs_fs_host_v2_valid(&writable));
        CHECK(core->probe(&writable, &found) == IFS_FS_OK && found == 1u);
        CHECK(group->format(&writable, 1u, "LD_SWAP", 0u) == IFS_FS_ACCESS);
    }
    uint8_t corrupt = 0;
    CHECK(pwrite(fd, &corrupt, 1, 4095) == 1);
    CHECK(m->verify(&io) != 0);
    CHECK(close(fd) == 0);
    return 0;
}
