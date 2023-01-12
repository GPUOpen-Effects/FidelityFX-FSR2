#pragma once

#include "helper.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

//fsr2 stuff
#include<ffx_fsr2.h>
#include<ffx_types.h>
#include<ffx_error.h>
#include<ffx_fsr2_interface.h>
#include<ffx_util.h>
#include<shaders/ffx_fsr2_common.h>
#include<shaders/ffx_fsr2_resources.h>
#include<vk/ffx_fsr2_vk.h>
#include<vk/shaders/ffx_fsr2_shaders_vk.h>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cstdint>
#include <limits>
#include <array>
#include <optional>
#include <set>
#include <unordered_map>

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#define NDEBUG

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

struct QueueFamilyIndices {
    uint32_t graphicsFamily;
    bool isComplete = false;
};

inline VkAccessFlags accessFlagsForImageLayout(VkImageLayout layout) {
    switch (layout) {
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        return VK_ACCESS_HOST_WRITE_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT;
    default:
        return VkAccessFlags();
    }
}

inline VkPipelineStageFlags pipelineStageForLayout(VkImageLayout layout) {
    switch (layout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        return VK_PIPELINE_STAGE_HOST_BIT;
    case VK_IMAGE_LAYOUT_UNDEFINED:
        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    default:
        return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
}

class Vulkan_FSR2 {
public:
    void run(std::vector<std::string>& file_list,std::string input_file_c, std::string input_file_md, std::string out_path) {
        initVulkan();
        copy_work(file_list,input_file_c, input_file_md, out_path);
        cleanup();
    }

private:

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    VkDevice device;

    VkQueue graphicsQueue;

    VkCommandPool commandPool;

    //hard-code area
    VkImage Fsr2_color_Image;
    VkDeviceMemory Fsr2_color_ImageMemory;
    VkImageView Fsr2_color_ImageView;

    VkImage Fsr2_motionVectors_Image;
    VkDeviceMemory Fsr2_motionVectors_ImageMemory;
    VkImageView Fsr2_motionVectors_ImageView;

    VkImage Fsr2_depth_Image;
    VkDeviceMemory Fsr2_depth_ImageMemory;
    VkImageView Fsr2_depth_ImageView;

    VkImage Fsr2_out_Image;
    VkDeviceMemory Fsr2_out_ImageMemory;
    VkImageView Fsr2_out_ImageView;

    //-------------

    //fsr2 stuff
    FfxFsr2ContextDescription   initializationParameters = {};
    FfxFsr2Context              context;
    double m_deltaTime;
    float scale_motion_x = 1.0f;
    float scale_motion_y = 1.0f;
    float scale_depth = 1.0f;

    //-----------


    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        pickPhysicalDevice();
        createLogicalDevice();
        createCommandPool();
    }

    void cleanup() {

        vkDestroyImage(device, Fsr2_color_Image, nullptr);
        vkFreeMemory(device, Fsr2_color_ImageMemory, nullptr);
        vkDestroyImageView(device, Fsr2_color_ImageView, nullptr);

        vkDestroyImage(device, Fsr2_motionVectors_Image, nullptr);
        vkFreeMemory(device, Fsr2_motionVectors_ImageMemory, nullptr);
        vkDestroyImageView(device, Fsr2_motionVectors_ImageView, nullptr);

        vkDestroyImage(device, Fsr2_depth_Image, nullptr);
        vkFreeMemory(device, Fsr2_depth_ImageMemory, nullptr);
        vkDestroyImageView(device, Fsr2_depth_ImageView, nullptr);

        vkDestroyImage(device, Fsr2_out_Image, nullptr);
        vkFreeMemory(device, Fsr2_out_ImageMemory, nullptr);
        vkDestroyImageView(device, Fsr2_out_ImageView, nullptr);

        ffxFsr2ContextDestroy(&context);

        vkDestroyCommandPool(device, commandPool, nullptr);

        vkDestroyDevice(device, nullptr);

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroyInstance(instance, nullptr);
    }

    void createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                msaaSamples = getMaxUsableSampleCount();
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily };

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};

        deviceFeatures.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
    }

    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics command pool!");
        }
    }

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
    }

    VkFormat findDepthFormat() {
        return findSupportedFormat(
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    VkSampleCountFlagBits getMaxUsableSampleCount() {
        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

        VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
        if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
        if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
        if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
        if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
        if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
        if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

        return VK_SAMPLE_COUNT_1_BIT;
    }

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }

        return imageView;
    }

    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = numSamples;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(device, image, imageMemory, 0);
    }

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspectMask;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            barrier.srcAccessMask = accessFlagsForImageLayout(oldLayout);
            barrier.dstAccessMask = accessFlagsForImageLayout(newLayout);

            sourceStage = pipelineStageForLayout(oldLayout);
            destinationStage = pipelineStageForLayout(newLayout);
        }
        /*else {
            throw std::invalid_argument("unsupported layout transition!");
        }*/

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        endSingleTimeCommands(commandBuffer);
    }

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = aspectMask;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = {
            width,
            height,
            1
        };

        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        endSingleTimeCommands(commandBuffer);
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }

    VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        return indices.isComplete && extensionsSupported && supportedFeatures.samplerAnisotropy;

    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)) {
                indices.graphicsFamily = i;
                indices.isComplete = true;
                break;
            }

            i++;
        }

        return indices;
    }

    std::vector<const char*> getRequiredExtensions() {

        std::vector<const char*> extensions;

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

    //hard-code area
    void copyImageToBuffer(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = {
            width,
            height,
            1
        };

        vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);

        endSingleTimeCommands(commandBuffer);
    }
    void CreateFSR2Image(std::string input_path, int& texWidth, int& texHeight, int& texChannels, VkImage& Fsr2_input_Image, VkDeviceMemory& Fsr2_input_ImageMemory, VkImageView& Fsr2_input_ImageView)
    {

        texWidth = 2; texHeight = 2;
        //mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
        int mipLevels_fsr = 1;
        std::vector<float> data_float = read_image_exr(input_path.c_str(), texWidth, texHeight, texChannels);
        VkDeviceSize imageSize = texWidth * texHeight * sizeof(float) * texChannels;
        /*std::vector<float> data_float = {
            1.0,0.5,0.5,1.0,
            0.5,1.0,1.0,1.0,
            0.5,0.5,1.0,1.0,
            1.0,1.0,1.0,1.0
        };
        */
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, data_float.data(), static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);

        createImage(texWidth, texHeight, mipLevels_fsr, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Fsr2_input_Image, Fsr2_input_ImageMemory);

        transitionImageLayout(Fsr2_input_Image, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels_fsr);
        copyBufferToImage(stagingBuffer, Fsr2_input_Image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        //transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);

        Fsr2_input_ImageView = createImageView(Fsr2_input_Image, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels_fsr);
    }

    void CreateFSR2Image_MotionDepth(std::string input_path, int& texWidth, int& texHeight, int& texChannels, VkImage& Fsr2_motion, VkDeviceMemory& Fsr2_motionMemory, VkImageView& Fsr2_motion_ImageView, VkImage& Fsr2_depth, VkDeviceMemory& Fsr2_depthMemory, VkImageView& Fsr2_depthMemory_ImageView)
    {

        texWidth = 2; texHeight = 2;
        //mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
        int mipLevels_fsr = 1;
        std::vector<float> data_float = read_image_exr(input_path.c_str(), texWidth, texHeight, texChannels);
        VkDeviceSize imageSize_motion = texWidth * texHeight * sizeof(float) * 2;
        VkDeviceSize imageSize_depth = texWidth * texHeight * sizeof(float);
        std::vector<float> data_motion;
        std::vector<float> data_depth;

        for (int i = 0; i < texWidth * texHeight; i++) {
            data_motion.push_back(scale_motion_x * data_float[texChannels * i + 0]);
            data_motion.push_back(scale_motion_y * data_float[texChannels * i + 1]);
            data_depth.push_back(1.0f / (scale_depth * data_float[texChannels * i + 2] + 0.00001f));
        }

        VkBuffer stagingBuffer_motion;
        VkDeviceMemory stagingBufferMemory_motion;
        createBuffer(imageSize_motion, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer_motion, stagingBufferMemory_motion);

        void* dataMotion;
        vkMapMemory(device, stagingBufferMemory_motion, 0, imageSize_motion, 0, &dataMotion);
        memcpy(dataMotion, data_motion.data(), static_cast<size_t>(imageSize_motion));
        vkUnmapMemory(device, stagingBufferMemory_motion);
        //motion image 
        createImage(texWidth, texHeight, mipLevels_fsr, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Fsr2_motion, Fsr2_motionMemory);

        transitionImageLayout(Fsr2_motion, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels_fsr);
        copyBufferToImage(stagingBuffer_motion, Fsr2_motion, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        //transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps

        vkDestroyBuffer(device, stagingBuffer_motion, nullptr);
        vkFreeMemory(device, stagingBufferMemory_motion, nullptr);

        //do the depth image
        VkBuffer stagingBuffer_depth;
        VkDeviceMemory stagingBufferMemory_depth;
        createBuffer(imageSize_depth, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer_depth, stagingBufferMemory_depth);

        void* dataDepth;
        vkMapMemory(device, stagingBufferMemory_depth, 0, imageSize_depth, 0, &dataDepth);
        memcpy(dataDepth, data_depth.data(), static_cast<size_t>(imageSize_depth));
        vkUnmapMemory(device, stagingBufferMemory_depth);
        //depth image 
        createImage(texWidth, texHeight, mipLevels_fsr, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Fsr2_depth, Fsr2_depthMemory);

        transitionImageLayout(Fsr2_depth, VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels_fsr, VK_IMAGE_ASPECT_DEPTH_BIT);
        copyBufferToImage(stagingBuffer_depth, Fsr2_depth, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), VK_IMAGE_ASPECT_DEPTH_BIT);
        //transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps

        vkDestroyBuffer(device, stagingBuffer_depth, nullptr);
        vkFreeMemory(device, stagingBufferMemory_depth, nullptr);

        Fsr2_motion_ImageView = createImageView(Fsr2_motion, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels_fsr);
        Fsr2_depth_ImageView = createImageView(Fsr2_depth, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, mipLevels_fsr);
    }

    void UpdateFSR2Image(std::string input_path, int& texWidth, int& texHeight, int& texChannels, VkImage& Fsr2_input_Image, VkDeviceMemory& Fsr2_input_ImageMemory, VkImageView& Fsr2_input_ImageView)
    {

        texWidth = 2; texHeight = 2;
        //mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
        int mipLevels_fsr = 1;
        std::vector<float> data_float = read_image_exr(input_path.c_str(), texWidth, texHeight, texChannels);
        VkDeviceSize imageSize = texWidth * texHeight * sizeof(float) * texChannels;
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, data_float.data(), static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);

        transitionImageLayout(Fsr2_input_Image, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels_fsr);
        copyBufferToImage(stagingBuffer, Fsr2_input_Image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        //transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);

        vkDestroyImageView(device, Fsr2_input_ImageView, nullptr);

        Fsr2_input_ImageView = createImageView(Fsr2_input_Image, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels_fsr);
    }

    void UpdateFSR2Image_MotionDepth(std::string input_path, int& texWidth, int& texHeight, int& texChannels, VkImage& Fsr2_motion, VkDeviceMemory& Fsr2_motionMemory, VkImageView& Fsr2_motion_ImageView, VkImage& Fsr2_depth, VkDeviceMemory& Fsr2_depthMemory, VkImageView& Fsr2_depthMemory_ImageView)
    {

        texWidth = 2; texHeight = 2;
        //mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
        int mipLevels_fsr = 1;
        std::vector<float> data_float = read_image_exr(input_path.c_str(), texWidth, texHeight, texChannels);
        VkDeviceSize imageSize_motion = texWidth * texHeight * sizeof(float) * 2;
        VkDeviceSize imageSize_depth = texWidth * texHeight * sizeof(float);
        std::vector<float> data_motion;
        std::vector<float> data_depth;

        for (int i = 0; i < texWidth * texHeight; i++) {
            data_motion.push_back(scale_motion_x * data_float[texChannels * i + 0]);
            data_motion.push_back(scale_motion_y * data_float[texChannels * i + 1]);
            data_depth.push_back(1.0f/(scale_depth * data_float[texChannels * i + 2]+0.00001f));
        }

        VkBuffer stagingBuffer_motion;
        VkDeviceMemory stagingBufferMemory_motion;
        createBuffer(imageSize_motion, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer_motion, stagingBufferMemory_motion);

        void* dataMotion;
        vkMapMemory(device, stagingBufferMemory_motion, 0, imageSize_motion, 0, &dataMotion);
        memcpy(dataMotion, data_motion.data(), static_cast<size_t>(imageSize_motion));
        vkUnmapMemory(device, stagingBufferMemory_motion);
        //motion image 

        transitionImageLayout(Fsr2_motion, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels_fsr);
        copyBufferToImage(stagingBuffer_motion, Fsr2_motion, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        //transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps

        vkDestroyBuffer(device, stagingBuffer_motion, nullptr);
        vkFreeMemory(device, stagingBufferMemory_motion, nullptr);

        //do the depth image
        VkBuffer stagingBuffer_depth;
        VkDeviceMemory stagingBufferMemory_depth;
        createBuffer(imageSize_depth, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer_depth, stagingBufferMemory_depth);

        void* dataDepth;
        vkMapMemory(device, stagingBufferMemory_depth, 0, imageSize_depth, 0, &dataDepth);
        memcpy(dataDepth, data_depth.data(), static_cast<size_t>(imageSize_depth));
        vkUnmapMemory(device, stagingBufferMemory_depth);
        //depth image 

        transitionImageLayout(Fsr2_depth, VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels_fsr, VK_IMAGE_ASPECT_DEPTH_BIT);
        copyBufferToImage(stagingBuffer_depth, Fsr2_depth, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), VK_IMAGE_ASPECT_DEPTH_BIT);
        //transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps

        vkDestroyBuffer(device, stagingBuffer_depth, nullptr);
        vkFreeMemory(device, stagingBufferMemory_depth, nullptr);

        vkDestroyImageView(device, Fsr2_motion_ImageView, nullptr);
        vkDestroyImageView(device, Fsr2_depth_ImageView, nullptr);

        Fsr2_motion_ImageView = createImageView(Fsr2_motion, VK_FORMAT_R32G32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels_fsr);
        Fsr2_depth_ImageView = createImageView(Fsr2_depth, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, mipLevels_fsr);
    }

    void DownloadFSR2Image(int index, std::string output_path, int& texWidth, int& texHeight, int& texChannels, VkImage& Fsr2_input_Image, VkDeviceMemory& Fsr2_input_ImageMemory)
    {
        VkDeviceSize imageSize = texWidth * texHeight * sizeof(float) * texChannels;
        //mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
        int mipLevels_fsr = 1;
        std::vector<float> data_float;
        data_float.resize(texWidth * texHeight * texChannels);
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        transitionImageLayout(Fsr2_input_Image, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mipLevels_fsr);
        copyImageToBuffer(stagingBuffer, Fsr2_input_Image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        //transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps

        //copy data from buffer
        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data_float.data(), data, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);

        //work with the vector
        std::string file_name = std::to_string(index) + ".exr";
        std::string output_exr = output_path + file_name;

        write_exr(output_exr, data_float, texWidth, texHeight, texChannels, false);
    }
    void Fsr2_work_init(uint32_t renderWidth,
        uint32_t renderHeight,
        uint32_t displayWidth,
        uint32_t displayHeight,
        bool hdr,
        bool InvertedDepth)
    {
        const size_t scratchBufferSize = ffxFsr2GetScratchMemorySizeVK(physicalDevice);
        void* scratchBuffer = malloc(scratchBufferSize);
        FfxErrorCode errorCode = ffxFsr2GetInterfaceVK(&initializationParameters.callbacks, scratchBuffer, scratchBufferSize, physicalDevice, vkGetDeviceProcAddr);
        assert(errorCode == FFX_OK);

        initializationParameters.device = ffxGetDeviceVK(device);
        initializationParameters.maxRenderSize.width = renderWidth;
        initializationParameters.maxRenderSize.height = renderHeight;
        initializationParameters.displaySize.width = displayWidth;
        initializationParameters.displaySize.height = displayHeight;
        initializationParameters.flags = FFX_FSR2_ENABLE_AUTO_EXPOSURE;

        if (InvertedDepth) {
            initializationParameters.flags |= FFX_FSR2_ENABLE_DEPTH_INVERTED;
        }

        if (hdr) {
            initializationParameters.flags |= FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE;
        }

        ffxFsr2ContextCreate(&context, &initializationParameters);
    }
    void Fsr2_work_OnRender(uint32_t m_index,uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight)
    {
        m_deltaTime = 33.3f;//33.3f for 30fps,if running at 60fps, the value passed should be around 16.6f.
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        FfxFsr2DispatchDescription dispatchParameters = {};
        dispatchParameters.commandList = ffxGetCommandListVK(commandBuffer);
        dispatchParameters.color = ffxGetTextureResourceVK(&context, Fsr2_color_Image, Fsr2_color_ImageView, renderWidth, renderHeight, VK_FORMAT_R32G32B32A32_SFLOAT, L"FSR2_InputColor");
        dispatchParameters.depth = ffxGetTextureResourceVK(&context, Fsr2_depth_Image, Fsr2_depth_ImageView, renderWidth, renderHeight, VK_FORMAT_D32_SFLOAT, L"FSR2_InputDepth");
        dispatchParameters.motionVectors = ffxGetTextureResourceVK(&context, Fsr2_motionVectors_Image, Fsr2_motionVectors_ImageView, renderWidth, renderHeight, VK_FORMAT_R32G32_SFLOAT, L"FSR2_InputMotionVectors");
        dispatchParameters.exposure = ffxGetTextureResourceVK(&context, nullptr, nullptr, 1, 1, VK_FORMAT_UNDEFINED, L"FSR2_InputExposure");

        //no reactive
        dispatchParameters.reactive = ffxGetTextureResourceVK(&context, nullptr, nullptr, 1, 1, VK_FORMAT_UNDEFINED, L"FSR2_EmptyInputReactiveMap");
        //no transparency and compositon
        dispatchParameters.transparencyAndComposition = ffxGetTextureResourceVK(&context, nullptr, nullptr, 1, 1, VK_FORMAT_UNDEFINED, L"FSR2_EmptyTransparencyAndCompositionMap");

        dispatchParameters.output = ffxGetTextureResourceVK(&context, Fsr2_out_Image, Fsr2_out_ImageView, displayWidth, displayHeight, VK_FORMAT_R32G32B32A32_SFLOAT, L"FSR2_OutputUpscaledColor", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
        dispatchParameters.jitterOffset.x = 0.0f;
        dispatchParameters.jitterOffset.y = 0.0f;
        dispatchParameters.motionVectorScale.x = (float)renderWidth;
        dispatchParameters.motionVectorScale.y = (float)renderHeight;
        dispatchParameters.reset = false;
        dispatchParameters.enableSharpening = false;
        dispatchParameters.sharpness = 0.0f;
        dispatchParameters.frameTimeDelta = (float)m_deltaTime;
        dispatchParameters.preExposure = 1.0f;
        dispatchParameters.renderSize.width = renderWidth;
        dispatchParameters.renderSize.height = renderHeight;
        dispatchParameters.cameraFar = 0.01f;
        dispatchParameters.cameraNear = 1e8;
        dispatchParameters.cameraFovAngleVertical = 1.56f;


        FfxErrorCode errorCode = ffxFsr2ContextDispatch(&context, &dispatchParameters);
        assert(errorCode == FFX_OK);

        endSingleTimeCommands(commandBuffer);
    }
    void Fsr2_work(std::string output_path, int index, std::vector<std::string>& color_list, std::vector<std::string>& motionVector_list,
        uint32_t renderWidth,
        uint32_t renderHeight,
        uint32_t displayWidth,
        uint32_t displayHeight,
        int outChannels,
        bool hdr,
        bool InvertedDepth)
    {
        Fsr2_work_init(renderWidth, renderHeight, displayWidth, displayHeight, hdr, InvertedDepth);
        for (int i = index; i <= color_list.size(); i++)
        {   
            clock_t cost_time = clock();
            Fsr2_work_OnRender(i,renderWidth, renderHeight, displayWidth, displayHeight);
            clock_t render_cost_time = clock() - cost_time;

            int width = displayWidth;
            int height = displayHeight;
            DownloadFSR2Image(i, output_path, width, height, outChannels, Fsr2_out_Image, Fsr2_out_ImageMemory);

            cost_time = clock() - (render_cost_time + cost_time);
            
            printf("(%d / %d) done. render cost %f s, download image cost %f s.\n", i, color_list.size(), (float)((float)render_cost_time / CLOCKS_PER_SEC), (float)((float)cost_time / CLOCKS_PER_SEC));
            
            if (i == color_list.size()) break;

            int texWidth, texHeight, texChannels = 4;
            UpdateFSR2Image(color_list[i], texWidth, texHeight, texChannels, Fsr2_color_Image, Fsr2_color_ImageMemory, Fsr2_color_ImageView);
            int texWidth_, texHeight_, texChannels_ = 4;
            UpdateFSR2Image_MotionDepth(motionVector_list[i], texWidth_, texHeight_, texChannels_,
                Fsr2_motionVectors_Image, Fsr2_motionVectors_ImageMemory, Fsr2_motionVectors_ImageView,
                Fsr2_depth_Image, Fsr2_depth_ImageMemory, Fsr2_depth_ImageView);
            

        }
    }

    void copy_work(std::vector<std::string> &file_list,std::string &input_path_color, std::string &input_path_motionDepth, std::string &output_path)
    {
        std::vector<std::string> source_list_color;
        std::vector<std::string> source_list_motionDepth;
        for (int i = 0; i < file_list.size(); i++)
        {
            std::string file_name = file_list[i];
            std::string color_path = input_path_color + file_name;
            std::string MotionDepth_path = input_path_motionDepth + file_name;
            source_list_color.push_back(color_path);
            source_list_motionDepth.push_back(MotionDepth_path);
        }
        scale_motion_x = -0.5f;
        scale_motion_y = 0.5f;
        scale_depth = 1000.0f;

        printf("hello,I'm ready to go!\n");
        int texWidth, texHeight;
        int texChannels = 4;//TODO: support 3 channels
        CreateFSR2Image(source_list_color[0], texWidth, texHeight, texChannels, Fsr2_color_Image, Fsr2_color_ImageMemory, Fsr2_color_ImageView);
        int texWidth_, texHeight_, texChannels_ = 4;
        CreateFSR2Image_MotionDepth(source_list_motionDepth[0], texWidth_, texHeight_, texChannels_,
            Fsr2_motionVectors_Image, Fsr2_motionVectors_ImageMemory, Fsr2_motionVectors_ImageView,
            Fsr2_depth_Image, Fsr2_depth_ImageMemory, Fsr2_depth_ImageView);

        //Let's do it!
        int outWidth, outHeight, outChannels;
        outWidth = 3840;
        outHeight = 2160;
        outChannels = texChannels;

        //create output result
        createImage(outWidth, outHeight, 1, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Fsr2_out_Image, Fsr2_out_ImageMemory);
        Fsr2_out_ImageView = createImageView(Fsr2_out_Image, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1);

        Fsr2_work(output_path, 1, source_list_color, source_list_motionDepth, texWidth, texHeight, outWidth, outHeight, outChannels, true, false);

        printf("I am done.\n");
    }
};
