/*
 * Copyright 2026 Ostap Brehin <osbre@protonmail.com>
 * SPDX-License-Identifier: MIT
 *
 * Crocus winsys for the LiberNT hdgfx WDDM driver.  Crocus speaks the i915
 * uAPI; this file implements that uAPI in user mode on top of D3DKMT, through
 * the intel_virtio_* hooks Mesa already uses for userspace i915 transports.
 * Batches go to the hdgfx Gen context (hdgfx_umd.h): the batch and the state
 * buffer are copied into the command buffer and relocations become patch
 * locations.
 */

#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windows.h>
#undef WIN32_NO_STATUS
#include <d3dkmthk.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(status) ((NTSTATUS)(status) >= 0)
#endif

#include "drivers/directx/hdgfx_umd.h"

#include "drm-uapi/drm.h"
#include "drm-uapi/i915_drm.h"
#include "common/intel_gem.h"
#include "dev/intel_device_info.h"
#include "dev/virtio/intel_virtio.h"
#include "pipe/p_context.h"
#include "pipe/p_screen.h"
#include "pipe/p_state.h"
#include "util/driconf.h"
#include "util/log.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"
#include "util/xmlconfig.h"
#include "c11/threads.h"
#include "crocus/crocus_screen.h"
#include "crocus_d3dkmt_public.h"

#define CD3D_MAX_DEVICES 4
#define CD3D_MAX_FDS 16
#define CD3D_BATCH_OFFSET 64u
#define CD3D_STATE_ALIGN 4096u
#define CD3D_APERTURE_SIZE (256ull * 1024 * 1024)
/* Pre-Haswell vertex buffer end addresses point past the buffer. */
#define CD3D_BO_TAIL 4096u

typedef NTSTATUS (NTAPI *PFN_NT_COMPARE_OBJECTS)(HANDLE, HANDLE);

struct cd3d_bo {
   D3DKMT_HANDLE allocation;
   uint32_t size;
   void *map;
   uint32_t tiling_mode;
   uint32_t stride;
   uint64_t fence;
};

struct cd3d_syncobj {
   bool used;
   bool submitted;
   uint64_t fence;
};

struct cd3d_device {
   int refcount;
   HANDLE identity;
   mtx_t lock;
   D3DKMT_HANDLE adapter;
   D3DKMT_HANDLE device;
   D3DKMT_HANDLE context;
   D3DKMT_HANDLE fence_object;
   volatile const uint64_t *fence_cpu;
   uint64_t fence_last;
   uint8_t *command_buffer;
   uint32_t command_buffer_size;
   D3DDDI_ALLOCATIONLIST *allocation_list;
   uint32_t allocation_list_size;
   D3DDDI_PATCHLOCATIONLIST *patch_list;
   uint32_t patch_list_size;
   struct cd3d_bo *bos;
   uint32_t bo_capacity;
   struct cd3d_syncobj *syncobjs;
   uint32_t syncobj_capacity;
   uint32_t next_context_id;
   uint16_t pci_device_id;
   uint8_t pci_revision_id;
   D3DKMT_ADAPTERADDRESS address;
};

struct cd3d_fd {
   int fd;
   struct cd3d_device *dev;
};

static mtx_t cd3d_registry_lock;
static once_flag cd3d_registry_once = ONCE_FLAG_INIT;
static struct cd3d_device *cd3d_devices[CD3D_MAX_DEVICES];
static struct cd3d_fd cd3d_fds[CD3D_MAX_FDS];
static PFN_NT_COMPARE_OBJECTS cd3d_compare_objects;

static void
cd3d_registry_init(void)
{
   mtx_init(&cd3d_registry_lock, mtx_plain);
   HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
   if (ntdll)
      cd3d_compare_objects =
         (PFN_NT_COMPARE_OBJECTS)GetProcAddress(ntdll, "NtCompareObjects");
}

static bool
cd3d_same_object(HANDLE a, HANDLE b)
{
   if (a == b)
      return true;
   return cd3d_compare_objects && cd3d_compare_objects(a, b) == STATUS_SUCCESS;
}

static struct cd3d_device *
cd3d_lookup_locked(int fd)
{
   for (unsigned i = 0; i < CD3D_MAX_FDS; i++) {
      if (cd3d_fds[i].dev && cd3d_fds[i].fd == fd)
         return cd3d_fds[i].dev;
   }

   HANDLE handle = (HANDLE)_get_osfhandle(fd);
   if (handle == INVALID_HANDLE_VALUE)
      return NULL;

   for (unsigned d = 0; d < CD3D_MAX_DEVICES; d++) {
      struct cd3d_device *dev = cd3d_devices[d];
      if (!dev || !cd3d_same_object(dev->identity, handle))
         continue;
      for (unsigned i = 0; i < CD3D_MAX_FDS; i++) {
         if (!cd3d_fds[i].dev) {
            cd3d_fds[i].fd = fd;
            cd3d_fds[i].dev = dev;
            break;
         }
      }
      return dev;
   }
   return NULL;
}

static struct cd3d_device *
cd3d_lookup(int fd)
{
   call_once(&cd3d_registry_once, cd3d_registry_init);
   mtx_lock(&cd3d_registry_lock);
   struct cd3d_device *dev = cd3d_lookup_locked(fd);
   mtx_unlock(&cd3d_registry_lock);
   return dev;
}

static int
cd3d_errno(NTSTATUS status)
{
   switch (status) {
   case STATUS_NO_MEMORY:
   case STATUS_INSUFFICIENT_RESOURCES:
   case STATUS_GRAPHICS_NO_VIDEO_MEMORY:
      return ENOMEM;
   case STATUS_DEVICE_REMOVED:
      return EIO;
   case STATUS_TIMEOUT:
      return ETIME;
   default:
      return EINVAL;
   }
}

static struct cd3d_bo *
cd3d_bo_get(struct cd3d_device *dev, uint32_t handle)
{
   if (handle == 0 || handle > dev->bo_capacity ||
       dev->bos[handle - 1].allocation == 0)
      return NULL;
   return &dev->bos[handle - 1];
}

static uint32_t
cd3d_bo_slot(struct cd3d_device *dev)
{
   for (uint32_t i = 0; i < dev->bo_capacity; i++) {
      if (dev->bos[i].allocation == 0)
         return i + 1;
   }
   uint32_t capacity = MAX2(dev->bo_capacity * 2, 256);
   struct cd3d_bo *bos = realloc(dev->bos, capacity * sizeof(*bos));
   if (!bos)
      return 0;
   memset(bos + dev->bo_capacity, 0,
          (capacity - dev->bo_capacity) * sizeof(*bos));
   uint32_t handle = dev->bo_capacity + 1;
   dev->bos = bos;
   dev->bo_capacity = capacity;
   return handle;
}

static struct cd3d_syncobj *
cd3d_syncobj_get(struct cd3d_device *dev, uint32_t handle)
{
   if (handle == 0 || handle > dev->syncobj_capacity ||
       !dev->syncobjs[handle - 1].used)
      return NULL;
   return &dev->syncobjs[handle - 1];
}

static uint64_t
cd3d_fence_completed(struct cd3d_device *dev)
{
   return p_atomic_read(dev->fence_cpu);
}

static uint64_t
cd3d_now_ns(void)
{
   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   return (uint64_t)now.tv_sec * 1000000000ull + now.tv_nsec;
}

static int
cd3d_wait_fence(struct cd3d_device *dev, uint64_t value, int64_t timeout_ns)
{
   if (value == 0 || cd3d_fence_completed(dev) >= value)
      return 0;
   if (timeout_ns == 0)
      return ETIME;

   D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU wait = { 0 };
   HANDLE event = NULL;
   wait.hDevice = dev->device;
   wait.ObjectCount = 1;
   wait.ObjectHandleArray = &dev->fence_object;
   wait.FenceValueArray = &value;
   if (timeout_ns > 0) {
      event = CreateEventW(NULL, TRUE, FALSE, NULL);
      if (!event)
         return ENOMEM;
      wait.hAsyncEvent = event;
   }

   NTSTATUS status = D3DKMTWaitForSynchronizationObjectFromCpu(&wait);
   int ret = 0;
   if (!NT_SUCCESS(status)) {
      ret = cd3d_errno(status);
   } else if (event) {
      uint64_t ms = (uint64_t)timeout_ns / 1000000;
      DWORD result = WaitForSingleObject(event, ms >= INFINITE ? INFINITE - 1 : (DWORD)MAX2(ms, 1));
      if (result != WAIT_OBJECT_0 && cd3d_fence_completed(dev) < value)
         ret = ETIME;
   }
   if (event)
      CloseHandle(event);
   return ret;
}

static void
cd3d_device_destroy(struct cd3d_device *dev)
{
   for (uint32_t i = 0; i < dev->bo_capacity; i++) {
      struct cd3d_bo *bo = &dev->bos[i];
      if (bo->allocation == 0)
         continue;
      D3DKMT_DESTROYALLOCATION destroy = { 0 };
      destroy.hDevice = dev->device;
      destroy.AllocationCount = 1;
      destroy.phAllocationList = &bo->allocation;
      D3DKMTDestroyAllocation(&destroy);
   }
   if (dev->fence_object) {
      D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroy = { dev->fence_object };
      D3DKMTDestroySynchronizationObject(&destroy);
   }
   if (dev->context) {
      D3DKMT_DESTROYCONTEXT destroy = { dev->context };
      D3DKMTDestroyContext(&destroy);
   }
   if (dev->device) {
      D3DKMT_DESTROYDEVICE destroy = { dev->device };
      D3DKMTDestroyDevice(&destroy);
   }
   if (dev->adapter) {
      D3DKMT_CLOSEADAPTER close = { dev->adapter };
      D3DKMTCloseAdapter(&close);
   }
   if (dev->identity)
      CloseHandle(dev->identity);
   free(dev->bos);
   free(dev->syncobjs);
   mtx_destroy(&dev->lock);
   free(dev);
}

static bool
cd3d_device_open(struct cd3d_device *dev)
{
   D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open = { 0 };
   wcscpy(open.DeviceName, L"\\\\.\\DISPLAY1");
   NTSTATUS status = D3DKMTOpenAdapterFromGdiDisplayName(&open);
   if (!NT_SUCCESS(status)) {
      mesa_loge("crocus: D3DKMTOpenAdapterFromGdiDisplayName failed: 0x%08lx", status);
      return false;
   }
   dev->adapter = open.hAdapter;

   D3DKMT_QUERY_DEVICE_IDS ids = { 0 };
   D3DKMT_QUERYADAPTERINFO query = { 0 };
   query.hAdapter = dev->adapter;
   query.Type = KMTQAITYPE_PHYSICALADAPTERDEVICEIDS;
   query.pPrivateDriverData = &ids;
   query.PrivateDriverDataSize = sizeof(ids);
   status = D3DKMTQueryAdapterInfo(&query);
   if (!NT_SUCCESS(status) || ids.DeviceIds.VendorID != 0x8086) {
      mesa_loge("crocus: adapter device IDs: 0x%08lx, vendor 0x%04x",
                status, ids.DeviceIds.VendorID);
      return false;
   }
   dev->pci_device_id = ids.DeviceIds.DeviceID;
   dev->pci_revision_id = ids.DeviceIds.RevisionID;

   query.Type = KMTQAITYPE_ADAPTERADDRESS;
   query.pPrivateDriverData = &dev->address;
   query.PrivateDriverDataSize = sizeof(dev->address);
   D3DKMTQueryAdapterInfo(&query);

   D3DKMT_CREATEDEVICE create_device = { 0 };
   create_device.hAdapter = dev->adapter;
   status = D3DKMTCreateDevice(&create_device);
   if (!NT_SUCCESS(status)) {
      mesa_loge("crocus: D3DKMTCreateDevice failed: 0x%08lx", status);
      return false;
   }
   dev->device = create_device.hDevice;

   HDGFX_GEN_CONTEXT_DATA data = { HDGFX_GEN_CONTEXT_MAGIC, HDGFX_GEN_CONTEXT_VERSION };
   D3DKMT_CREATECONTEXT create_context = { 0 };
   create_context.hDevice = dev->device;
   create_context.pPrivateDriverData = &data;
   create_context.PrivateDriverDataSize = sizeof(data);
   create_context.ClientHint = D3DKMT_CLIENTHINT_OPENGL;
   status = D3DKMTCreateContext(&create_context);
   if (!NT_SUCCESS(status) ||
       create_context.CommandBufferSize < HDGFX_GEN_COMMAND_BUFFER_SIZE ||
       create_context.AllocationListSize < HDGFX_GEN_ALLOCATION_LIST_SIZE ||
       create_context.PatchLocationListSize < HDGFX_GEN_PATCH_LIST_SIZE) {
      mesa_loge("crocus: D3DKMTCreateContext: 0x%08lx, buffers %u/%u/%u",
                status, create_context.CommandBufferSize,
                create_context.AllocationListSize,
                create_context.PatchLocationListSize);
      if (create_context.hContext)
         dev->context = create_context.hContext;
      return false;
   }
   dev->context = create_context.hContext;
   dev->command_buffer = create_context.pCommandBuffer;
   dev->command_buffer_size = create_context.CommandBufferSize;
   dev->allocation_list = create_context.pAllocationList;
   dev->allocation_list_size = create_context.AllocationListSize;
   dev->patch_list = create_context.pPatchLocationList;
   dev->patch_list_size = create_context.PatchLocationListSize;

   D3DKMT_CREATESYNCHRONIZATIONOBJECT2 sync = { 0 };
   sync.hDevice = dev->device;
   sync.Info.Type = D3DDDI_MONITORED_FENCE;
   sync.Info.MonitoredFence.InitialFenceValue = 0;
   status = D3DKMTCreateSynchronizationObject2(&sync);
   if (!NT_SUCCESS(status)) {
      mesa_loge("crocus: monitored fence creation failed: 0x%08lx", status);
      return false;
   }
   dev->fence_object = sync.hSyncObject;
   dev->fence_cpu = sync.Info.MonitoredFence.FenceValueCPUVirtualAddress;
   return dev->fence_cpu != NULL;
}

static int
cd3d_create_fd(void)
{
   call_once(&cd3d_registry_once, cd3d_registry_init);

   struct cd3d_device *dev = calloc(1, sizeof(*dev));
   if (!dev)
      return -1;
   mtx_init(&dev->lock, mtx_plain);
   dev->refcount = 1;
   dev->next_context_id = 1;
   if (!cd3d_device_open(dev)) {
      cd3d_device_destroy(dev);
      return -1;
   }

   int fd = _open("NUL", _O_RDWR | _O_BINARY);
   if (fd < 0) {
      mesa_loge("crocus: cannot open NUL: %d", errno);
      cd3d_device_destroy(dev);
      return -1;
   }
   if (!DuplicateHandle(GetCurrentProcess(), (HANDLE)_get_osfhandle(fd),
                        GetCurrentProcess(), &dev->identity, 0, FALSE,
                        DUPLICATE_SAME_ACCESS)) {
      mesa_loge("crocus: DuplicateHandle failed: %lu", GetLastError());
      _close(fd);
      cd3d_device_destroy(dev);
      return -1;
   }

   mtx_lock(&cd3d_registry_lock);
   unsigned slot = CD3D_MAX_DEVICES;
   for (unsigned i = 0; i < CD3D_MAX_DEVICES; i++) {
      if (!cd3d_devices[i]) {
         slot = i;
         break;
      }
   }
   if (slot == CD3D_MAX_DEVICES) {
      mtx_unlock(&cd3d_registry_lock);
      _close(fd);
      cd3d_device_destroy(dev);
      return -1;
   }
   cd3d_devices[slot] = dev;
   cd3d_lookup_locked(fd);
   mtx_unlock(&cd3d_registry_lock);
   return fd;
}

int
intel_virtio_init_fd(int fd)
{
   return cd3d_lookup(fd) ? 1 : 0;
}

bool
is_intel_virtio_fd(int fd)
{
   return cd3d_lookup(fd) != NULL;
}

void
intel_virtio_ref_fd(int fd)
{
   struct cd3d_device *dev = cd3d_lookup(fd);
   if (dev)
      p_atomic_inc(&dev->refcount);
}

void
intel_virtio_unref_fd(int fd)
{
   call_once(&cd3d_registry_once, cd3d_registry_init);
   mtx_lock(&cd3d_registry_lock);
   struct cd3d_device *dev = cd3d_lookup_locked(fd);
   for (unsigned i = 0; i < CD3D_MAX_FDS; i++) {
      if (cd3d_fds[i].dev && cd3d_fds[i].fd == fd)
         cd3d_fds[i].dev = NULL;
   }
   if (dev && p_atomic_dec_zero(&dev->refcount)) {
      for (unsigned i = 0; i < CD3D_MAX_FDS; i++) {
         if (cd3d_fds[i].dev == dev)
            cd3d_fds[i].dev = NULL;
      }
      for (unsigned i = 0; i < CD3D_MAX_DEVICES; i++) {
         if (cd3d_devices[i] == dev)
            cd3d_devices[i] = NULL;
      }
   } else {
      dev = NULL;
   }
   mtx_unlock(&cd3d_registry_lock);
   if (dev)
      cd3d_device_destroy(dev);
}

bool
intel_virtio_get_pci_device_info(int fd, struct intel_device_info *devinfo)
{
   struct cd3d_device *dev = cd3d_lookup(fd);
   if (!dev)
      return false;
   devinfo->pci_domain = 0;
   devinfo->pci_bus = dev->address.BusNumber;
   devinfo->pci_dev = dev->address.DeviceNumber;
   devinfo->pci_func = dev->address.FunctionNumber;
   devinfo->pci_device_id = dev->pci_device_id;
   devinfo->pci_revision_id = dev->pci_revision_id;
   return true;
}

void *
intel_virtio_bo_mmap(int fd, uint32_t handle, size_t size, void *placed_addr)
{
   return MAP_FAILED;
}

struct util_sync_provider *
intel_virtio_sync_provider(int fd)
{
   return NULL;
}

static int
cd3d_getparam(struct cd3d_device *dev, struct drm_i915_getparam *gp)
{
   int value;

   switch (gp->param) {
   case I915_PARAM_CHIPSET_ID:
      value = dev->pci_device_id;
      break;
   case I915_PARAM_REVISION:
      value = dev->pci_revision_id;
      break;
   case I915_PARAM_HAS_EXEC_NO_RELOC:
   case I915_PARAM_HAS_EXEC_HANDLE_LUT:
   case I915_PARAM_HAS_EXEC_BATCH_FIRST:
   case I915_PARAM_HAS_EXEC_FENCE_ARRAY:
   case I915_PARAM_HAS_WAIT_TIMEOUT:
      value = 1;
      break;
   case I915_PARAM_HAS_EXEC_SOFTPIN:
   case I915_PARAM_HAS_ALIASING_PPGTT:
   case I915_PARAM_HAS_CONTEXT_ISOLATION:
   case I915_PARAM_MMAP_GTT_VERSION:
      value = 0;
      break;
   default:
      return EINVAL;
   }
   *gp->value = value;
   return 0;
}

static int
cd3d_gem_create(struct cd3d_device *dev, struct drm_i915_gem_create *create)
{
   if (create->size == 0 || create->size > UINT32_MAX - CD3D_BO_TAIL)
      return EINVAL;

   uint32_t handle = cd3d_bo_slot(dev);
   if (!handle)
      return ENOMEM;

   UINT size = (UINT)create->size + CD3D_BO_TAIL;
   D3DDDI_ALLOCATIONINFO info = { 0 };
   info.pPrivateDriverData = &size;
   info.PrivateDriverDataSize = sizeof(size);
   D3DKMT_CREATEALLOCATION alloc = { 0 };
   alloc.hDevice = dev->device;
   alloc.NumAllocations = 1;
   alloc.pAllocationInfo = &info;
   NTSTATUS status = D3DKMTCreateAllocation(&alloc);
   if (!NT_SUCCESS(status)) {
      mesa_loge("crocus: D3DKMTCreateAllocation(%u) failed: 0x%08lx", size, status);
      return cd3d_errno(status);
   }

   struct cd3d_bo *bo = &dev->bos[handle - 1];
   memset(bo, 0, sizeof(*bo));
   bo->allocation = info.hAllocation;
   bo->size = (uint32_t)create->size;
   create->handle = handle;
   return 0;
}

static int
cd3d_gem_close(struct cd3d_device *dev, struct drm_gem_close *close)
{
   struct cd3d_bo *bo = cd3d_bo_get(dev, close->handle);
   if (!bo)
      return EINVAL;

   if (bo->map) {
      D3DKMT_UNLOCK2 unlock = { 0 };
      unlock.hDevice = dev->device;
      unlock.hAllocation = bo->allocation;
      D3DKMTUnlock2(&unlock);
   }
   D3DKMT_DESTROYALLOCATION destroy = { 0 };
   destroy.hDevice = dev->device;
   destroy.AllocationCount = 1;
   destroy.phAllocationList = &bo->allocation;
   D3DKMTDestroyAllocation(&destroy);
   memset(bo, 0, sizeof(*bo));
   return 0;
}

static void *
cd3d_bo_map(struct cd3d_device *dev, struct cd3d_bo *bo)
{
   if (bo->map)
      return bo->map;

   D3DKMT_LOCK2 lock = { 0 };
   lock.hDevice = dev->device;
   lock.hAllocation = bo->allocation;
   NTSTATUS status = D3DKMTLock2(&lock);
   if (!NT_SUCCESS(status)) {
      mesa_loge("crocus: D3DKMTLock2 failed: 0x%08lx", status);
      return NULL;
   }
   bo->map = lock.pData;
   return bo->map;
}

static int
cd3d_gem_mmap(struct cd3d_device *dev, struct drm_i915_gem_mmap *mmap)
{
   struct cd3d_bo *bo = cd3d_bo_get(dev, mmap->handle);
   if (!bo || mmap->offset != 0 || mmap->size > bo->size)
      return EINVAL;
   void *map = cd3d_bo_map(dev, bo);
   if (!map)
      return ENOMEM;
   mmap->addr_ptr = (uintptr_t)map;
   return 0;
}

static int
cd3d_gem_set_tiling(struct cd3d_device *dev, struct drm_i915_gem_set_tiling *tiling)
{
   struct cd3d_bo *bo = cd3d_bo_get(dev, tiling->handle);
   if (!bo || tiling->tiling_mode > I915_TILING_Y)
      return EINVAL;
   bo->tiling_mode = tiling->tiling_mode;
   bo->stride = tiling->tiling_mode == I915_TILING_NONE ? 0 : tiling->stride;
   tiling->stride = bo->stride;
   return 0;
}

static uint32_t
cd3d_swizzle(const struct cd3d_device *dev, uint32_t tiling_mode)
{
   struct intel_device_info devinfo;

   if (tiling_mode == I915_TILING_NONE ||
       !intel_get_device_info_from_pci_id(dev->pci_device_id, &devinfo) ||
       devinfo.ver != 5)
      return I915_BIT_6_SWIZZLE_NONE;

   /* Ironlake swizzles address bit 6 the same way for every DRAM layout. */
   return tiling_mode == I915_TILING_X ? I915_BIT_6_SWIZZLE_9_10 : I915_BIT_6_SWIZZLE_9;
}

static int
cd3d_gem_get_tiling(struct cd3d_device *dev, struct drm_i915_gem_get_tiling *tiling)
{
   struct cd3d_bo *bo = cd3d_bo_get(dev, tiling->handle);
   if (!bo)
      return EINVAL;
   tiling->tiling_mode = bo->tiling_mode;
   tiling->swizzle_mode = cd3d_swizzle(dev, bo->tiling_mode);
   tiling->phys_swizzle_mode = tiling->swizzle_mode;
   return 0;
}

static int
cd3d_gem_busy(struct cd3d_device *dev, struct drm_i915_gem_busy *busy)
{
   struct cd3d_bo *bo = cd3d_bo_get(dev, busy->handle);
   if (!bo)
      return ENOENT;
   busy->busy = cd3d_fence_completed(dev) < bo->fence;
   return 0;
}

static int
cd3d_gem_wait(struct cd3d_device *dev, struct drm_i915_gem_wait *wait)
{
   struct cd3d_bo *bo = cd3d_bo_get(dev, wait->bo_handle);
   if (!bo)
      return ENOENT;
   uint64_t fence = bo->fence;
   mtx_unlock(&dev->lock);
   int ret = cd3d_wait_fence(dev, fence, wait->timeout_ns);
   mtx_lock(&dev->lock);
   return ret;
}

static int
cd3d_syncobj_create(struct cd3d_device *dev, struct drm_syncobj_create *create)
{
   uint32_t i;
   for (i = 0; i < dev->syncobj_capacity; i++) {
      if (!dev->syncobjs[i].used)
         break;
   }
   if (i == dev->syncobj_capacity) {
      uint32_t capacity = MAX2(dev->syncobj_capacity * 2, 64);
      struct cd3d_syncobj *objs = realloc(dev->syncobjs, capacity * sizeof(*objs));
      if (!objs)
         return ENOMEM;
      memset(objs + dev->syncobj_capacity, 0,
             (capacity - dev->syncobj_capacity) * sizeof(*objs));
      dev->syncobjs = objs;
      dev->syncobj_capacity = capacity;
   }
   dev->syncobjs[i].used = true;
   dev->syncobjs[i].submitted = (create->flags & DRM_SYNCOBJ_CREATE_SIGNALED) != 0;
   dev->syncobjs[i].fence = 0;
   create->handle = i + 1;
   return 0;
}

static int
cd3d_syncobj_destroy(struct cd3d_device *dev, struct drm_syncobj_destroy *destroy)
{
   struct cd3d_syncobj *obj = cd3d_syncobj_get(dev, destroy->handle);
   if (!obj)
      return EINVAL;
   memset(obj, 0, sizeof(*obj));
   return 0;
}

static int
cd3d_syncobj_wait(struct cd3d_device *dev, struct drm_syncobj_wait *wait)
{
   const uint32_t *handles = (const uint32_t *)(uintptr_t)wait->handles;
   bool wait_all = (wait->flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL) != 0;
   uint64_t completed = cd3d_fence_completed(dev);
   uint64_t target = 0;
   bool any_signaled = false;

   if (wait->count_handles == 0)
      return EINVAL;
   for (uint32_t i = 0; i < wait->count_handles; i++) {
      struct cd3d_syncobj *obj = cd3d_syncobj_get(dev, handles[i]);
      if (!obj)
         return EINVAL;
      if (!obj->submitted)
         return (wait->flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT) ? ETIME : EINVAL;
      if (obj->fence <= completed) {
         any_signaled = true;
         wait->first_signaled = i;
      }
      if (wait_all)
         target = MAX2(target, obj->fence);
      else if (target == 0 || obj->fence < target)
         target = obj->fence;
   }
   if (!wait_all && any_signaled)
      return 0;

   int64_t remaining = 0;
   if (wait->timeout_nsec > 0) {
      uint64_t now = cd3d_now_ns();
      remaining = wait->timeout_nsec == INT64_MAX ? -1 :
                  (uint64_t)wait->timeout_nsec > now ? (int64_t)(wait->timeout_nsec - now) : 0;
   }
   mtx_unlock(&dev->lock);
   int ret = cd3d_wait_fence(dev, target, remaining);
   mtx_lock(&dev->lock);
   return ret;
}

static int
cd3d_add_patch(struct cd3d_device *dev, uint32_t *patch_count,
               uint32_t patch_offset, uint32_t allocation_index,
               uint32_t allocation_offset, uint32_t driver_id)
{
   if (*patch_count >= dev->patch_list_size)
      return ENOSPC;
   D3DDDI_PATCHLOCATIONLIST *patch = &dev->patch_list[(*patch_count)++];
   memset(patch, 0, sizeof(*patch));
   patch->AllocationIndex = allocation_index;
   patch->DriverId = driver_id;
   patch->AllocationOffset = allocation_offset;
   patch->PatchOffset = patch_offset;
   return 0;
}

static int
cd3d_execbuffer2(struct cd3d_device *dev, struct drm_i915_gem_execbuffer2 *exec)
{
   struct drm_i915_gem_exec_object2 *objects =
      (struct drm_i915_gem_exec_object2 *)(uintptr_t)exec->buffers_ptr;
   uint32_t count = exec->buffer_count;
   uint32_t allocation_count = 0;
   uint32_t patch_count = 0;
   int ret;

   if (count == 0 || !(exec->flags & I915_EXEC_HANDLE_LUT) ||
       (exec->flags & I915_EXEC_RING_MASK) > I915_EXEC_RENDER ||
       (exec->batch_len & 7) || exec->batch_start_offset & 7)
      return EINVAL;

   uint32_t batch_index = (exec->flags & I915_EXEC_BATCH_FIRST) ? 0 : count - 1;
   uint32_t state_index = UINT32_MAX;
   for (uint32_t i = 0; i < count; i++) {
      if (i != batch_index && objects[i].relocation_count) {
         if (state_index != UINT32_MAX)
            return EINVAL;
         state_index = i;
      }
   }

   struct cd3d_bo *batch = cd3d_bo_get(dev, objects[batch_index].handle);
   struct cd3d_bo *state = state_index == UINT32_MAX ? NULL :
                           cd3d_bo_get(dev, objects[state_index].handle);
   if (!batch || (state_index != UINT32_MAX && !state) ||
       exec->batch_len == 0 || exec->batch_len > HDGFX_GEN_MAX_BATCH_SIZE ||
       (uint64_t)exec->batch_start_offset + exec->batch_len > batch->size)
      return EINVAL;

   uint32_t state_length = state ? state->size : 0;
   if (state_length > HDGFX_GEN_MAX_STATE_SIZE)
      return EINVAL;
   uint32_t state_offset = state ?
      align(CD3D_BATCH_OFFSET + exec->batch_len, CD3D_STATE_ALIGN) : 0;
   uint32_t command_length = state ? state_offset + state_length :
                             CD3D_BATCH_OFFSET + exec->batch_len;
   if (command_length > dev->command_buffer_size)
      return EINVAL;

   uint32_t *allocation_index = malloc(count * sizeof(*allocation_index));
   if (!allocation_index)
      return ENOMEM;
   for (uint32_t i = 0; i < count; i++) {
      allocation_index[i] = UINT32_MAX;
      if (i == batch_index)
         continue;
      struct cd3d_bo *bo = cd3d_bo_get(dev, objects[i].handle);
      if (!bo || allocation_count >= dev->allocation_list_size) {
         free(allocation_index);
         return bo ? ENOSPC : ENOENT;
      }
      D3DDDI_ALLOCATIONLIST *entry = &dev->allocation_list[allocation_count];
      memset(entry, 0, sizeof(*entry));
      entry->hAllocation = bo->allocation;
      entry->WriteOperation = (objects[i].flags & EXEC_OBJECT_WRITE) != 0;
      allocation_index[i] = allocation_count++;
   }

   void *batch_map = cd3d_bo_map(dev, batch);
   void *state_map = state ? cd3d_bo_map(dev, state) : NULL;
   if (!batch_map || (state && !state_map)) {
      free(allocation_index);
      return ENOMEM;
   }

   HDGFX_GEN_SUBMIT header = { 0 };
   header.Magic = HDGFX_GEN_SUBMIT_MAGIC;
   header.Version = HDGFX_GEN_SUBMIT_VERSION;
   header.BatchOffset = CD3D_BATCH_OFFSET;
   header.BatchLength = exec->batch_len;
   header.StateOffset = state_offset;
   header.StateLength = state_length;
   memcpy(dev->command_buffer, &header, sizeof(header));
   memcpy(dev->command_buffer + CD3D_BATCH_OFFSET,
          (const uint8_t *)batch_map + exec->batch_start_offset, exec->batch_len);
   if (state)
      memcpy(dev->command_buffer + state_offset, state_map, state_length);

   for (uint32_t pass = 0; pass < 2; pass++) {
      uint32_t source = pass == 0 ? batch_index : state_index;
      if (source == UINT32_MAX)
         continue;
      const struct drm_i915_gem_relocation_entry *relocs =
         (const struct drm_i915_gem_relocation_entry *)(uintptr_t)objects[source].relocs_ptr;
      for (uint32_t r = 0; r < objects[source].relocation_count; r++) {
         const struct drm_i915_gem_relocation_entry *reloc = &relocs[r];
         uint32_t patch_offset;

         if (pass == 0) {
            if (reloc->offset < exec->batch_start_offset ||
                reloc->offset - exec->batch_start_offset + 4 > exec->batch_len) {
               free(allocation_index);
               return EINVAL;
            }
            patch_offset = CD3D_BATCH_OFFSET + (uint32_t)(reloc->offset - exec->batch_start_offset);
         } else {
            if (reloc->offset + 4 > state_length) {
               free(allocation_index);
               return EINVAL;
            }
            patch_offset = state_offset + (uint32_t)reloc->offset;
         }
         if (reloc->target_handle >= count || reloc->target_handle == batch_index) {
            free(allocation_index);
            return EINVAL;
         }
         if (reloc->target_handle == state_index)
            ret = cd3d_add_patch(dev, &patch_count, patch_offset,
                                 allocation_index[state_index],
                                 reloc->delta, HDGFX_GEN_PATCH_STATE);
         else
            ret = cd3d_add_patch(dev, &patch_count, patch_offset,
                                 allocation_index[reloc->target_handle],
                                 reloc->delta, HDGFX_GEN_PATCH_ALLOCATION);
         if (ret) {
            free(allocation_index);
            return ret;
         }
      }
   }

   D3DKMT_RENDER render = { 0 };
   render.hContext = dev->context;
   render.CommandLength = command_length;
   render.AllocationCount = allocation_count;
   render.PatchLocationCount = patch_count;
   NTSTATUS status = D3DKMTRender(&render);
   if (!NT_SUCCESS(status)) {
      mesa_loge("crocus: D3DKMTRender failed: 0x%08lx, %u bytes, %u allocations, %u patches",
                status, command_length, allocation_count, patch_count);
      free(allocation_index);
      return cd3d_errno(status);
   }
   dev->command_buffer = render.pNewCommandBuffer;
   dev->command_buffer_size = render.NewCommandBufferSize;
   dev->allocation_list = render.pNewAllocationList;
   dev->allocation_list_size = render.NewAllocationListSize;
   dev->patch_list = render.pNewPatchLocationList;
   dev->patch_list_size = render.NewPatchLocationListSize;

   uint64_t fence = dev->fence_last + 1;
   D3DKMT_SIGNALSYNCHRONIZATIONOBJECTFROMGPU signal = { 0 };
   signal.hContext = dev->context;
   signal.ObjectCount = 1;
   signal.ObjectHandleArray = &dev->fence_object;
   signal.MonitoredFenceValueArray = &fence;
   status = D3DKMTSignalSynchronizationObjectFromGpu(&signal);
   free(allocation_index);
   if (!NT_SUCCESS(status)) {
      mesa_loge("crocus: fence signal failed: 0x%08lx", status);
      return cd3d_errno(status);
   }
   dev->fence_last = fence;

   for (uint32_t i = 0; i < count; i++) {
      struct cd3d_bo *bo = cd3d_bo_get(dev, objects[i].handle);
      if (bo)
         bo->fence = fence;
      objects[i].offset = 0;
   }
   if (exec->flags & I915_EXEC_FENCE_ARRAY) {
      const struct drm_i915_gem_exec_fence *fences =
         (const struct drm_i915_gem_exec_fence *)(uintptr_t)exec->cliprects_ptr;
      for (uint32_t i = 0; i < exec->num_cliprects; i++) {
         struct cd3d_syncobj *obj = cd3d_syncobj_get(dev, fences[i].handle);
         if (obj && (fences[i].flags & I915_EXEC_FENCE_SIGNAL)) {
            obj->submitted = true;
            obj->fence = fence;
         }
      }
   }
   return 0;
}

static int
cd3d_ioctl_locked(struct cd3d_device *dev, unsigned long request, void *arg)
{
   switch (request) {
   case DRM_IOCTL_I915_GETPARAM:
      return cd3d_getparam(dev, arg);
   case DRM_IOCTL_I915_GEM_GET_APERTURE: {
      struct drm_i915_gem_get_aperture *aperture = arg;
      aperture->aper_size = CD3D_APERTURE_SIZE;
      aperture->aper_available_size = CD3D_APERTURE_SIZE;
      return 0;
   }
   case DRM_IOCTL_I915_GEM_CONTEXT_CREATE: {
      struct drm_i915_gem_context_create *create = arg;
      create->ctx_id = dev->next_context_id++;
      return 0;
   }
   case DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT: {
      struct drm_i915_gem_context_create_ext *create = arg;
      if (create->extensions)
         return EINVAL;
      create->ctx_id = dev->next_context_id++;
      return 0;
   }
   case DRM_IOCTL_I915_GEM_CONTEXT_DESTROY:
      return 0;
   case DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM: {
      struct drm_i915_gem_context_param *param = arg;
      switch (param->param) {
      case I915_CONTEXT_PARAM_GTT_SIZE:
         param->value = CD3D_APERTURE_SIZE;
         return 0;
      case I915_CONTEXT_PARAM_PRIORITY:
         param->value = 0;
         return 0;
      default:
         return EINVAL;
      }
   }
   case DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM: {
      struct drm_i915_gem_context_param *param = arg;
      switch (param->param) {
      case I915_CONTEXT_PARAM_RECOVERABLE:
      case I915_CONTEXT_PARAM_BANNABLE:
         return 0;
      case I915_CONTEXT_PARAM_PRIORITY:
         return param->value == 0 ? 0 : EPERM;
      default:
         return EINVAL;
      }
   }
   case DRM_IOCTL_I915_GET_RESET_STATS: {
      struct drm_i915_reset_stats *stats = arg;
      stats->reset_count = 0;
      stats->batch_active = 0;
      stats->batch_pending = 0;
      return 0;
   }
   case DRM_IOCTL_I915_GEM_CREATE:
      return cd3d_gem_create(dev, arg);
   case DRM_IOCTL_GEM_CLOSE:
      return cd3d_gem_close(dev, arg);
   case DRM_IOCTL_I915_GEM_SET_DOMAIN:
      return cd3d_bo_get(dev, ((struct drm_i915_gem_set_domain *)arg)->handle) ? 0 : ENOENT;
   case DRM_IOCTL_I915_GEM_MADVISE: {
      struct drm_i915_gem_madvise *madv = arg;
      if (!cd3d_bo_get(dev, madv->handle))
         return ENOENT;
      madv->retained = 1;
      return 0;
   }
   case DRM_IOCTL_I915_GEM_SET_CACHING:
      return cd3d_bo_get(dev, ((struct drm_i915_gem_caching *)arg)->handle) ? 0 : ENOENT;
   case DRM_IOCTL_I915_GEM_MMAP:
      return cd3d_gem_mmap(dev, arg);
   case DRM_IOCTL_I915_GEM_SET_TILING:
      return cd3d_gem_set_tiling(dev, arg);
   case DRM_IOCTL_I915_GEM_GET_TILING:
      return cd3d_gem_get_tiling(dev, arg);
   case DRM_IOCTL_I915_GEM_BUSY:
      return cd3d_gem_busy(dev, arg);
   case DRM_IOCTL_I915_GEM_WAIT:
      return cd3d_gem_wait(dev, arg);
   case DRM_IOCTL_I915_GEM_EXECBUFFER2:
      return cd3d_execbuffer2(dev, arg);
   case DRM_IOCTL_SYNCOBJ_CREATE:
      return cd3d_syncobj_create(dev, arg);
   case DRM_IOCTL_SYNCOBJ_DESTROY:
      return cd3d_syncobj_destroy(dev, arg);
   case DRM_IOCTL_SYNCOBJ_WAIT:
      return cd3d_syncobj_wait(dev, arg);
   case DRM_IOCTL_I915_QUERY:
   case DRM_IOCTL_I915_GEM_USERPTR:
      return ENODEV;
   default:
      mesa_logw("crocus: unhandled ioctl 0x%lx", request);
      return ENODEV;
   }
}

int
intel_virtio_ioctl(int fd, unsigned long request, void *arg)
{
   struct cd3d_device *dev = cd3d_lookup(fd);
   if (!dev) {
      errno = EBADF;
      return -1;
   }
   mtx_lock(&dev->lock);
   int ret = cd3d_ioctl_locked(dev, request, arg);
   mtx_unlock(&dev->lock);
   if (ret) {
      errno = ret;
      return -1;
   }
   return 0;
}

static const driOptionDescription crocus_d3dkmt_driconf[] = {
#include "crocus/driinfo_crocus.h"
};

struct pipe_screen *
crocus_d3dkmt_screen_create(void)
{
   driOptionCache options_info = { 0 };
   driOptionCache options = { 0 };

   int fd = cd3d_create_fd();
   if (fd < 0)
      return NULL;

   driParseOptionInfo(&options_info, crocus_d3dkmt_driconf,
                      ARRAY_SIZE(crocus_d3dkmt_driconf));
   struct pipe_screen_config config = { 0 };
   config.options = &options;
   config.options_info = &options_info;
   struct pipe_screen *screen = crocus_screen_create(fd, &config);
   driDestroyOptionCache(&options);
   driDestroyOptionInfo(&options_info);
   if (!screen) {
      intel_virtio_unref_fd(fd);
      _close(fd);
   }
   return screen;
}

void
crocus_d3dkmt_present(struct pipe_context *context,
                      struct pipe_resource *resource, HDC hdc)
{
   struct pipe_transfer *transfer = NULL;
   struct pipe_box box;

   u_box_2d(0, 0, resource->width0, resource->height0, &box);
   const uint8_t *map = context->texture_map(context, resource, 0,
                                             PIPE_MAP_READ, &box, &transfer);
   if (!map)
      return;

   BITMAPINFO bmi = { 0 };
   bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
   bmi.bmiHeader.biWidth = transfer->stride / 4;
   bmi.bmiHeader.biHeight = -(LONG)resource->height0;
   bmi.bmiHeader.biPlanes = 1;
   bmi.bmiHeader.biBitCount = 32;
   bmi.bmiHeader.biCompression = BI_RGB;
   SetDIBitsToDevice(hdc, 0, 0, resource->width0, resource->height0, 0, 0, 0,
                     resource->height0, map, &bmi, DIB_RGB_COLORS);
   context->texture_unmap(context, transfer);
}
