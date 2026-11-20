/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <infiltratr/filesystem_module_v2.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/random.h>
#include <errno.h>
extern const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void);
#define CHECK(x) do { if (!(x)) { fprintf(stderr, \
    "check failed at %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1); } } while(0)
static int rd(void *ctx,uint64_t at,void *p,uint32_t n)
{ return pread(*(int *)ctx,p,n,(off_t)at)==(ssize_t)n ? 0 : -1; }
static int wr(void *ctx,uint64_t at,const void *p,uint32_t n)
{ return pwrite(*(int *)ctx,p,n,(off_t)at)==(ssize_t)n ? 0 : -1; }
static int flush_fd(void *ctx) { return fsync(*(int *)ctx); }
static void *allocate(void *ctx,size_t n) { (void)ctx; return malloc(n); }
static void release(void *ctx,void *p) { (void)ctx; free(p); }
/* Only the hosted test fixture obtains entropy from Linux. The filesystem
 * production module consumes the Common V2 host callback exclusively. */
static int secure_entropy(void *ctx,void *out,uint32_t bytes)
{
    (void)ctx;
    uint8_t *p=out;
    uint32_t done=0u;
    while(done<bytes) {
        ssize_t got=getrandom(p+done,bytes-done,0u);
        if(got<0 && errno==EINTR)continue;
        if(got<=0)return -1;
        done+=(uint32_t)got;
    }
    return 0;
}
static int unavailable_entropy(void *ctx,void *out,uint32_t bytes)
{ (void)ctx;(void)out;(void)bytes;return -1; }
int main(void)
{
    const IfsFsModuleApiV2 *api=infiltrator_filesystem_api_v2();
    const IfsFsCoreOpsV2 *core;
    const IfsFsVolumeOpsV2 *volume;
    IfsFsHostRuntimeV2 host={0};
    char path[]="/tmp/ifs-swap-v2.XXXXXX";
    uint32_t variant=0u;
    uint8_t corrupt=0u;
    uint8_t before[4096]={0},after[4096]={0};
    int fd;
    CHECK(ifs_fs_api_v2_valid(api,"swap"));
    CHECK(api->group_mask==(IFS_FS_GROUP_BIT(IFS_FS_GROUP_CORE) |
                            IFS_FS_GROUP_BIT(IFS_FS_GROUP_VOLUME)));
    core=(const IfsFsCoreOpsV2 *)ifs_fs_group_v2(api,IFS_FS_GROUP_CORE);
    volume=(const IfsFsVolumeOpsV2 *)ifs_fs_group_v2(api,IFS_FS_GROUP_VOLUME);
    CHECK(core && volume && core->probe && volume->format);
    /* This is not a bootable volume: no mount, namespace or file callbacks. */
    CHECK(api->runtime_profiles == IFS_FS_PROFILE_HOSTED);
    CHECK(core->header.capabilities == IFS_FS_CORE_PROBE);
    CHECK(!core->mount && !core->unmount && !core->stat &&
          !core->directory_entry && !core->read && !core->write);
    CHECK(volume->header.capabilities==IFS_FS_VOLUME_FORMAT);
    fd=mkstemp(path); CHECK(fd>=0);
    CHECK(unlink(path)==0); CHECK(ftruncate(fd,16*4096)==0);
    host.abi_version=IFS_FS_HOST_ABI_V2;
    host.structure_bytes=sizeof(host);
    host.capabilities=IFS_FS_HOST_READ_ONLY;
    host.volume_bytes=16u*4096u;
    host.logical_sector_bytes=512u;
    host.physical_sector_bytes=4096u;
    host.maximum_transfer_bytes=4096u;
    host.context=&fd;
    host.read_at=rd;
    host.allocate=allocate;
    host.release=release;
    CHECK(ifs_fs_host_v2_valid(&host));
    CHECK(core->probe(&host,&variant)==IFS_FS_NOT_FOUND);
    CHECK(volume->format(&host,4096u,"LD_SWAP",0u)==IFS_FS_ACCESS);
    host.capabilities=IFS_FS_HOST_EXCLUSIVE|IFS_FS_HOST_DURABLE;
    host.write_at=wr; host.flush=flush_fd;
    CHECK(ifs_fs_host_v2_valid(&host));
    /* Common V2 entropy is mandatory for genuine UUIDs. Both absent and
     * failing callbacks must reject formatting without touching the media. */
    CHECK(pread(fd,before,sizeof(before),0)==(ssize_t)sizeof(before));
    CHECK(volume->format(&host,4096u,"LD_SWAP",0u)==IFS_FS_UNSUPPORTED);
    CHECK(pread(fd,after,sizeof(after),0)==(ssize_t)sizeof(after));
    CHECK(memcmp(before,after,sizeof(before))==0);
    host.random_bytes=unavailable_entropy;
    CHECK(volume->format(&host,4096u,"LD_SWAP",0u)==IFS_FS_IO_ERROR);
    CHECK(pread(fd,after,sizeof(after),0)==(ssize_t)sizeof(after));
    CHECK(memcmp(before,after,sizeof(before))==0);
    host.random_bytes=secure_entropy;
    CHECK(volume->format(&host,4096u,"LD_SWAP",0u)==IFS_FS_OK);
    CHECK(core->probe(&host,&variant)==IFS_FS_OK && variant==4096u);
    uint8_t uuid1[16]={0},uuid2[16]={0};
    CHECK(pread(fd,uuid1,sizeof(uuid1),1036)==(ssize_t)sizeof(uuid1));
    CHECK((uuid1[6]&0xf0u)==0x40u && (uuid1[8]&0xc0u)==0x80u);
    CHECK(volume->format(&host,4096u,"SECOND_SWAP",0u)==IFS_FS_OK);
    CHECK(pread(fd,uuid2,sizeof(uuid2),1036)==(ssize_t)sizeof(uuid2));
    CHECK((uuid2[6]&0xf0u)==0x40u && (uuid2[8]&0xc0u)==0x80u);
    CHECK(memcmp(uuid1,uuid2,sizeof(uuid1))!=0);
    CHECK(core->probe(&host,&variant)==IFS_FS_OK && variant==4096u);
    CHECK(volume->format(&host,4096u,"LD_SWAP",1u)==IFS_FS_UNSUPPORTED);
    CHECK(volume->format(&host,8192u,"LD_SWAP",0u)==IFS_FS_UNSUPPORTED);
    /* A committed SWAPSPACE2 header cannot list the same bad page twice.
     * Neither duplicate nor out-of-range metadata may be accepted. */
    {
        const uint8_t two[4]={2u,0u,0u,0u};
        const uint8_t zero[4]={0u,0u,0u,0u};
        const uint8_t bad[4]={16u,0u,0u,0u};
        CHECK(pwrite(fd,two,4u,1032)==4);
        CHECK(pwrite(fd,two,4u,1536)==4);
        CHECK(pwrite(fd,two,4u,1540)==4);
        CHECK(core->probe(&host,&variant)==IFS_FS_CORRUPT);
        CHECK(pwrite(fd,bad,4u,1540)==4);
        CHECK(core->probe(&host,&variant)==IFS_FS_CORRUPT);
        CHECK(pwrite(fd,zero,4u,1032)==4);
        CHECK(core->probe(&host,&variant)==IFS_FS_OK && variant==4096u);
    }
    CHECK(pwrite(fd,&corrupt,1u,4095)==1);
    CHECK(core->probe(&host,&variant)!=IFS_FS_OK);
    CHECK(close(fd)==0);
    return 0;
}
