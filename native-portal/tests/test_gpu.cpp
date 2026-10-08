// SPDX-License-Identifier: MIT
// Owned offscreen Vulkan fixture. No layer, camera descriptors, or VR APIs.
#include "ft_aperture.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
using namespace ft::portal;
namespace {
constexpr uint32_t width = 32, height = 32;
void ok(VkResult r) {
    if (r != VK_SUCCESS) {
        std::fprintf(stderr, "Vulkan error %d\n", r);
        std::exit(2);
    }
}
std::vector<uint32_t> read(const std::string &p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) {
        std::fprintf(stderr, "missing %s\n", p.c_str());
        std::exit(3);
    }
    auto n = f.tellg();
    if (n <= 0 || n % 4 != 0)
        std::exit(3);
    std::vector<uint32_t> b(static_cast<size_t>(n) / 4);
    f.seekg(0);
    if (!f.read(reinterpret_cast<char *>(b.data()), n))
        std::exit(3);
    return b;
}
struct Case {
    std::string name;
    Parameters p;
    std::array<float, 4> clip_w{.5f, 2, 4, 0}, color{.25f, .125f, 0, .5f};
};
std::vector<Case> cases() {
    Parameters p;
    p.q0 = {2, 0, 0, 0};
    p.q1 = {0, 2, 0, 0};
    p.q2 = {0, 0, -1, 0};
    p.opacity = 1;
    p.flags = 1;
    std::vector<Case> out;
    auto add = [&](const char *name) {
        out.push_back({name, p});
        return &out.back();
    };
    add("unequal-clip-w");
    add("uniform-clip-w")->clip_w = {1, 1, 1, 0};
    add("opacity-zero")->p.opacity = 0;
    add("opacity-quarter")->p.opacity = .25f;
    add("translated")->p.q0 = {2, 0, 0, -.4f};
    auto *r = add("rotated");
    r->p.q0 = {1.41421356f, 1.41421356f, 0, 0};
    r->p.q1 = {-1.41421356f, 1.41421356f, 0, 0};
    add("behind")->p.q2 = {0, 0, 1, 0};
    add("zero-qz")->p.q2 = {0, 0, 0, 0};
    add("crossing-eye-plane")->p.q2 = {1, 0, 0, 0};
    add("invalid-flag")->p.flags = 0;
    add("one-sided-validated")->p.flags = 3;
    auto *e = add("epoch-diagnostic-only");
    e->p.epoch_low = 0x76543210;
    e->p.epoch_high = 0xfedcba98;
    for (int axis = 0; axis < 3; ++axis)
        for (int kind = 0; kind < 2; ++kind) {
            Case bad{"nonfinite-q" + std::to_string(axis) + "-" + std::to_string(kind), p};
            auto &row = axis == 0 ? bad.p.q0 : axis == 1 ? bad.p.q1 : bad.p.q2;
            row[3] = kind ? std::numeric_limits<float>::infinity()
                          : std::numeric_limits<float>::quiet_NaN();
            out.push_back(bad);
        }
    add("nan-opacity")->p.opacity = std::numeric_limits<float>::quiet_NaN();
    add("high-opacity")->p.opacity = 1.1f;
    add("negative-opacity")->p.opacity = -.1f;
    add("native-black")->color = {0, 0, 0, 0};
    add("native-fade")->color = {.05f, .025f, 0, .1f};
    return out;
}
} // namespace
int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: test-gpu BUILD/gpu\n");
        return 2;
    }
    VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ai.pApplicationName = "ft-owned-aperture-test";
    ai.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &ai;
    VkInstance instance;
    ok(vkCreateInstance(&ici, nullptr, &instance));
    uint32_t n = 0;
    ok(vkEnumeratePhysicalDevices(instance, &n, nullptr));
    if (!n)
        return 4;
    std::vector<VkPhysicalDevice> devices(n);
    ok(vkEnumeratePhysicalDevices(instance, &n, devices.data()));
    auto physical = devices[0];
    VkPhysicalDeviceProperties prop;
    vkGetPhysicalDeviceProperties(physical, &prop);
    std::printf("device=%s type=%u image=%ux%u\n", prop.deviceName, prop.deviceType, width, height);
    // Refuse a CPU implementation: this test is explicitly real device verification.
    if (prop.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
        std::fprintf(stderr, "CPU Vulkan device is not GPU evidence\n");
        return 4;
    }
    uint32_t qn = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &qn, nullptr);
    std::vector<VkQueueFamilyProperties> families(qn);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &qn, families.data());
    uint32_t q = 0;
    while (q < qn && !(families[q].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        ++q;
    if (q == qn)
        return 5;
    float priority = 0;
    VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qi.queueFamilyIndex = q;
    qi.queueCount = 1;
    qi.pQueuePriorities = &priority;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qi;
    VkDevice d;
    ok(vkCreateDevice(physical, &dci, nullptr, &d));
    VkQueue queue;
    vkGetDeviceQueue(d, q, 0, &queue);
    VkPhysicalDeviceMemoryProperties memory;
    vkGetPhysicalDeviceMemoryProperties(physical, &memory);
    auto allocate = [&](VkMemoryRequirements req, VkMemoryPropertyFlags flags) {
        uint32_t t = 0;
        while (t < memory.memoryTypeCount &&
               (!(req.memoryTypeBits & (1u << t)) ||
                (memory.memoryTypes[t].propertyFlags & flags) != flags))
            ++t;
        if (t == memory.memoryTypeCount)
            std::exit(6);
        VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        a.allocationSize = req.size;
        a.memoryTypeIndex = t;
        VkDeviceMemory m;
        ok(vkAllocateMemory(d, &a, nullptr, &m));
        return m;
    };
    VkImageCreateInfo imci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imci.imageType = VK_IMAGE_TYPE_2D;
    imci.format = VK_FORMAT_R8G8B8A8_UNORM;
    imci.extent = {width, height, 1};
    imci.mipLevels = 1;
    imci.arrayLayers = 1;
    imci.samples = VK_SAMPLE_COUNT_1_BIT;
    imci.tiling = VK_IMAGE_TILING_OPTIMAL;
    imci.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkImage image;
    ok(vkCreateImage(d, &imci, nullptr, &image));
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(d, image, &req);
    VkDeviceMemory image_mem = allocate(req, 0);
    ok(vkBindImageMemory(d, image, image_mem, 0));
    VkImageViewCreateInfo ivci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ivci.image = image;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = imci.format;
    ivci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView view;
    ok(vkCreateImageView(d, &ivci, nullptr, &view));
    auto make_buffer = [&](VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer &b,
                           VkDeviceMemory &m) {
        VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        ci.size = size;
        ci.usage = usage;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ok(vkCreateBuffer(d, &ci, nullptr, &b));
        VkMemoryRequirements r;
        vkGetBufferMemoryRequirements(d, b, &r);
        m = allocate(r, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        ok(vkBindBufferMemory(d, b, m, 0));
    };
    VkBuffer readback, uniform;
    VkDeviceMemory readback_mem, uniform_mem;
    make_buffer(width * height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, readback, readback_mem);
    make_buffer(592, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, uniform, uniform_mem);
    VkDescriptorSetLayoutBinding bindings[2] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        {4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
    VkDescriptorSetLayoutCreateInfo slci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    slci.bindingCount = 2;
    slci.pBindings = bindings;
    VkDescriptorSetLayout set_layout;
    ok(vkCreateDescriptorSetLayout(d, &slci, nullptr, &set_layout));
    VkDescriptorPoolSize poolsize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2};
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 1;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &poolsize;
    VkDescriptorPool descriptor_pool;
    ok(vkCreateDescriptorPool(d, &dpci, nullptr, &descriptor_pool));
    VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = descriptor_pool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &set_layout;
    VkDescriptorSet set;
    ok(vkAllocateDescriptorSets(d, &dsai, &set));
    VkDescriptorBufferInfo bi{uniform, 0, 592};
    VkWriteDescriptorSet writes[2]{};
    for (int k = 0; k < 2; ++k) {
        writes[k].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[k].dstSet = set;
        writes[k].dstBinding = k ? 4 : 0;
        writes[k].descriptorCount = 1;
        writes[k].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[k].pBufferInfo = &bi;
    }
    vkUpdateDescriptorSets(d, 2, writes, 0, nullptr);
    VkAttachmentDescription ad{};
    ad.format = imci.format;
    ad.samples = VK_SAMPLE_COUNT_1_BIT;
    ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ad.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ad.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkAttachmentReference ar{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ar;
    VkSubpassDependency deps[2]{};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpci.attachmentCount = 1;
    rpci.pAttachments = &ad;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sub;
    rpci.dependencyCount = 2;
    rpci.pDependencies = deps;
    VkRenderPass rp;
    ok(vkCreateRenderPass(d, &rpci, nullptr, &rp));
    VkFramebufferCreateInfo fbci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbci.renderPass = rp;
    fbci.attachmentCount = 1;
    fbci.pAttachments = &view;
    fbci.width = width;
    fbci.height = height;
    fbci.layers = 1;
    VkFramebuffer fb;
    ok(vkCreateFramebuffer(d, &fbci, nullptr, &fb));
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &set_layout;
    VkPipelineLayout layout;
    ok(vkCreatePipelineLayout(d, &plci, nullptr, &layout));
    VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex = q;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool pool;
    ok(vkCreateCommandPool(d, &cpci, nullptr, &pool));
    VkCommandBufferAllocateInfo cba{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cba.commandPool = pool;
    cba.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cba.commandBufferCount = 1;
    VkCommandBuffer cmd;
    ok(vkAllocateCommandBuffers(d, &cba, &cmd));
    auto module = [&](const char *file) {
        auto b = read(std::string(argv[1]) + "/" + file);
        VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        sm.codeSize = b.size() * 4;
        sm.pCode = b.data();
        VkShaderModule m;
        ok(vkCreateShaderModule(d, &sm, nullptr, &m));
        return m;
    };
    VkShaderModule vert = module("vertex.spv"), frag = module("fragment.spv");
    VkPipelineShaderStageCreateInfo stages[2]{};
    for (auto &s : stages) {
        s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        s.pName = "main";
    }
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    VkPipelineVertexInputStateCreateInfo vin{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport viewport{0, 0, float(width), float(height), 0, 1};
    VkRect2D scissor{{0, 0}, {width, height}};
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.pViewports = &viewport;
    vp.scissorCount = 1;
    vp.pScissors = &scissor;
    VkPipelineRasterizationStateCreateInfo rs{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1;
    VkPipelineMultisampleStateCreateInfo ms{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend{VK_TRUE,
                                              VK_BLEND_FACTOR_ONE,
                                              VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                              VK_BLEND_OP_ADD,
                                              VK_BLEND_FACTOR_ONE,
                                              VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                              VK_BLEND_OP_ADD,
                                              15};
    VkPipelineColorBlendStateCreateInfo cb{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &blend;
    VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vin;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vp;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pColorBlendState = &cb;
    gp.layout = layout;
    gp.renderPass = rp;
    VkPipeline pipeline;
    ok(vkCreateGraphicsPipelines(d, VK_NULL_HANDLE, 1, &gp, nullptr, &pipeline));
    unsigned verified = 0, case_count = 0;
    for (const auto &test : cases()) {
        NativeConstants native{};
        std::memcpy(native.data(), test.clip_w.data(), 16);
        std::memcpy(native.data() + 16, test.color.data(), 16);
        Result result;
        result.error = Error::none;
        result.parameters = test.p;
        auto constants = snapshot(native, result);
        void *data;
        ok(vkMapMemory(d, uniform_mem, 0, 592, 0, &data));
        std::memcpy(data, constants.data(), 592);
        vkUnmapMemory(d, uniform_mem);
        ok(vkResetCommandBuffer(cmd, 0));
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        ok(vkBeginCommandBuffer(cmd, &begin));
        VkClearValue clear{};
        clear.color.float32[2] = .25f;
        clear.color.float32[3] = .25f;
        VkRenderPassBeginInfo rb{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rb.renderPass = rp;
        rb.framebuffer = fb;
        rb.renderArea = scissor;
        rb.clearValueCount = 1;
        rb.pClearValues = &clear;
        vkCmdBeginRenderPass(cmd, &rb, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0,
                                nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {width, height, 1};
        vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1,
                               &copy);
        VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
                             &mb, 0, nullptr, 0, nullptr);
        ok(vkEndCommandBuffer(cmd));
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkFence fence;
        ok(vkCreateFence(d, &fi, nullptr, &fence));
        ok(vkQueueSubmit(queue, 1, &submit, fence));
        ok(vkWaitForFences(d, 1, &fence, VK_TRUE, 5000000000ULL));
        ok(vkMapMemory(d, readback_mem, 0, width * height * 4, 0, &data));
        const auto *bytes = static_cast<const unsigned char *>(data);
        unsigned inside_count = 0;
        for (uint32_t y = 0; y < height; ++y)
            for (uint32_t x = 0; x < width; ++x) {
                double nx = 2 * (x + .5) / width - 1, ny = 2 * (y + .5) / height - 1,
                       b1 = (nx + 1) / 4, b2 = (ny + 1) / 4, b0 = 1 - b1 - b2,
                       denom = b0 / test.clip_w[0] + b1 / test.clip_w[1] + b2 / test.clip_w[2];
                Vec3 raw{nx / denom, ny / denom, -1 / denom};
                auto aq = evaluate(test.p, raw);
                bool inside = contains(aq) && (test.p.flags & 1) && std::isfinite(test.p.opacity) &&
                              test.p.opacity >= 0 && test.p.opacity <= 1;
                inside_count += inside;
                for (int channel = 0; channel < 4; ++channel) {
                    double background = channel >= 2 ? .25 : 0;
                    double value = inside ? test.color[channel] * test.p.opacity +
                                                background * (1 - test.color[3] * test.p.opacity)
                                          : background;
                    int wanted = static_cast<int>(std::lround(value * 255));
                    int got = bytes[4 * (y * width + x) + channel];
                    if (std::abs(got - wanted) > 1) {
                        std::fprintf(
                            stderr,
                            "FAIL case=%s x=%u y=%u channel=%d got=%d wanted=%d q=%g,%g,%g\n",
                            test.name.c_str(), x, y, channel, got, wanted, aq.x, aq.y, aq.z);
                        return 7;
                    }
                }
                ++verified;
            }
        std::printf("PASS synthetic case=%s checked=%u inside=%u\n", test.name.c_str(),
                    width * height, inside_count);
        ++case_count;
        vkUnmapMemory(d, readback_mem);
        vkDestroyFence(d, fence, nullptr);
    }
    vkDestroyPipeline(d, pipeline, nullptr);
    vkDestroyShaderModule(d, vert, nullptr);
    vkDestroyShaderModule(d, frag, nullptr);
    vkDestroyCommandPool(d, pool, nullptr);
    vkDestroyPipelineLayout(d, layout, nullptr);
    vkDestroyDescriptorPool(d, descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(d, set_layout, nullptr);
    vkDestroyFramebuffer(d, fb, nullptr);
    vkDestroyRenderPass(d, rp, nullptr);
    vkDestroyBuffer(d, readback, nullptr);
    vkFreeMemory(d, readback_mem, nullptr);
    vkDestroyBuffer(d, uniform, nullptr);
    vkFreeMemory(d, uniform_mem, nullptr);
    vkDestroyImageView(d, view, nullptr);
    vkDestroyImage(d, image, nullptr);
    vkFreeMemory(d, image_mem, nullptr);
    vkDestroyDevice(d, nullptr);
    vkDestroyInstance(instance, nullptr);
    std::printf(
        "PASS SYNTHETIC GPU cases=%u verified_pixels=%u. Not native camera or optical proof.\n",
        case_count, verified);
}
