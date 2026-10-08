// SPDX-License-Identifier: MIT
// Explicit Vulkan layer. No constructors, IPC, native offsets, or global
// replacement.
#include "bundle.hpp"
#include "ft_pipeline.h"
#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <vulkan/vk_layer.h>
#define EXPORT extern "C" FT_PIPELINE_EXPORT VKAPI_ATTR
EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance,
                                                           const char *);
EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice,
                                                         const char *);
namespace {
constexpr size_t max_devices = 16, max_instances = 16, max_modules = 1024,
                 max_pipelines = 4096, max_batch = 64;
constexpr size_t bundle_count = sizeof(ft_bundle) / sizeof(ft_bundle[0]);
struct Module {
  VkShaderModule handle{};
  uint32_t id{};
  bool compatible{};
};
struct Record {
  bool reserved{};
  FtPipelineRecord data{};
};
struct Device {
  VkDevice handle{};
  void *key{};
  PFN_vkGetDeviceProcAddr gdpa{};
  PFN_vkCreateShaderModule shader{};
  PFN_vkDestroyShaderModule destroy_shader{};
  PFN_vkCreateGraphicsPipelines create{};
  PFN_vkDestroyPipeline destroy_pipeline{};
  std::mutex mutex;
  uint64_t generation{};
  bool ready = false, revoked = false;
  std::array<VkShaderModule, bundle_count> patched{};
  std::array<Module, max_modules> modules{};
  std::array<Record, max_pipelines> records{};
};
struct Instance {
  VkInstance handle{};
  void *key{};
  PFN_vkGetInstanceProcAddr gipa{};
  PFN_GetPhysicalDeviceProcAddr physical{};
};
std::mutex global_mutex;
std::array<std::shared_ptr<Device>, max_devices> devices;
std::array<Instance, max_instances> instances;
std::atomic<uint64_t> sequence{1};
struct Context {
  uint64_t token{}, generation{};
  VkDevice device{};
  uint32_t vertex{}, fragment{};
  bool poisoned = false, in_driver = false;
};
thread_local Context scope;
uint64_t fresh() {
  uint64_t n = sequence.load(std::memory_order_relaxed);
  while (n && n != UINT64_MAX) {
    if (sequence.compare_exchange_weak(n, n + 1, std::memory_order_relaxed))
      return n;
  }
  return 0;
}
void *dispatch_key(const void *h) {
  return h ? *static_cast<void *const *>(h) : nullptr;
}
std::shared_ptr<Device> get(VkDevice h) {
  std::lock_guard<std::mutex> lock(global_mutex);
  for (auto &d : devices)
    if (d && d->handle == h)
      return d;
  return {};
}
std::shared_ptr<Device> queue_device(VkQueue h) {
  auto key = dispatch_key(h);
  std::lock_guard<std::mutex> lock(global_mutex);
  for (auto &d : devices)
    if (d && d->key == key)
      return d;
  return {};
}
Instance get_instance(const void *h) {
  auto key = dispatch_key(h);
  std::lock_guard<std::mutex> lock(global_mutex);
  for (auto &i : instances)
    if (i.handle && i.key == key)
      return i;
  return {};
}
const FtBundle *bundle(uint32_t id) {
  for (auto &b : ft_bundle)
    if (b.id == id)
      return &b;
  return nullptr;
}
bool pair(uint32_t v, uint32_t f) {
  for (auto &p : ft_pairs)
    if (p[0] == v && p[1] == f)
      return true;
  return false;
}
VkShaderModule patched(const Device &d, uint32_t id) {
  for (size_t i = 0; i < bundle_count; i++)
    if (ft_bundle[i].id == id)
      return d.patched[i];
  return {};
}
Module module(const Device &d, VkShaderModule m) {
  for (auto &s : d.modules)
    if (s.handle == m)
      return s;
  return {};
}
void revoke_locked(Device &d) {
  d.revoked = true;
  for (auto &r : d.records)
    r = {};
}
void observe(const std::shared_ptr<Device> &d, VkResult r) {
  if (d && r == VK_ERROR_DEVICE_LOST) {
    std::lock_guard<std::mutex> lock(d->mutex);
    revoke_locked(*d);
  }
}
bool valid_locked(const Device &d, const Context &c) {
  return d.ready && !d.revoked && c.device == d.handle &&
         c.generation == d.generation && !c.poisoned;
}
VkResult refuse(uint32_t n, VkPipeline *out,
                VkResult result = VK_ERROR_INITIALIZATION_FAILED) {
  if (out)
    for (uint32_t i = 0; i < n; i++)
      out[i] = VK_NULL_HANDLE;
  if (scope.token)
    scope.poisoned = true;
  return result;
}

VkResult VKAPI_CALL create_shader(VkDevice h, const VkShaderModuleCreateInfo *c,
                                  const VkAllocationCallbacks *a,
                                  VkShaderModule *out) {
  auto d = get(h);
  if (!d)
    return VK_ERROR_INITIALIZATION_FAILED;
  auto result = d->shader(h, c, a, out);
  observe(d, result);
  if (result != VK_SUCCESS || !out || !*out || !c || !c->pCode)
    return result;
  uint32_t id = 0;
  for (auto &b : ft_bundle)
    if (c->codeSize == b.original_size &&
        !std::memcmp(c->pCode, b.original, b.original_size)) {
      id = b.id;
      break;
    }
  if (id) {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (auto &m : d->modules)
      if (!m.handle) {
        m = {*out, id, c->pNext == nullptr && c->flags == 0};
        return result;
      }
    // Losing exact recognition must not permit a later owned fallback.
    revoke_locked(*d);
  }
  return result;
}
void VKAPI_CALL destroy_shader(VkDevice h, VkShaderModule m,
                               const VkAllocationCallbacks *a) {
  auto d = get(h);
  if (!d)
    return;
  {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (auto &s : d->modules)
      if (s.handle == m)
        s = {};
  }
  d->destroy_shader(h, m, a);
}
void VKAPI_CALL destroy_pipeline(VkDevice h, VkPipeline p,
                                 const VkAllocationCallbacks *a) {
  auto d = get(h);
  if (!d)
    return;
  {
    std::lock_guard<std::mutex> lock(d->mutex);
    for (auto &r : d->records)
      if (p && r.data.pipeline == p)
        r = {};
  }
  d->destroy_pipeline(h, p, a);
}

bool compatible(const VkGraphicsPipelineCreateInfo &c) {
  if (c.sType != VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO || c.pNext ||
      c.stageCount != 2 || !c.pStages)
    return false;
// No extension-chain allowlist has been established on the pinned native path.
#define NO_NEXT(p)                                                             \
  if (c.p && c.p->pNext)                                                       \
  return false
  NO_NEXT(pVertexInputState);
  NO_NEXT(pInputAssemblyState);
  NO_NEXT(pTessellationState);
  NO_NEXT(pViewportState);
  NO_NEXT(pRasterizationState);
  NO_NEXT(pMultisampleState);
  NO_NEXT(pDepthStencilState);
  NO_NEXT(pColorBlendState);
  NO_NEXT(pDynamicState);
#undef NO_NEXT
  for (uint32_t j = 0; j < 2; j++) {
    auto &s = c.pStages[j];
    if (s.sType != VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO ||
        s.pNext || s.flags || s.pSpecializationInfo || !s.pName)
      return false;
  }
  return true;
}
VkResult VKAPI_CALL create_pipelines(VkDevice h, VkPipelineCache cache,
                                     uint32_t n,
                                     const VkGraphicsPipelineCreateInfo *c,
                                     const VkAllocationCallbacks *a,
                                     VkPipeline *out) {
  auto d = get(h);
  if (!d)
    return refuse(n, out);
  if (!scope.token) {
    auto result = d->create(h, cache, n, c, a, out);
    observe(d, result);
    return result;
  }
  if (scope.in_driver || scope.poisoned || scope.device != h || !n ||
      n > max_batch || !c || !out)
    return refuse(n, out);
  Context checked = scope;
  std::array<VkGraphicsPipelineCreateInfo, max_batch> copies;
  std::array<std::array<VkPipelineShaderStageCreateInfo, 2>, max_batch> stages;
  std::array<FtPipelineRecord, max_batch> records{};
  std::array<size_t, max_batch> slots{};
  size_t owned = 0;
  {
    std::lock_guard<std::mutex> lock(d->mutex);
    if (!valid_locked(*d, checked))
      return refuse(n, out);
    std::memcpy(copies.data(), c, n * sizeof(*c));
    for (uint32_t i = 0; i < n; i++) {
      auto &ci = c[i];
      bool camera = false;
      if (ci.stageCount && !ci.pStages)
        return refuse(n, out);
      if (ci.stageCount > 32)
        return refuse(n, out);
      for (uint32_t j = 0; j < ci.stageCount; j++)
        if (module(*d, ci.pStages[j].module).id)
          camera = true;
      if (!camera)
        continue; // unrelated stock entries retain their exact original
                  // pointers
      if (!compatible(ci))
        return refuse(n, out);
      Module vs{}, fs{};
      int vi = -1, fi = -1;
      for (unsigned j = 0; j < 2; j++) {
        auto &s = ci.pStages[j];
        auto m = module(*d, s.module);
        auto b = bundle(m.id);
        if (!b || !m.compatible || s.stage != b->stage ||
            std::strcmp(s.pName, b->entry))
          return refuse(n, out);
        if (s.stage == VK_SHADER_STAGE_VERTEX_BIT && vi == -1) {
          vs = m;
          vi = int(j);
        } else if (s.stage == VK_SHADER_STAGE_FRAGMENT_BIT && fi == -1) {
          fs = m;
          fi = int(j);
        } else
          return refuse(n, out);
      }
      if (vi < 0 || fi < 0 || vs.id != checked.vertex ||
          fs.id != checked.fragment)
        return refuse(n, out);
      auto pv = patched(*d, vs.id), pf = patched(*d, fs.id);
      if (!pv || !pf)
        return refuse(n, out);
      std::memcpy(stages[i].data(), ci.pStages, 2 * sizeof(*ci.pStages));
      stages[i][vi].module = pv;
      stages[i][fi].module = pf;
      copies[i].pStages = stages[i].data();
      records[i] = {FT_PIPELINE_ABI, vs.id,     fs.id, 0, d->generation, {},
                    vs.handle,       fs.handle, pv,    pf};
      ++owned;
    }
    // Reserve the complete registry transaction before the one downstream call.
    size_t found = 0;
    for (size_t i = 0; i < d->records.size() && found < owned; i++)
      if (!d->records[i].reserved)
        slots[found++] = i;
    if (found != owned)
      return refuse(n, out, VK_ERROR_OUT_OF_HOST_MEMORY);
    for (size_t i = 0; i < owned; i++)
      d->records[slots[i]].reserved = true;
  }
  for (uint32_t i = 0; i < n; i++)
    out[i] = VK_NULL_HANDLE;
  scope.in_driver = true;
  auto result = d->create(h, cache, n, copies.data(), a, out);
  scope.in_driver = false;
  bool accepted = false;
  {
    std::lock_guard<std::mutex> lock(d->mutex);
    if (result == VK_ERROR_DEVICE_LOST)
      revoke_locked(*d);
    accepted = result == VK_SUCCESS && scope.token == checked.token &&
               !scope.poisoned && valid_locked(*d, checked);
    for (uint32_t i = 0; i < n; i++)
      if (records[i].abi && !out[i])
        accepted = false;
    if (accepted) {
      size_t j = 0;
      for (uint32_t i = 0; i < n; i++)
        if (records[i].abi) {
          records[i].pipeline = out[i];
          d->records[slots[j++]].data = records[i];
        }
    } else
      for (size_t i = 0; i < owned; i++)
        d->records[slots[i]] = {};
  }
  if (!accepted) {
    // All outputs from the failed transaction, including stock derivative
    // dependencies, are destroyed with the original allocator. No partial owned
    // handle escapes.
    for (uint32_t i = 0; i < n; i++)
      if (out[i])
        d->destroy_pipeline(h, out[i], a);
    return refuse(
        n, out, result == VK_SUCCESS ? VK_ERROR_INITIALIZATION_FAILED : result);
  }
  return result;
}

VkResult VKAPI_CALL create_instance(const VkInstanceCreateInfo *c,
                                    const VkAllocationCallbacks *a,
                                    VkInstance *out) {
  auto *chain = (VkLayerInstanceCreateInfo *)c->pNext;
  while (chain &&
         (chain->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO ||
          chain->function != VK_LAYER_LINK_INFO))
    chain = (VkLayerInstanceCreateInfo *)chain->pNext;
  if (!chain || !chain->u.pLayerInfo)
    return VK_ERROR_INITIALIZATION_FAILED;
  auto *link = chain->u.pLayerInfo;
  auto next = link->pfnNextGetInstanceProcAddr;
  auto physical = link->pfnNextGetPhysicalDeviceProcAddr;
  auto create = (PFN_vkCreateInstance)next({}, "vkCreateInstance");
  if (!create)
    return VK_ERROR_INITIALIZATION_FAILED;
  chain->u.pLayerInfo = link->pNext;
  auto result = create(c, a, out);
  if (result != VK_SUCCESS)
    return result;
  {
    std::lock_guard<std::mutex> lock(global_mutex);
    for (auto &i : instances)
      if (!i.handle) {
        i = {*out, dispatch_key(*out), next, physical};
        return result;
      }
  }
  ((PFN_vkDestroyInstance)next(*out, "vkDestroyInstance"))(*out, a);
  *out = {};
  return VK_ERROR_OUT_OF_HOST_MEMORY;
}
void VKAPI_CALL destroy_instance(VkInstance h, const VkAllocationCallbacks *a) {
  auto i = get_instance(h);
  if (!i.handle)
    return;
  {
    std::lock_guard<std::mutex> lock(global_mutex);
    for (auto &x : instances)
      if (x.handle == h)
        x = {};
  }
  ((PFN_vkDestroyInstance)i.gipa(h, "vkDestroyInstance"))(h, a);
}
VkResult VKAPI_CALL create_device(VkPhysicalDevice p,
                                  const VkDeviceCreateInfo *c,
                                  const VkAllocationCallbacks *a,
                                  VkDevice *out) {
  auto parent = get_instance(p);
  auto *chain = (VkLayerDeviceCreateInfo *)c->pNext;
  while (chain &&
         (chain->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO ||
          chain->function != VK_LAYER_LINK_INFO))
    chain = (VkLayerDeviceCreateInfo *)chain->pNext;
  if (!parent.handle || !chain || !chain->u.pLayerInfo)
    return VK_ERROR_INITIALIZATION_FAILED;
  auto *link = chain->u.pLayerInfo;
  auto gdpa = link->pfnNextGetDeviceProcAddr;
  auto create = (PFN_vkCreateDevice)link->pfnNextGetInstanceProcAddr(
      parent.handle, "vkCreateDevice");
  std::shared_ptr<Device> d;
  try {
    d = std::make_shared<Device>();
  } catch (const std::bad_alloc &) {
    return VK_ERROR_OUT_OF_HOST_MEMORY;
  }
  chain->u.pLayerInfo = link->pNext;
  auto result = create(p, c, a, out);
  if (result != VK_SUCCESS)
    return result;
  d->handle = *out;
  d->key = dispatch_key(*out);
  d->gdpa = gdpa;
  d->generation = fresh();
  d->shader = (PFN_vkCreateShaderModule)gdpa(*out, "vkCreateShaderModule");
  d->destroy_shader =
      (PFN_vkDestroyShaderModule)gdpa(*out, "vkDestroyShaderModule");
  d->create =
      (PFN_vkCreateGraphicsPipelines)gdpa(*out, "vkCreateGraphicsPipelines");
  d->destroy_pipeline = (PFN_vkDestroyPipeline)gdpa(*out, "vkDestroyPipeline");
  d->ready = d->generation && d->shader && d->destroy_shader && d->create &&
             d->destroy_pipeline;
  if (d->ready)
    for (size_t j = 0; j < bundle_count; j++) {
      VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
      sm.codeSize = ft_bundle[j].portal_size;
      sm.pCode = ft_bundle[j].portal;
      VkShaderModule created{};
      auto r = d->shader(*out, &sm, nullptr, &created);
      if (r != VK_SUCCESS || !created) {
        d->ready = false;
        break;
      }
      d->patched[j] = created;
    }
  if (!d->ready) {
    for (auto &m : d->patched)
      if (m) {
        d->destroy_shader(*out, m, nullptr);
        m = {};
      }
  }
  {
    std::lock_guard<std::mutex> lock(global_mutex);
    for (auto &slot : devices)
      if (!slot) {
        slot = d;
        return result;
      }
  }
  for (auto m : d->patched)
    if (m)
      d->destroy_shader(*out, m, nullptr);
  ((PFN_vkDestroyDevice)gdpa(*out, "vkDestroyDevice"))(*out, a);
  *out = {};
  return VK_ERROR_OUT_OF_HOST_MEMORY;
}
void VKAPI_CALL destroy_device(VkDevice h, const VkAllocationCallbacks *a) {
  auto d = get(h);
  if (!d)
    return;
  {
    std::lock_guard<std::mutex> lock(d->mutex);
    revoke_locked(*d);
  }
  {
    std::lock_guard<std::mutex> lock(global_mutex);
    for (auto &slot : devices)
      if (slot == d)
        slot.reset();
  }
  for (auto m : d->patched)
    if (m)
      d->destroy_shader(h, m, nullptr);
  ((PFN_vkDestroyDevice)d->gdpa(h, "vkDestroyDevice"))(h, a);
}
// Observe common submission/wait loss paths without rewriting any caller bytes.
VkResult VKAPI_CALL queue_submit(VkQueue q, uint32_t n, const VkSubmitInfo *s,
                                 VkFence f) {
  auto d = queue_device(q);
  if (!d)
    return VK_ERROR_DEVICE_LOST;
  auto r = ((PFN_vkQueueSubmit)d->gdpa(d->handle, "vkQueueSubmit"))(q, n, s, f);
  observe(d, r);
  return r;
}
VkResult VKAPI_CALL queue_wait(VkQueue q) {
  auto d = queue_device(q);
  if (!d)
    return VK_ERROR_DEVICE_LOST;
  auto r = ((PFN_vkQueueWaitIdle)d->gdpa(d->handle, "vkQueueWaitIdle"))(q);
  observe(d, r);
  return r;
}
VkResult VKAPI_CALL device_wait(VkDevice h) {
  auto d = get(h);
  if (!d)
    return VK_ERROR_DEVICE_LOST;
  auto r = ((PFN_vkDeviceWaitIdle)d->gdpa(h, "vkDeviceWaitIdle"))(h);
  observe(d, r);
  return r;
}
VkResult VKAPI_CALL wait_fences(VkDevice h, uint32_t n, const VkFence *f,
                                VkBool32 all, uint64_t timeout) {
  auto d = get(h);
  if (!d)
    return VK_ERROR_DEVICE_LOST;
  auto r = ((PFN_vkWaitForFences)d->gdpa(h, "vkWaitForFences"))(h, n, f, all,
                                                                timeout);
  observe(d, r);
  return r;
}
VkResult VKAPI_CALL wait_semaphores(VkDevice h, const VkSemaphoreWaitInfo *i,
                                    uint64_t timeout) {
  auto d = get(h);
  if (!d)
    return VK_ERROR_DEVICE_LOST;
  auto fn = (PFN_vkWaitSemaphores)d->gdpa(h, "vkWaitSemaphores");
  if (!fn)
    fn = (PFN_vkWaitSemaphores)d->gdpa(h, "vkWaitSemaphoresKHR");
  auto r = fn(h, i, timeout);
  observe(d, r);
  return r;
}
VkResult submit2(VkQueue q, uint32_t n, const VkSubmitInfo2 *s, VkFence f,
                 const char *name) {
  auto d = queue_device(q);
  if (!d)
    return VK_ERROR_DEVICE_LOST;
  auto r = ((PFN_vkQueueSubmit2)d->gdpa(d->handle, name))(q, n, s, f);
  observe(d, r);
  return r;
}
VkResult VKAPI_CALL queue_submit2(VkQueue q, uint32_t n, const VkSubmitInfo2 *s,
                                  VkFence f) {
  return submit2(q, n, s, f, "vkQueueSubmit2");
}
VkResult VKAPI_CALL queue_submit2_khr(VkQueue q, uint32_t n,
                                      const VkSubmitInfo2 *s, VkFence f) {
  return submit2(q, n, s, f, "vkQueueSubmit2KHR");
}
PFN_vkVoidFunction intercept(const char *n) {
#define MAP(name, fn)                                                          \
  if (!std::strcmp(n, name))                                                   \
  return reinterpret_cast<PFN_vkVoidFunction>(fn)
  MAP("vkCreateShaderModule", create_shader);
  MAP("vkDestroyShaderModule", destroy_shader);
  MAP("vkCreateGraphicsPipelines", create_pipelines);
  MAP("vkDestroyPipeline", destroy_pipeline);
  MAP("vkDestroyDevice", destroy_device);
  MAP("vkQueueSubmit", queue_submit);
  MAP("vkQueueSubmit2", queue_submit2);
  MAP("vkQueueSubmit2KHR", queue_submit2_khr);
  MAP("vkQueueWaitIdle", queue_wait);
  MAP("vkDeviceWaitIdle", device_wait);
  MAP("vkWaitForFences", wait_fences);
  MAP("vkWaitSemaphores", wait_semaphores);
  MAP("vkWaitSemaphoresKHR", wait_semaphores);
  return nullptr;
}
} // namespace

uint64_t ft_pipeline_generation(VkDevice h) {
  auto d = get(h);
  if (!d)
    return 0;
  std::lock_guard<std::mutex> lock(d->mutex);
  return d->ready && !d->revoked ? d->generation : 0;
}
uint64_t ft_pipeline_begin(VkDevice h, uint64_t gen, uint32_t v, uint32_t f) {
  if (scope.token) {
    scope.poisoned = true;
    return 0;
  }
  auto d = get(h);
  if (!d || !gen || !pair(v, f))
    return 0;
  std::lock_guard<std::mutex> lock(d->mutex);
  if (!d->ready || d->revoked || gen != d->generation)
    return 0;
  auto token = fresh();
  if (!token)
    return 0;
  scope = {token, gen, h, v, f, false, false};
  return token;
}
int ft_pipeline_end(uint64_t token) {
  if (!token || token != scope.token || scope.in_driver) {
    if (scope.token)
      scope.poisoned = true;
    return 0;
  }
  bool ok = !scope.poisoned;
  scope = {};
  return ok;
}
int ft_pipeline_approved(VkDevice h, uint64_t gen, VkPipeline p, uint32_t v,
                         uint32_t f, FtPipelineRecord *out) {
  if (out)
    *out = {};
  auto d = get(h);
  if (!d || !p || !gen)
    return 0;
  // Cache hits inside a poisoned/reentrant checked lookup cannot escape via old
  // approval.
  if (scope.token &&
      (scope.poisoned || scope.in_driver || scope.device != h ||
       scope.generation != gen || scope.vertex != v || scope.fragment != f))
    return 0;
  std::lock_guard<std::mutex> lock(d->mutex);
  if (!d->ready || d->revoked || gen != d->generation)
    return 0;
  for (auto &r : d->records)
    if (r.reserved && r.data.pipeline == p && r.data.vertex_id == v &&
        r.data.fragment_id == f && r.data.generation == gen) {
      if (out)
        *out = r.data;
      return 1;
    }
  return 0;
}
void ft_pipeline_revoke(VkDevice h, uint64_t gen) {
  auto d = get(h);
  if (!d)
    return;
  std::lock_guard<std::mutex> lock(d->mutex);
  if (d->generation == gen)
    revoke_locked(*d);
}

EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice h,
                                                         const char *n) {
  if (!h || !n)
    return nullptr;
  auto d = get(h);
  if (!d)
    return nullptr;
  MAP("vkGetDeviceProcAddr", vkGetDeviceProcAddr);
  auto next = d->gdpa(h, n);
  if (!next)
    return nullptr;
  auto own = intercept(n);
  return own ? own : next;
}
EXPORT PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance h,
                                                           const char *n) {
  if (!n)
    return nullptr;
  MAP("vkGetInstanceProcAddr", vkGetInstanceProcAddr);
  MAP("vkCreateInstance", create_instance);
  if (!h)
    return nullptr;
  auto i = get_instance(h);
  if (!i.handle)
    return nullptr;
  MAP("vkGetDeviceProcAddr", vkGetDeviceProcAddr);
  MAP("vkCreateDevice", create_device);
  MAP("vkDestroyInstance", destroy_instance);
  auto next = i.gipa(h, n);
  if (!next)
    return nullptr;
  auto own = intercept(n);
  return own ? own : next;
}
EXPORT PFN_vkVoidFunction VKAPI_CALL
vk_layerGetPhysicalDeviceProcAddr(VkInstance h, const char *n) {
  auto i = get_instance(h);
  return i.handle && i.physical ? i.physical(h, n) : nullptr;
}
EXPORT VkResult VKAPI_CALL
vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *v) {
  if (!v || v->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT ||
      v->loaderLayerInterfaceVersion < 2)
    return VK_ERROR_INITIALIZATION_FAILED;
  v->loaderLayerInterfaceVersion = 2;
  v->pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
  v->pfnGetDeviceProcAddr = vkGetDeviceProcAddr;
  v->pfnGetPhysicalDeviceProcAddr = vk_layerGetPhysicalDeviceProcAddr;
  return VK_SUCCESS;
}
