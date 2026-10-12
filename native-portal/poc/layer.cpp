// Explicit POC layer. Native commands and resource lifetimes are untouched.
// The optional Aurora patch is a separate launch-only gate DSO.
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dlfcn.h>
#include <fcntl.h>
#include <mutex>
#include <openssl/evp.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
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
  PFN_vkCreatePipelineLayout create_layout;
  PFN_vkDestroyPipelineLayout destroy_layout;
  PFN_vkDestroyPipeline destroy_pipeline;
  PFN_vkCmdBindPipeline bind_pipeline;
  PFN_vkCmdPushConstants push;
  PFN_vkCmdDrawIndexed draw_indexed;
  PFN_vkAllocateCommandBuffers allocate_commands;
  PFN_vkFreeCommandBuffers free_commands;
  PFN_vkBeginCommandBuffer begin_command;
  PFN_vkResetCommandBuffer reset_command;
  PFN_vkResetCommandPool reset_pool;
  PFN_vkDestroyCommandPool destroy_pool;
};
std::mutex dispatch_mutex;
std::unordered_map<void *, InstanceDispatch> instances;
std::unordered_map<void *, DeviceDispatch> devices;
std::unordered_map<void *, std::unordered_map<VkShaderModule, int>> modules;
struct CommandState {
  void *device{};
  VkCommandPool pool{};
  VkPipelineLayout layout{};
  bool sampled{};
  std::array<float, 16> geometry{};
};
std::unordered_map<void *, std::unordered_set<VkPipelineLayout>> layouts;
std::unordered_map<void *, std::unordered_map<VkPipeline, VkPipelineLayout>>
    camera_pipelines;
std::unordered_map<VkCommandBuffer, CommandState> commands;
unsigned draw_logs = 0;
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
      gdpa,
      (PFN_vkCreateShaderModule)gdpa(*out, "vkCreateShaderModule"),
      (PFN_vkDestroyDevice)gdpa(*out, "vkDestroyDevice"),
      (PFN_vkDestroyShaderModule)gdpa(*out, "vkDestroyShaderModule"),
      (PFN_vkCreateGraphicsPipelines)gdpa(*out, "vkCreateGraphicsPipelines"),
      (PFN_vkCreatePipelineLayout)gdpa(*out, "vkCreatePipelineLayout"),
      (PFN_vkDestroyPipelineLayout)gdpa(*out, "vkDestroyPipelineLayout"),
      (PFN_vkDestroyPipeline)gdpa(*out, "vkDestroyPipeline"),
      (PFN_vkCmdBindPipeline)gdpa(*out, "vkCmdBindPipeline"),
      (PFN_vkCmdPushConstants)gdpa(*out, "vkCmdPushConstants"),
      (PFN_vkCmdDrawIndexed)gdpa(*out, "vkCmdDrawIndexed"),
      (PFN_vkAllocateCommandBuffers)gdpa(*out, "vkAllocateCommandBuffers"),
      (PFN_vkFreeCommandBuffers)gdpa(*out, "vkFreeCommandBuffers"),
      (PFN_vkBeginCommandBuffer)gdpa(*out, "vkBeginCommandBuffer"),
      (PFN_vkResetCommandBuffer)gdpa(*out, "vkResetCommandBuffer"),
      (PFN_vkResetCommandPool)gdpa(*out, "vkResetCommandPool"),
      (PFN_vkDestroyCommandPool)gdpa(*out, "vkDestroyCommandPool")};
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
    layouts.erase(key(device));
    camera_pipelines.erase(key(device));
    for (auto it = commands.begin(); it != commands.end();)
      if (it->second.device == key(device))
        it = commands.erase(it);
      else
        ++it;
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
    std::vector<bool> camera(count, false);
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
          auto compatible = layouts.find(key(device));
          if (compatible == layouts.end() ||
              !compatible->second.count(c.layout)) {
            fprintf(stderr,
                    "ft-poc: camera pipeline has unsupported push layout\n");
            return VK_ERROR_INITIALIZATION_FAILED;
          }
          camera[i] = true;
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
    {
      std::lock_guard<std::mutex> lock(dispatch_mutex);
      for (uint32_t i = 0; i < count; i++)
        if (camera[i] && out[i] != VK_NULL_HANDLE)
          camera_pipelines[key(device)][out[i]] = ci[i].layout;
    }
    if (changed)
      fprintf(stderr, "ft-poc: source-over pipelines %u result %d\n", changed,
              int(result));
    return result;
  } catch (...) {
    return VK_ERROR_OUT_OF_HOST_MEMORY;
  }
}

std::array<float, 16> read_geometry() {
  std::array<float, 16> result{};
  const char *path = getenv("FT_POC_GEOMETRY");
  if (!path || path[0] != '/')
    return result;
  int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
  if (fd < 0)
    return result;
  struct stat st{};
  timespec now{};
  bool ok = !fstat(fd, &st) && S_ISREG(st.st_mode) && st.st_size == 64 &&
            st.st_uid == getuid() && !clock_gettime(CLOCK_REALTIME, &now);
  double age = double(now.tv_sec - st.st_mtim.tv_sec) +
               double(now.tv_nsec - st.st_mtim.tv_nsec) * 1e-9;
  ok = ok && age >= -0.1 && age <= 0.5 && read(fd, result.data(), 64) == 64;
  close(fd);
  for (float value : result)
    ok = ok && std::isfinite(value);
  if (!ok)
    result.fill(0);
  result[12] = result[12] > 0.5f ? 1.0f : 0.0f;
  return result;
}
void update_native_order(const std::array<float,16> &geometry) {
  using SetOrder = void (*)(uint32_t);
  static const auto set_order = reinterpret_cast<SetOrder>(
      dlsym(RTLD_DEFAULT,"ft_poc_set_above_menu"));
  if (set_order) set_order(geometry[12] > .5f && geometry[13] > .5f);
}
VkResult VKAPI_CALL create_layout(VkDevice device,
                                  const VkPipelineLayoutCreateInfo *ci,
                                  const VkAllocationCallbacks *a,
                                  VkPipelineLayout *out) {
  auto d = device_dispatch(device);
  if (!d.create_layout)
    return VK_ERROR_INITIALIZATION_FAILED;
  bool compatible = enabled && ci->pushConstantRangeCount == 0;
  VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT, 0, 64};
  auto copy = *ci;
  if (compatible) {
    copy.pushConstantRangeCount = 1;
    copy.pPushConstantRanges = &range;
  }
  VkResult result = d.create_layout(device, compatible ? &copy : ci, a, out);
  if (enabled)
    fprintf(stderr, "ft-poc: layout original_ranges=%u added64=%d result=%d\n",
            ci->pushConstantRangeCount, int(compatible), int(result));
  if (result == VK_SUCCESS && compatible) {
    try {
      std::lock_guard<std::mutex> lock(dispatch_mutex);
      layouts[key(device)].insert(*out);
    } catch (...) {
      d.destroy_layout(device, *out, a);
      *out = VK_NULL_HANDLE;
      return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
  }
  return result;
}
void VKAPI_CALL destroy_layout(VkDevice device, VkPipelineLayout layout,
                               const VkAllocationCallbacks *a) {
  auto d = device_dispatch(device);
  {
    std::lock_guard<std::mutex> lock(dispatch_mutex);
    layouts[key(device)].erase(layout);
    auto &known = camera_pipelines[key(device)];
    for (auto it = known.begin(); it != known.end();)
      if (it->second == layout) {
        fprintf(stderr,
                "ft-poc: camera layout destroyed while pipeline retained\n");
        it = known.erase(it);
      } else
        ++it;
    for (auto &item : commands)
      if (item.second.device == key(device) && item.second.layout == layout)
        item.second.layout = VK_NULL_HANDLE;
  }
  d.destroy_layout(device, layout, a);
}
void VKAPI_CALL destroy_pipeline(VkDevice device, VkPipeline pipeline,
                                 const VkAllocationCallbacks *a) {
  auto d = device_dispatch(device);
  {
    std::lock_guard<std::mutex> lock(dispatch_mutex);
    camera_pipelines[key(device)].erase(pipeline);
  }
  d.destroy_pipeline(device, pipeline, a);
}
VkResult VKAPI_CALL allocate_commands(VkDevice device,
                                      const VkCommandBufferAllocateInfo *ci,
                                      VkCommandBuffer *out) {
  auto d = device_dispatch(device);
  VkResult result = d.allocate_commands(device, ci, out);
  if (result == VK_SUCCESS) {
    try {
      std::lock_guard<std::mutex> lock(dispatch_mutex);
      for (uint32_t i = 0; i < ci->commandBufferCount; i++)
        commands[out[i]] = {key(device), ci->commandPool};
    } catch (...) {
      d.free_commands(device, ci->commandPool, ci->commandBufferCount, out);
      return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
  }
  return result;
}
void VKAPI_CALL free_commands(VkDevice device, VkCommandPool pool,
                              uint32_t count, const VkCommandBuffer *buffers) {
  auto d = device_dispatch(device);
  {
    std::lock_guard<std::mutex> lock(dispatch_mutex);
    for (uint32_t i = 0; i < count; i++)
      commands.erase(buffers[i]);
  }
  d.free_commands(device, pool, count, buffers);
}
void clear_command(CommandState &state) {
  state.layout = VK_NULL_HANDLE;
  state.sampled = false;
  state.geometry.fill(0);
}
VkResult VKAPI_CALL begin_command(VkCommandBuffer command,
                                  const VkCommandBufferBeginInfo *ci) {
  auto d = device_dispatch(reinterpret_cast<VkDevice>(command));
  {
    std::lock_guard<std::mutex> lock(dispatch_mutex);
    auto it = commands.find(command);
    if (it != commands.end())
      clear_command(it->second);
  }
  return d.begin_command(command, ci);
}
VkResult VKAPI_CALL reset_command(VkCommandBuffer command,
                                  VkCommandBufferResetFlags flags) {
  auto d = device_dispatch(reinterpret_cast<VkDevice>(command));
  VkResult result = d.reset_command(command, flags);
  if (result == VK_SUCCESS) {
    std::lock_guard<std::mutex> lock(dispatch_mutex);
    auto it = commands.find(command);
    if (it != commands.end())
      clear_command(it->second);
  }
  return result;
}
VkResult VKAPI_CALL reset_pool(VkDevice device, VkCommandPool pool,
                               VkCommandPoolResetFlags flags) {
  auto d = device_dispatch(device);
  VkResult result = d.reset_pool(device, pool, flags);
  if (result == VK_SUCCESS) {
    std::lock_guard<std::mutex> lock(dispatch_mutex);
    for (auto &item : commands)
      if (item.second.device == key(device) && item.second.pool == pool)
        clear_command(item.second);
  }
  return result;
}
void VKAPI_CALL destroy_pool(VkDevice device, VkCommandPool pool,
                             const VkAllocationCallbacks *a) {
  auto d = device_dispatch(device);
  {
    std::lock_guard<std::mutex> lock(dispatch_mutex);
    for (auto it = commands.begin(); it != commands.end();)
      if (it->second.device == key(device) && it->second.pool == pool)
        it = commands.erase(it);
      else
        ++it;
  }
  d.destroy_pool(device, pool, a);
}
void VKAPI_CALL bind_pipeline(VkCommandBuffer command,
                              VkPipelineBindPoint point, VkPipeline pipeline) {
  auto d = device_dispatch(reinterpret_cast<VkDevice>(command));
  if (point == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    std::lock_guard<std::mutex> lock(dispatch_mutex);
    auto state = commands.find(command);
    if (state != commands.end()) {
      state->second.layout = VK_NULL_HANDLE;
      auto known = camera_pipelines.find(key(command));
      if (known != camera_pipelines.end()) {
        auto it = known->second.find(pipeline);
        if (it != known->second.end())
          state->second.layout = it->second;
      }
    }
  }
  d.bind_pipeline(command, point, pipeline);
}
void VKAPI_CALL draw_indexed(VkCommandBuffer command, uint32_t indexCount,
                             uint32_t instanceCount, uint32_t firstIndex,
                             int32_t vertexOffset, uint32_t firstInstance) {
  auto d = device_dispatch(reinterpret_cast<VkDevice>(command));
  VkPipelineLayout layout = VK_NULL_HANDLE;
  std::array<float, 16> geometry{};
  bool log_draw = false;
  {
    std::lock_guard<std::mutex> lock(dispatch_mutex);
    auto it = commands.find(command);
    if (it != commands.end() && it->second.layout) {
      auto &state = it->second;
      layout = state.layout;
      if (!state.sampled) {
        state.geometry = read_geometry();
        update_native_order(state.geometry);
        state.sampled = true;
      }
      geometry = state.geometry;
      if (draw_logs < 4) {
        ++draw_logs;
        log_draw = true;
      }
    }
  }
  if (layout)
    d.push(command, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, geometry.data());
  if (log_draw)
    fprintf(stderr, "ft-poc: push draw enabled=%.0f center=%.3f,%.3f,%.3f\n",
            geometry[12], geometry[0], geometry[1], geometry[2]);
  d.draw_indexed(command, indexCount, instanceCount, firstIndex, vertexOffset,
                 firstInstance);
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
  if (!strcmp(n, "vkCreatePipelineLayout"))
    return (PFN_vkVoidFunction)create_layout;
  if (!strcmp(n, "vkDestroyPipelineLayout"))
    return (PFN_vkVoidFunction)destroy_layout;
  if (!strcmp(n, "vkDestroyPipeline"))
    return (PFN_vkVoidFunction)destroy_pipeline;
  if (!strcmp(n, "vkCmdBindPipeline"))
    return (PFN_vkVoidFunction)bind_pipeline;
  if (!strcmp(n, "vkCmdDrawIndexed"))
    return (PFN_vkVoidFunction)draw_indexed;
  if (!strcmp(n, "vkAllocateCommandBuffers"))
    return (PFN_vkVoidFunction)allocate_commands;
  if (!strcmp(n, "vkFreeCommandBuffers"))
    return (PFN_vkVoidFunction)free_commands;
  if (!strcmp(n, "vkBeginCommandBuffer"))
    return (PFN_vkVoidFunction)begin_command;
  if (!strcmp(n, "vkResetCommandBuffer"))
    return (PFN_vkVoidFunction)reset_command;
  if (!strcmp(n, "vkResetCommandPool"))
    return (PFN_vkVoidFunction)reset_pool;
  if (!strcmp(n, "vkDestroyCommandPool"))
    return (PFN_vkVoidFunction)destroy_pool;
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
  if (!strcmp(n, "vkCreatePipelineLayout"))
    return (PFN_vkVoidFunction)create_layout;
  if (!strcmp(n, "vkDestroyPipelineLayout"))
    return (PFN_vkVoidFunction)destroy_layout;
  if (!strcmp(n, "vkDestroyPipeline"))
    return (PFN_vkVoidFunction)destroy_pipeline;
  if (!strcmp(n, "vkCmdBindPipeline"))
    return (PFN_vkVoidFunction)bind_pipeline;
  if (!strcmp(n, "vkCmdDrawIndexed"))
    return (PFN_vkVoidFunction)draw_indexed;
  if (!strcmp(n, "vkAllocateCommandBuffers"))
    return (PFN_vkVoidFunction)allocate_commands;
  if (!strcmp(n, "vkFreeCommandBuffers"))
    return (PFN_vkVoidFunction)free_commands;
  if (!strcmp(n, "vkBeginCommandBuffer"))
    return (PFN_vkVoidFunction)begin_command;
  if (!strcmp(n, "vkResetCommandBuffer"))
    return (PFN_vkVoidFunction)reset_command;
  if (!strcmp(n, "vkResetCommandPool"))
    return (PFN_vkVoidFunction)reset_pool;
  if (!strcmp(n, "vkDestroyCommandPool"))
    return (PFN_vkVoidFunction)destroy_pool;
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
