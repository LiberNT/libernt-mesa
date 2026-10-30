/*
 * Copyright 2026 Ostap Brehin <osbre@protonmail.com>
 * SPDX-License-Identifier: MIT
 *
 * The i915 perf stream is not available over D3DKMT.
 */

#ifndef INTEL_COMPAT_POLL_H
#define INTEL_COMPAT_POLL_H

#include <errno.h>

#define POLLIN 0x0001

struct pollfd {
   int fd;
   short events;
   short revents;
};

static inline int
poll(struct pollfd *fds, unsigned nfds, int timeout)
{
   errno = ENOSYS;
   return -1;
}

#endif /* INTEL_COMPAT_POLL_H */
