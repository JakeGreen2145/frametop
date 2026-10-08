// SPDX-License-Identifier: MIT
#ifndef FT_PORTAL_PIPELINE_H
#define FT_PORTAL_PIPELINE_H
#include <stdint.h>
#include <vulkan/vulkan.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FT_PIPELINE_ABI 1u
#define FT_PIPELINE_EXPORT __attribute__((visibility("default")))
typedef struct FtPipelineRecord {
  uint32_t abi;
  uint32_t vertex_id;
  uint32_t fragment_id;
  uint32_t reserved;
  uint64_t generation;
  VkPipeline pipeline;
  VkShaderModule original_vertex;
  VkShaderModule original_fragment;
  VkShaderModule portal_vertex;
  VkShaderModule portal_fragment;
} FtPipelineRecord;
// Trusted in-process callers only. Zero means unavailable/refused.
FT_PIPELINE_EXPORT uint64_t ft_pipeline_generation(VkDevice device);
FT_PIPELINE_EXPORT uint64_t ft_pipeline_begin(VkDevice device,
                                              uint64_t generation,
                                              uint32_t vertex_id,
                                              uint32_t fragment_id);
// Matching thread/token required. A mismatch poisons, but does not clear, the
// scope.
FT_PIPELINE_EXPORT int ft_pipeline_end(uint64_t token);
FT_PIPELINE_EXPORT int
ft_pipeline_approved(VkDevice device, uint64_t generation, VkPipeline pipeline,
                     uint32_t vertex_id, uint32_t fragment_id,
                     FtPipelineRecord *out);
// Permanently revoke this device generation; normal device recreation is
// required.
FT_PIPELINE_EXPORT void ft_pipeline_revoke(VkDevice device,
                                           uint64_t generation);
#ifdef __cplusplus
}
namespace ft::pipeline {
class Scope {
  uint64_t token_;

public:
  Scope(VkDevice d, uint64_t g, uint32_t v, uint32_t f)
      : token_(ft_pipeline_begin(d, g, v, f)) {}
  ~Scope() {
    if (token_)
      ft_pipeline_end(token_);
  }
  explicit operator bool() const { return token_ != 0; }
  Scope(const Scope &) = delete;
  Scope &operator=(const Scope &) = delete;
};
} // namespace ft::pipeline
#endif
#endif
