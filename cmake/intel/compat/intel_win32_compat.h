/*
 * Copyright 2026 Ostap Brehin <osbre@protonmail.com>
 * SPDX-License-Identifier: MIT
 *
 * POSIX pieces the Intel libraries and Crocus use, for the Windows build.
 */

#ifndef INTEL_WIN32_COMPAT_H
#define INTEL_WIN32_COMPAT_H

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "util/os_time.h"
#include "util/u_string.h"

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#endif

#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISCHR
#define S_ISCHR(m) (((m) & _S_IFMT) == _S_IFCHR)
#endif
#define S_ISLNK(m) 0
#define S_IRUSR _S_IREAD
#define S_IWUSR _S_IWRITE
#define major(dev) ((unsigned)(dev) >> 8)
#define minor(dev) ((unsigned)(dev) & 0xff)
#define lstat stat
#define AT_FDCWD (-100)

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0
#endif
#define F_GETFL 3
#define F_SETFL 4

#ifdef __cplusplus
extern "C" {
#endif

typedef int clockid_t;
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_MONOTONIC_RAW 4
#define CLOCK_BOOTTIME 7

static inline int
clock_gettime(clockid_t clock, struct timespec *ts)
{
   if (clock == CLOCK_REALTIME)
      return timespec_get(ts, TIME_UTC) == TIME_UTC ? 0 : -1;

   int64_t ns = os_time_get_nano();
   ts->tv_sec = (time_t)(ns / 1000000000);
   ts->tv_nsec = (long)(ns % 1000000000);
   return 0;
}

static inline int
fcntl(int fd, int cmd, ...)
{
   errno = ENOSYS;
   return -1;
}

static inline int
openat(int dirfd, const char *path, int flags, ...)
{
   errno = ENOSYS;
   return -1;
}

static inline int
mkfifoat(int dirfd, const char *path, int mode)
{
   errno = ENOSYS;
   return -1;
}

static inline int
usleep(unsigned usec)
{
   os_time_sleep(usec);
   return 0;
}

static inline char *
strndup(const char *s, size_t n)
{
   size_t len = strnlen(s, n);
   char *copy = (char *)malloc(len + 1);
   if (copy) {
      memcpy(copy, s, len);
      copy[len] = '\0';
   }
   return copy;
}

static inline int
getpagesize(void)
{
   return 4096;
}

static inline unsigned
geteuid(void)
{
   return 1;
}

static inline void *
memmem(const void *haystack, size_t haystack_len, const void *needle, size_t needle_len)
{
   if (needle_len == 0)
      return (void *)haystack;
   for (const char *p = (const char *)haystack;
        haystack_len >= needle_len; p++, haystack_len--) {
      if (memcmp(p, needle, needle_len) == 0)
         return (void *)p;
   }
   return NULL;
}

static inline int
posix_memalign(void **ptr, size_t alignment, size_t size)
{
   /* The CRT malloc is the only allocator free() accepts; it aligns to 2 pointers. */
   if (alignment > 2 * sizeof(void *))
      return EINVAL;
   *ptr = malloc(size);
   return *ptr ? 0 : ENOMEM;
}

#ifdef __cplusplus
}
#endif

#endif /* INTEL_WIN32_COMPAT_H */
