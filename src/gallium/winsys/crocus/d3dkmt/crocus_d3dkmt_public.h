/*
 * Copyright 2026 Ostap Brehin <osbre@protonmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef CROCUS_D3DKMT_PUBLIC_H
#define CROCUS_D3DKMT_PUBLIC_H

#include <windows.h>

struct pipe_screen;
struct pipe_context;
struct pipe_resource;

struct pipe_screen *crocus_d3dkmt_screen_create(void);

void crocus_d3dkmt_present(struct pipe_context *context,
                           struct pipe_resource *resource, HDC hdc);

#endif /* CROCUS_D3DKMT_PUBLIC_H */
