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
    // instance         : vulkan instance. surface와 physical device 생성에 쓰임
    // physical device  : gpu. 메모리 정보와 command submit을 위한 queue 정보를 얻는데 쓰임
    // queue family     : queue family는 동일한 property를 가진 queue들의 집합이다. (queue의 property에 따라 수행할 수 있는 command의 종류가 다르다.)
    //                  : 여기에선 graphics property를 가진 큐들의 집합(queue family)을 구해서 사용한다. => graphics command를 submit할꺼니까
    // device           : graphics queue property를 가진 queue family를 가지고 device를 초기화 했음 -> graphics용 device 초기화

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
    assert( queueFamilyIndex < queueFamilyCount );
    device.queueFamilyIndex_ = queueFamilyIndex;

    std::array<float, 1> priorities{ 1.f };
    VkDeviceQueueCreateInfo queueCreateInfo;
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.pNext = nullptr;
    queueCreateInfo.flags = 0;
    queueCreateInfo.pQueuePriorities = priorities.data();
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex; // graphics queue property를 가진 queue family를 가지고 device를 초기화 했음 -> graphics용 device 초기화
    queueCreateInfo.queueCount = 1; // set of queue중 하나의 queue만을 사

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

    vkGetDeviceQueue( device.device_, queueFamilyIndex, 0, &device.queue_ ); // queue family중에서 사용하는 하나의 queue를 얻어온다.
}

void CreateSwapChain( void )
{
    // GPU가 android surface에게 지원하는 capability를 가져온다.
    // GPU가 android surface에게 지원하는 format을 가져온다. => VK_FORMAT_R8G8B8_UNORM format에 대한 index를 얻는다.
    // => capability와 format 정보를 통해 swapchain을 생성한다

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
        if( it->format == VK_FORMAT_R8G8B8_UNORM ) // 24BIT RGB unsigned normalized
        {
            chosenFormat = it - surfaceFormats.begin();
        }
    }
    assert( chosenFormat < surfaceFormatCount );

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
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR; // 다른 surface와 혼합될때의 mode
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // present requests와 큐잉에 대한 정책
    swapchainCreateInfo.clipped = VK_FALSE; // 보이지않는 부분에 대한 rendering operation을 discard 할지
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE; //  VK_NULL_HANDLE or 현재 surface에 연결된 swapchain (리소스 재사용을 돕는다)
    VkResult result = vkCreateSwapchainKHR( device.device_, &swapchainCreateInfo, nullptr, &swapchain.swapchain_ );
    assert( result == VK_SUCCESS );

    swapchain.displayImages_.resize( surfaceCapabilities.minImageCount );
    vkGetSwapchainImagesKHR( device.device_, swapchain.swapchain_, &swapchain.swapchainLength_, nullptr );
}

void CreateRenderPass( void )
{
    // https://lifeisforu.tistory.com/462
    // renderpass dependency    : 렌더패스가 사용하는 attachment들의 종속성에 의해 렌더패스간의 종속성이 결정된다.
    // attachment description   : 렌더패스에 attachment를 지정할때의 속성. (포맷, 용도, MSAA, load clear op, save or not, layout etc..)
    // renderpass object        : vkCreateRenderPass에 의해 생성되는 렌더패스객체는 템플릿으로써 존재함.
    //                          : VkCmdBeginRenderPass가 호출될때 실제 인스턴스가 생성되고, 각 어태치먼트와 관련된 리소스들을 프레임버퍼로 바인딩합니다

    // renderpass command를 위해 기본적으로 세 객체가 필요: renderpass, framebuffer, command
    // 이것의 장점 -> no validation, no exception & dependency management & life cycle management

    // no validation no exception   : render pass 이외에도 descriptor-instance 쌍을 이루는 경우가 많음 (예를들어 descriptor set layout)
    //                              : descriptor가 존재하는 이유는 vulkan이 리소스들의 메모리구조를 알지 못하기 때문이다.
    //                              : descriptor가 메모리에 대한 모든 정보를 가지고 있다.
    //                              : 이렇게하면 개체를 생성하는 시점에 validation을 수행가능 (리소스를 바인딩하는 시점에서 API 내부적인 검증을 할 필요가 없다.)
    //                              : => 성능상의 이점
    // dependency management        : 커맨드 버퍼를 통해 렌더패스간 의존성을 관리 (의존성이 있는 렌더패스를 가지고 있는 커맨드버퍼들을 동기화)
    // life cycle management        : 멀티스레딩 환경에서 렌더패스 인스턴스, 커맨드 버퍼, 프레임 버퍼 등의 생명주기를 관리하는데 용이

    VkAttachmentDescription attachmentDescription; // attachment의 다양한 속성 지정
    attachmentDescription.flags = 0;
    attachmentDescription.format = swapchain.displayFormat_;
    attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT; // msaa
    attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // load할때 clear해서 load한다. read 안하고 write만 할꺼면 don't care로 하면 됨
    attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // 쓰고 버릴거면 don't care로하고, 저장할꺼면 store로 한다.
    attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // renderpass 이전의 layout.
    attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // renderpass 이후의 layout

    // attachment reference 에서의 layout은 실제로 렌더링 파이프라인에서 쓰일 layout
    // 드라이버는 이 부분을 알아서 전환해주지 않음, 응용 프로그램에서 모두 정의 해줘야 함
    VkAttachmentReference attachmentReference;
    attachmentReference.attachment = 0;
    attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // renderpass에서 layout

    VkSubpassDescription subpassDescription;
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // graphics or compute중 어떤 파이프라인에 바인드 될 것인
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr; // 쉐이더와 공유할 첨부 목록
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &attachmentReference; // 컬러 첨부
    subpassDescription.pResolveAttachments = nullptr; // 멀티샘플링에서 쓰이는 resolve 첨부
    subpassDescription.pDepthStencilAttachment = nullptr; // 깊이/스텐실 첨부
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr; // 서브패스에선 쓰이진 않지만 보존되어야 하는 컨텐츠, 다음 패스에서 쓰기위해 임시저장

    VkRenderPassCreateInfo renderPassCreateInfo;
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.pNext = nullptr;
    renderPassCreateInfo.flags = 0;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &attachmentDescription; // 첨부 목록
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription; // 서브패스 목록
    renderPassCreateInfo.dependencyCount = 0;
    renderPassCreateInfo.pDependencies = nullptr; // VkSubpassDependency 를 통해 서브패스간의 dependency 정의

    vkCreateRenderPass( device.device_, &renderPassCreateInfo, nullptr, &render.renderPass_ );
}

void CreateFramebuffers( VkRenderPass renderPass, VkImageView depthView = VK_NULL_HANDLE )
{
    VkResult result = vkGetSwapchainImagesKHR( device.device_, swapchain.swapchain_, &swapchain.swapchainLength_, swapchain.displayImages_.data() );
    assert( result == VK_SUCCESS );

    swapchain.displayViews_.resize( swapchain.swapchainLength_ );
    swapchain.framebuffers_.resize( swapchain.swapchainLength_ );

    for( unsigned int i = 0; i < swapchain.swapchainLength_; ++i )
    {
        VkImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = nullptr;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.image = swapchain.displayImages_.at( i );
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = swapchain.displayFormat_;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R; // imageViewCreateInfo.components 는 component의 remapping을 나타낸다.
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G; // VK_COMPONENT_SWIZZLE_R: output vector의 R component 값
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // which aspects of an image are included in a view
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0; // selecting the set of mipmap levels
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0; // array layers to be accessible to the view
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        vkCreateImageView( device.device_, &imageViewCreateInfo, nullptr, &swapchain.displayViews_.at( i ) );

        VkFramebufferCreateInfo framebufferCreateInfo;
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.pNext = nullptr;
        framebufferCreateInfo.flags = 0;
        framebufferCreateInfo.renderPass = renderPass;
        framebufferCreateInfo.attachmentCount = 1;
        framebufferCreateInfo.pAttachments = &swapchain.displayViews_.at( i );
        framebufferCreateInfo.width = swapchain.displaySize_.width; // width, height and layers define the dimensions of the framebuffer
        framebufferCreateInfo.height = swapchain.displaySize_.height;
        framebufferCreateInfo.layers = 1;
        vkCreateFramebuffer( device.device_, &framebufferCreateInfo, nullptr, &swapchain.framebuffers_.at( i ) );
    }
}

// GPU가 가진 메모리 타입중에, 필요로하는 메모리 특성을 모두 가지고 있는 메모리 타입의 index를 반환한다.
// VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : 이 타입으로 할당된 메모리는 vkMapMemory 를 통해 host가 접근 가능하다.
// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT: host와 device가 밀착된 메모리(호스트게 메모리에 쓴 글을 flush하지 않아도 device가 바로 읽을 수 있고, device가 메모리에 쓴 글도 호스트에게 visible함)
uint32_t getMemoryTypeIndex( int memoryTypeBits, VkFlags requirementMask )
{
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties( device.physicalDevice_, &memoryProperties ); // GPU가 가지고 있는 메모리 타입을 가져온다.
    for( uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; ++i )
    {
        if( ( memoryTypeBits & 1 ) == 1 )
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
    const float vertexData[]{ -1, -1, 0, 1, -1, 0, 0, 1, 0 };
    VkBufferCreateInfo bufferCreateInfo;
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.pNext = nullptr;
    bufferCreateInfo.flags = 0;
    bufferCreateInfo.size = sizeof( vertexData );
    bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // access to any range or image subresource of the object will be exclusive to a single queue family at a time.
    bufferCreateInfo.queueFamilyIndexCount = 1;
    bufferCreateInfo.pQueueFamilyIndices = &device.queueFamilyIndex_;
    vkCreateBuffer( device.device_, &bufferCreateInfo, nullptr, &buffers.vertexBuf_ );

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements( device.device_, buffers.vertexBuf_, &memoryRequirements );

    VkMemoryAllocateInfo allocateInfo;
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = nullptr;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = getMemoryTypeIndex( memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

    VkDeviceMemory deviceMemory;
    VkResult result = vkAllocateMemory( device.device_, &allocateInfo, nullptr, &deviceMemory );
    assert( result == VK_SUCCESS );

    void* data{ nullptr };
    vkMapMemory( device.device_, deviceMemory, 0, allocateInfo.allocationSize, 0, &data );
    memcpy( data, vertexData, sizeof( vertexData ) );
    vkUnmapMemory( device.device_, deviceMemory );
    vkBindBufferMemory( device.device_, buffers.vertexBuf_, deviceMemory, 0 );
}

enum ShaderType
{
    VERTEX_SHADER, FRAGMENT_SHADER
};

VkResult loadShaderFromFile( const char* filePath, VkShaderModule* shaderOut, ShaderType type )
{
    // Read the file
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
    // 쉐이더 리소스         : 리소스(버퍼와 이미지 뷰)를 쉐이더와 연결시
    // 디스크립터 세트 레이아웃 : 쉐이더 리소스(버퍼와 이미지 뷰에 연결되는 변수)를 관리
    // 버퍼와 이미지뷰 => 쉐이더 리소스를 통해 쉐이더에 간접적으로 연결된다.

    // VkPipelineLayout : 파이프라인 내에서 디스크립터 세트 레이아웃의 순서를 관리
    // VkPipelineCache  : PCO. 저장된 파이프라인을 빠르게 검색하고 재사용하기 위한 매커니즘 제공 (중복 파이프라인을 피할 수 있음)
    // VkPipeline       : blend, depth/stencil test, primitive assembly, viewport 등의 하드웨어 설정 제어 기능 제공
    memset( &gfxPipeline, 0, sizeof( gfxPipeline ) );

    VkPipelineLayoutCreateInfo layoutCreateInfo; // layout: 데이터(주로 이미지)의 형식이나 종류를 정의한다.
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pNext = nullptr;
    layoutCreateInfo.flags = 0;
    layoutCreateInfo.setLayoutCount = 0; // vkCmdBindVertexBuffers 로 vertexbuffer 전달하고 외에는 전달할 데이터 없음
    layoutCreateInfo.pSetLayouts = nullptr;
    layoutCreateInfo.pushConstantRangeCount = 0;
    layoutCreateInfo.pPushConstantRanges = nullptr;
    VkResult result = vkCreatePipelineLayout( device.device_, &layoutCreateInfo, nullptr, &gfxPipeline.layout_ );
    assert( result == VK_SUCCESS );

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
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo;
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.pNext = nullptr;
    colorBlendStateCreateInfo.flags = 0;
    colorBlendStateCreateInfo.logicOpEnable = VK_TRUE;
    colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.pNext = nullptr;
    rasterizationStateCreateInfo.flags = 0;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizationStateCreateInfo.lineWidth = 1;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo;
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCreateInfo.pNext = nullptr;
    inputAssemblyStateCreateInfo.flags = 0;
    inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE; // a special vertex index value is treated as restarting the assembly of primitives

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

    VkPipelineCacheCreateInfo cacheCreateInfo;
    cacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    cacheCreateInfo.pNext = nullptr;
    cacheCreateInfo.flags = 0;
    cacheCreateInfo.initialDataSize = 0;
    cacheCreateInfo.pInitialData = nullptr;

    VkGraphicsPipelineCreateInfo pipelineCreateInfo;
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.pNext = nullptr;
    pipelineCreateInfo.flags = 0;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStageCreateInfo;
    pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo; // attribute에 대한 정보. format, offset, stride, inputrate, binding 등등
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo; // primitive assembly
    pipelineCreateInfo.pTessellationState = nullptr;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    pipelineCreateInfo.pDepthStencilState = nullptr;
    pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    pipelineCreateInfo.pDynamicState = nullptr;
    pipelineCreateInfo.layout = gfxPipeline.layout_; // 디스크립터 세트 레이아웃의 순서 관리
    pipelineCreateInfo.renderPass = render.renderPass_;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = 0;

    result = vkCreateGraphicsPipelines( device.device_, gfxPipeline.cache_, 1, &pipelineCreateInfo, nullptr, &gfxPipeline.pipeline_ );
    assert( result == VK_SUCCESS );

    vkDestroyShaderModule( device.device_, vertexShader, nullptr );
    vkDestroyShaderModule( device.device_, fragmentShader, nullptr );
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

    VkCommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = nullptr;
    commandPoolCreateInfo.flags = 0;
    commandPoolCreateInfo.queueFamilyIndex = device.queueFamilyIndex_;
    VkResult result = vkCreateCommandPool( device.device_, &commandPoolCreateInfo, nullptr, &render.cmdPool_ );
    assert( result == VK_SUCCESS );

    render.cmdBufferLen_ = swapchain.swapchainLength_;
    render.cmdBuffer_ = new VkCommandBuffer[render.cmdBufferLen_];
    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = nullptr;
    commandBufferAllocateInfo.commandPool = render.cmdPool_;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = render.cmdBufferLen_;
    result = vkAllocateCommandBuffers( device.device_, &commandBufferAllocateInfo, render.cmdBuffer_ );
    assert( result == VK_SUCCESS );

    for( int bufferIndex = 0; bufferIndex < swapchain.swapchainLength_; ++bufferIndex )
    {
        VkCommandBufferBeginInfo commandBufferBeginInfo;
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.pNext = nullptr;
        commandBufferBeginInfo.flags = 0;
        commandBufferBeginInfo.pInheritanceInfo = nullptr;
        result = vkBeginCommandBuffer( render.cmdBuffer_[bufferIndex], &commandBufferBeginInfo );
        assert( result == VK_SUCCESS );

        setImageLayout( render.cmdBuffer_[bufferIndex], swapchain.displayImages_[bufferIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );

        VkClearValue clearVals;
        clearVals.color.float32[0] = 0.0f;
        clearVals.color.float32[1] = 0.34f;
        clearVals.color.float32[2] = 0.9f;
        clearVals.color.float32[3] = 1.0f;

        VkRenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.pNext = nullptr;
        renderPassBeginInfo.renderPass = render.renderPass_;
        renderPassBeginInfo.framebuffer = swapchain.framebuffers_.at( bufferIndex );
        renderPassBeginInfo.renderArea.offset = { .x=0, .y=0 };
        renderPassBeginInfo.renderArea.extent = swapchain.displaySize_;
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearVals;
        vkCmdBeginRenderPass( render.cmdBuffer_[bufferIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE );

        vkCmdBindPipeline( render.cmdBuffer_[bufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline.pipeline_ );

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers( render.cmdBuffer_[bufferIndex], 0, 1, &buffers.vertexBuf_, &offset );

        vkCmdDraw( render.cmdBuffer_[bufferIndex], 3, 1, 0, 0 );

        vkCmdEndRenderPass( render.cmdBuffer_[bufferIndex] );

        result = vkEndCommandBuffer( render.cmdBuffer_[bufferIndex] );
        assert( result == VK_SUCCESS );
    }

    VkFenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = nullptr;
    fenceCreateInfo.flags = 0;
    result = vkCreateFence( device.device_, &fenceCreateInfo, nullptr, &render.fence_ );
    assert( result == VK_SUCCESS );

    VkSemaphoreCreateInfo semaphoreCreateInfo;
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;
    semaphoreCreateInfo.flags = 0;
    result = vkCreateSemaphore( device.device_, &semaphoreCreateInfo, nullptr, &render.semaphore_ );
    assert( result == VK_SUCCESS );

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
    uint32_t nextImage;
    VkResult result = vkAcquireNextImageKHR( device.device_, swapchain.swapchain_, UINT64_MAX, render.semaphore_, render.fence_, &nextImage );
    assert( result == VK_SUCCESS );

    result = vkResetFences( device.device_, 1, &render.fence_ );
    assert( result == VK_SUCCESS );

    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &render.semaphore_;
    submitInfo.pWaitDstStageMask = &waitStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &render.cmdBuffer_[nextImage];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &render.semaphore_;
    result = vkQueueSubmit( device.queue_, 1, &submitInfo, render.fence_ );
    assert( result == VK_SUCCESS );

    result = vkWaitForFences( device.device_, 1, &render.fence_, VK_TRUE, UINT64_MAX );
    assert( result == VK_SUCCESS );

    __android_log_print( ANDROID_LOG_DEBUG, "", "drawing frames......" );

    VkResult presentResult;
    VkPresentInfoKHR presentInfo;
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 0;
    presentInfo.pWaitSemaphores = nullptr;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain.swapchain_;
    presentInfo.pImageIndices = &nextImage;
    presentInfo.pResults = &presentResult;
    vkQueuePresentKHR( device.queue_, &presentInfo );

    return true;
}

void setImageLayout( VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStages, VkPipelineStageFlags destStages )
{
    VkImageMemoryBarrier imageMemoryBarrier = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .pNext = NULL, .srcAccessMask = 0, .dstAccessMask = 0, .oldLayout = oldImageLayout, .newLayout = newImageLayout, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = image, .subresourceRange =
            { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1, }, };

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
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            break;

        default:
            break;
    }

    vkCmdPipelineBarrier( cmdBuffer, srcStages, destStages, 0, 0, NULL, 0, NULL, 1, &imageMemoryBarrier );
}