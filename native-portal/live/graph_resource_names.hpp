// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace native_portal::live {
// Pinned STATIC-FACTS selected supported camera shader family. These constants
// are input validation, not ownership or proof of an arbitrary provider.
struct GraphResourceName {uint64_t pointer,string;const char* text;};
inline constexpr GraphResourceName graph_resource_names[]={
    {0x59b210,0x47c690,"shaders/vulkan/tracked_camera_reprojection_simplified_vs.spv"},
    {0x59b230,0x47c6d0,"shaders/vulkan/tracked_camera_reprojection_simplified_sharpen_vs.spv"},
    {0x59b3d0,0x457600,""},
    {0x59b3e0,0x47b4f8,"shaders/vulkan/distort_hs.spv"},
    {0x59b3f0,0x47b520,"shaders/vulkan/distort_grid_hs.spv"},
    {0x59b400,0x47b548,"shaders/vulkan/distort_ptnorm_hs.spv"},
    {0x59b410,0x457600,""},
    {0x59b420,0x47b480,"shaders/vulkan/distort_ds.spv"},
    {0x59b430,0x47b4a8,"shaders/vulkan/distort_grid_ds.spv"},
    {0x59b440,0x47b4d0,"shaders/vulkan/distort_ptnorm_ds.spv"},
    {0x59a8e0,0x47ab38,"shaders/vulkan/tracked_camera_reprojection_simplified_monochrome_ps.spv"},
    {0x59a8f0,0x47ab80,"shaders/vulkan/tracked_camera_reprojection_simplified_monochrome_nv12_ps.spv"},
    {0x59a900,0x47abd0,"shaders/vulkan/tracked_camera_reprojection_simplified_rgb_nv12_ps.spv"},
    {0x59a910,0x47ac18,"shaders/vulkan/tracked_camera_reprojection_simplified_sharpen_monochrome_ps.spv"},
    {0x59a920,0x47ac68,"shaders/vulkan/tracked_camera_reprojection_simplified_sharpen_monochrome_nv12_ps.spv"},
    {0x59a930,0x47acc0,"shaders/vulkan/tracked_camera_reprojection_simplified_sharpen_rgb_nv12_ps.spv"},
    {0,0x457600,""},
};
}
