/*
 * Copyright 2026 Ostap Brehin <osbre@protonmail.com>
 * SPDX-License-Identifier: MIT
 *
 * libdrm entry points Crocus uses, routed to the D3DKMT transport.
 */

#ifndef INTEL_COMPAT_XF86DRM_H
#define INTEL_COMPAT_XF86DRM_H

#include "util/libdrm.h"

int intel_virtio_ioctl(int fd, unsigned long request, void *arg);

static inline int
drmIoctl(int fd, unsigned long request, void *arg)
{
   return intel_virtio_ioctl(fd, request, arg);
}

#endif /* INTEL_COMPAT_XF86DRM_H */
