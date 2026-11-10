/* SPDX-License-Identifier: GPL-3.0-or-later
 * Disposable image exercises the actual SO with only Common V2.
 */
#include <infiltratr/filesystem_module_v2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) {fprintf(stderr,"failed %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);} } while(0)
extern const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void);
static int rd(void *ctx,uint64_t off,void *data,uint32_t n)
{
    return pread(*(int *)ctx,data,n,(off_t)off)==(ssize_t)n ? 0:-1;
}
static int wr(void *ctx,uint64_t off,const void *data,uint32_t n)
{
    return pwrite(*(int *)ctx,data,n,(off_t)off)==(ssize_t)n ? 0:-1;
}
static int flush(void *ctx) {return fsync(*(int *)ctx);}
static void *alloc(void *ctx,size_t n) {(void)ctx;return malloc(n);}
static void release(void *ctx,void *p) {(void)ctx;free(p);}
int main(void)
{
    const IfsFsModuleApiV2 *module=infiltrator_filesystem_api_v2();
    const IfsFsCoreOpsV2 *core;
    const IfsFsVolumeOpsV2 *volume;
    IfsFsHostRuntimeV2 host={0};
    uint32_t variant=0u;
    char path[]="/tmp/ifs-v2-swap-XXXXXX";
    unsigned char corrupt=0;
    int fd=mkstemp(path);
    CHECK(fd>=0);
    CHECK(unlink(path)==0);
    CHECK(ftruncate(fd,16*4096)==0);
    CHECK(ifs_fs_api_v2_valid(module,"swap"));
    CHECK(module->group_mask==(IFS_FS_GROUP_BIT(IFS_FS_GROUP_CORE)|
                               IFS_FS_GROUP_BIT(IFS_FS_GROUP_VOLUME)));
    core=(const IfsFsCoreOpsV2 *)ifs_fs_group_v2(module,IFS_FS_GROUP_CORE);
    volume=(const IfsFsVolumeOpsV2 *)ifs_fs_group_v2(module,IFS_FS_GROUP_VOLUME);
    CHECK(core && core->header.capabilities==IFS_FS_CORE_PROBE &&
          core->probe && !core->mount && !core->read);
    CHECK(volume && volume->header.capabilities==IFS_FS_VOLUME_FORMAT &&
          volume->format);
    host.abi_version=IFS_FS_HOST_ABI_V2;
    host.structure_bytes=sizeof(host);
    host.capabilities=IFS_FS_HOST_EXCLUSIVE|IFS_FS_HOST_DURABLE;
    host.volume_bytes=16u*4096u;
    host.logical_sector_bytes=512u;
    host.physical_sector_bytes=4096u;
    host.maximum_transfer_bytes=4096u;
    host.context=&fd;
    host.read_at=rd;host.write_at=wr;host.flush=flush;
    host.allocate=alloc;host.release=release;
    CHECK(ifs_fs_host_v2_valid(&host));
    CHECK(core->probe(&host,&variant)==IFS_FS_NOT_FOUND);
    CHECK(volume->format(&host,1u,"LD_SWAP",0u)==IFS_FS_OK);
    CHECK(core->probe(&host,&variant)==IFS_FS_OK && variant==1u);
    CHECK(volume->format(&host,1u,"LD_SWAP",1u)==IFS_FS_UNSUPPORTED);
    host.capabilities=IFS_FS_HOST_READ_ONLY;
    host.write_at=NULL;host.flush=NULL;
    CHECK(ifs_fs_host_v2_valid(&host));
    CHECK(core->probe(&host,&variant)==IFS_FS_OK && variant==1u);
    CHECK(volume->format(&host,1u,"LD_SWAP",0u)==IFS_FS_ACCESS);
    CHECK(pwrite(fd,&corrupt,1,4095)==1);
    CHECK(core->probe(&host,&variant)==IFS_FS_NOT_FOUND);
    CHECK(close(fd)==0);
    return 0;
}
