/*
 * Copyright 2026 Ostap Brehin <osbre@protonmail.com>
 * SPDX-License-Identifier: MIT
 *
 * The pthread mutex subset intel_measure and intel_aux_map use.
 */

#ifndef INTEL_COMPAT_PTHREAD_H
#define INTEL_COMPAT_PTHREAD_H

#include "c11/threads.h"

typedef mtx_t pthread_mutex_t;

static inline int
pthread_mutex_init(pthread_mutex_t *mutex, const void *attr)
{
   return mtx_init(mutex, mtx_plain) == thrd_success ? 0 : -1;
}

static inline int
pthread_mutex_destroy(pthread_mutex_t *mutex)
{
   mtx_destroy(mutex);
   return 0;
}

static inline int
pthread_mutex_lock(pthread_mutex_t *mutex)
{
   return mtx_lock(mutex) == thrd_success ? 0 : -1;
}

static inline int
pthread_mutex_unlock(pthread_mutex_t *mutex)
{
   return mtx_unlock(mutex) == thrd_success ? 0 : -1;
}

#endif /* INTEL_COMPAT_PTHREAD_H */
