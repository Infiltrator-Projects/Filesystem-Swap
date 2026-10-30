/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ifs_swap_module_v1.h"
#include <assert.h>
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
int main(void) {
    const IfsSwapModuleV1 *m = infiltrator_filesystem_swap_v1();
    assert(m && m->abi_version == 1 && m->structure_bytes >= sizeof(*m));
    assert(strcmp(m->format_id, "swap") == 0);
    assert(m->capabilities == (IFS_SWAP_CAP_RUN | IFS_SWAP_CAP_FORMAT | IFS_SWAP_CAP_VERIFY));
    assert(m->run && m->format && m->verify);
    char path[] = "/tmp/ifs-swap-module.XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    assert(unlink(path) == 0);
    assert(ftruncate(fd, 16 * 4096) == 0);
    IfsSwapIoV1 io = {&fd, 16 * 4096, rd, wr, fl};
    assert(m->verify(&io) != 0);
    assert(m->format(&io, "LD_SWAP") == 0);
    assert(m->verify(&io) == 0);
    uint8_t corrupt = 0;
    assert(pwrite(fd, &corrupt, 1, 4095) == 1);
    assert(m->verify(&io) != 0);
    assert(close(fd) == 0);
    return 0;
}
