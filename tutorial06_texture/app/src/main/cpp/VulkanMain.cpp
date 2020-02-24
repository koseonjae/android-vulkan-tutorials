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

#include <android/log.h>
#include <cassert>
#include <vector>
#include <array>
#include "vulkan_wrapper.h"

using namespace std;

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG

#include <stb/stb_image.h>
#include "CreateShaderModule.h"
#include "VulkanMain.hpp"

static const char* kTAG = "Vulkan-Tutorial06";
#define LOGI( ... ) \
  ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW( ... ) \
  ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE( ... ) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))

// Vulkan call wrapper
#define CALL_VK( func )                                                 \
  if (VK_SUCCESS != (func)) {                                         \
    __android_log_print(ANDROID_LOG_ERROR, "Tutorial ",               \
                        "Vulkan error. File[%s], line[%d]", __FILE__, \
                        __LINE__);                                    \
    assert(false);                                                    \
  }

#define VK_CHECK( x ) CALL_VK(x)

struct VulkanDeviceInfo
{
    bool initialized_;
    VkInstance instance_;
    VkPhysicalDevice physicalDevice_;
    VkPhysicalDeviceMemoryProperties gpuMemoryProperties_;
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
    VkFramebuffer* framebuffers_;
    VkImage* displayImages_;
    VkImageView* displayViews_;
};
VulkanSwapchainInfo swapchain;

typedef struct texture_object
{
    VkSampler sampler;
    VkImage image;
    VkImageLayout imageLayout;
    VkDeviceMemory mem;
    VkImageView view;
    int32_t tex_width;
    int32_t tex_height;
} texture_object;
static const VkFormat kTexFmt = VK_FORMAT_R8G8B8A8_UNORM;
#define TUTORIAL_TEXTURE_COUNT 1
const char* texFiles[TUTORIAL_TEXTURE_COUNT] = { "sample_tex.png", };
struct texture_object textures[TUTORIAL_TEXTURE_COUNT];

struct VulkanBufferInfo
{
    VkBuffer vertexBuf_;
};
VulkanBufferInfo buffers;

struct VulkanGfxPipelineInfo
{
    VkDescriptorSetLayout dscLayout_;
    VkDescriptorPool descPool_;
    VkDescriptorSet descSet_;
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

void setImageLayout( VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStages, VkPipelineStageFlags destStages );

void CreateVulkanDevice( ANativeWindow* platformWindow, VkApplicationInfo* appInfo )
{
    std::vector<const char*> instance_extensions;
    std::vector<const char*> device_extensions;

    instance_extensions.push_back( "VK_KHR_surface" );
    instance_extensions.push_back( "VK_KHR_android_surface" );

    device_extensions.push_back( "VK_KHR_swapchain" );

    std::vector<const char*> instanceExtensions{ "VK_KHR_surface", "VK_KHR_android_surface" };
    std::vector<const char*> deviceExtensions{ "VK_KHR_swapchain" };

    VkApplicationInfo applicationInfo;
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pNext = nullptr;
    applicationInfo.pApplicationName = "vktutorial";
    applicationInfo.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
    applicationInfo.pEngineName = "vktutorial";
    applicationInfo.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );;
    applicationInfo.apiVersion = VK_MAKE_VERSION( 1, 0, 0 );

    VkInstanceCreateInfo instanceCreateInfo;
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = nullptr;
    instanceCreateInfo.flags = 0;
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.enabledLayerCount = 0;
    instanceCreateInfo.ppEnabledLayerNames = nullptr;
    instanceCreateInfo.enabledExtensionCount = instanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
    vkCreateInstance( &instanceCreateInfo, nullptr, &device.instance_ );

    VkAndroidSurfaceCreateInfoKHR androidSurfaceCreateInfo;
    androidSurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    androidSurfaceCreateInfo.pNext = nullptr;
    androidSurfaceCreateInfo.flags = 0;
    androidSurfaceCreateInfo.window = platformWindow;
    vkCreateAndroidSurfaceKHR( device.instance_, &androidSurfaceCreateInfo, nullptr, &device.surface_ );

    uint32_t gpuCount{ 0 };
    vkEnumeratePhysicalDevices( device.instance_, &gpuCount, nullptr );
    vector<VkPhysicalDevice> gpus( gpuCount );
    vkEnumeratePhysicalDevices( device.instance_, &gpuCount, gpus.data() );
    device.physicalDevice_ = gpus[0];

    vkGetPhysicalDeviceMemoryProperties( device.physicalDevice_, &device.gpuMemoryProperties_ );

    uint32_t indexCount{ 0 };
    vkGetPhysicalDeviceQueueFamilyProperties( device.physicalDevice_, &indexCount, nullptr );
    vector<VkQueueFamilyProperties> properties( indexCount );
    vkGetPhysicalDeviceQueueFamilyProperties( device.physicalDevice_, &indexCount, properties.data() );

    auto found = find_if( properties.begin(), properties.end(), [&]( VkQueueFamilyProperties properties ) {
        return properties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
    } );
    assert( found != properties.end() );
    device.queueFamilyIndex_ = found - properties.begin();

    array<float, 1> priority{ 1.0f };
    VkDeviceQueueCreateInfo deviceQueueCreateInfo;
    deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo.pNext = nullptr;
    deviceQueueCreateInfo.flags = 0;
    deviceQueueCreateInfo.queueFamilyIndex = device.queueFamilyIndex_;
    deviceQueueCreateInfo.queueCount = 1;
    deviceQueueCreateInfo.pQueuePriorities = priority.data();

    VkDeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = nullptr;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = nullptr;
    deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = nullptr;
    vkCreateDevice( device.physicalDevice_, &deviceCreateInfo, nullptr, &device.device_ );

    vkGetDeviceQueue( device.device_, device.queueFamilyIndex_, 0, &device.queue_ );
}

void CreateSwapChain( void )
{
    LOGI( "->createSwapChain" );
    memset( &swapchain, 0, sizeof( swapchain ) );

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device.physicalDevice_, device.surface_, &surfaceCapabilities );

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR( device.physicalDevice_, device.surface_, &formatCount, nullptr );
    VkSurfaceFormatKHR* formats = new VkSurfaceFormatKHR[formatCount];
    vkGetPhysicalDeviceSurfaceFormatsKHR( device.physicalDevice_, device.surface_, &formatCount, formats );
    LOGI( "Got %d formats", formatCount );

    uint32_t chosenFormat;
    for( chosenFormat = 0; chosenFormat < formatCount; chosenFormat++ )
    {
        if( formats[chosenFormat].format == VK_FORMAT_R8G8B8A8_UNORM )
            break;
    }
    assert( chosenFormat < formatCount );

    swapchain.displaySize_ = surfaceCapabilities.currentExtent;
    swapchain.displayFormat_ = formats[chosenFormat].format;

    // **********************************************************
    // Create a swap chain (here we choose the minimum available number of surface
    // in the chain)
    VkSwapchainCreateInfoKHR swapchainCreateInfo;
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.pNext = nullptr;
    swapchainCreateInfo.surface = device.surface_;
    swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount;
    swapchainCreateInfo.imageFormat = formats[chosenFormat].format;
    swapchainCreateInfo.imageColorSpace = formats[chosenFormat].colorSpace;
    swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.queueFamilyIndexCount = 1;
    swapchainCreateInfo.pQueueFamilyIndices = &device.queueFamilyIndex_;
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
    swapchainCreateInfo.clipped = VK_FALSE;

    CALL_VK( vkCreateSwapchainKHR( device.device_, &swapchainCreateInfo, nullptr, &swapchain.swapchain_ ) );

    // Get the length of the created swap chain
    CALL_VK( vkGetSwapchainImagesKHR( device.device_, swapchain.swapchain_, &swapchain.swapchainLength_, nullptr ) );
    delete[] formats;
    LOGI( "<-createSwapChain" );
}

void CreateRenderPass()
{
    // -----------------------------------------------------------------
    // Create render pass
    VkAttachmentDescription attachmentDescriptions;
    attachmentDescriptions.format = swapchain.displayFormat_;
    attachmentDescriptions.samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


    VkAttachmentReference colourReference;
    colourReference.attachment = 0;
    colourReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.flags = 0;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colourReference;
    subpassDescription.pResolveAttachments = nullptr;
    subpassDescription.pDepthStencilAttachment = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;

    VkRenderPassCreateInfo renderPassCreateInfo;
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.pNext = nullptr;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &attachmentDescriptions;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.dependencyCount = 0;
    renderPassCreateInfo.pDependencies = nullptr;

    CALL_VK( vkCreateRenderPass( device.device_, &renderPassCreateInfo, nullptr, &render.renderPass_ ) );
}

void CreateFrameBuffers( VkRenderPass& renderPass, VkImageView depthView = VK_NULL_HANDLE )
{
    // query display attachment to swapchain
    uint32_t SwapchainImagesCount = 0;
    CALL_VK( vkGetSwapchainImagesKHR( device.device_, swapchain.swapchain_, &SwapchainImagesCount, nullptr ) );
    swapchain.displayImages_ = new VkImage[SwapchainImagesCount];
    CALL_VK( vkGetSwapchainImagesKHR( device.device_, swapchain.swapchain_, &SwapchainImagesCount, swapchain.displayImages_ ) );

    // create image view for each swapchain image
    swapchain.displayViews_ = new VkImageView[SwapchainImagesCount];
    for( uint32_t i = 0; i < SwapchainImagesCount; i++ )
    {
        VkImageViewCreateInfo viewCreateInfo;
        viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCreateInfo.pNext = nullptr;
        viewCreateInfo.image = swapchain.displayImages_[i];
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = swapchain.displayFormat_;
        viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;

        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCreateInfo.subresourceRange.baseMipLevel = 0;
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = 1;

        viewCreateInfo.flags = 0;

        CALL_VK( vkCreateImageView( device.device_, &viewCreateInfo, nullptr, &swapchain.displayViews_[i] ) );
    }

    // create a framebuffer from each swapchain image
    swapchain.framebuffers_ = new VkFramebuffer[swapchain.swapchainLength_];
    for( uint32_t i = 0; i < swapchain.swapchainLength_; i++ )
    {
        VkImageView attachments[2] = { swapchain.displayViews_[i], depthView, };
        VkFramebufferCreateInfo fbCreateInfo;
        fbCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbCreateInfo.pNext = nullptr;
        fbCreateInfo.renderPass = renderPass;
        fbCreateInfo.layers = 1;
        fbCreateInfo.attachmentCount = 1,  // 2 if using dept;
                fbCreateInfo.pAttachments = attachments;
        fbCreateInfo.width = static_cast<uint32_t>(swapchain.displaySize_.width);
        fbCreateInfo.height = static_cast<uint32_t>(swapchain.displaySize_.height);
        fbCreateInfo.attachmentCount = ( depthView == VK_NULL_HANDLE ? 1 : 2 );

        CALL_VK( vkCreateFramebuffer( device.device_, &fbCreateInfo, nullptr, &swapchain.framebuffers_[i] ) );
    }
}

// A help function to map required memory property into a VK memory type
// memory type is an index into the array of 32 entries; or the bit index
// for the memory type ( each BIT of an 32 bit integer is a type ).
VkResult AllocateMemoryTypeFromProperties( uint32_t typeBits, VkFlags requirements_mask, uint32_t* typeIndex )
{
    // Search memtypes to find first index with those properties
    for( uint32_t i = 0; i < 32; i++ )
    {
        if( ( typeBits & 1 ) == 1 )
        {
            // Type is available, does it match user properties?
            if( ( device.gpuMemoryProperties_.memoryTypes[i].propertyFlags & requirements_mask ) == requirements_mask )
            {
                *typeIndex = i;
                return VK_SUCCESS;
            }
        }
        typeBits >>= 1;
    }
    // No memory types matched, return failure
    return VK_ERROR_MEMORY_MAP_FAILED;
}

VkResult LoadTextureFromFile( const char* filePath, struct texture_object* tex_obj, VkImageUsageFlags usage, VkFlags required_props )
{
    if( !( usage | required_props ) )
    {
        __android_log_print( ANDROID_LOG_ERROR, "tutorial texture", "No usage and required_pros" );
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    // Check for linear supportability
    VkFormatProperties props;
    bool needBlit = true;
    vkGetPhysicalDeviceFormatProperties( device.physicalDevice_, kTexFmt, &props );
    assert( ( props.linearTilingFeatures | props.optimalTilingFeatures ) & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT );

    if( props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT )
    {
        // linear format supporting the required texture
        needBlit = false;
    }

    // Read the file:
    AAsset* file = AAssetManager_open( androidAppCtx->activity->assetManager, filePath, AASSET_MODE_BUFFER );
    size_t fileLength = AAsset_getLength( file );
    stbi_uc* fileContent = new unsigned char[fileLength];
    AAsset_read( file, fileContent, fileLength );
    AAsset_close( file );

    uint32_t imgWidth, imgHeight, n;
    unsigned char* imageData = stbi_load_from_memory( fileContent, fileLength, reinterpret_cast<int*>(&imgWidth), reinterpret_cast<int*>(&imgHeight), reinterpret_cast<int*>(&n), 4 );
    assert( n == 4 );

    tex_obj->tex_width = imgWidth;
    tex_obj->tex_height = imgHeight;

    // Allocate the linear texture so texture could be copied over
    VkImageCreateInfo image_create_info;
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.pNext = nullptr;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = kTexFmt;
    image_create_info.extent = { static_cast<uint32_t>(imgWidth), static_cast<uint32_t>(imgHeight), 1 };
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_LINEAR;
    image_create_info.usage = ( needBlit ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : VK_IMAGE_USAGE_SAMPLED_BIT );
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 1;
    image_create_info.pQueueFamilyIndices = &device.queueFamilyIndex_;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    image_create_info.flags = 0;

    VkMemoryAllocateInfo mem_alloc;
    mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_alloc.pNext = nullptr;
    mem_alloc.allocationSize = 0;
    mem_alloc.memoryTypeIndex = 0;

    VkMemoryRequirements mem_reqs;
    CALL_VK( vkCreateImage( device.device_, &image_create_info, nullptr, &tex_obj->image ) );
    vkGetImageMemoryRequirements( device.device_, tex_obj->image, &mem_reqs );
    mem_alloc.allocationSize = mem_reqs.size;
    VK_CHECK( AllocateMemoryTypeFromProperties( mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &mem_alloc.memoryTypeIndex ) );
    CALL_VK( vkAllocateMemory( device.device_, &mem_alloc, nullptr, &tex_obj->mem ) );
    CALL_VK( vkBindImageMemory( device.device_, tex_obj->image, tex_obj->mem, 0 ) );

    if( required_props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT )
    {
        const VkImageSubresource subres = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .arrayLayer = 0, };
        VkSubresourceLayout layout;
        void* data;

        vkGetImageSubresourceLayout( device.device_, tex_obj->image, &subres, &layout );
        CALL_VK( vkMapMemory( device.device_, tex_obj->mem, 0, mem_alloc.allocationSize, 0, &data ) );

        for( int32_t y = 0; y < imgHeight; y++ )
        {
            unsigned char* row = ( unsigned char* ) ( ( char* ) data + layout.rowPitch * y );
            for( int32_t x = 0; x < imgWidth; x++ )
            {
                row[x * 4] = imageData[( x + y * imgWidth ) * 4];
                row[x * 4 + 1] = imageData[( x + y * imgWidth ) * 4 + 1];
                row[x * 4 + 2] = imageData[( x + y * imgWidth ) * 4 + 2];
                row[x * 4 + 3] = imageData[( x + y * imgWidth ) * 4 + 3];
            }
        }

        vkUnmapMemory( device.device_, tex_obj->mem );
        stbi_image_free( imageData );
    }
    delete[] fileContent;

    tex_obj->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkCommandPoolCreateInfo cmdPoolCreateInfo;
    cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolCreateInfo.pNext = nullptr;
    cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolCreateInfo.queueFamilyIndex = device.queueFamilyIndex_;

    VkCommandPool cmdPool;
    CALL_VK( vkCreateCommandPool( device.device_, &cmdPoolCreateInfo, nullptr, &cmdPool ) );

    VkCommandBuffer gfxCmd;
    VkCommandBufferAllocateInfo cmd;
    cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd.pNext = nullptr;
    cmd.commandPool = cmdPool;
    cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd.commandBufferCount = 1;

    CALL_VK( vkAllocateCommandBuffers( device.device_, &cmd, &gfxCmd ) );
    VkCommandBufferBeginInfo cmd_buf_info;
    cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buf_info.pNext = nullptr;
    cmd_buf_info.flags = 0;
    cmd_buf_info.pInheritanceInfo = nullptr;
    CALL_VK( vkBeginCommandBuffer( gfxCmd, &cmd_buf_info ) );

    // If linear is supported, we are done
    VkImage stageImage = VK_NULL_HANDLE;
    VkDeviceMemory stageMem = VK_NULL_HANDLE;
    if( !needBlit )
    {
        setImageLayout( gfxCmd, tex_obj->image, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
    }
    else
    {
        // save current image and mem as staging image and memory
        stageImage = tex_obj->image;
        stageMem = tex_obj->mem;
        tex_obj->image = VK_NULL_HANDLE;
        tex_obj->mem = VK_NULL_HANDLE;

        // Create a tile texture to blit into
        image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        CALL_VK( vkCreateImage( device.device_, &image_create_info, nullptr, &tex_obj->image ) );
        vkGetImageMemoryRequirements( device.device_, tex_obj->image, &mem_reqs );

        mem_alloc.allocationSize = mem_reqs.size;
        VK_CHECK( AllocateMemoryTypeFromProperties( mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &mem_alloc.memoryTypeIndex ) );
        CALL_VK( vkAllocateMemory( device.device_, &mem_alloc, nullptr, &tex_obj->mem ) );
        CALL_VK( vkBindImageMemory( device.device_, tex_obj->image, tex_obj->mem, 0 ) );

        // transitions image out of UNDEFINED type
        setImageLayout( gfxCmd, stageImage, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
        setImageLayout( gfxCmd, tex_obj->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
        VkImageCopy bltInfo;
        bltInfo.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, bltInfo.srcSubresource.mipLevel = 0, bltInfo.srcSubresource.baseArrayLayer = 0, bltInfo.srcSubresource.layerCount = 1, bltInfo.srcOffset.x = 0, bltInfo.srcOffset.y = 0, bltInfo.srcOffset.z = 0, bltInfo.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, bltInfo.dstSubresource.mipLevel = 0, bltInfo.dstSubresource.baseArrayLayer = 0, bltInfo.dstSubresource.layerCount = 1, bltInfo.dstOffset.x = 0, bltInfo.dstOffset.y = 0, bltInfo.dstOffset.z = 0, bltInfo.extent.width = imgWidth, bltInfo.extent.height = imgHeight, bltInfo.extent.depth = 1,

                vkCmdCopyImage( gfxCmd, stageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tex_obj->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bltInfo );

        setImageLayout( gfxCmd, tex_obj->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
    }

    CALL_VK( vkEndCommandBuffer( gfxCmd ) );
    VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = 0, };
    VkFence fence;
    CALL_VK( vkCreateFence( device.device_, &fenceInfo, nullptr, &fence ) );

    VkSubmitInfo submitInfo;
    submitInfo.pNext = nullptr;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &gfxCmd;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;

    CALL_VK( vkQueueSubmit( device.queue_, 1, &submitInfo, fence ) != VK_SUCCESS );
    CALL_VK( vkWaitForFences( device.device_, 1, &fence, VK_TRUE, 100000000 ) != VK_SUCCESS );
    vkDestroyFence( device.device_, fence, nullptr );

    vkFreeCommandBuffers( device.device_, cmdPool, 1, &gfxCmd );
    vkDestroyCommandPool( device.device_, cmdPool, nullptr );
    if( stageImage != VK_NULL_HANDLE )
    {
        vkDestroyImage( device.device_, stageImage, nullptr );
        vkFreeMemory( device.device_, stageMem, nullptr );
    }
    return VK_SUCCESS;
}

void CreateTexture( void )
{
    for( uint32_t i = 0; i < TUTORIAL_TEXTURE_COUNT; i++ )
    {
        LoadTextureFromFile( texFiles[i], &textures[i], VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT );

        VkSamplerCreateInfo sampler;
        sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler.pNext = nullptr;
        sampler.magFilter = VK_FILTER_NEAREST;
        sampler.minFilter = VK_FILTER_NEAREST;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.mipLodBias = 0.0f;
        sampler.maxAnisotropy = 1;
        sampler.compareOp = VK_COMPARE_OP_NEVER;
        sampler.minLod = 0.0f;
        sampler.maxLod = 0.0f;
        sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        sampler.unnormalizedCoordinates = VK_FALSE;

        VkImageViewCreateInfo view;
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.pNext = nullptr;
        view.image = VK_NULL_HANDLE;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = kTexFmt;
        view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A, }, view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, view.flags = 0;

        CALL_VK( vkCreateSampler( device.device_, &sampler, nullptr, &textures[i].sampler ) );
        view.image = textures[i].image;
        CALL_VK( vkCreateImageView( device.device_, &view, nullptr, &textures[i].view ) );
    }
}

bool MapMemoryTypeToIndex( uint32_t typeBits, VkFlags requirements_mask, uint32_t* typeIndex )
{
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties( device.physicalDevice_, &memoryProperties );
    // Search memtypes to find first index with those properties
    for( uint32_t i = 0; i < 32; i++ )
    {
        if( ( typeBits & 1 ) == 1 )
        {
            // Type is available, does it match user properties?
            if( ( memoryProperties.memoryTypes[i].propertyFlags & requirements_mask ) == requirements_mask )
            {
                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }
    return false;
}

bool CreateBuffers( void )
{
    // Vertex positions
    const float vertexData[] = { -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f, };

    // Create a vertex buffer
    VkBufferCreateInfo createBufferInfo;
    createBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createBufferInfo.pNext = nullptr;
    createBufferInfo.size = sizeof( vertexData );
    createBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    createBufferInfo.flags = 0;
    createBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createBufferInfo.queueFamilyIndexCount = 1;
    createBufferInfo.pQueueFamilyIndices = &device.queueFamilyIndex_;

    CALL_VK( vkCreateBuffer( device.device_, &createBufferInfo, nullptr, &buffers.vertexBuf_ ) );

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements( device.device_, buffers.vertexBuf_, &memReq );

    VkMemoryAllocateInfo allocInfo;
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = 0;

    MapMemoryTypeToIndex( memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &allocInfo.memoryTypeIndex );

    VkDeviceMemory deviceMemory;
    CALL_VK( vkAllocateMemory( device.device_, &allocInfo, nullptr, &deviceMemory ) );

    void* data;
    CALL_VK( vkMapMemory( device.device_, deviceMemory, 0, allocInfo.allocationSize, 0, &data ) );
    memcpy( data, vertexData, sizeof( vertexData ) );
    vkUnmapMemory( device.device_, deviceMemory );

    CALL_VK( vkBindBufferMemory( device.device_, buffers.vertexBuf_, deviceMemory, 0 ) );
    return true;
}

void CreateGraphicsPipeline( void )
{
    memset( &gfxPipeline, 0, sizeof( gfxPipeline ) );

    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding;
    descriptorSetLayoutBinding.binding = 0;
    descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorSetLayoutBinding.descriptorCount = TUTORIAL_TEXTURE_COUNT;
    descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    descriptorSetLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pNext = nullptr;
    descriptorSetLayoutCreateInfo.bindingCount = 1;
    descriptorSetLayoutCreateInfo.pBindings = &descriptorSetLayoutBinding;

    CALL_VK( vkCreateDescriptorSetLayout( device.device_, &descriptorSetLayoutCreateInfo, nullptr, &gfxPipeline.dscLayout_ ) );
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = nullptr;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &gfxPipeline.dscLayout_;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    CALL_VK( vkCreatePipelineLayout( device.device_, &pipelineLayoutCreateInfo, nullptr, &gfxPipeline.layout_ ) );

    // No dynamic state in that tutorial
    VkPipelineDynamicStateCreateInfo dynamicStateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .pNext = nullptr, .dynamicStateCount = 0, .pDynamicStates = nullptr };

    VkShaderModule vertexShader, fragmentShader;
    buildShaderFromFile( androidAppCtx, "shaders/tri.vert", VK_SHADER_STAGE_VERTEX_BIT, device.device_, &vertexShader );
    buildShaderFromFile( androidAppCtx, "shaders/tri.frag", VK_SHADER_STAGE_FRAGMENT_BIT, device.device_, &fragmentShader );
    // Specify vertex and fragment shader stages
    VkPipelineShaderStageCreateInfo shaderStages[2];

    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].pNext = nullptr;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertexShader;
    shaderStages[0].pSpecializationInfo = nullptr;
    shaderStages[0].flags = 0;
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].pNext = nullptr;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragmentShader;
    shaderStages[1].pSpecializationInfo = nullptr;
    shaderStages[1].flags = 0;
    shaderStages[1].pName = "main";

    VkViewport viewports;
    viewports.minDepth = 0.0f;
    viewports.maxDepth = 1.0f;
    viewports.x = 0;
    viewports.y = 0;
    viewports.width = ( float ) swapchain.displaySize_.width;
    viewports.height = ( float ) swapchain.displaySize_.height;

    VkRect2D scissor;
    scissor.extent = swapchain.displaySize_;
    scissor.offset = { .x = 0, .y = 0, };

    // Specify viewport info
    VkPipelineViewportStateCreateInfo viewportInfo;
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.pNext = nullptr;
    viewportInfo.viewportCount = 1;
    viewportInfo.pViewports = &viewports;
    viewportInfo.scissorCount = 1;
    viewportInfo.pScissors = &scissor;

    VkSampleMask sampleMask = ~0u;
    VkPipelineMultisampleStateCreateInfo multisampleInfo;
    multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleInfo.pNext = nullptr;
    multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleInfo.sampleShadingEnable = VK_FALSE;
    multisampleInfo.minSampleShading = 0;
    multisampleInfo.pSampleMask = &sampleMask;
    multisampleInfo.alphaToCoverageEnable = VK_FALSE;
    multisampleInfo.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState attachmentStates{ .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, .blendEnable = VK_FALSE, };
    VkPipelineColorBlendStateCreateInfo colorBlendInfo;
    colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendInfo.pNext = nullptr;
    colorBlendInfo.logicOpEnable = VK_FALSE;
    colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &attachmentStates;
    colorBlendInfo.flags = 0;

    VkPipelineRasterizationStateCreateInfo rasterInfo;
    rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterInfo.pNext = nullptr;
    rasterInfo.depthClampEnable = VK_FALSE;
    rasterInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterInfo.cullMode = VK_CULL_MODE_NONE;
    rasterInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterInfo.depthBiasEnable = VK_FALSE;
    rasterInfo.lineWidth = 1;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.pNext = nullptr;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    VkVertexInputBindingDescription vertex_input_bindings;
    vertex_input_bindings.binding = 0;
    vertex_input_bindings.stride = 5 * sizeof( float );
    vertex_input_bindings.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription vertex_input_attributes[2];

    vertex_input_attributes[0].binding = 0;
    vertex_input_attributes[0].location = 0;
    vertex_input_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_input_attributes[0].offset = 0;

    vertex_input_attributes[1].binding = 0;
    vertex_input_attributes[1].location = 1;
    vertex_input_attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_input_attributes[1].offset = sizeof( float ) * 3;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.pNext = nullptr;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertex_input_bindings;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = vertex_input_attributes;

    VkPipelineCacheCreateInfo pipelineCacheInfo;
    pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipelineCacheInfo.pNext = nullptr;
    pipelineCacheInfo.initialDataSize = 0;
    pipelineCacheInfo.pInitialData = nullptr;
    pipelineCacheInfo.flags = 0;  // reserved, must be 0

    CALL_VK( vkCreatePipelineCache( device.device_, &pipelineCacheInfo, nullptr, &gfxPipeline.cache_ ) );

    VkGraphicsPipelineCreateInfo pipelineCreateInfo;
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.pNext = nullptr;
    pipelineCreateInfo.flags = 0;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineCreateInfo.pTessellationState = nullptr;
    pipelineCreateInfo.pViewportState = &viewportInfo;
    pipelineCreateInfo.pRasterizationState = &rasterInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleInfo;
    pipelineCreateInfo.pDepthStencilState = nullptr;
    pipelineCreateInfo.pColorBlendState = &colorBlendInfo;
    pipelineCreateInfo.pDynamicState = &dynamicStateInfo;
    pipelineCreateInfo.layout = gfxPipeline.layout_;
    pipelineCreateInfo.renderPass = render.renderPass_;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = 0;


    CALL_VK( vkCreateGraphicsPipelines( device.device_, gfxPipeline.cache_, 1, &pipelineCreateInfo, nullptr, &gfxPipeline.pipeline_ ) );

    vkDestroyShaderModule( device.device_, vertexShader, nullptr );
    vkDestroyShaderModule( device.device_, fragmentShader, nullptr );
}

VkResult CreateDescriptorSet( void )
{
    VkDescriptorPoolSize type_count;
    type_count.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    type_count.descriptorCount = TUTORIAL_TEXTURE_COUNT;

    VkDescriptorPoolCreateInfo descriptor_pool;
    descriptor_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool.pNext = nullptr;
    descriptor_pool.maxSets = 1;
    descriptor_pool.poolSizeCount = 1;
    descriptor_pool.pPoolSizes = &type_count;


    CALL_VK( vkCreateDescriptorPool( device.device_, &descriptor_pool, nullptr, &gfxPipeline.descPool_ ) );

    VkDescriptorSetAllocateInfo alloc_info;
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.descriptorPool = gfxPipeline.descPool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &gfxPipeline.dscLayout_;
    CALL_VK( vkAllocateDescriptorSets( device.device_, &alloc_info, &gfxPipeline.descSet_ ) );

    VkDescriptorImageInfo texDsts[TUTORIAL_TEXTURE_COUNT];
    memset( texDsts, 0, sizeof( texDsts ) );
    for( int32_t idx = 0; idx < TUTORIAL_TEXTURE_COUNT; idx++ )
    {
        texDsts[idx].sampler = textures[idx].sampler;
        texDsts[idx].imageView = textures[idx].view;
        texDsts[idx].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }

    VkWriteDescriptorSet writeDst;
    writeDst.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDst.pNext = nullptr;
    writeDst.dstSet = gfxPipeline.descSet_;
    writeDst.dstBinding = 0;
    writeDst.dstArrayElement = 0;
    writeDst.descriptorCount = TUTORIAL_TEXTURE_COUNT;
    writeDst.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDst.pImageInfo = texDsts;
    writeDst.pBufferInfo = nullptr;
    writeDst.pTexelBufferView = nullptr;
    vkUpdateDescriptorSets( device.device_, 1, &writeDst, 0, nullptr );
    return VK_SUCCESS;
}

bool VulkanDrawFrame( void )
{
    uint32_t nextIndex;
    // Get the framebuffer index we should draw in
    CALL_VK( vkAcquireNextImageKHR( device.device_, swapchain.swapchain_, UINT64_MAX, render.semaphore_, VK_NULL_HANDLE, &nextIndex ) );
    CALL_VK( vkResetFences( device.device_, 1, &render.fence_ ) );

    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &render.semaphore_;
    submit_info.pWaitDstStageMask = &waitStageMask;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &render.cmdBuffer_[nextIndex];
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores = nullptr;
    CALL_VK( vkQueueSubmit( device.queue_, 1, &submit_info, render.fence_ ) );
    CALL_VK( vkWaitForFences( device.device_, 1, &render.fence_, VK_TRUE, 100000000 ) );

    LOGI( "Drawing frames......" );

    VkResult result;
    VkPresentInfoKHR presentInfo;
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain.swapchain_;
    presentInfo.pImageIndices = &nextIndex;
    presentInfo.waitSemaphoreCount = 0;
    presentInfo.pWaitSemaphores = nullptr;
    presentInfo.pResults = &result;

    vkQueuePresentKHR( device.queue_, &presentInfo );
    return true;
}

bool InitVulkan( android_app* app )
{
    androidAppCtx = app;

    if( !InitVulkan() )
    {
        LOGW( "Vulkan is unavailable, install vulkan and re-start" );
        return false;
    }

    VkApplicationInfo appInfo;
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.apiVersion = VK_MAKE_VERSION( 1, 0, 0 );
    appInfo.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
    appInfo.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
    appInfo.pApplicationName = "tutorial05_triangle_window";
    appInfo.pEngineName = "tutorial";

    // create a device
    CreateVulkanDevice( app->window, &appInfo );

    CreateSwapChain();

    CreateRenderPass();

    CreateFrameBuffers( render.renderPass_ );
    CreateTexture();
    CreateBuffers();

    // Create graphics pipeline
    CreateGraphicsPipeline();

    CreateDescriptorSet();

    // -----------------------------------------------
    // Create a pool of command buffers to allocate command buffer from
    VkCommandPoolCreateInfo cmdPoolCreateInfo;
    cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolCreateInfo.pNext = nullptr;
    cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolCreateInfo.queueFamilyIndex = 0;

    CALL_VK( vkCreateCommandPool( device.device_, &cmdPoolCreateInfo, nullptr, &render.cmdPool_ ) );

    // Record a command buffer that just clear the screen
    // 1 command buffer draw in 1 framebuffer
    // In our case we need 2 command as we have 2 framebuffer
    render.cmdBufferLen_ = swapchain.swapchainLength_;
    render.cmdBuffer_ = new VkCommandBuffer[swapchain.swapchainLength_];
    VkCommandBufferAllocateInfo cmdBufferCreateInfo;
    cmdBufferCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufferCreateInfo.pNext = nullptr;
    cmdBufferCreateInfo.commandPool = render.cmdPool_;
    cmdBufferCreateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufferCreateInfo.commandBufferCount = render.cmdBufferLen_;

    CALL_VK( vkAllocateCommandBuffers( device.device_, &cmdBufferCreateInfo, render.cmdBuffer_ ) );

    for( int bufferIndex = 0; bufferIndex < swapchain.swapchainLength_; bufferIndex++ )
    {
        // We start by creating and declare the "beginning" our command buffer
        VkCommandBufferBeginInfo cmdBufferBeginInfo;
        cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBufferBeginInfo.pNext = nullptr;
        cmdBufferBeginInfo.flags = 0;
        cmdBufferBeginInfo.pInheritanceInfo = nullptr;

        CALL_VK( vkBeginCommandBuffer( render.cmdBuffer_[bufferIndex], &cmdBufferBeginInfo ) );

        // transition the buffer into color attachment
        setImageLayout( render.cmdBuffer_[bufferIndex], swapchain.displayImages_[bufferIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );

        // Now we start a renderpass. Any draw command has to be recorded in a
        // renderpass
        VkClearValue clearVals;
        clearVals.color.float32[0] = 0.0f;
        clearVals.color.float32[1] = 0.34f;
        clearVals.color.float32[2] = 0.90f;
        clearVals.color.float32[3] = 1.0f;


        VkRenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.renderPass = render.renderPass_;
        renderPassBeginInfo.framebuffer = swapchain.framebuffers_[bufferIndex];
        renderPassBeginInfo.renderArea = { .offset =
                { .x = 0, .y = 0, }, .extent = swapchain.displaySize_ }, renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearVals;
        vkCmdBeginRenderPass( render.cmdBuffer_[bufferIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE );
        // Bind what is necessary to the command buffer
        vkCmdBindPipeline( render.cmdBuffer_[bufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline.pipeline_ );
        vkCmdBindDescriptorSets( render.cmdBuffer_[bufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline.layout_, 0, 1, &gfxPipeline.descSet_, 0, nullptr );
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers( render.cmdBuffer_[bufferIndex], 0, 1, &buffers.vertexBuf_, &offset );

        // Draw Triangle
        vkCmdDraw( render.cmdBuffer_[bufferIndex], 3, 1, 0, 0 );

        vkCmdEndRenderPass( render.cmdBuffer_[bufferIndex] );
        setImageLayout( render.cmdBuffer_[bufferIndex], swapchain.displayImages_[bufferIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT );
        CALL_VK( vkEndCommandBuffer( render.cmdBuffer_[bufferIndex] ) );
    }

    // We need to create a fence to be able, in the main loop, to wait for our
    // draw command(s) to finish before swapping the framebuffers
    VkFenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = 0;

    CALL_VK( vkCreateFence( device.device_, &fenceCreateInfo, nullptr, &render.fence_ ) );

    // We need to create a semaphore to be able to wait, in the main loop, for our
    // framebuffer to be available for us before drawing.
    VkSemaphoreCreateInfo semaphoreCreateInfo;
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0;

    CALL_VK( vkCreateSemaphore( device.device_, &semaphoreCreateInfo, nullptr, &render.semaphore_ ) );

    device.initialized_ = true;
    return true;
}

bool IsVulkanReady( void )
{
    return device.initialized_;
}

void DeleteSwapChain( void )
{
    for( int i = 0; i < swapchain.swapchainLength_; i++ )
    {
        vkDestroyFramebuffer( device.device_, swapchain.framebuffers_[i], nullptr );
        vkDestroyImageView( device.device_, swapchain.displayViews_[i], nullptr );
    }
    delete[] swapchain.framebuffers_;
    delete[] swapchain.displayViews_;
    delete[] swapchain.displayImages_;

    vkDestroySwapchainKHR( device.device_, swapchain.swapchain_, nullptr );
}

void DeleteBuffers( void )
{
    vkDestroyBuffer( device.device_, buffers.vertexBuf_, nullptr );
}

void DeleteGraphicsPipeline( void )
{
    if( gfxPipeline.pipeline_ == VK_NULL_HANDLE )
        return;
    vkDestroyPipeline( device.device_, gfxPipeline.pipeline_, nullptr );
    vkDestroyPipelineCache( device.device_, gfxPipeline.cache_, nullptr );
    vkFreeDescriptorSets( device.device_, gfxPipeline.descPool_, 1, &gfxPipeline.descSet_ );
    vkDestroyDescriptorPool( device.device_, gfxPipeline.descPool_, nullptr );
    vkDestroyPipelineLayout( device.device_, gfxPipeline.layout_, nullptr );
}

void DeleteVulkan()
{
    vkFreeCommandBuffers( device.device_, render.cmdPool_, render.cmdBufferLen_, render.cmdBuffer_ );
    delete[] render.cmdBuffer_;

    vkDestroyCommandPool( device.device_, render.cmdPool_, nullptr );
    vkDestroyRenderPass( device.device_, render.renderPass_, nullptr );
    DeleteSwapChain();
    DeleteGraphicsPipeline();
    DeleteBuffers();

    vkDestroyDevice( device.device_, nullptr );
    vkDestroyInstance( device.instance_, nullptr );

    device.initialized_ = false;
}

void setImageLayout( VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStages, VkPipelineStageFlags destStages )
{
    VkImageMemoryBarrier imageMemoryBarrier;
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.pNext = NULL;
    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstAccessMask = 0;
    imageMemoryBarrier.oldLayout = oldImageLayout;
    imageMemoryBarrier.newLayout = newImageLayout;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = image;


    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount = 1;


    switch( oldImageLayout )
    {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        default:
            break;
    }

    switch( newImageLayout )
    {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        default:
            break;
    }

    vkCmdPipelineBarrier( cmdBuffer, srcStages, destStages, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier );
}
