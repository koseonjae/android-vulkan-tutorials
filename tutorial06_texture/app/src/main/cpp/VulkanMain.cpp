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
    VkColorSpaceKHR colorSpace_;
    std::vector<VkImage> displayImages_;
    std::vector<VkImageView> displayViews_;
    std::vector<VkFramebuffer> framebuffers_;
};
VulkanSwapchainInfo swapchain;

struct TextureObject
{
    VkSampler sampler_;
    VkImage image_;
    VkImageLayout imageLayout_;
    VkDeviceMemory deviceMemory_;
    VkImageView imageView_;
    int32_t width_;
    int32_t height_;
};
static const VkFormat kTexFmt = VK_FORMAT_R8G8B8A8_UNORM;
#define TUTORIAL_TEXTURE_COUNT 1
const char* texFiles[TUTORIAL_TEXTURE_COUNT] = { "sample_tex.png", };
struct TextureObject textures[TUTORIAL_TEXTURE_COUNT];

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

void CreateVulkanDevice( ANativeWindow* platformWindow )
{
    // instance         : vulkan instance. surface와 physical device 생성에 쓰임
    // android surface  : ANativeWindow와 vulkan instance를 통해 surface 생성
    // physical device  : gpu. 메모리 정보와 command submit을 위한 queue 정보를 얻는데 쓰임
    // queue family     : queue family는 동일한 property를 가진 queue들의 집합이다. (queue의 property에 따라 수행할 수 있는 command의 종류가 다르다.)
    //                  : 여기에선 graphics property를 가진 큐들의 집합(queue family)을 구해서 사용한다. => graphics command를 submit할꺼니까
    // device           : graphics queue property를 가진 queue family를 가지고 device를 초기화 했음 -> graphics용 device 초기화
    // layer            : 특정한(주로 검증) 목적을 위해 구성된 vulkan 소프트웨어 계층
    //                  : 레이어는 기존 vulkan api에 연결되고, 지정된 레이어와 연결된 vulkan 명령체인에 삽입됨
    //                  : 예를들어 vulkan api로 올바른 파라미터가 들어오는지 검증한다.
    //                  : 이러한 레이어는 릴리즈에선 사용하지 않게 설정하면 불필요한 오버헤드를 줄일 수 있다.
    // Commands that enumerate instance properties, or that accept a VkInstance object as a parameter, are considered instance-level functionality.
    // Commands that enumerate physical device properties, or that accept a VkDevice object or any of a device’s child objects as a parameter,
    // are considered device-level functionality.

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
    // GPU가 android surface에게 지원하는 capability를 가져온다.
    // GPU가 android surface에게 지원하는 format을 가져온다. => VK_FORMAT_R8G8B8_UNORM format에 대한 index를 얻는다.
    // => capability와 format 정보를 통해 swapchain을 생성한다

    uint32_t formatCount{ 0 };
    vkGetPhysicalDeviceSurfaceFormatsKHR( device.physicalDevice_, device.surface_, &formatCount, nullptr );
    vector<VkSurfaceFormatKHR> formats( formatCount );
    vkGetPhysicalDeviceSurfaceFormatsKHR( device.physicalDevice_, device.surface_, &formatCount, formats.data() );

    auto found = std::find_if( formats.begin(), formats.end(), [=]( const VkSurfaceFormatKHR& format ) {
        return format.format == VK_FORMAT_R8G8B8A8_UNORM;
    } );
    assert( found != formats.end() );
    VkSurfaceFormatKHR format = *found;

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device.physicalDevice_, device.surface_, &capabilities );

    swapchain.swapchainLength_ = capabilities.minImageCount;
    swapchain.displaySize_ = capabilities.currentExtent;
    swapchain.displayFormat_ = format.format;
    swapchain.colorSpace_ = format.colorSpace;

    VkSwapchainCreateInfoKHR swapchainCreateInfo;
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.pNext = nullptr;
    swapchainCreateInfo.flags = 0;
    swapchainCreateInfo.surface = device.surface_;
    swapchainCreateInfo.minImageCount = swapchain.swapchainLength_;
    swapchainCreateInfo.imageFormat = swapchain.displayFormat_;
    swapchainCreateInfo.imageColorSpace = swapchain.colorSpace_;
    swapchainCreateInfo.imageExtent = swapchain.displaySize_;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.queueFamilyIndexCount = 1;
    swapchainCreateInfo.pQueueFamilyIndices = &device.queueFamilyIndex_;
    swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
    vkCreateSwapchainKHR( device.device_, &swapchainCreateInfo, nullptr, &swapchain.swapchain_ );
}

void CreateRenderPass()
{
    // https://vulkan.lunarg.com/doc/view/1.0.37.0/linux/vkspec.chunked/ch07.html
    // https://lifeisforu.tistory.com/462
    // renderpass dependency    : 렌더패스가 사용하는 attachment들의 종속성에 의해 렌더패스간의 종속성이 결정된다.
    // attachment description   : 렌더패스에 attachment를 지정할때의 속성. (포맷, 용도, MSAA, load clear op, save or not, layout etc..)
    // renderpass object        : vkCreateRenderPass에 의해 생성되는 렌더패스객체는 템플릿으로써 존재함.
    //                          : VkCmdBeginRenderPass가 호출될때 실제 인스턴스가 생성되고, 각 어태치먼트와 관련된 리소스들을 프레임버퍼로 바인딩합니다
    // subpass                  : deferred shading 같은 여러개의 파이프라인을 거칠때, 서브패스를 추가하여 renderpass를 구성할 수 있다.
    //                          : subpass는 정확히 해당 픽셀에만 접근이 가능하고, 주변 픽셀엔 접근이 불가능하다는 제약이 있다. -> blur 같은 효과를 할 수 없음(주변픽셀에도 접근해야하니까)

    // renderpass command를 위해 기본적으로 세 객체가 필요: renderpass, framebuffer, command
    // 이것의 장점 -> no validation, no exception & dependency management & life cycle management

    // no validation no exception   : render pass 이외에도 descriptor-instance 쌍을 이루는 경우가 많음 (예를들어 descriptor set layout)
    //                              : descriptor가 존재하는 이유는 vulkan이 리소스들의 메모리구조를 알지 못하기 때문이다.
    //                              : descriptor가 메모리에 대한 모든 정보를 가지고 있다.
    //                              : 이렇게하면 개체를 생성하는 시점에 validation을 수행가능 (리소스를 바인딩하는 시점에서 API 내부적인 검증을 할 필요가 없다.)
    //                              : => 성능상의 이점
    // dependency management        : 커맨드 버퍼를 통해 렌더패스간 의존성을 관리 (의존성이 있는 렌더패스를 가지고 있는 커맨드버퍼들을 동기화)
    // life cycle management        : 멀티스레딩 환경에서 렌더패스 인스턴스, 커맨드 버퍼, 프레임 버퍼 등의 생명주기를 관리하는데 용이

    VkAttachmentDescription colorAttachmentDescription;
    colorAttachmentDescription.flags = 0;
    colorAttachmentDescription.format = swapchain.displayFormat_;
    colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentReference;
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription;
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    subpassDescription.pResolveAttachments = 0;
    subpassDescription.pDepthStencilAttachment = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;

    VkRenderPassCreateInfo renderPassCreateInfo;
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.pNext = nullptr;
    renderPassCreateInfo.flags = 0;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &colorAttachmentDescription;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.dependencyCount = 0;
    renderPassCreateInfo.pDependencies = nullptr;
    vkCreateRenderPass( device.device_, &renderPassCreateInfo, nullptr, &render.renderPass_ );
}

void CreateFrameBuffers( VkImageView depthView = VK_NULL_HANDLE )
{
    // https://stackoverflow.com/questions/39557141/what-is-the-difference-between-framebuffer-and-image-in-vulkan
    // VkImage          : 어떤 VkMemory가 사용되는지와, 어떤 texel format인지를 정의한다.
    //                  : swapchain이 생성될때 내부적으로 swapchainLen만큼 image생성 (swapchain을 생성할때 VkImage 생성에 대한 정보를 넘겨줬음)
    // VkImageView      : VkImage의 어느 부분을 사용할지 정의한다. & 호환불가능한 interface와 매치할 수 있도록 정의 (format 변환을 통해)
    //                  : image로부터 imageView생성
    // VKFramebuffer    : 어떤 imageView가 attachment가 될 것이며, 어떤 format으로 쓰일지 결정한다.

    // Swapchain Image  : 스왑 체인 이미지는 드라이버가 소유권을 가지고 있으며 할당, 해제할 수 없다.
    //                  : 단지 acquire & present operation 할때 잠시 빌려서 쓰는것 뿐임

    // baseArrayLayer   : VkImage
    //                      : imageArrayLayers  : VkImage가 갖는 image의 수 (multi view나 stereo surface가 아니면 1 사용)
    //                  : VkImageSubresourceRange
    //                      : layerCount        : VkImage가 멀티뷰일때 그중 몇개의 이미지를 사용하는가
    //                      : baseArrayLayer    : 사용하는 이미지들(imageArrayLayers)중 몇개의 이미지를 접근 가능한 이미지로 지정할것인가

    swapchain.displayImages_.resize( swapchain.swapchainLength_ );
    swapchain.displayViews_.resize( swapchain.swapchainLength_ );
    swapchain.framebuffers_.resize( swapchain.swapchainLength_ );
    vkGetSwapchainImagesKHR( device.device_, swapchain.swapchain_, &swapchain.swapchainLength_, swapchain.displayImages_.data() );

    for( uint32_t i = 0; i < swapchain.swapchainLength_; ++i )
    {
        VkImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = nullptr;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.image = swapchain.displayImages_.at( i );
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = swapchain.displayFormat_;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        vkCreateImageView( device.device_, &imageViewCreateInfo, nullptr, &swapchain.displayViews_.at( i ) );

        vector<VkImageView> views{ swapchain.displayViews_.at( i ) };
        if( depthView )
        {
            views.push_back( depthView );
        }

        VkFramebufferCreateInfo framebufferCreateInfo;
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.pNext = nullptr;
        framebufferCreateInfo.flags = 0;
        framebufferCreateInfo.renderPass = render.renderPass_;
        framebufferCreateInfo.attachmentCount = views.size();
        framebufferCreateInfo.pAttachments = views.data();
        framebufferCreateInfo.width = swapchain.displaySize_.width;
        framebufferCreateInfo.height = swapchain.displaySize_.height;
        framebufferCreateInfo.layers = 1;
        vkCreateFramebuffer( device.device_, &framebufferCreateInfo, nullptr, &swapchain.framebuffers_.at( i ) );
    }
}

// A help function to map required memory property into a VK memory type
// memory type is an index into the array of 32 entries; or the bit index
// for the memory type ( each BIT of an 32 bit integer is a type ).
VkResult findMemoryTypeIndex( uint32_t typeBits, VkFlags requirementsMask, uint32_t* typeIndex )
{
    // GPU가 가진 메모리 타입중에, 필요로하는 메모리 특성을 모두 가지고 있는 메모리 타입의 index를 반환한다.
    // requirementMask                      : 필요한 메모리 특성을 flag로 전달

    // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  : 이 타입으로 할당된 메모리는 vkMapMemory를 통해 host가 접근 가능하다.
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : host와 device가 밀착된 메모리
    //                                      : 호스트게 메모리에 쓴 글을 flush하지 않아도 device가 바로 읽을 수 있고
    //                                      : device가 메모리에 쓴 글도 호스트에게 visible함

    // Search memtypes to find first index with those properties
    for( uint32_t i = 0; i < 32; i++ )
    {
        if( ( typeBits & 1 ) == 1 )
        {
            // Type is available, does it match user properties?
            if( ( device.gpuMemoryProperties_.memoryTypes[i].propertyFlags & requirementsMask ) == requirementsMask )
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

VkResult LoadTextureFromFile( const char* filePath, struct TextureObject* textureObject, VkImageUsageFlags usage, VkFlags requiredProps )
{
    // blit         : bit block trasnfer의 약어, 데이터 배열을 목적지 배열에 복사하는것을 뜻함
    if( !( usage | requiredProps ) )
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

    textureObject->width_ = imgWidth;
    textureObject->height_ = imgHeight;

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
    CALL_VK( vkCreateImage( device.device_, &image_create_info, nullptr, &textureObject->image_ ) );

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements( device.device_, textureObject->image_, &memoryRequirements );

    VkMemoryAllocateInfo memoryAllocateInfo;
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = nullptr;
    memoryAllocateInfo.allocationSize = 0;
    memoryAllocateInfo.memoryTypeIndex = 0;
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    VK_CHECK( findMemoryTypeIndex( memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memoryAllocateInfo.memoryTypeIndex ) );
    CALL_VK( vkAllocateMemory( device.device_, &memoryAllocateInfo, nullptr, &textureObject->deviceMemory_ ) );
    CALL_VK( vkBindImageMemory( device.device_, textureObject->image_, textureObject->deviceMemory_, 0 ) );

    if( requiredProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT )
    {
        VkImageSubresource imageSubresource;
        imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageSubresource.mipLevel = 0;
        imageSubresource.arrayLayer = 0;

        VkSubresourceLayout subresourceLayout;
        vkGetImageSubresourceLayout( device.device_, textureObject->image_, &imageSubresource, &subresourceLayout );

        void* data;
        CALL_VK( vkMapMemory( device.device_, textureObject->deviceMemory_, 0, memoryAllocateInfo.allocationSize, 0, &data ) );

        for( int32_t y = 0; y < imgHeight; y++ )
        {
            unsigned char* row = ( unsigned char* ) ( ( char* ) data + subresourceLayout.rowPitch * y );
            for( int32_t x = 0; x < imgWidth; x++ )
            {
                row[x * 4] = imageData[( x + y * imgWidth ) * 4];
                row[x * 4 + 1] = imageData[( x + y * imgWidth ) * 4 + 1];
                row[x * 4 + 2] = imageData[( x + y * imgWidth ) * 4 + 2];
                row[x * 4 + 3] = imageData[( x + y * imgWidth ) * 4 + 3];
            }
        }

        vkUnmapMemory( device.device_, textureObject->deviceMemory_ );
        stbi_image_free( imageData );
    }
    delete[] fileContent;

    textureObject->imageLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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
        setImageLayout( gfxCmd, textureObject->image_, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
    }
    else
    {
        // save current image and mem as staging image and memory
        stageImage = textureObject->image_;
        stageMem = textureObject->deviceMemory_;
        textureObject->image_ = VK_NULL_HANDLE;
        textureObject->deviceMemory_ = VK_NULL_HANDLE;

        // Create a tile texture to blit into
        image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        CALL_VK( vkCreateImage( device.device_, &image_create_info, nullptr, &textureObject->image_ ) );
        vkGetImageMemoryRequirements( device.device_, textureObject->image_, &memoryRequirements );

        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        VK_CHECK( findMemoryTypeIndex( memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memoryAllocateInfo.memoryTypeIndex ) );
        CALL_VK( vkAllocateMemory( device.device_, &memoryAllocateInfo, nullptr, &textureObject->deviceMemory_ ) );
        CALL_VK( vkBindImageMemory( device.device_, textureObject->image_, textureObject->deviceMemory_, 0 ) );

        // transitions image out of UNDEFINED type
        setImageLayout( gfxCmd, stageImage, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
        setImageLayout( gfxCmd, textureObject->image_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
        VkImageCopy bltInfo;
        bltInfo.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, bltInfo.srcSubresource.mipLevel = 0, bltInfo.srcSubresource.baseArrayLayer = 0, bltInfo.srcSubresource.layerCount = 1, bltInfo.srcOffset.x = 0, bltInfo.srcOffset.y = 0, bltInfo.srcOffset.z = 0, bltInfo.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, bltInfo.dstSubresource.mipLevel = 0, bltInfo.dstSubresource.baseArrayLayer = 0, bltInfo.dstSubresource.layerCount = 1, bltInfo.dstOffset.x = 0, bltInfo.dstOffset.y = 0, bltInfo.dstOffset.z = 0, bltInfo.extent.width = imgWidth, bltInfo.extent.height = imgHeight, bltInfo.extent.depth = 1,

                vkCmdCopyImage( gfxCmd, stageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, textureObject->image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bltInfo );

        setImageLayout( gfxCmd, textureObject->image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
    }

    CALL_VK( vkEndCommandBuffer( gfxCmd ) );

    VkFenceCreateInfo fenceInfo;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = 0;

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

        CALL_VK( vkCreateSampler( device.device_, &sampler, nullptr, &textures[i].sampler_ ) );
        view.image = textures[i].image_;
        CALL_VK( vkCreateImageView( device.device_, &view, nullptr, &textures[i].imageView_ ) );
    }
}

bool CreateBuffers( void )
{
    // VkBuffer             : size, usage, sharding mode, 어떤 property를 가진 queue에서 접근할지 등을 정의
    //                      : 이 버퍼를 cpu에서 write할 수 있도록 하려면, VkDeviceMemory를 만들어서 cpu address와 binding해야함
    // VkDeviceMemory       : MemoryRequirements와 allocationInfo를 통해 device memory 객체를 생성한다.
    //                      : cpu voide pointer와 mapping하여 cpu에서 VkBuffer 메모리 write 할 수 있게 한다.

    // todo: staging buffer를 이용한 최적화

    const float vertexData[] = { -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f, };

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

    VK_CHECK( findMemoryTypeIndex( memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &allocInfo.memoryTypeIndex ) );

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
    // shader resource          : 리소스(버퍼와 이미지 뷰)와 쉐이더를 연결하는데 필요한 변수

    // Descriptor               : 디스크립터 세트 개체로 구성되어있다
    //                          : 셰이더와 통신하기 위한 프로토콜을 정의하며, 위치 바인딩을 사용해 리소스 메모리를 셰이더와 연결하는 자동 메커니즘 제공
    //                          : 즉, App과 쉐이더 프로그램의 데이터 통신을 위한 객체
    // Descriptor Set           : 쉐이더와 리소스를 연결
    //                          : Descriptor Set Layout을 사용하여, 레이아웃 바인딩으로 들어오는 리소스 데이터를 읽고 해석하는 것을 돕는다
    // Descriptor Set Layout    : 쉐이더가 지정된 위치의 리소스를 읽을 수 있게 하는 인터페이스 제공

    // VkPipelineLayout         : 파이프라인 내에서 디스크립터 세트 레이아웃의 순서를 관리
    // VkPipelineCache          : PCO. 저장된 파이프라인을 빠르게 검색하고 재사용하기 위한 매커니즘 제공 (중복 파이프라인 생성을 피할 수 있음)
    // VkPipeline               : blend, depth/stencil test, primitive assembly, viewport 등의 하드웨어 설정 제어 기능 제공

    // GPU instancing           : 같은 메쉬를 여러곳에 그릴 때(예를들어 나무를),
    //                          : 같은 draw call을 여러번 하지 않고 인스턴싱하면 오버헤드를 줄일 수 있음

    // vertexInputBindingDescription    : vertex 입력 비율 저장 (inputRate, stride)
    //                                  : inputRate : vertex index의 addressing 모드를 결정 (instance는 GPU instancing 할 때 쓰임)
    // vertexInputAttributeDescription  : 데이터 해석에 도움을 주는 메타 데이터 저장
    //                                  : location, offset, format 등

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
    VkPipelineDynamicStateCreateInfo dynamicStateInfo;
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.pNext = nullptr;
    dynamicStateInfo.dynamicStateCount = 0;
    dynamicStateInfo.pDynamicStates = nullptr;

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

    VkPipelineColorBlendAttachmentState attachmentStates;
    attachmentStates.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    attachmentStates.blendEnable = VK_FALSE;

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
    VkDescriptorPoolSize descriptorPoolSize;
    descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorPoolSize.descriptorCount = TUTORIAL_TEXTURE_COUNT;

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.pNext = nullptr;
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;


    CALL_VK( vkCreateDescriptorPool( device.device_, &descriptorPoolCreateInfo, nullptr, &gfxPipeline.descPool_ ) );

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.pNext = nullptr;
    descriptorSetAllocateInfo.descriptorPool = gfxPipeline.descPool_;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &gfxPipeline.dscLayout_;
    CALL_VK( vkAllocateDescriptorSets( device.device_, &descriptorSetAllocateInfo, &gfxPipeline.descSet_ ) );

    VkDescriptorImageInfo descriptorImageInfo[TUTORIAL_TEXTURE_COUNT];
    memset( descriptorImageInfo, 0, sizeof( descriptorImageInfo ) );
    for( int32_t idx = 0; idx < TUTORIAL_TEXTURE_COUNT; idx++ )
    {
        descriptorImageInfo[idx].sampler = textures[idx].sampler_;
        descriptorImageInfo[idx].imageView = textures[idx].imageView_;
        descriptorImageInfo[idx].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }

    VkWriteDescriptorSet writeDescriptorSet;
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.pNext = nullptr;
    writeDescriptorSet.dstSet = gfxPipeline.descSet_;
    writeDescriptorSet.dstBinding = 0;
    writeDescriptorSet.dstArrayElement = 0;
    writeDescriptorSet.descriptorCount = TUTORIAL_TEXTURE_COUNT;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.pImageInfo = descriptorImageInfo;
    writeDescriptorSet.pBufferInfo = nullptr;
    writeDescriptorSet.pTexelBufferView = nullptr;
    vkUpdateDescriptorSets( device.device_, 1, &writeDescriptorSet, 0, nullptr );
    return VK_SUCCESS;
}

void CreateCommand()
{
    // https://vulkan.lunarg.com/doc/view/1.0.37.0/linux/vkspec.chunked/ch07.html
    // CommandPool      : queue property를 위해 queueFamilyIndex를 가지고 초기화
    // CommandBuffer    : primary command buffer    : 실행을 위해 큐로 보내지는 명령들의 집합
    //                  : secondary command buffer  : 직접 큐로 보내지지 않고, primary command buffer에 의해 실행됨
    //                                              : vkCmdExecuteCommands(primary_command_buffer, secondary_command_buffer_count, secondary_command_buffers);
    //                                              : frame마다 변하지 않는 command들을 레코딩하기에 유용하다. & 사이즈가 큰 primary command buffer를 줄일 수 있다.

    // Command Recording
    //                  : beginCommandBuffer    : 커맨드 버퍼 레코딩 시작
    //                  : setImageLayout        :
    //                  : beginRenderPass       : 렌더패스 인스턴스를 만들고, 렌드패스 인스턴스 레코딩을 시작
    //                  : bindPipeline          : 파이프라인 바인딩
    //                  : bindVertexBuffers     : 파이프라인에서 사용하는 리소스 바인딩
    //                  : draw                  : 드로우 동작을 정의한다. (실제 드로잉 되는게 아님)
    //                  : endRenderPass         : 렌더패스 인스턴스 레코딩종료 (커맨드가 execute될때 렌더패스 인스턴스가 실행됨)
    //                  : endCommandBuffer      : 커맨드 버퍼 레코딩 종료

    // vkCmdNextSubpass : To transition to the next subpass in the render pass instance after recording the commands for a subpass
    //                  : The subpass index for a render pass begins at zero when vkCmdBeginRenderPass is recorded, and increments each time vkCmdNextSubpass is recorded.


    // Rendering commands are recorded into a particular subpass of a render pass instance

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
        renderPassBeginInfo.renderArea.offset = { .x = 0, .y = 0 };
        renderPassBeginInfo.renderArea.extent = swapchain.displaySize_;
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearVals;
        vkCmdBeginRenderPass( render.cmdBuffer_[bufferIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE );

        vkCmdBindPipeline( render.cmdBuffer_[bufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline.pipeline_ );

        vkCmdBindDescriptorSets( render.cmdBuffer_[bufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline.layout_, 0, 1, &gfxPipeline.descSet_, 0, nullptr );

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers( render.cmdBuffer_[bufferIndex], 0, 1, &buffers.vertexBuf_, &offset );

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
}

bool VulkanDrawFrame( void )
{
    // fence        : device와 host사이의 동기화 객체
    //              : vkResetFences     : fence가 unsignaled 된다.
    //              : vkQueueSubmit     : submit한 command가 수행을 마치면 fence가 signaled 된다.
    //              : vkWaitForFence    : fence가 signaled가 될 때 까지 기다린다.
    //              : reset함수에 device를 전달하는데, 이 device가 fence를 reset 시키는 논리적 장치이다
    // semephore    : queue 사이의 동기화 객체
    //              : submit할때 semaphore전달. 내부적으로 큐들 사이의 동기화해줌, fence와 다르게 해줄게 별로 없음

    //              : fence, semaphore => 시작할때 unsignaled로 하고, 끝나면 signaled로 변경
    //              : vkAcquireNextImageKHR가 호출될때 세마포어가 unsignaled상태이면 singaled가 될때까지 기다린다? 아니면 미정의 동작?

    uint32_t nextIndex;
    CALL_VK( vkAcquireNextImageKHR( device.device_, swapchain.swapchain_, UINT64_MAX, render.semaphore_, VK_NULL_HANDLE, &nextIndex ) );

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
    CALL_VK( vkResetFences( device.device_, 1, &render.fence_ ) );
    CALL_VK( vkQueueSubmit( device.queue_, 1, &submit_info, render.fence_ ) );
    CALL_VK( vkWaitForFences( device.device_, 1, &render.fence_, VK_TRUE, 100000000 ) );

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

    CreateVulkanDevice( app->window );

    CreateSwapChain();

    CreateRenderPass();

    CreateFrameBuffers();

    CreateTexture();

    CreateBuffers();

    CreateGraphicsPipeline();

    CreateDescriptorSet();

    CreateCommand();

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

// oldImageLayout에서 newImageLayout으로의 전환이 srcStages와 destStages 사이에 일어나야 한다.
// => srcStages가 모두 끝나고, destStages가 시작되기 전에 전환이 완료되어야 한다.
void setImageLayout( VkCommandBuffer cmdBuffer,         // render.cmdBuffer_[i]
                     VkImage image,                     // swapchain.displayImages_[i]
                     VkImageLayout oldImageLayout,      // VK_IMAGE_LAYOUT_UNDEFINED
                     VkImageLayout newImageLayout,      // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                     VkPipelineStageFlags srcStages,    // VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
                     VkPipelineStageFlags destStages    // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
)
{
    // https://vulkan.lunarg.com/doc/view/1.0.26.0/linux/vkspec.chunked/ch06s05.html
    // https://gpuopen.com/vulkan-barriers-explained/
    // http://cpp-rendering.io/barriers-vulkan-not-difficult/

    // image layout                     : GPU의 이미지 접근방식
    //                                  : 주어진 용도 특성에 맞춰 구현에 지정한 방식으로, 메모리 내용을 액세스 할 수 있게 한다.
    //                                  : 이미지에 사용할 수 있는 일반 레이아웃(VK_IMAGE_LAYOUT_GENERAL)이 있지만, 이 레이아웃 하나만으로는 적절하지 않을 때가 있다.

    // image layout transition
    // optimal layout <-> linear layout : 최적 레이아웃 <-> 선형 레이아웃 상호 전환(transition) 기능 필요 (host는 최적 레이아웃 메모리 직접 액세스 불가)
    //                                  : 메모리 장벽을 사용해 레이아웃 전환이 가능하다
    //                                  : CPU는 이미지 데이터를 선형 레이아웃 버퍼에 저장 후, 최적 레이아웃으로 변경 할 수 있음 (GPU가 더 효율적으로 읽을 수 있도록)

    // memory barrier   : 데이터 읽기와 쓰기를 동기화 (메모리장벽 전후에 지정한 작업이 동기화 되도록 보장)
    //                  : global memory barrier (VkMemoryBarrier)       : 모든 종류의 실행 메모리 개체에 적용
    //                  : buffer memory barrier (VkBufferMemoryBarrier) : 지정된 버퍼 개체의 특정 범위에 적용
    //                  : image memory barrier  (VkImageMemoryBarrier)  : 지정된 이미지 개체의 특정 이미지 하위 리소스 범위를 통해 다른 메모리 엑세스 유형에 적용
    //                  : vkCmdPipelineBarrier를 통해 메모리 장벽을 삽입한다.

    // srcAccessMask    : 어떤 작업에 대한 완료를 보장할지 정한다
    // dstAccessMask    : 변경된 layout이 어떤 리소스로 부터 접근 가능할지 정한다.

    VkImageMemoryBarrier imageMemoryBarrier;
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.pNext = nullptr;
    imageMemoryBarrier.oldLayout = oldImageLayout;
    imageMemoryBarrier.newLayout = newImageLayout;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = image;                                                               // 1. 지정된 이미지 개체의
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;                     // 2. 특정 이미지 하위 리소스 범위를 통해
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
    imageMemoryBarrier.subresourceRange.layerCount = 1;

    switch( oldImageLayout )
    {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: // image의 원래의 접근 용도가 컬러 첨부 였다면
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // transition이 일어나기 전에 color에 모든 write가 끝남을 보장해야 한다.
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: // 이미지의 원래 접근 용도가 dst_optimal이었다면 (예를들어 copy)
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // transfer_write가 모두 끝나고 transition이 되어야 함을 보장해야 한다.
            break;
        case VK_IMAGE_LAYOUT_PREINITIALIZED: // 미리 초기화된 레이아웃 이었다면,
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT; // 앱쪽에서 이미지에 write한 명령이 끝나고 transition이 되어야 함을 보장해야 한다.
            break;
        default:
            break;
    }

    switch( newImageLayout )                                                                        // 3. 다른 메모리 엑세스 유형에 적용
    {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: // 전환된 이미지의 목적이 transfer_dst_optimal이라면,
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // tranfer_write 목적으로만 전환된 이미지에 접근 가능하다
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: // 전환된 이미지의 목적이 transfer_src_optimal이라면,
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT; // transfer_read 목적으로만 전환된 이미지에 접근 가능하다
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: // 쉐이더에서 읽으려는 목적이라면
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // 쉐이더에서 읽을때만 접근 가능
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: // 컬러 첨부가 목적이라면
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // 컬러 첨부 write 할때만 이미지에 접근 가능
            break;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: // present하는게 전환된 이미지의 목적이라면
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT; // memory_read를 위해서만 이미지에 접근이 가능하다
            break;
        default:
            break;
    }

    vkCmdPipelineBarrier( cmdBuffer,          // 메모리 장벽이 정의된 커맨드 버퍼
                          srcStages,          // 장벽 구현 전에 수행이 완료돼야 하는 파이프라인 스테이지
                          destStages,         // 장벽 이전의 명령이 모두 수행되기 전까지는 시작하면 안되는 파이프라인 스테이
                          0,                  // 스크린 공간 지역성(locality)가 있는지 알려준다.
                          0,                  // global memory barrier count
                          nullptr,            // global memory barriers
                          0,                  // buffer memory barrier count
                          nullptr,            // buffer memory barriers
                          1,                  // image memory barrier count
                          &imageMemoryBarrier // image memory barriers
    );
}