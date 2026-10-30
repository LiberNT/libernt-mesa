/*
 * Copyright © 2023 Imagination Technologies Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pvr_rt_dataset.h"

#include "pvr_device.h"
#include "pvr_free_list.h"

static void pvr_rt_rgn_headers_data_fini(struct pvr_rt_dataset *rt_dataset)
{
   for (uint32_t i = 0; i < ARRAY_SIZE(rt_dataset->rt_datas); i++)
      rt_dataset->rt_datas[i].rgn_headers_dev_addr = PVR_DEV_ADDR_INVALID;

   pvr_bo_free(rt_dataset->device, rt_dataset->rgn_headers_bo);
   rt_dataset->rgn_headers_bo = NULL;
}

void pvr_rt_mlist_data_fini(struct pvr_rt_dataset *rt_dataset)
{
   for (uint32_t i = 0; i < ARRAY_SIZE(rt_dataset->rt_datas); i++)
      rt_dataset->rt_datas[i].mlist_dev_addr = PVR_DEV_ADDR_INVALID;

   pvr_bo_free(rt_dataset->device, rt_dataset->mlist_bo);
   rt_dataset->mlist_bo = NULL;
}

void pvr_rt_mta_data_fini(struct pvr_rt_dataset *rt_dataset)
{
   if (rt_dataset->mta_bo == NULL)
      return;

   for (uint32_t i = 0; i < ARRAY_SIZE(rt_dataset->rt_datas); i++)
      rt_dataset->rt_datas[i].mta_dev_addr = PVR_DEV_ADDR_INVALID;

   pvr_bo_free(rt_dataset->device, rt_dataset->mta_bo);
   rt_dataset->mta_bo = NULL;
}

void pvr_rt_datas_fini(struct pvr_rt_dataset *rt_dataset)
{
   pvr_rt_rgn_headers_data_fini(rt_dataset);
   pvr_rt_mlist_data_fini(rt_dataset);
   pvr_rt_mta_data_fini(rt_dataset);
}

void pvr_rt_tpc_data_fini(struct pvr_rt_dataset *rt_dataset)
{
   pvr_bo_free(rt_dataset->device, rt_dataset->tpc_bo);
   rt_dataset->tpc_bo = NULL;
}

void pvr_rt_vheap_rtc_data_fini(struct pvr_rt_dataset *rt_dataset)
{
   rt_dataset->rtc_dev_addr = PVR_DEV_ADDR_INVALID;

   pvr_bo_free(rt_dataset->device, rt_dataset->vheap_rtc_bo);
   rt_dataset->vheap_rtc_bo = NULL;
}

#define PVR_RT_DATASET_CACHE_MAX 8

static void pvr_rt_dataset_free(struct pvr_rt_dataset *rt_dataset)
{
   struct pvr_device *device = rt_dataset->device;

   device->ws->ops->render_target_dataset_destroy(rt_dataset->ws_rt_dataset);

   pvr_rt_datas_fini(rt_dataset);
   pvr_rt_tpc_data_fini(rt_dataset);
   pvr_rt_vheap_rtc_data_fini(rt_dataset);

   pvr_free_list_destroy(rt_dataset->local_free_list);

   vk_free(&device->vk.alloc, rt_dataset);
}

/* Dynamic rendering builds a render state per render: keep released datasets for reuse. */
void pvr_render_target_dataset_destroy(struct pvr_rt_dataset *rt_dataset)
{
   struct pvr_device *device = rt_dataset->device;
   struct pvr_rt_dataset *evicted = NULL;

   simple_mtx_lock(&device->rt_dataset_cache_mtx);
   list_add(&rt_dataset->cache_link, &device->rt_dataset_cache);
   if (++device->rt_dataset_cache_count > PVR_RT_DATASET_CACHE_MAX) {
      evicted = list_last_entry(&device->rt_dataset_cache,
                                struct pvr_rt_dataset,
                                cache_link);
      list_del(&evicted->cache_link);
      device->rt_dataset_cache_count--;
   }
   simple_mtx_unlock(&device->rt_dataset_cache_mtx);

   if (evicted)
      pvr_rt_dataset_free(evicted);
}

struct pvr_rt_dataset *pvr_rt_dataset_cache_take(struct pvr_device *device,
                                                 uint32_t width,
                                                 uint32_t height,
                                                 uint32_t samples,
                                                 uint32_t layers)
{
   struct pvr_rt_dataset *found = NULL;

   simple_mtx_lock(&device->rt_dataset_cache_mtx);
   list_for_each_entry (struct pvr_rt_dataset,
                        rt_dataset,
                        &device->rt_dataset_cache,
                        cache_link) {
      if (rt_dataset->width == width && rt_dataset->height == height &&
          rt_dataset->samples == samples && rt_dataset->layers == layers) {
         list_del(&rt_dataset->cache_link);
         device->rt_dataset_cache_count--;
         found = rt_dataset;
         break;
      }
   }
   simple_mtx_unlock(&device->rt_dataset_cache_mtx);

   return found;
}

void pvr_rt_dataset_cache_finish(struct pvr_device *device)
{
   list_for_each_entry_safe (struct pvr_rt_dataset,
                             rt_dataset,
                             &device->rt_dataset_cache,
                             cache_link) {
      list_del(&rt_dataset->cache_link);
      pvr_rt_dataset_free(rt_dataset);
   }
   device->rt_dataset_cache_count = 0;
   simple_mtx_destroy(&device->rt_dataset_cache_mtx);
}
