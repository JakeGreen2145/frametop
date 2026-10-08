// SPDX-License-Identifier: MIT
// Actual exported layer entrypoints, fake NEXT Vulkan boundary. No private test
// hooks.
#include "bundle.hpp"
#include "ft_pipeline.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>
#include <vulkan/vk_layer.h>
extern "C" PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance, const char *);
extern "C" PFN_vkVoidFunction vkGetDeviceProcAddr(VkDevice, const char *);
namespace {
uintptr_t ikey = 1, dkey = 2, dkey2 = 3;
VkInstance instance = reinterpret_cast<VkInstance>(&ikey);
VkDevice device = reinterpret_cast<VkDevice>(&dkey),
         other = reinterpret_cast<VkDevice>(&dkey2);
uint64_t handle = 100;
unsigned calls = 0, destroyed = 0, shader_calls = 0;
VkResult driver_result = VK_SUCCESS;
bool reenter = false, lose = false, create_shader_fail = false,
     null_success = false, destroy_null = false;
const VkShaderModuleCreateInfo *expected_shader = nullptr;
std::vector<VkGraphicsPipelineCreateInfo> seen;
std::vector<VkPipelineShaderStageCreateInfo> seen_stages;
PFN_vkCreateGraphicsPipelines create;
PFN_vkDestroyPipeline destroy;
PFN_vkCreateShaderModule shader;
PFN_vkDestroyShaderModule destroy_shader;
VkResult VKAPI_CALL fake_instance(const VkInstanceCreateInfo *,
                                  const VkAllocationCallbacks *,
                                  VkInstance *out) {
  *out = instance;
  return VK_SUCCESS;
}
VkResult VKAPI_CALL fake_device(VkPhysicalDevice, const VkDeviceCreateInfo *,
                                const VkAllocationCallbacks *, VkDevice *out) {
  *out = device;
  return VK_SUCCESS;
}
void VKAPI_CALL fake_destroy_device(VkDevice, const VkAllocationCallbacks *) {}
void VKAPI_CALL fake_destroy_instance(VkInstance,
                                      const VkAllocationCallbacks *) {}
VkResult VKAPI_CALL fake_shader(VkDevice, const VkShaderModuleCreateInfo *ci,
                                const VkAllocationCallbacks *,
                                VkShaderModule *out) {
  ++shader_calls;
  if (expected_shader)
    assert(ci == expected_shader);
  assert(ci->pCode && ci->codeSize);
  if (create_shader_fail) {
    *out = (VkShaderModule)UINT64_MAX; // Never an observed successful creation.
    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
  }
  *out = (VkShaderModule)++handle;
  return VK_SUCCESS;
}
void VKAPI_CALL fake_destroy_shader(VkDevice, VkShaderModule m,
                                    const VkAllocationCallbacks *) { assert(m != (VkShaderModule)UINT64_MAX); }
void VKAPI_CALL fake_destroy_pipeline(VkDevice, VkPipeline,
                                      const VkAllocationCallbacks *) {
  ++destroyed;
}
VkResult VKAPI_CALL fake_submit(VkQueue, uint32_t, const VkSubmitInfo *,
                                VkFence) {
  return lose ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
}
VkResult VKAPI_CALL fake_wait_device(VkDevice) {
  return lose ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
}
VkResult VKAPI_CALL fake_wait_queue(VkQueue) {
  return lose ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
}
VkResult VKAPI_CALL fake_wait_fences(VkDevice, uint32_t, const VkFence *,
                                     VkBool32, uint64_t) {
  return lose ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
}
VkResult VKAPI_CALL fake_wait_semaphores(VkDevice, const VkSemaphoreWaitInfo *,
                                         uint64_t) {
  return lose ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
}
VkResult VKAPI_CALL fake_submit2(VkQueue, uint32_t, const VkSubmitInfo2 *,
                                 VkFence) {
  return lose ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
}
VkResult VKAPI_CALL fake_create(VkDevice d, VkPipelineCache, uint32_t n,
                                const VkGraphicsPipelineCreateInfo *ci,
                                const VkAllocationCallbacks *,
                                VkPipeline *out) {
  ++calls;
  seen.assign(ci, ci + n);
  seen_stages.clear();
  for (uint32_t i = 0; i < n; i++)
    for (uint32_t j = 0; j < ci[i].stageCount; j++)
      seen_stages.push_back(ci[i].pStages[j]);
  if (destroy_null) destroy(d, VK_NULL_HANDLE, nullptr);
  if (reenter) {
    reenter = false;
    VkPipeline p = (VkPipeline)42;
    assert(create(d, {}, 1, ci, nullptr, &p) != VK_SUCCESS);
    assert(!p);
  }
  for (uint32_t i = 0; i < n; i++)
    out[i] = (VkPipeline)++handle;
  if (null_success)
    out[0] = {};
  return lose ? VK_ERROR_DEVICE_LOST : driver_result;
}
PFN_vkVoidFunction VKAPI_CALL gdpa(VkDevice, const char *n) {
#define MAP(s, f)                                                              \
  if (!strcmp(n, s))                                                           \
  return reinterpret_cast<PFN_vkVoidFunction>(f)
  MAP("vkCreateShaderModule", fake_shader);
  MAP("vkDestroyShaderModule", fake_destroy_shader);
  MAP("vkCreateGraphicsPipelines", fake_create);
  MAP("vkDestroyPipeline", fake_destroy_pipeline);
  MAP("vkDestroyDevice", fake_destroy_device);
  MAP("vkQueueSubmit", fake_submit);
  MAP("vkQueueWaitIdle", fake_wait_queue);
  MAP("vkDeviceWaitIdle", fake_wait_device);
  MAP("vkWaitForFences", fake_wait_fences);
  MAP("vkWaitSemaphores", fake_wait_semaphores);
  MAP("vkWaitSemaphoresKHR", fake_wait_semaphores);
  MAP("vkQueueSubmit2", fake_submit2);
  MAP("vkQueueSubmit2KHR", fake_submit2);
  return nullptr;
}
PFN_vkVoidFunction VKAPI_CALL gipa(VkInstance, const char *n) {
  MAP("vkCreateInstance", fake_instance);
  MAP("vkDestroyInstance", fake_destroy_instance);
  MAP("vkCreateDevice", fake_device);
  return gdpa({}, n);
}
void setup() {
  VkLayerInstanceLink il{};
  il.pfnNextGetInstanceProcAddr = gipa;
  VkLayerInstanceCreateInfo li{};
  li.sType = VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO;
  li.function = VK_LAYER_LINK_INFO;
  li.u.pLayerInfo = &il;
  VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ici.pNext = &li;
  auto ci = (PFN_vkCreateInstance)vkGetInstanceProcAddr({}, "vkCreateInstance");
  assert(ci);
  assert(ci(&ici, nullptr, &instance) == VK_SUCCESS);
  VkLayerDeviceLink dl{};
  dl.pfnNextGetInstanceProcAddr = gipa;
  dl.pfnNextGetDeviceProcAddr = gdpa;
  VkLayerDeviceCreateInfo ld{};
  ld.sType = VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO;
  ld.function = VK_LAYER_LINK_INFO;
  ld.u.pLayerInfo = &dl;
  VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  dci.pNext = &ld;
  auto cd =
      (PFN_vkCreateDevice)vkGetInstanceProcAddr(instance, "vkCreateDevice");
  assert(cd);
  assert(cd((VkPhysicalDevice)&ikey, &dci, nullptr, &device) == VK_SUCCESS);
  create = (PFN_vkCreateGraphicsPipelines)vkGetDeviceProcAddr(
      device, "vkCreateGraphicsPipelines");
  assert(create);
  destroy =
      (PFN_vkDestroyPipeline)vkGetDeviceProcAddr(device, "vkDestroyPipeline");
  shader = (PFN_vkCreateShaderModule)vkGetDeviceProcAddr(
      device, "vkCreateShaderModule");
  destroy_shader = (PFN_vkDestroyShaderModule)vkGetDeviceProcAddr(
      device, "vkDestroyShaderModule");
}
VkShaderModule original(unsigned id) {
  for (auto &b : ft_bundle)
    if (b.id == id) {
      VkShaderModuleCreateInfo c{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
      c.codeSize = b.original_size;
      c.pCode = b.original;
      VkShaderModule m;
      expected_shader = &c;
      assert(shader(device, &c, nullptr, &m) == VK_SUCCESS);
      expected_shader = nullptr;
      return m;
    }
  assert(false);
  return {};
}
} // namespace
int main() {
  setup();
  auto gen = ft_pipeline_generation(device);
  assert(gen);
  auto v = original(57), f = original(125);
  VkPipelineShaderStageCreateInfo stages[2]{};
  for (auto &s : stages) {
    s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    s.pName = ft_bundle[0].entry;
  }
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = v;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = f;
  for (auto &b : ft_bundle)
    if (b.id == 125)
      stages[1].pName = b.entry;
  VkGraphicsPipelineCreateInfo c[3]{};
  for (auto &i : c) {
    i.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    i.stageCount = 2;
    i.pStages = stages;
    i.layout = (VkPipelineLayout)701;
    i.renderPass = (VkRenderPass)702;
    i.basePipelineIndex = -1;
  }
  c[0].flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
  c[1].flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
  c[1].basePipelineIndex = 0;
  auto stock_stages =
      std::vector<VkPipelineShaderStageCreateInfo>(stages, stages + 2);
  stock_stages[0].module = (VkShaderModule)9991;
  stock_stages[1].module = (VkShaderModule)9992;
  c[2].pStages = stock_stages.data();
  VkPipeline p[3]{};
  auto saved = std::vector<VkGraphicsPipelineCreateInfo>(c, c + 3);
  auto saved_stages =
      std::vector<VkPipelineShaderStageCreateInfo>(stages, stages + 2);
  assert(create(device, {}, 3, c, nullptr, p) == VK_SUCCESS);
  assert(seen[0].pStages == stages);
  assert(!ft_pipeline_approved(device, gen, p[0], 57, 125, nullptr));
  unsigned before = calls;
  uint64_t token = ft_pipeline_begin(device, gen, 57, 125);
  assert(token);
  assert(create(device, {}, 3, c, nullptr, p) == VK_SUCCESS);
  assert(calls == before + 1);
  assert(seen[1].basePipelineIndex == 0 && seen[1].flags == c[1].flags);
  assert(seen[2].pStages == stock_stages.data());
  assert(seen_stages[0].module != v && seen_stages[1].module != f);
  auto expected = saved[0];
  expected.pStages = seen[0].pStages;
  assert(!memcmp(&expected, &seen[0], sizeof(expected)));
  assert(!memcmp(c, saved.data(), sizeof(c)));
  assert(!memcmp(stages, saved_stages.data(), sizeof(stages)));
  FtPipelineRecord rec{};
  assert(ft_pipeline_approved(device, gen, p[0], 57, 125, &rec));
  assert(rec.abi == 1 && rec.original_vertex == v &&
         rec.original_fragment == f &&
         rec.portal_vertex == seen_stages[0].module &&
         rec.portal_fragment == seen_stages[1].module);
  assert(!ft_pipeline_approved(device, gen, p[2], 57, 125, nullptr));
  assert(ft_pipeline_end(token));
  assert(ft_pipeline_approved(device, gen, p[0], 57, 125,
                              nullptr)); // cache hit, no TLS required
  assert(!ft_pipeline_approved(device, gen + 1, p[0], 57, 125, nullptr));
  assert(!ft_pipeline_approved(other, gen, p[0], 57, 125, nullptr));
  destroy(device, p[0], nullptr);
  assert(!ft_pipeline_approved(device, gen, p[0], 57, 125, nullptr));
  // Every generated pinned pair recognizes both original byte arrays and exact
  // names.
  unsigned recognized = 0;
  for (auto &pair : ft_pairs) {
    auto vm = original(pair[0]), fm = original(pair[1]);
    stages[0].module = vm;
    stages[1].module = fm;
    for (auto &b : ft_bundle) {
      if (b.id == pair[0])
        stages[0].pName = b.entry;
      if (b.id == pair[1])
        stages[1].pName = b.entry;
    }
    auto t = ft_pipeline_begin(device, gen, pair[0], pair[1]);
    assert(t);
    assert(create(device, {}, 1, c, nullptr, p) == VK_SUCCESS);
    assert(ft_pipeline_approved(device, gen, p[0], pair[0], pair[1], nullptr));
    assert(ft_pipeline_end(t));
    destroy(device, p[0], nullptr);
    for (auto &b : ft_bundle)
      if (b.id == pair[0]) {
        std::vector<uint32_t> damaged(b.original,
                                      b.original + b.original_size / 4);
        damaged.back() ^= 1;
        VkShaderModuleCreateInfo sm{
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        sm.codeSize = b.original_size;
        sm.pCode = damaged.data();
        VkShaderModule wrong;
        assert(shader(device, &sm, nullptr, &wrong) == VK_SUCCESS);
        stages[0].module = wrong;
        t = ft_pipeline_begin(device, gen, pair[0], pair[1]);
        assert(t);
        auto old = calls;
        assert(create(device, {}, 1, c, nullptr, p) != VK_SUCCESS && !p[0] &&
               calls == old);
        assert(!ft_pipeline_end(t));
        destroy_shader(device, wrong, nullptr);
      }
    destroy_shader(device, vm, nullptr);
    destroy_shader(device, fm, nullptr);
    ++recognized;
  }
  std::memcpy(stages, saved_stages.data(), sizeof(stages));
  std::printf("PASS exact original bytes and entrypoints for %u generated "
              "pairs; one-bit variants refused\n",
              recognized);
  // Every owned refusal must clear outputs, make no next call, and never
  // approve stock.
  auto reject = [&]() {
    auto t = ft_pipeline_begin(device, gen, 57, 125);
    assert(t);
    auto old = calls;
    p[0] = (VkPipeline)42;
    assert(create(device, {}, 1, c, nullptr, p) != VK_SUCCESS);
    assert(!p[0] && calls == old);
    ft_pipeline_end(t);
  };
  VkBaseInStructure unknown{VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr};
  VkPipelineRasterizationStateCreateInfo raster{
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  raster.pNext = &unknown;
  c[0].pRasterizationState = &raster;
  reject();
  c[0].pRasterizationState = nullptr;
  c[0].pNext = &unknown;
  reject();
  c[0].pNext = nullptr;
  stages[0].pNext = &unknown;
  reject();
  stages[0].pNext = nullptr;
  auto name = stages[0].pName;
  stages[0].pName = "wrong";
  reject();
  stages[0].pName = name;
  stages[0].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
  reject();
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[1].module = (VkShaderModule)9992;
  reject();
  stages[1].module = f;
  c[0].stageCount = 1;
  reject();
  c[0].stageCount = 2;
  VkSpecializationInfo special{};
  stages[1].pSpecializationInfo = &special;
  reject();
  stages[1].pSpecializationInfo = nullptr;
  token = ft_pipeline_begin(device, gen, 57, 125);
  assert(token);
  assert(create(other, {}, 1, c, nullptr, p) != VK_SUCCESS && !p[0]);
  assert(!ft_pipeline_end(token));
  assert(!ft_pipeline_begin(device, gen + 1, 57, 125));
  assert(!ft_pipeline_begin(device, gen, 57, 130));
  token = ft_pipeline_begin(device, gen, 57, 125);
  assert(token);
  assert(!ft_pipeline_begin(device, gen, 57, 125));
  before = calls;
  assert(create(device, {}, 1, c, nullptr, p) != VK_SUCCESS && !p[0] &&
         calls == before);
  assert(!ft_pipeline_end(token));
  token = ft_pipeline_begin(device, gen, 57, 125);
  assert(token);
  assert(!ft_pipeline_end(token + 1));
  assert(create(device, {}, 1, c, nullptr, p) != VK_SUCCESS);
  assert(!ft_pipeline_end(token));
  token = ft_pipeline_begin(device, gen, 57, 125);
  assert(token);
  bool cross = true;
  std::thread th([&] { cross = ft_pipeline_end(token); });
  th.join();
  assert(!cross);
  assert(ft_pipeline_end(token));
  token = ft_pipeline_begin(device, gen, 57, 125);
  assert(token);
  driver_result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
  before = destroyed;
  assert(create(device, {}, 3, c, nullptr, p) == driver_result);
  assert(!p[0] && !p[1] && !p[2]);
  assert(destroyed == before + 3);
  assert(!ft_pipeline_end(token));
  driver_result = VK_SUCCESS;
  token = ft_pipeline_begin(device, gen, 57, 125);
  assert(token);
  null_success = true;
  assert(create(device, {}, 1, c, nullptr, p) != VK_SUCCESS && !p[0]);
  assert(!ft_pipeline_end(token));
  null_success = false;
  token = ft_pipeline_begin(device, gen, 57, 125);
  assert(token);
  reenter = true;
  assert(create(device, {}, 1, c, nullptr, p) != VK_SUCCESS && !p[0]);
  assert(!ft_pipeline_end(token));
  // Vulkan permits destroying NULL; it cannot erase in-flight reservations.
  token = ft_pipeline_begin(device, gen, 57, 125);
  assert(token); destroy_null = true;
  assert(create(device, {}, 1, c, nullptr, p) == VK_SUCCESS);
  assert(ft_pipeline_approved(device, gen, p[0], 57, 125, nullptr));
  assert(ft_pipeline_end(token)); destroy_null = false;
  destroy(device, p[0], nullptr);
  // Registry admission is bounded and never allocates after the downstream call.
  std::vector<VkPipeline> held;
  for (unsigned i = 0; i < 5000; i++) {
    auto t = ft_pipeline_begin(device, gen, 57, 125);
    assert(t);
    auto old = calls;
    auto r = create(device, {}, 1, c, nullptr, p);
    if (r != VK_SUCCESS) {
      assert(r == VK_ERROR_OUT_OF_HOST_MEMORY && !p[0] && old == calls);
      assert(!ft_pipeline_end(t));
      break;
    }
    assert(ft_pipeline_end(t));
    held.push_back(p[0]);
  }
  assert(held.size() > 4000 && held.size() < 4096);
  for (auto h : held)
    destroy(device, h, nullptr);
  std::puts("PASS bounded registry exhaustion rejects before forwarding; "
            "destroy reclaims slots");
  // Shader destruction removes recognition, not already created pipeline
  // approval.
  token = ft_pipeline_begin(device, gen, 57, 125);
  assert(token);
  assert(create(device, {}, 1, c, nullptr, p) == VK_SUCCESS);
  assert(ft_pipeline_end(token));
  destroy_shader(device, v, nullptr);
  assert(ft_pipeline_approved(device, gen, p[0], 57, 125, nullptr));
  reject();
  ft_pipeline_revoke(device, gen);
  assert(!ft_pipeline_generation(device));
  assert(!ft_pipeline_approved(device, gen, p[0], 57, 125, nullptr));
  assert(!ft_pipeline_begin(device, gen, 57, 125));
  // Stock remains unchanged after revocation.
  assert(create(device, {}, 3, c, nullptr, p) == VK_SUCCESS &&
         seen[0].pStages == stages);
  auto dd = (PFN_vkDestroyDevice)vkGetDeviceProcAddr(device, "vkDestroyDevice");
  dd(device, nullptr);
  setup();
  auto gen2 = ft_pipeline_generation(device);
  assert(gen2 && gen2 != gen);
  v = original(57);
  f = original(125);
  stages[0].module = v;
  stages[1].module = f;
  token = ft_pipeline_begin(device, gen2, 57, 125);
  assert(token);
  lose = true;
  assert(create(device, {}, 1, c, nullptr, p) == VK_ERROR_DEVICE_LOST);
  assert(!p[0] && !ft_pipeline_generation(device));
  assert(!ft_pipeline_end(token));
  lose = false;
  dd(device, nullptr);
  for (unsigned kind = 0; kind < 8; kind++) {
    setup();
    auto g = ft_pipeline_generation(device);
    assert(g);
    v = original(57);
    f = original(125);
    stages[0].module = v;
    stages[1].module = f;
    token = ft_pipeline_begin(device, g, 57, 125);
    assert(token);
    assert(create(device, {}, 1, c, nullptr, p) == VK_SUCCESS);
    assert(ft_pipeline_end(token));
    auto queue = reinterpret_cast<VkQueue>(&dkey);
    VkResult r = VK_SUCCESS;
    lose = true;
    if (kind == 0)
      r = ((PFN_vkQueueSubmit)vkGetDeviceProcAddr(device, "vkQueueSubmit"))(
          queue, 0, nullptr, {});
    if (kind == 1)
      r = ((PFN_vkQueueWaitIdle)vkGetDeviceProcAddr(device, "vkQueueWaitIdle"))(
          queue);
    if (kind == 2)
      r = ((PFN_vkDeviceWaitIdle)vkGetDeviceProcAddr(
          device, "vkDeviceWaitIdle"))(device);
    if (kind == 3)
      r = ((PFN_vkWaitForFences)vkGetDeviceProcAddr(device, "vkWaitForFences"))(
          device, 0, nullptr, 0, 0);
    if (kind == 4 || kind == 5)
      r = ((PFN_vkWaitSemaphores)vkGetDeviceProcAddr(
          device, kind == 4 ? "vkWaitSemaphores" : "vkWaitSemaphoresKHR"))(
          device, nullptr, 0);
    if (kind == 6 || kind == 7)
      r = ((PFN_vkQueueSubmit2)vkGetDeviceProcAddr(
          device, kind == 6 ? "vkQueueSubmit2" : "vkQueueSubmit2KHR"))(
          queue, 0, nullptr, {});
    assert(r == VK_ERROR_DEVICE_LOST);
    assert(!ft_pipeline_generation(device));
    assert(!ft_pipeline_approved(device, g, p[0], 57, 125, nullptr));
    lose = false;
    dd(device, nullptr);
  }
  std::puts("PASS device loss revocation from all intercepted submit/wait "
            "entrypoints");
  create_shader_fail = true;
  setup();
  assert(!ft_pipeline_generation(device));
  assert(!ft_pipeline_begin(device, gen2, 57, 125));
  dd(device, nullptr);
  std::puts(
      "PASS actual layer entrypoints: stock preservation, paired substitution, "
      "mixed batch derivatives, cache registry, fail-closed errors, tokens, "
      "reentry, destruction, loss, generation");
}
