/*
 * Copyright 2026 Ostap Brehin <osbre@protonmail.com>
 * SPDX-License-Identifier: MIT
 *
 * The batch decoder parses genxml with expat, which the ReactOS SDK build
 * does not have.  INTEL_DEBUG=bat prints nothing in this build.
 */

#include "decoder/intel_decoder.h"

void
intel_batch_decode_ctx_init_elk(struct intel_batch_decode_ctx *ctx,
                                const struct elk_isa_info *isa,
                                const struct intel_device_info *devinfo,
                                FILE *fp, enum intel_batch_decode_flags flags,
                                const char *xml_path,
                                struct intel_batch_decode_bo (*get_bo)(void *, bool, uint64_t),
                                unsigned (*get_state_size)(void *, uint64_t, uint64_t),
                                void *user_data)
{
   memset(ctx, 0, sizeof(*ctx));
}

void
intel_batch_decode_ctx_finish(struct intel_batch_decode_ctx *ctx)
{
}

void
intel_print_batch(struct intel_batch_decode_ctx *ctx, const uint32_t *batch,
                  uint32_t batch_size, uint64_t batch_addr, bool from_ring)
{
}
