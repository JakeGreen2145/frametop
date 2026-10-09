// SPDX-License-Identifier: MIT
// Application linked to the real loader, not layer.cpp. Runs without WSI/VR.
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#define CHECK(expr) do { if (!(expr)) { std::fprintf(stderr, "FAIL %s\n", #expr); return 1; } } while (0)
int main(int argc, char **argv) {
    CHECK(argc == 3);
    std::setbuf(stdout, nullptr);
    void *loader = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_GLOBAL);
    CHECK(loader);
#ifdef FT_TEST_DYNAMIC_LOADER
    // Native clients can load Vulkan after the preloaded owner dependency.
    // Resolve application entrypoints from global scope, not the layer handle.
#define LOAD(name) auto name = (PFN_##name)dlsym(RTLD_DEFAULT, #name); CHECK(name)
    LOAD(vkGetInstanceProcAddr);
    LOAD(vkGetDeviceProcAddr);
    LOAD(vkCreateInstance);
    LOAD(vkGetPhysicalDeviceQueueFamilyProperties);
#undef LOAD
#endif
    if (!std::strcmp(argv[1], "symbols")) {
        bool matches = true;
        for (const char *name : {"vkGetInstanceProcAddr", "vkGetDeviceProcAddr", "vkCreateInstance"}) {
            void *global = dlsym(RTLD_DEFAULT, name);
            void *expected = dlsym(loader, name);
            Dl_info info{};
            CHECK(global && expected && dladdr(global, &info));
            std::printf("%s -> %s\n", name, info.dli_fname);
            matches &= global == expected;
        }
        CHECK(matches);
        return 0;
    }
    bool device_only = !std::strcmp(argv[1], "device");
    bool direct = !std::strcmp(argv[1], "stack") || device_only;
    // Isolate GDPA bypass from the independent GIPA/handle-unwrapping failure.
    auto gipa = device_only ? (PFN_vkGetInstanceProcAddr)dlsym(loader, "vkGetInstanceProcAddr")
                            : vkGetInstanceProcAddr;
    const char *layers[] = {"VK_LAYER_FT_test_stack", "VK_LAYER_FT_scoped_portal"};
    if (!std::strcmp(argv[2], "below")) {
        layers[0] = "VK_LAYER_FT_scoped_portal";
        layers[1] = "VK_LAYER_FT_test_stack";
    }
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "ft-loader-preload-regression";
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    ci.enabledLayerCount = 2;
    ci.ppEnabledLayerNames = layers;
    auto create = direct ? vkCreateInstance : (PFN_vkCreateInstance)gipa({}, "vkCreateInstance");
    CHECK(create);
    VkInstance instance{};
    CHECK(create(&ci, nullptr, &instance) == VK_SUCCESS);
    auto enumerate = (PFN_vkEnumeratePhysicalDevices)gipa(instance, "vkEnumeratePhysicalDevices");
    CHECK(enumerate);
    uint32_t count = 0;
    CHECK(enumerate(instance, &count, nullptr) == VK_SUCCESS && count);
    std::vector<VkPhysicalDevice> physical(count);
    CHECK(enumerate(instance, &count, physical.data()) == VK_SUCCESS);
    void *stack = dlopen(std::getenv("FT_TEST_STACK"), RTLD_NOW | RTLD_NOLOAD);
    CHECK(stack);
    auto calls = (unsigned (*)(unsigned))dlsym(stack, "ft_test_stack_count");
    CHECK(calls);
    unsigned before = calls(0);
    auto properties = (PFN_vkGetPhysicalDeviceProperties)gipa(instance, "vkGetPhysicalDeviceProperties");
    CHECK(properties);
    VkPhysicalDeviceProperties props{};
    properties(physical[0], &props);
    std::printf("GPU=%s physical chain delta=%u\n", props.deviceName, calls(0) - before);
    CHECK(calls(0) == before + 1);
    uint32_t queues = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical[0], &queues, nullptr);
    std::vector<VkQueueFamilyProperties> families(queues);
    vkGetPhysicalDeviceQueueFamilyProperties(physical[0], &queues, families.data());
    uint32_t family = 0;
    while (family < queues && !families[family].queueCount) ++family;
    CHECK(family < queues);
    float priority = 1;
    VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qi.queueFamilyIndex = family; qi.queueCount = 1; qi.pQueuePriorities = &priority;
    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    di.queueCreateInfoCount = 1; di.pQueueCreateInfos = &qi;
    auto create_device = (PFN_vkCreateDevice)gipa(instance, "vkCreateDevice");
    CHECK(create_device);
    VkDevice device{};
    CHECK(create_device(physical[0], &di, nullptr, &device) == VK_SUCCESS);
    void *pipeline = dlopen(std::getenv("FT_TEST_LAYER"), RTLD_NOW | RTLD_NOLOAD);
    CHECK(pipeline);
    auto generation = (uint64_t (*)(VkDevice))dlsym(pipeline, "ft_pipeline_generation");
    CHECK(generation && generation(device));
    auto wait = (PFN_vkDeviceWaitIdle)vkGetDeviceProcAddr(device, "vkDeviceWaitIdle");
    CHECK(wait);
    before = calls(1);
    CHECK(wait(device) == VK_SUCCESS);
    std::printf("device chain delta=%u\n", calls(1) - before);
    CHECK(calls(1) == before + 1);
    auto destroy_device = (PFN_vkDestroyDevice)vkGetDeviceProcAddr(device, "vkDestroyDevice");
    auto destroy_instance = (PFN_vkDestroyInstance)gipa(instance, "vkDestroyInstance");
    CHECK(destroy_device && destroy_instance);
    destroy_device(device, nullptr);
    destroy_instance(instance, nullptr);
    dlclose(pipeline); dlclose(stack); dlclose(loader);
    std::puts("PASS loader create/physical/device/teardown chain and production pipeline ABI");
}
