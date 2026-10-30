/*
 * Copyright 2026 Ostap Brehin <osbre@protonmail.com>
 * SPDX-License-Identifier: MIT
 *
 * Buffer objects are mapped through the D3DKMT transport, never through mmap.
 */

#ifndef INTEL_COMPAT_SYS_MMAN_H
#define INTEL_COMPAT_SYS_MMAN_H

#include <errno.h>
#include <stddef.h>
#include <sys/types.h>

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_FAILED ((void *)-1)

static inline void *
mmap(void *addr, size_t length, int prot, int flags, int fd, int64_t offset)
{
   errno = ENODEV;
   return MAP_FAILED;
}

static inline int
munmap(void *addr, size_t length)
{
   return 0;
}

#endif /* INTEL_COMPAT_SYS_MMAN_H */
