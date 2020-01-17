// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "VulkanMain.hpp"
#include <vulkan_wrapper.h>

#include <android/log.h>

#include <cassert>
#include <cstring>
#include <vector>
#include <string>
#include <array>

struct VulkanDeviceInfo
{
    bool initialized_;
    VkInstance instance_;
    VkPhysicalDevice physicalDevice_;
    VkDevice device_;
    uint32_t queueFamilyIndex_;
    VkSurfaceKHR surface_;
    VkQueue queue_;
};
VulkanDeviceInfo device;

struct VulkanSwapchainInfo
{
    VkSwapchainKHR swapchain_;
    uint32_t swapchainLength_;
    VkExtent2D displaySize_;
    VkFormat displayFormat_;
    std::vector<VkImage> displayImages_;
    std::vector<VkImageView> displayVies_;
    std::vector<VkFramebuffer> framebuffers_;
};
VulkanSwapchainInfo swapchain;

struct VulkanBufferInfo
{
    VkBuffer vertexBuf_;
};
VulkanBufferInfo buffers;

struct VulkanGfxPipelineInfo
{
    VkPipelineLayout layout_;
    VkPipelineCache cache_;
    VkPipeline pipeline_;
};
VulkanGfxPipelineInfo gfxPipeline;

struct VulkanRenderInfo
{
    VkRenderPass renderPass_;
    VkCommandPool cmdPool_;
    VkCommandBuffer* cmdBuffer_;
    uint32_t cmdBufferLen_;
    VkSemaphore semaphore_;
    VkFence fence_;
};
VulkanRenderInfo render;

android_app* androidAppCtx = nullptr;

/*
 * setImageLayout():
 *    Helper function to transition color buffer layout
 */
void setImageLayout( VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStages, VkPipelineStageFlags destStages );

void CreateVulkanDevice( ANativeWindow* platformWindow, VkApplicationInfo* appInfo )
{
    std::vector<const char*> instance_extensions;
    std::vector<const char*> device_extensions;

    instance_extensions.emplace_back( "VK_KHR_surface" );
    instance_extensions.emplace_back( "VK_KHR_android_surface" );

    device_extensions.emplace_back( "VK_KHR_swapchain" );

    VkInstanceCreateInfo instanceCreateInfo;
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = nullptr;
    instanceCreateInfo.pApplicationInfo = appInfo;
    instanceCreateInfo.enabledExtensionCount = instance_extensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = instance_extensions.data();
    instanceCreateInfo.enabledLayerCount = 0;
    instanceCreateInfo.ppEnabledLayerNames = nullptr;
    VkResult result = vkCreateInstance( &instanceCreateInfo, nullptr, &device.instance_ );
    assert( result == VK_SUCCESS );

    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo;
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.pNext = nullptr;
    surfaceCreateInfo.window = platformWindow;
    surfaceCreateInfo.flags = 0;
    result = vkCreateAndroidSurfaceKHR( device.instance_, &surfaceCreateInfo, nullptr, &device.surface_ );
    assert( result == VK_SUCCESS );

    uint32_t gpuCount{ 0 };
    std::vector<VkPhysicalDevice> gpus;
    vkEnumeratePhysicalDevices( device.instance_, &gpuCount, nullptr );
    gpus.resize( gpuCount );
    vkEnumeratePhysicalDevices( device.instance_, &gpuCount, gpus.data() );
    device.physicalDevice_ = gpus.at( 0 );

    uint32_t queueFamilyCount;
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    vkGetPhysicalDeviceQueueFamilyProperties( device.physicalDevice_, &queueFamilyCount, nullptr );
    queueFamilyProperties.resize( queueFamilyCount );
    vkGetPhysicalDeviceQueueFamilyProperties( device.physicalDevice_, &queueFamilyCount, queueFamilyProperties.data() );

    uint32_t queueFamilyIndex{ 0 };
    for( auto it = queueFamilyProperties.begin(); it != queueFamilyProperties.end(); ++it )
    {
        if( it->queueFlags & VK_QUEUE_GRAPHICS_BIT )
        {
            queueFamilyIndex = it - queueFamilyProperties.begin();
            break;
        }
    }
    assert( queueFamilyIndex );
    device.queueFamilyIndex_ = queueFamilyIndex;

    std::array<float, 1> priorities{ 1.f };
    VkDeviceQueueCreateInfo queueCreateInfo;
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.pNext = nullptr;
    queueCreateInfo.flags = 0;
    queueCreateInfo.pQueuePriorities = priorities.data();
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;

    VkDeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = nullptr;
    deviceCreateInfo.ppEnabledExtensionNames = device_extensions.data();
    deviceCreateInfo.enabledExtensionCount = device_extensions.size();
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.pEnabledFeatures = nullptr;
    result = vkCreateDevice( device.physicalDevice_, &deviceCreateInfo, nullptr, &device.device_ );
    assert( result == VK_SUCCESS );

    vkGetDeviceQueue( device.device_, queueFamilyIndex, 0, &device.queue_ );
}

void CreateSwapChain( void )
{
    // memset( &swapchain, 0, sizeof( swapchain ) );

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device.physicalDevice_, device.surface_, &surfaceCapabilities );

    uint32_t surfaceFormatCount{ 0 };
    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    vkGetPhysicalDeviceSurfaceFormatsKHR( device.physicalDevice_, device.surface_, &surfaceFormatCount, nullptr );
    surfaceFormats.resize( surfaceFormatCount );
    vkGetPhysicalDeviceSurfaceFormatsKHR( device.physicalDevice_, device.surface_, &surfaceFormatCount, surfaceFormats.data() );

    uint32_t chosenFormat{ 0 };
    for( auto it = surfaceFormats.begin(); it != surfaceFormats.end(); ++it )
    {
        if( it->format == VK_FORMAT_R8G8B8_UNORM )
        {
            chosenFormat = it - surfaceFormats.begin();
        }
    }
    assert( chosenFormat );

    swapchain.displaySize_ = surfaceCapabilities.currentExtent;
    swapchain.displayFormat_ = surfaceFormats[chosenFormat].format;

    assert( surfaceCapabilities.supportedCompositeAlpha | VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR ); // TODO: PR duplicate logic

    VkSwapchainCreateInfoKHR swapchainCreateInfo;
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.pNext = nullptr;
    swapchainCreateInfo.flags = 0;
    swapchainCreateInfo.surface = device.surface_;
    swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount;
    swapchainCreateInfo.imageFormat = surfaceFormats[chosenFormat].format;
    swapchainCreateInfo.imageColorSpace = surfaceFormats[chosenFormat].colorSpace;
    swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
    swapchainCreateInfo.imageArrayLayers = 1; // stereo에서 view의 수 (3D가 아닌 경우는 1)
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // queue에서 이미지에 어떻게 접근할지에 대한 공유 모드 설정
    swapchainCreateInfo.queueFamilyIndexCount = 1;
    swapchainCreateInfo.pQueueFamilyIndices = &device.queueFamilyIndex_;
    swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainCreateInfo.clipped = VK_FALSE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
    VkResult result = vkCreateSwapchainKHR( device.device_, &swapchainCreateInfo, nullptr, &swapchain.swapchain_ );
    assert( result == VK_SUCCESS );

    swapchain.displayImages_.resize( surfaceCapabilities.minImageCount );
    vkGetSwapchainImagesKHR( device.device_, swapchain.swapchain_, &swapchain.swapchainLength_, nullptr );
}

// Initialize vulkan device context
// after return, vulkan is ready to draw
bool InitVulkan( android_app* app )
{
    androidAppCtx = app;
    if( InitVulkan() )
    {
        assert( false );
    }

    VkApplicationInfo appInfo;
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.apiVersion = VK_MAKE_VERSION( 1, 0, 0 );
    appInfo.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
    appInfo.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
    appInfo.pApplicationName = "tutorial_custom";
    appInfo.pEngineName = "tutorial";

    CreateVulkanDevice( app->window, &appInfo );

    CreateSwapChain();

    return true;
}

// delete vulkan device context when application goes away
void DeleteVulkan( void )
{

}

// Check if vulkan is ready to draw
bool IsVulkanReady( void )
{
    return true;
}

// Ask Vulkan to Render a frame
bool VulkanDrawFrame( void )
{
    return true;
}
