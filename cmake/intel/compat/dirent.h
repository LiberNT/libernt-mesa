/*
 * Copyright 2026 Ostap Brehin <osbre@protonmail.com>
 * SPDX-License-Identifier: MIT
 *
 * The i915 perf code looks up its sysfs directory; there is none on Windows.
 */

#ifndef INTEL_COMPAT_DIRENT_H
#define INTEL_COMPAT_DIRENT_H

#include <errno.h>
#include <stddef.h>

struct dirent {
   char d_name[260];
};

typedef struct intel_compat_dir DIR;

static inline DIR *
opendir(const char *name)
{
   errno = ENOENT;
   return NULL;
}

static inline struct dirent *
readdir(DIR *dir)
{
   return NULL;
}

static inline int
closedir(DIR *dir)
{
   return 0;
}

#endif /* INTEL_COMPAT_DIRENT_H */
