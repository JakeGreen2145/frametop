// SPDX-License-Identifier: MIT
// Included by the generated reuse of tests/test_gpu.cpp. No layer
// implementation symbols are linked into this executable. Always use the
// explicit loader handle.
#include "ft_pipeline.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
static bool pipeline_stock_mode = false;
static PFN_vkGetInstanceProcAddr real_gipa;
static decltype(&ft_pipeline_generation) generation;
static decltype(&ft_pipeline_begin) begin_scope;
static decltype(&ft_pipeline_end) end_scope;
static decltype(&ft_pipeline_approved) approved;
static uint64_t active_generation;
static VkResult gpu_create_instance(const VkInstanceCreateInfo *ci,
                                    const VkAllocationCallbacks *a,
                                    VkInstance *out) {
  void *loader = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
  assert(loader);
  real_gipa = (PFN_vkGetInstanceProcAddr)dlsym(loader, "vkGetInstanceProcAddr");
  assert(real_gipa);
  void *layer = dlopen(std::getenv("FT_TEST_LAYER"), RTLD_NOW | RTLD_LOCAL);
  assert(layer);
  generation = (decltype(generation))dlsym(layer, "ft_pipeline_generation");
  assert(generation);
  begin_scope = (decltype(begin_scope))dlsym(layer, "ft_pipeline_begin");
  assert(begin_scope);
  end_scope = (decltype(end_scope))dlsym(layer, "ft_pipeline_end");
  assert(end_scope);
  approved = (decltype(approved))dlsym(layer, "ft_pipeline_approved");
  assert(approved);
  const char *name = "VK_LAYER_FT_scoped_portal";
  auto copy = *ci;
  copy.enabledLayerCount = 1;
  copy.ppEnabledLayerNames = &name;
  return ((PFN_vkCreateInstance)real_gipa({}, "vkCreateInstance"))(&copy, a,
                                                                   out);
}
static VkResult gpu_create_device(VkPhysicalDevice p,
                                  const VkDeviceCreateInfo *c,
                                  const VkAllocationCallbacks *a,
                                  VkDevice *out) {
  auto r = vkCreateDevice(p, c, a, out);
  if (r == VK_SUCCESS) {
    active_generation = generation(*out);
    assert(active_generation);
  }
  return r;
}
static VkResult gpu_create_pipelines(VkDevice d, VkPipelineCache cache,
                                     uint32_t n,
                                     const VkGraphicsPipelineCreateInfo *c,
                                     const VkAllocationCallbacks *a,
                                     VkPipeline *out) {
  uint64_t token = 0;
  if (!pipeline_stock_mode) {
    token = begin_scope(d, active_generation, 57, 125);
    assert(token);
  }
  auto r = vkCreateGraphicsPipelines(d, cache, n, c, a, out);
  if (r == VK_SUCCESS) {
    for (uint32_t i = 0; i < n; i++) {
      FtPipelineRecord record{};
      bool registered =
          approved(d, active_generation, out[i], 57, 125, &record);
      if (registered == pipeline_stock_mode) {
        std::fprintf(stderr, "FAIL registry stock=%d approved=%d\n",
                     pipeline_stock_mode, registered);
        std::exit(8);
      }
      if (registered) {
        assert(record.original_vertex == c[i].pStages[0].module &&
               record.original_fragment == c[i].pStages[1].module);
        assert(record.portal_vertex != record.original_vertex &&
               record.portal_fragment != record.original_fragment);
      }
    }
  }
  if (token) {
    assert(end_scope(token));
    assert(approved(d, active_generation, out[0], 57, 125, nullptr));
  }
  return r;
}
static void gpu_destroy_pipeline(VkDevice d, VkPipeline p,
                                 const VkAllocationCallbacks *a) {
  vkDestroyPipeline(d, p, a);
  assert(!approved(d, active_generation, p, 57, 125, nullptr));
}
#define vkCreateInstance gpu_create_instance
#define vkCreateDevice gpu_create_device
#define vkCreateGraphicsPipelines gpu_create_pipelines
#define vkDestroyPipeline gpu_destroy_pipeline
