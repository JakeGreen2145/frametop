// SPDX-License-Identifier: MIT
// Transparent test layer above or below the production DSO. No WSI or VR.
#include <vulkan/vk_layer.h>
#include <cstring>

namespace {
PFN_vkGetInstanceProcAddr next_instance;
PFN_vkGetDeviceProcAddr next_device;
PFN_GetPhysicalDeviceProcAddr next_physical;
PFN_vkGetPhysicalDeviceProperties next_properties;
PFN_vkCreateDevice next_create_device;
PFN_vkDeviceWaitIdle next_wait;
unsigned physical_calls, wait_calls;

PFN_vkVoidFunction VKAPI_CALL instance_proc(VkInstance, const char *);
PFN_vkVoidFunction VKAPI_CALL device_proc(VkDevice, const char *);
VkResult VKAPI_CALL create_instance(const VkInstanceCreateInfo *info,
                                    const VkAllocationCallbacks *alloc, VkInstance *out) {
    auto *chain = (VkLayerInstanceCreateInfo *)info->pNext;
    while (chain && (chain->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO ||
                    chain->function != VK_LAYER_LINK_INFO))
        chain = (VkLayerInstanceCreateInfo *)chain->pNext;
    if (!chain || !chain->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
    auto *link = chain->u.pLayerInfo;
    next_instance = link->pfnNextGetInstanceProcAddr;
    next_physical = link->pfnNextGetPhysicalDeviceProcAddr;
    chain->u.pLayerInfo = link->pNext;
    return ((PFN_vkCreateInstance)next_instance({}, "vkCreateInstance"))(info, alloc, out);
}
VkResult VKAPI_CALL create_device(VkPhysicalDevice physical, const VkDeviceCreateInfo *info,
                                  const VkAllocationCallbacks *alloc, VkDevice *out) {
    auto *chain = (VkLayerDeviceCreateInfo *)info->pNext;
    while (chain && (chain->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO ||
                    chain->function != VK_LAYER_LINK_INFO))
        chain = (VkLayerDeviceCreateInfo *)chain->pNext;
    if (!chain || !chain->u.pLayerInfo) return VK_ERROR_INITIALIZATION_FAILED;
    auto *link = chain->u.pLayerInfo;
    next_device = link->pfnNextGetDeviceProcAddr;
    chain->u.pLayerInfo = link->pNext;
    auto result = next_create_device(physical, info, alloc, out);
    if (result == VK_SUCCESS)
        next_wait = (PFN_vkDeviceWaitIdle)next_device(*out, "vkDeviceWaitIdle");
    return result;
}
VkResult VKAPI_CALL tracked_instance(const VkInstanceCreateInfo *info,
                                     const VkAllocationCallbacks *alloc, VkInstance *out) {
    auto result = create_instance(info, alloc, out);
    // Cache dispatch while the downstream chain is being constructed.
    if (result == VK_SUCCESS) {
        next_properties = (PFN_vkGetPhysicalDeviceProperties)next_instance(*out, "vkGetPhysicalDeviceProperties");
        next_create_device = (PFN_vkCreateDevice)next_instance(*out, "vkCreateDevice");
    }
    return result;
}
void VKAPI_CALL properties(VkPhysicalDevice physical, VkPhysicalDeviceProperties *out) {
    ++physical_calls;
    next_properties(physical, out);
}
VkResult VKAPI_CALL wait_idle(VkDevice device) {
    ++wait_calls;
    return next_wait(device);
}
#define MAP(name, func) if (!std::strcmp(n, name)) return (PFN_vkVoidFunction)func
PFN_vkVoidFunction VKAPI_CALL device_proc(VkDevice device, const char *n) {
    MAP("vkGetDeviceProcAddr", device_proc);
    MAP("vkDeviceWaitIdle", wait_idle);
    return next_device ? next_device(device, n) : nullptr;
}
PFN_vkVoidFunction VKAPI_CALL instance_proc(VkInstance inst, const char *n) {
    MAP("vkGetInstanceProcAddr", instance_proc);
    MAP("vkGetDeviceProcAddr", device_proc);
    MAP("vkCreateInstance", tracked_instance);
    MAP("vkCreateDevice", create_device);
    MAP("vkGetPhysicalDeviceProperties", properties);
    MAP("vkDeviceWaitIdle", wait_idle);
    return inst && next_instance ? next_instance(inst, n) : nullptr;
}
PFN_vkVoidFunction VKAPI_CALL physical_proc(VkInstance inst, const char *n) {
    MAP("vkGetPhysicalDeviceProperties", properties);
    return next_physical ? next_physical(inst, n) : nullptr;
}
}
extern "C" __attribute__((visibility("default"))) unsigned ft_test_stack_count(unsigned which) {
    return which ? wait_calls : physical_calls;
}
extern "C" __attribute__((visibility("default"))) VkResult VKAPI_CALL
vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface *v) {
    if (!v || v->loaderLayerInterfaceVersion < 2) return VK_ERROR_INITIALIZATION_FAILED;
    v->loaderLayerInterfaceVersion = 2;
    v->pfnGetInstanceProcAddr = instance_proc;
    v->pfnGetDeviceProcAddr = device_proc;
    v->pfnGetPhysicalDeviceProcAddr = physical_proc;
    return VK_SUCCESS;
}
