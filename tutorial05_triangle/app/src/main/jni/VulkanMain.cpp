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

using namespace std;

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
    VkColorSpaceKHR colorSpace_;
    std::vector<VkImage> displayImages_;
    std::vector<VkImageView> displayViews_;
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
    applicationInfo.apiVersion = VK_MAKE_VERSION( 1, 0, 0 );;

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

    uint32_t indexCount{ 0 };
    vkGetPhysicalDeviceQueueFamilyProperties( device.physicalDevice_, &indexCount, nullptr );
    vector<VkQueueFamilyProperties> properties( indexCount );
    vkGetPhysicalDeviceQueueFamilyProperties( device.physicalDevice_, &indexCount, properties.data() );

    auto found = find_if( properties.begin(), properties.end(), [=]( const VkQueueFamilyProperties& properties ) {
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

void CreateRenderPass( void )
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
    colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
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

void CreateFramebuffers( VkRenderPass renderPass, VkImageView depthView = VK_NULL_HANDLE )
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

uint32_t getMemoryTypeIndex( int memoryTypeBits, VkFlags requirementMask )
{
    // GPU가 가진 메모리 타입중에, 필요로하는 메모리 특성을 모두 가지고 있는 메모리 타입의 index를 반환한다.
    // requirementMask                      : 필요한 메모리 특성을 flag로 전달

    // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  : 이 타입으로 할당된 메모리는 vkMapMemory를 통해 host가 접근 가능하다.
    // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : host와 device가 밀착된 메모리
    //                                      : 호스트게 메모리에 쓴 글을 flush하지 않아도 device가 바로 읽을 수 있고
    //                                      : device가 메모리에 쓴 글도 호스트에게 visible함
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties( device.physicalDevice_, &memoryProperties );
    for( uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i )
    {
        if( memoryTypeBits & 1 << i )
        {
            if( ( memoryProperties.memoryTypes[i].propertyFlags & requirementMask ) == requirementMask )
            {
                return i;
            }
        }
    }
    assert( false );
}

void CreateBuffers( void )
{
    // VkBuffer             : size, usage, sharding mode, 어떤 property를 가진 queue에서 접근할지 등을 정의
    //                      : 이 버퍼를 cpu에서 write할 수 있도록 하려면, VkDeviceMemory를 만들어서 cpu address와 binding해야함
    // VkDeviceMemory       : MemoryRequirements와 allocationInfo를 통해 device memory 객체를 생성한다.
    //                      : cpu voide pointer와 mapping하여 cpu에서 VkBuffer 메모리 write 할 수 있게 한다.

    array<float, 9> vertice{ -1, -1, 0, 1, -1, 0, 0, 1, 0 };

    VkBufferCreateInfo bufferCreateInfo;
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.pNext = nullptr;
    bufferCreateInfo.flags = 0;
    bufferCreateInfo.size = vertice.size() * sizeof( float );
    bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.queueFamilyIndexCount = 1;
    bufferCreateInfo.pQueueFamilyIndices = &device.queueFamilyIndex_;
    vkCreateBuffer( device.device_, &bufferCreateInfo, nullptr, &buffers.vertexBuf_ );

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements( device.device_, buffers.vertexBuf_, &memoryRequirements );

    VkDeviceMemory deviceMemory;

    VkMemoryAllocateInfo memoryAllocateInfo;
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.pNext = nullptr;
    memoryAllocateInfo.allocationSize = vertice.size() * sizeof( float );
    memoryAllocateInfo.memoryTypeIndex = getMemoryTypeIndex( memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
    vkAllocateMemory( device.device_, &memoryAllocateInfo, nullptr, &deviceMemory );

    void* data{ nullptr };
    vkMapMemory( device.device_, deviceMemory, 0, memoryRequirements.size, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, &data );

    memcpy( data, vertice.data(), vertice.size() * sizeof( float ) );

    vkUnmapMemory( device.device_, deviceMemory );

    vkBindBufferMemory( device.device_, buffers.vertexBuf_, deviceMemory, 0 );
}

enum ShaderType
{
    VERTEX_SHADER, FRAGMENT_SHADER
};

VkResult loadShaderFromFile( const char* filePath, VkShaderModule* shaderOut, ShaderType type )
{
    // VkShaderModule   : shader source를 통해 shader module 생성

    assert( androidAppCtx );
    AAsset* file = AAssetManager_open( androidAppCtx->activity->assetManager, filePath, AASSET_MODE_BUFFER );
    size_t fileLength = AAsset_getLength( file );

    char* fileContent = new char[fileLength];

    AAsset_read( file, fileContent, fileLength );
    AAsset_close( file );

    VkShaderModuleCreateInfo shaderModuleCreateInfo;
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.pNext = nullptr;
    shaderModuleCreateInfo.codeSize = fileLength;
    shaderModuleCreateInfo.pCode = ( const uint32_t* ) fileContent;
    shaderModuleCreateInfo.flags = 0;

    VkResult result = vkCreateShaderModule( device.device_, &shaderModuleCreateInfo, nullptr, shaderOut );
    assert( result == VK_SUCCESS );

    delete[] fileContent;

    return result;
}

void CreateGraphicsPipeline( void )
{
    // shader resource          : 리소스(버퍼와 이미지 뷰)와 쉐이더를 연결하는데 필요한 변수
    // Descriptor Set Layout    : 쉐이더 리소스를 관리
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

    VkPipelineLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pNext = nullptr;
    layoutCreateInfo.flags = 0;
    layoutCreateInfo.setLayoutCount = 0;
    layoutCreateInfo.pSetLayouts = nullptr;
    layoutCreateInfo.pushConstantRangeCount = 0;
    layoutCreateInfo.pPushConstantRanges = nullptr;
    VkResult result = vkCreatePipelineLayout( device.device_, &layoutCreateInfo, nullptr, &gfxPipeline.layout_ );
    assert( VK_SUCCESS == result );

    VkShaderModule vertexShader, fragmentShader;
    loadShaderFromFile( "shaders/tri.vert.spv", &vertexShader, VERTEX_SHADER );
    loadShaderFromFile( "shaders/tri.frag.spv", &fragmentShader, FRAGMENT_SHADER );

    VkPipelineShaderStageCreateInfo shaderStageCreateInfo[2];
    shaderStageCreateInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfo[0].pNext = nullptr;
    shaderStageCreateInfo[0].flags = 0;
    shaderStageCreateInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStageCreateInfo[0].module = vertexShader;
    shaderStageCreateInfo[0].pName = "main";
    shaderStageCreateInfo[0].pSpecializationInfo = nullptr;
    shaderStageCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfo[1].pNext = nullptr;
    shaderStageCreateInfo[1].flags = 0;
    shaderStageCreateInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStageCreateInfo[1].module = fragmentShader;
    shaderStageCreateInfo[1].pName = "main";
    shaderStageCreateInfo[1].pSpecializationInfo = nullptr;

    VkVertexInputBindingDescription vertexInputBindingDescription;
    vertexInputBindingDescription.binding = 0;
    vertexInputBindingDescription.stride = 3 * sizeof( float );
    vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertexInputAttributeDescription;
    vertexInputAttributeDescription.location = 0;
    vertexInputAttributeDescription.binding = 0;
    vertexInputAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescription.offset = 0;

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo;
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.pNext = nullptr;
    vertexInputStateCreateInfo.flags = 0;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInputBindingDescription;
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 1;
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = &vertexInputAttributeDescription;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo;
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCreateInfo.pNext = nullptr;
    inputAssemblyStateCreateInfo.flags = 0;
    inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = swapchain.displaySize_.width;
    viewport.height = swapchain.displaySize_.height;
    viewport.minDepth = 0;
    viewport.maxDepth = 1;

    VkRect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = swapchain.displaySize_;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo;
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.pNext = nullptr;
    viewportStateCreateInfo.flags = 0;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.pNext = nullptr;
    rasterizationStateCreateInfo.flags = 0;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizationStateCreateInfo.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo;
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.pNext = nullptr;
    multisampleStateCreateInfo.flags = 0;
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleStateCreateInfo.minSampleShading = 0;
    multisampleStateCreateInfo.pSampleMask = nullptr;
    multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
    multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
    colorBlendAttachmentState.blendEnable = VK_FALSE;
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo;
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.pNext = nullptr;
    colorBlendStateCreateInfo.flags = 0;
    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

    VkPipelineCacheCreateInfo cacheCreateInfo;
    cacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    cacheCreateInfo.pNext = nullptr;
    cacheCreateInfo.flags = 0;
    cacheCreateInfo.initialDataSize = 0;
    cacheCreateInfo.pInitialData = nullptr;
    result = vkCreatePipelineCache( device.device_, &cacheCreateInfo, nullptr, &gfxPipeline.cache_ );
    assert( VK_SUCCESS == result );

    VkGraphicsPipelineCreateInfo pipelineCreateInfo;
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.pNext = nullptr;
    pipelineCreateInfo.flags = 0;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStageCreateInfo;
    pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    pipelineCreateInfo.pTessellationState = nullptr;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    pipelineCreateInfo.pDepthStencilState = nullptr;
    pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    pipelineCreateInfo.pDynamicState = nullptr;
    pipelineCreateInfo.layout = gfxPipeline.layout_;
    pipelineCreateInfo.renderPass = render.renderPass_;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = 0;
    result = vkCreateGraphicsPipelines( device.device_, gfxPipeline.cache_, 1, &pipelineCreateInfo, nullptr, &gfxPipeline.pipeline_ );
    assert( VK_SUCCESS == result );

    vkDestroyShaderModule( device.device_, vertexShader, nullptr );
    vkDestroyShaderModule( device.device_, fragmentShader, nullptr );
}

void CreateCommand( void )
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

    VkCommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = nullptr;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = device.queueFamilyIndex_;
    vkCreateCommandPool( device.device_, &commandPoolCreateInfo, nullptr, &render.cmdPool_ );

    render.cmdBuffer_ = new VkCommandBuffer[swapchain.swapchainLength_];

    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = nullptr;
    commandBufferAllocateInfo.commandPool = render.cmdPool_;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = swapchain.swapchainLength_;
    vkAllocateCommandBuffers( device.device_, &commandBufferAllocateInfo, render.cmdBuffer_ );

    for( uint32_t i = 0; i < swapchain.swapchainLength_; ++i )
    {
        VkCommandBufferBeginInfo commandBufferBeginInfo;
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.pNext = nullptr;
        commandBufferBeginInfo.flags = 0;
        commandBufferBeginInfo.pInheritanceInfo = nullptr;
        vkBeginCommandBuffer( render.cmdBuffer_[i], &commandBufferBeginInfo );

        setImageLayout( render.cmdBuffer_[i], swapchain.displayImages_.at( i ), VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );

        VkClearValue clearValue;
        clearValue.color.float32[0] = 0.0f;
        clearValue.color.float32[1] = 0.0f;
        clearValue.color.float32[2] = 0.0f;
        clearValue.color.float32[3] = 1.0f;

        VkRenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.renderPass = render.renderPass_;
        renderPassBeginInfo.framebuffer = swapchain.framebuffers_.at( i );
        renderPassBeginInfo.renderArea.offset = { .x = 0, .y = 0 };
        renderPassBeginInfo.renderArea.extent = swapchain.displaySize_;
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearValue;
        vkCmdBeginRenderPass( render.cmdBuffer_[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE );

        vkCmdBindPipeline( render.cmdBuffer_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline.pipeline_ );

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers( render.cmdBuffer_[i], 0, 1, &buffers.vertexBuf_, &offset );

        vkCmdDraw( render.cmdBuffer_[i], 3, 1, 0, 0 );

        vkCmdEndRenderPass( render.cmdBuffer_[i] );

        vkEndCommandBuffer( render.cmdBuffer_[i] );
    }

    VkFenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = 0;
    vkCreateFence( device.device_, &fenceCreateInfo, nullptr, &render.fence_ );

    VkSemaphoreCreateInfo semaphoreCreateInfo;
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0;
    vkCreateSemaphore( device.device_, &semaphoreCreateInfo, nullptr, &render.semaphore_ );
}

// Initialize vulkan device context
// after return, vulkan is ready to draw
bool InitVulkan( android_app* app )
{
    androidAppCtx = app;
    if( !InitVulkan() )
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

    CreateRenderPass();

    CreateFramebuffers( render.renderPass_ );

    CreateBuffers();

    CreateGraphicsPipeline();

    CreateCommand();

    device.initialized_ = true;

    return true;
}

// delete vulkan device context when application goes away
void DeleteVulkan( void )
{

}

// Check if vulkan is ready to draw
bool IsVulkanReady( void )
{
    return device.initialized_;
}

// Ask Vulkan to Render a frame
bool VulkanDrawFrame( void )
{
    // fence        : device와 host사이의 동기화 객체
    //              : vkResetFences     : fence가 unsignaled 된다.
    //              : vkQueueSubmit     : fence가 signaled 된다.
    //              : vkWaitForFence    : fence가 signaled가 될 때 까지 기다린다.
    //              : reset함수에 device를 전달하는데, 이 device가 fence를 reset 시키는 논리적 장치이다
    // semephore    : queue 사이의 동기화 객체
    //              : submit할때 semaphore전달. 내부적으로 큐들 사이의 동기화해줌, fence와 다르게 해줄게 별로 없음

    //              : fence, semaphore => 시작할때 unsignaled로 하고, 끝나면 signaled로 변경
    //              : vkAcquireNextImageKHR가 호출될때 세마포어가 unsignaled상태이면 singaled가 될때까지 기다린다? 아니면 미정의 동작?

    uint32_t index{ 0 };
    VkResult result = vkAcquireNextImageKHR( device.device_, swapchain.swapchain_, UINT64_MAX, render.semaphore_, render.fence_, &index );
    assert( VK_SUCCESS == result );

    result = vkResetFences( device.device_, 1, &render.fence_ );
    assert( VK_SUCCESS == result );

    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &render.semaphore_;
    submitInfo.pWaitDstStageMask = &waitStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &render.cmdBuffer_[index];
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;
    result = vkQueueSubmit( device.queue_, 1, &submitInfo, render.fence_ );
    assert( VK_SUCCESS == result );

    result = vkWaitForFences( device.device_, 1, &render.fence_, VK_TRUE, UINT64_MAX );
    assert( VK_SUCCESS == result );

    VkResult presentResult;
    VkPresentInfoKHR presentInfo;
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 0;
    presentInfo.pWaitSemaphores = nullptr;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain.swapchain_;
    presentInfo.pImageIndices = &index;
    presentInfo.pResults = &presentResult;
    result = vkQueuePresentKHR( device.queue_, &presentInfo );
    assert( VK_SUCCESS == result );
    assert( VK_SUCCESS == presentResult );

    return true;
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