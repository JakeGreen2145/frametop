// Explicit POC layer. Native commands and resource lifetimes are untouched.
// The optional Aurora patch is a separate launch-only gate DSO.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <openssl/evp.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <vulkan/vk_layer.h>

#include "pinned.hpp"
#define EXPORT extern "C" __attribute__((visibility("default")))
namespace {
struct InstanceDispatch {
  VkInstance instance;
  PFN_vkGetInstanceProcAddr gipa;
  PFN_GetPhysicalDeviceProcAddr gpdpa;
  PFN_vkDestroyInstance destroy;
};
struct DeviceDispatch {
  PFN_vkGetDeviceProcAddr gdpa;
  PFN_vkCreateShaderModule module;
  PFN_vkDestroyDevice destroy;
  PFN_vkDestroyShaderModule destroy_module;
  PFN_vkCreateGraphicsPipelines pipelines;
};
std::mutex dispatch_mutex;
std::unordered_map<void *, InstanceDispatch> instances;
std::unordered_map<void *, DeviceDispatch> devices;
std::unordered_map<void *, std::unordered_map<VkShaderModule, int>> modules;
void *key(const void *handle) {
  return handle ? *static_cast<void *const *>(handle) : nullptr;
}
InstanceDispatch instance_dispatch(VkInstance i) {
  std::lock_guard<std::mutex> l(dispatch_mutex);
  auto p = instances.find(key(i));
  return p == instances.end() ? InstanceDispatch{} : p->second;
}
DeviceDispatch device_dispatch(VkDevice d) {
  std::lock_guard<std::mutex> l(dispatch_mutex);
  auto p = devices.find(key(d));
  return p == devices.end() ? DeviceDispatch{} : p->second;
}

std::once_flag config_once;
bool enabled = false;
bool pin_executable() {
  FILE *f = fopen("/proc/self/exe", "rb");
  if (!f)
    return false;
  auto *ctx = EVP_MD_CTX_new();
  bool ok = ctx && EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1;
  unsigned char buffer[65536], digest[32];
  size_t n;
  unsigned size = 0;
  while (ok && (n = fread(buffer, 1, sizeof buffer, f)))
    ok = EVP_DigestUpdate(ctx, buffer, n) == 1;
  ok = ok && !ferror(f) && EVP_DigestFinal_ex(ctx, digest, &size) == 1 &&
       size == 32;
  fclose(f);
  EVP_MD_CTX_free(ctx);
  char hex[65]{};
  if (ok)
    for (unsigned i = 0; i < 32; i++)
      snprintf(hex + i * 2, 3, "%02x", digest[i]);
  return ok && !strcmp(hex, "9b1bd5fe4463592cbc71b2517b9ceb0a0fcd66cd6885a19dca"
                            "4be0784bf6c265");
}
void load_configuration() {
  enabled = pin_executable();
  fprintf(stderr, "ft-poc: executable pin %s\n",
          enabled ? "accepted" : "rejected");
}
VkResult VKAPI_CALL create_instance(const VkInstanceCreateInfo *ci,
                                    const VkAllocationCallbacks *a,
                                    VkInstance *out) {
  std::call_once(config_once, load_configuration);
  auto chain = (VkLayerInstanceCreateInfo *)ci->pNext;
  while (chain &&
         (chain->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO ||
          chain->function != VK_LAYER_LINK_INFO))
    chain = (VkLayerInstanceCreateInfo *)chain->pNext;
  if (!chain || !chain->u.pLayerInfo)
    return VK_ERROR_INITIALIZATION_FAILED;
  auto link = chain->u.pLayerInfo;
  auto next = link->pfnNextGetInstanceProcAddr;
  auto physical = link->pfnNextGetPhysicalDeviceProcAddr;
  auto create = (PFN_vkCreateInstance)next(nullptr, "vkCreateInstance");
  if (!create)
    return VK_ERROR_INITIALIZATION_FAILED;
  chain->u.pLayerInfo = link->pNext;
  VkResult r = create(ci, a, out);
  if (r != VK_SUCCESS)
    return r;
  InstanceDispatch dispatch{
      *out, next, physical,
      (PFN_vkDestroyInstance)next(*out, "vkDestroyInstance")};
  try {
    std::lock_guard<std::mutex> l(dispatch_mutex);
    instances.emplace(key(*out), dispatch);
  } catch (...) {
    dispatch.destroy(*out, a);
    return VK_ERROR_OUT_OF_HOST_MEMORY;
  }
  return r;
}
void VKAPI_CALL destroy_instance(VkInstance i, const VkAllocationCallbacks *a) {
  auto d = instance_dispatch(i);
  if (!d.destroy)
    return;
  {
    std::lock_guard<std::mutex> l(dispatch_mutex);
    instances.erase(key(i));
  }
  d.destroy(i, a);
}
VkResult VKAPI_CALL create_device(VkPhysicalDevice p,
                                  const VkDeviceCreateInfo *ci,
                                  const VkAllocationCallbacks *a,
                                  VkDevice *out) {
  auto parent = instance_dispatch((VkInstance)p);
  if (!parent.gipa)
    return VK_ERROR_INITIALIZATION_FAILED;
  auto chain = (VkLayerDeviceCreateInfo *)ci->pNext;
  while (chain &&
         (chain->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO ||
          chain->function != VK_LAYER_LINK_INFO))
    chain = (VkLayerDeviceCreateInfo *)chain->pNext;
  if (!chain || !chain->u.pLayerInfo)
    return VK_ERROR_INITIALIZATION_FAILED;
  auto link = chain->u.pLayerInfo;
  auto gdpa = link->pfnNextGetDeviceProcAddr;
  auto create = (PFN_vkCreateDevice)link->pfnNextGetInstanceProcAddr(
      parent.instance, "vkCreateDevice");
  if (!create || !gdpa)
    return VK_ERROR_INITIALIZATION_FAILED;
  chain->u.pLayerInfo = link->pNext;
  VkResult r = create(p, ci, a, out);
  if (r != VK_SUCCESS)
    return r;
  DeviceDispatch d{
      gdpa, (PFN_vkCreateShaderModule)gdpa(*out, "vkCreateShaderModule"),
      (PFN_vkDestroyDevice)gdpa(*out, "vkDestroyDevice"),
      (PFN_vkDestroyShaderModule)gdpa(*out, "vkDestroyShaderModule"),
      (PFN_vkCreateGraphicsPipelines)gdpa(*out, "vkCreateGraphicsPipelines")};
  try {
    std::lock_guard<std::mutex> l(dispatch_mutex);
    devices.emplace(key(*out), d);
  } catch (...) {
    d.destroy(*out, a);
    return VK_ERROR_OUT_OF_HOST_MEMORY;
  }
  return r;
}
void VKAPI_CALL destroy_device(VkDevice device,
                               const VkAllocationCallbacks *a) {
  auto d = device_dispatch(device);
  if (!d.destroy)
    return;
  {
    std::lock_guard<std::mutex> l(dispatch_mutex);
    devices.erase(key(device));
    modules.erase(key(device));
  }
  d.destroy(device, a);
}
VkResult VKAPI_CALL create_module(VkDevice device,
                                  const VkShaderModuleCreateInfo *ci,
                                  const VkAllocationCallbacks *a,
                                  VkShaderModule *out) {
  auto d = device_dispatch(device);
  if (!d.module)
    return VK_ERROR_INITIALIZATION_FAILED;
  std::call_once(config_once, load_configuration);
  if (enabled && ci && ci->pCode)
    for (const auto &p : pins) {
      if (ci->codeSize == p.size && !memcmp(ci->pCode, p.data, p.size)) {
        VkShaderModuleCreateInfo replacement = *ci;
        replacement.codeSize = p.replacement_size;
        replacement.pCode = p.replacement;

        VkResult result = d.module(device, &replacement, a, out);
        if (result == VK_SUCCESS) {
          try {
            std::lock_guard<std::mutex> l(dispatch_mutex);
            modules[key(device)][*out] = p.id;
          } catch (...) {
            d.destroy_module(device, *out, a);
            *out = VK_NULL_HANDLE;
            return VK_ERROR_OUT_OF_HOST_MEMORY;
          }
        }
        fprintf(stderr, "ft-poc: shader %d result %d\n", p.id, int(result));
        return result;
      }
    }
  return d.module(device, ci, a, out);
}

void VKAPI_CALL destroy_module(VkDevice device, VkShaderModule module,
                               const VkAllocationCallbacks *a) {
  auto d = device_dispatch(device);
  if (!d.destroy_module)
    return;
  {
    std::lock_guard<std::mutex> l(dispatch_mutex);
    auto it = modules.find(key(device));
    if (it != modules.end())
      it->second.erase(module);
  }
  d.destroy_module(device, module, a);
}
VkResult VKAPI_CALL create_pipelines(VkDevice device, VkPipelineCache cache,
                                     uint32_t count,
                                     const VkGraphicsPipelineCreateInfo *ci,
                                     const VkAllocationCallbacks *a,
                                     VkPipeline *out) {
  auto d = device_dispatch(device);
  if (!d.pipelines)
    return VK_ERROR_INITIALIZATION_FAILED;
  try {
    std::vector<VkGraphicsPipelineCreateInfo> copies(ci, ci + count);
    std::vector<VkPipelineColorBlendStateCreateInfo> blends(count);
    std::vector<VkPipelineColorBlendAttachmentState> attachments(count);
    unsigned changed = 0;
    {
      std::lock_guard<std::mutex> l(dispatch_mutex);
      auto known = modules.find(key(device));
      if (known != modules.end())
        for (uint32_t i = 0; i < count; i++) {
          int vs = 0, ps = 0;
          const auto &c = ci[i];
          if (c.stageCount != 2 || !c.pStages)
            continue;
          for (uint32_t j = 0; j < c.stageCount; j++) {
            const auto &stage = c.pStages[j];
            auto m = known->second.find(stage.module);
            if (m == known->second.end() || !stage.pName)
              continue;
            if (stage.stage == VK_SHADER_STAGE_VERTEX_BIT &&
                !strcmp(stage.pName, "VS_MAIN"))
              vs = m->second;
            if (stage.stage == VK_SHADER_STAGE_FRAGMENT_BIT &&
                !strcmp(stage.pName, "PS_MAIN"))
              ps = m->second;
          }
          if (!((vs == 57 && ps >= 125 && ps <= 127) ||
                (vs == 58 && ps >= 128 && ps <= 130)))
            continue;
          const auto *b = c.pColorBlendState;
          if (!b || b->attachmentCount != 1 || !b->pAttachments ||
              b->logicOpEnable)
            continue;
          const auto &original = b->pAttachments[0];
          if (!original.blendEnable ||
              original.colorBlendOp != VK_BLEND_OP_ADD ||
              original.alphaBlendOp != VK_BLEND_OP_ADD)
            continue;
          blends[i] = *b;
          attachments[i] = b->pAttachments[0];
          auto &v = attachments[i];
          v.srcColorBlendFactor = v.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
          v.dstColorBlendFactor = v.dstAlphaBlendFactor =
              VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
          blends[i].pAttachments = &v;
          copies[i].pColorBlendState = &blends[i];
          changed++;
        }
    }
    VkResult result =
        d.pipelines(device, cache, count, changed ? copies.data() : ci, a, out);
    if (changed)
      fprintf(stderr, "ft-poc: source-over pipelines %u result %d\n", changed,
              int(result));
    return result;
  } catch (...) {
    return VK_ERROR_OUT_OF_HOST_MEMORY;
  }
}
} // namespace
EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vkGetInstanceProcAddr(VkInstance, const char *);
EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vkGetDeviceProcAddr(VkDevice, const char *);
EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vkGetDeviceProcAddr(VkDevice d, const char *n) {
  if (!n || !d)
    return nullptr;
  if (!strcmp(n, "vkGetDeviceProcAddr"))
    return (PFN_vkVoidFunction)vkGetDeviceProcAddr;
  if (!strcmp(n, "vkDestroyShaderModule"))
    return (PFN_vkVoidFunction)destroy_module;
  if (!strcmp(n, "vkCreateGraphicsPipelines"))
    return (PFN_vkVoidFunction)create_pipelines;
  if (!strcmp(n, "vkCreateShaderModule"))
    return (PFN_vkVoidFunction)create_module;
  if (!strcmp(n, "vkDestroyDevice"))
    return (PFN_vkVoidFunction)destroy_device;
  auto next = device_dispatch(d);
  return next.gdpa ? next.gdpa(d, n) : nullptr;
}
EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vkGetInstanceProcAddr(VkInstance i, const char *n) {
  if (!n)
    return nullptr;
  if (!strcmp(n, "vkGetInstanceProcAddr"))
    return (PFN_vkVoidFunction)vkGetInstanceProcAddr;
  if (!strcmp(n, "vkCreateInstance"))
    return (PFN_vkVoidFunction)create_instance;
  if (!i)
    return nullptr;
  if (!strcmp(n, "vkGetDeviceProcAddr"))
    return (PFN_vkVoidFunction)vkGetDeviceProcAddr;
  if (!strcmp(n, "vkDestroyInstance"))
    return (PFN_vkVoidFunction)destroy_instance;
  if (!strcmp(n, "vkCreateDevice"))
    return (PFN_vkVoidFunction)create_device;
  if (!strcmp(n, "vkDestroyDevice"))
    return (PFN_vkVoidFunction)destroy_device;
  if (!strcmp(n, "vkDestroyShaderModule"))
    return (PFN_vkVoidFunction)destroy_module;
  if (!strcmp(n, "vkCreateGraphicsPipelines"))
    return (PFN_vkVoidFunction)create_pipelines;
  if (!strcmp(n, "vkCreateShaderModule"))
    return (PFN_vkVoidFunction)create_module;
  auto next = instance_dispatch(i);
  return next.gipa ? next.gipa(i, n) : nullptr;
}
static PFN_vkVoidFunction VKAPI_CALL physical_proc(VkInstance i,
                                                   const char *n) {
  if (!i || !n)
    return nullptr;
  auto d = instance_dispatch(i);
  return d.gpdpa ? d.gpdpa(i, n) : nullptr;
}
EXPORT VKAPI_ATTR VkResult VKAPI_CALL
vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *p) {
  if (!p || p->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT ||
      p->loaderLayerInterfaceVersion < 2)
    return VK_ERROR_INITIALIZATION_FAILED;
  p->loaderLayerInterfaceVersion = 2;
  p->pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
  p->pfnGetDeviceProcAddr = vkGetDeviceProcAddr;
  p->pfnGetPhysicalDeviceProcAddr = physical_proc;
  return VK_SUCCESS;
}
