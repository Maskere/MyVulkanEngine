#include "SDL_events.h"
#include "SDL_keycode.h"
#include "SDL_scancode.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/geometric.hpp"
#include "systems.h"
#include "vk_pipelines.h"
#include <filesystem>
#include <string>
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <chrono>
#include <iostream>

#include <windows.h>
#include "vk_engine.h"
#include <winbase.h>
#include "fmt/core.h"
#include "vk_images.h"
#include "SDL_video.h"
#include "../third_party/SDL/src/video/khronos/vulkan/vulkan_core.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <cassert>
#include <cstdint>
#include <vk_initializers.h>
#include <vk_types.h>

#include "VkBootstrap.h"

#include <thread>
#include <vk_descriptors.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"


const SceneVertex CUBE_VERTS[] = {
    // Front  (z+)
    { {-0.5f,-0.5f, 0.5f}, { 0, 0, 1} },
    { { 0.5f,-0.5f, 0.5f}, { 0, 0, 1} },
    { { 0.5f, 0.5f, 0.5f}, { 0, 0, 1} },
    { {-0.5f, 0.5f, 0.5f}, { 0, 0, 1} },
    // Back   (z-)
    { { 0.5f,-0.5f,-0.5f}, { 0, 0,-1} },
    { {-0.5f,-0.5f,-0.5f}, { 0, 0,-1} },
    { {-0.5f, 0.5f,-0.5f}, { 0, 0,-1} },
    { { 0.5f, 0.5f,-0.5f}, { 0, 0,-1} },
    // Top    (y+)
    { {-0.5f, 0.5f, 0.5f}, { 0, 1, 0} },
    { { 0.5f, 0.5f, 0.5f}, { 0, 1, 0} },
    { { 0.5f, 0.5f,-0.5f}, { 0, 1, 0} },
    { {-0.5f, 0.5f,-0.5f}, { 0, 1, 0} },
    // Bottom (y-)
    { {-0.5f,-0.5f,-0.5f}, { 0,-1, 0} },
    { { 0.5f,-0.5f,-0.5f}, { 0,-1, 0} },
    { { 0.5f,-0.5f, 0.5f}, { 0,-1, 0} },
    { {-0.5f,-0.5f, 0.5f}, { 0,-1, 0} },
    // Right  (x+)
    { { 0.5f,-0.5f, 0.5f}, { 1, 0, 0} },
    { { 0.5f,-0.5f,-0.5f}, { 1, 0, 0} },
    { { 0.5f, 0.5f,-0.5f}, { 1, 0, 0} },
    { { 0.5f, 0.5f, 0.5f}, { 1, 0, 0} },
    // Left   (x-)
    { {-0.5f,-0.5f,-0.5f}, {-1, 0, 0} },
    { {-0.5f,-0.5f, 0.5f}, {-1, 0, 0} },
    { {-0.5f, 0.5f, 0.5f}, {-1, 0, 0} },
    { {-0.5f, 0.5f,-0.5f}, {-1, 0, 0} },
};

const uint32_t CUBE_INDICES[] = {
    0,1,2,  0,2,3,   // front
    4,5,6,  4,6,7,   // back
    8,9,10, 8,10,11, // top
    12,13,14,12,14,15,// bottom
    16,17,18,16,18,19,// right
    20,21,22,20,22,23 // left
};
constexpr bool bUseValidationLayers = true;

VulkanEngine* loadedEngine = nullptr;

//Singleton Pattern
VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }

void VulkanEngine::init(){
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    _window = SDL_CreateWindow(
        "3D viewerr",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags
    );

    init_vulkan();

    init_swapchain();

    init_commands();

    init_sync_structures();

    init_descriptors();

    init_pipelines();

    init_imgui();

    _sceneManager.init(&scene);
    init_scene();

    _isInitialized = true;
}

void VulkanEngine::init_pipelines(){
    init_background_pipelines();
}

//Uses Vulkan Bootstrap to bypass alot of initializations
void VulkanEngine::init_vulkan(){
    vkb::InstanceBuilder builder;

    auto inst_ret = builder.set_app_name("Example Vulkan Application")
        .request_validation_layers(bUseValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1,3,0)
        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

    VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features.dynamicRendering = true;
    features.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    vkb::PhysicalDeviceSelector selector{ vkb_inst };
    vkb::PhysicalDevice PhysicalDevice = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features)
        .set_required_features_12(features12)
        .set_surface(_surface)
        .select()
        .value();

    vkb::DeviceBuilder deviceBuilder { PhysicalDevice };

    vkb::Device vkbDevice = deviceBuilder.build().value();

    _device = vkbDevice.device;
    _chosenGPU = PhysicalDevice.physical_device;

    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _chosenGPU;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder SwapchainBuilder {_chosenGPU, _device, _surface};

    vkb::Swapchain vkbSwapchain = SwapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{ .format = VK_FORMAT_B8G8R8A8_UNORM, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) // Fix: Added COLOR_ATTACHMENT_BIT
        .build()
        .value();

    _swapchainExtent = vkbSwapchain.extent;
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();
    
    _swapchainImageFormat = vkbSwapchain.image_format; 
}


void VulkanEngine::init_swapchain(){
    create_swapchain(_windowExtent.width, _windowExtent.height);

    _imageRenderSemaphores.resize(_swapchainImages.size());
    VkSemaphoreCreateInfo semInfo {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for(size_t i = 0; i < _swapchainImages.size(); i++){
        VK_CHECK(vkCreateSemaphore(_device,&semInfo, nullptr, &_imageRenderSemaphores[i]));
    }

    VkExtent3D drawImageExtent = {
        _windowExtent.width,
        _windowExtent.height,
        1
    };

    //16 bit floats for all 4 channels and will use 64 bits per pixel
    _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat,drawImageUsages, drawImageExtent);

    VmaAllocationCreateInfo rimg_allocInfo = {};
    //now the texture wont ever be accessed from CPU. So the texture resides in the VRAM
    rimg_allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    //Imageview is needed to access images. Imageview is a thin wrapper over the image itself
    vmaCreateImage(_allocator, &rimg_info, &rimg_allocInfo, &_drawImage.image, &_drawImage.allocation, nullptr);

    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat,_drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));

    _mainDeletionQueue.push(DestroyType::Image, (uint64_t)_drawImage.image,(uint64_t)_drawImage.allocation);
    _mainDeletionQueue.push(DestroyType::ImageView, (uint64_t)_drawImage.imageView);
}

void VulkanEngine::destroy_swapchain(){
    for(auto sem : _imageRenderSemaphores){
        vkDestroySemaphore(_device,sem, nullptr);
    }
    _imageRenderSemaphores.clear();

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    for(int i = 0; i < _swapchainImageViews.size(); i++){
        vkDestroyImageView(_device, _swapchainImageViews[i],nullptr);
    }
}

void VulkanEngine::init_commands(){
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
        _graphicsQueueFamily,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    );
    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));

    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool,1);

    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

    _mainDeletionQueue.push(DestroyType::CommandPool, (uint64_t)_immCommandPool);

    for(int i = 0; i < FRAME_OVERLAP; i++){
        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool,1);

        VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));
    }

}

void VulkanEngine::init_sync_structures(){
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for(int i = 0; i < FRAME_OVERLAP; i++){
        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));
        // swapchain acquire semaphore is still per-frame
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
        // REMOVED: _renderSemaphore  ← now lives in _imageRenderSemaphores
    }

    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
    _mainDeletionQueue.push(DestroyType::Fence, (uint64_t)_immFence);
}

void VulkanEngine::cleanup() {
    if (!_isInitialized) { loadedEngine = nullptr; return; }

    vkDeviceWaitIdle(_device);
    ImGui_ImplVulkan_Shutdown();

    for (int i = 0; i < FRAME_OVERLAP; ++i) {
        vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
        vkDestroyFence(_device,  _frames[i]._renderFence, nullptr);
        vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);
        // REMOVED: vkDestroySemaphore(_device, _frames[i]._renderSemaphore, ...);
        _frames[i]._deletionQueue.flush(_device, _allocator);
    }

    // destroy per-swapchain-image render semaphores
    for (auto sem : _imageRenderSemaphores)
    vkDestroySemaphore(_device, sem, nullptr);
    _imageRenderSemaphores.clear();

    _mainDeletionQueue.flush(_device, _allocator);
    destroy_swapchain();
    vmaDestroyAllocator(_allocator);

    vkDestroyDevice(_device, nullptr);
    vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
    vkDestroySurfaceKHR(_instance, _surface, nullptr);   // ← fix #5
    vkDestroyInstance(_instance, nullptr);

    SDL_DestroyWindow(_window);
    _isInitialized = false;
    loadedEngine   = nullptr;
}

void VulkanEngine::draw() {
    VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000));
    get_current_frame()._deletionQueue.flush(_device, _allocator);
    VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000,
                                   get_current_frame()._swapchainSemaphore, nullptr, &swapchainImageIndex));

    VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo cmdBeginInfo =
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    _drawExtent.width  = _drawImage.imageExtent.width;
    _drawExtent.height = _drawImage.imageExtent.height;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    draw_scene(cmd);

    // ── copy to swapchain ──
    // vkutil::transition_image(cmd, _drawImage.image,
    //                          VK_IMAGE_LAYOUT_GENERAL,
    //                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    //                          VK_IMAGE_ASPECT_COLOR_BIT);
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);

    vkutil::copy_image_to_image(cmd, _drawImage.image,
                                _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_ASPECT_COLOR_BIT);
    draw_imgui(cmd, swapchainImageIndex);

    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                             VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkEndCommandBuffer(cmd));

    // ── submit ──
    VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphore imgSem = _imageRenderSemaphores[swapchainImageIndex];   // ← fix #3

    // VkSemaphoreSubmitInfo waitInfo   = vkinit::semaphore_submit_info(
    //     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
    //     get_current_frame()._swapchainSemaphore);
    VkSemaphoreSubmitInfo waitInfo   = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_TRANSFER_BIT |
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        get_current_frame()._swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        imgSem);                          // ← was _renderSemaphore

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, &signalInfo, &waitInfo);
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

    // ── present ──
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType            = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pSwapchains      = &_swapchain;
    presentInfo.swapchainCount   = 1;
    presentInfo.pWaitSemaphores  = &imgSem;           // ← was _renderSemaphore
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices    = &swapchainImageIndex;
    VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

    _frameNumber++;
}

VkFenceCreateInfo vkinit::fence_create_info(VkFenceCreateFlags flags /*=0*/){
    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.pNext = nullptr;

    info.flags = flags;

    return info;
}

VkSemaphoreCreateInfo vkinit::semaphore_create_info(VkSemaphoreCreateFlags flags /*=0*/){
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = flags;
    return info;
}

VkCommandBufferBeginInfo vkinit::command_buffer_begin_info(VkCommandBufferUsageFlags flags /*=0*/){
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = nullptr;

    info.pInheritanceInfo = nullptr;
    info.flags = flags;

    return info;
}

VkSemaphoreSubmitInfo vkinit::semaphore_submit_info(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore){
    VkSemaphoreSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.semaphore = semaphore;
    submitInfo.stageMask = stageMask;
    submitInfo.deviceIndex = 0;
    submitInfo.value = 0;

    return submitInfo;
}

VkCommandBufferSubmitInfo vkinit::command_buffer_submit_info(VkCommandBuffer cmd){
    VkCommandBufferSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    info.pNext = nullptr;
    info.commandBuffer = cmd;
    info.deviceMask = 0;

    return info;
}

VkSubmitInfo2 vkinit::submit_info(VkCommandBufferSubmitInfo *cmd, VkSemaphoreSubmitInfo *signalSemaphoreInfo, VkSemaphoreSubmitInfo *waitSemaphoreInfo){
    VkSubmitInfo2 info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    info.pNext = nullptr;

    info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
    info.pWaitSemaphoreInfos = waitSemaphoreInfo;

    info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
    info.pSignalSemaphoreInfos = signalSemaphoreInfo;

    info.commandBufferInfoCount = 1;
    info.pCommandBufferInfos = cmd;

    return info;
}

void VulkanEngine::draw_background(VkCommandBuffer cmd){
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout,0,1,&_drawImageDescriptors,0,nullptr);
    vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0),1);
}

void VulkanEngine::run(){
    SDL_Event e;
    bool bQuit = false;
    auto lastTime = std::chrono::steady_clock::now();

    while(!bQuit){
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        _dt = dt;
        lastTime = now;

        while(SDL_PollEvent(&e) != 0){
            if(e.type == SDL_QUIT)
                bQuit = true;

            if(e.type == SDL_WINDOWEVENT){
                if(e.window.event == SDL_WINDOWEVENT_MINIMIZED){
                    stop_rendering = true;
                }
                if(e.window.event == SDL_WINDOWEVENT_RESTORED){
                    stop_rendering = false;
                }
                if(e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED){
                    int w = e.window.data1, h = e.window.data2;
                    if(w == 0 || h == 0) return;

                    vkDeviceWaitIdle(_device);
                    destroy_swapchain();
                    create_swapchain(w, h);

                    _imageRenderSemaphores.resize(_swapchainImages.size());
                    VkSemaphoreCreateInfo si{}; si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                    for(size_t i = 0; i < _swapchainImages.size(); i++){
                        VK_CHECK(vkCreateSemaphore(_device, &si, nullptr, &_imageRenderSemaphores[i]));
                    }
                }
            }
            // if(e.type == SDL_MOUSEMOTION){
            //     float sens = 0.003f;
            //     _yaw += e.motion.xrel * sens;
            //     _pitch += e.motion.yrel * sens;
            //     _pitch = glm::clamp(_pitch, -1.55f, 1.55f);
            // }

            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        if(stop_rendering){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        handle_input(_dt);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        const Scene* currentScene = _sceneManager.raw();
        if (!_sceneManager.is_alive(_selectedEntity)) _selectedEntity = {};

        ImGui::Begin("Scene");
        ImGui::Text("Objects: %u / %u", currentScene->count, MAX_ENTITIES);
        ImGui::Text("Meshes: %u / %u", currentScene->meshCount, MAX_MESHES);
        ImGui::Text("Materials: %u / %u", currentScene->matCount, MAX_MATERIALS);
        ImGui::Text("Frame rate: %.1f FPS (%.2f ms)", ImGui::GetIO().Framerate, _dt * 1000.0f);
        ImGui::Text("Render size: %u x %u", _drawImage.imageExtent.width, _drawImage.imageExtent.height);
        ImGui::Text("Camera: %.2f, %.2f, %.2f", _cameraPos.x, _cameraPos.y, _cameraPos.z);
        ImGui::Separator();

        glm::vec3 cameraForward = glm::normalize(glm::vec3(
            glm::sin(_yaw) * glm::cos(_pitch),
            glm::sin(_pitch),
            glm::cos(_yaw) * glm::cos(_pitch)));
        if (ImGui::Button("Add cube")) {
            _selectedEntity = spawn_cube(_cameraPos + cameraForward * 5.0f);
        }

        ImGui::BeginChild("Object list", ImVec2(0.0f, 150.0f), true);
        for (uint32_t denseIndex = 0; denseIndex < currentScene->count; ++denseIndex) {
            const EntityId id = _sceneManager.entity_at_dense(denseIndex);
            ImGui::PushID(static_cast<int>(id.index));
            ImGui::PushID(static_cast<int>(id.generation));
            const bool selected = id.index == _selectedEntity.index &&
                                  id.generation == _selectedEntity.generation;
            if (ImGui::Selectable("Object", selected)) _selectedEntity = id;
            ImGui::SameLine();
            ImGui::Text("ID %u / generation %u", id.index, id.generation);
            ImGui::PopID();
            ImGui::PopID();
        }
        ImGui::EndChild();

        EntityTransform selectedTransform;
        if (_sceneManager.get_transform(_selectedEntity, selectedTransform)) {
            ImGui::Text("Selected ID: %u / generation %u",
                        _selectedEntity.index, _selectedEntity.generation);
            ImGui::Text("Position: %.2f, %.2f, %.2f",
                        selectedTransform.position.x, selectedTransform.position.y, selectedTransform.position.z);
            ImGui::Text("Scale: %.2f, %.2f, %.2f",
                        selectedTransform.scale.x, selectedTransform.scale.y, selectedTransform.scale.z);
            ImGui::Text("Rotation (quaternion): %.2f, %.2f, %.2f, %.2f",
                        selectedTransform.rotation.x, selectedTransform.rotation.y,
                        selectedTransform.rotation.z, selectedTransform.rotation.w);
            if (ImGui::Button("Delete selected")) {
                _sceneManager.despawn(_selectedEntity);
                _selectedEntity = {};
            }
        }
        ImGui::End();
        ImGui::Render();

        // draw_background(cmd);
        draw();
    }
}

void VulkanEngine::init_descriptors(){
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes ={
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1}
    };

    globalDescriptorAllocator.init_pool(_device, 10, sizes);

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    _drawImageDescriptors = globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgInfo.imageView = _drawImage.imageView;

    VkWriteDescriptorSet drawImageWrite = {};
    drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawImageWrite.pNext = nullptr;

    drawImageWrite.dstBinding = 0;
    drawImageWrite.dstSet = _drawImageDescriptors;
    drawImageWrite.descriptorCount = 1;
    drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    drawImageWrite.pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(_device, 1, &drawImageWrite, 0, nullptr);

    _mainDeletionQueue.push(DestroyType::DescriptorPool, (uint64_t)globalDescriptorAllocator.pool);
    _mainDeletionQueue.push(DestroyType::DescriptorSetLayout, (uint64_t)_drawImageDescriptorLayout);
}

void VulkanEngine::init_background_pipelines(){
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_gradientPipelineLayout));

    VkShaderModule computeDrawShader;
    if(!vkutil::load_shader_module("shaders/gradient.comp.spv",_device,&computeDrawShader)){
        fmt::print("Error when building the compute shader \n");
    }

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = computeDrawShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = _gradientPipelineLayout;
    computePipelineCreateInfo.stage = stageInfo;

    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &_gradientPipeline));

    vkDestroyShaderModule(_device, computeDrawShader, nullptr);

    _mainDeletionQueue.push(DestroyType::PipelineLayout, (uint64_t)_gradientPipelineLayout);
    _mainDeletionQueue.push(DestroyType::Pipeline, (uint64_t)_gradientPipeline);
}

//can be moved to operation outside of the render loop
void VulkanEngine::immidiate_submit(std::function<void(VkCommandBuffer cmd)>&& function){
    VK_CHECK(vkResetFences(_device,1,&_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer,0));

    VkCommandBuffer cmd = _immCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd,&cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

    VK_CHECK(vkWaitForFences(_device,1,&_immFence, true, 9999999999));
}

void VulkanEngine::init_imgui(){
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,    1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,             1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,             1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,      1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,      1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,            1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,            1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,    1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,    1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,          1000 },
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

    // ── render pass for ImGui (targets the swapchain image) ──
    VkAttachmentDescription color{};
    color.format         = _swapchainImageFormat;
    color.samples        = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
    color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_GENERAL};
    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription sub{};
    sub.pipelineBindPoint   = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments   = &ref;

    VkRenderPassCreateInfo rpci{};
    rpci.sType         = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1;
    rpci.pAttachments    = &color;
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &sub;

    VK_CHECK(vkCreateRenderPass(_device, &rpci, nullptr, &_imguiRenderPass));
    for (auto iv : _swapchainImageViews) {
        VkFramebufferCreateInfo fbi{};
        fbi.sType      = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbi.renderPass = _imguiRenderPass;
        fbi.attachmentCount = 1;
        fbi.pAttachments    = &iv;
        fbi.width           = _swapchainExtent.width;
        fbi.height          = _swapchainExtent.height;
        fbi.layers = 1;
        VkFramebuffer fb;
        VK_CHECK(vkCreateFramebuffer(_device, &fbi, nullptr, &fb));
        _imguiFramebuffers.push_back(fb);
        _mainDeletionQueue.push(DestroyType::FrameBuffer, (uint64_t)fb);
    }
    _mainDeletionQueue.push(DestroyType::RenderPass, (uint64_t)_imguiRenderPass);

    // ── ImGui init (render-pass mode — no UseDynamicRendering) ──
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(_window);

    ImGui_ImplVulkan_InitInfo ii{};
    ii.Instance       = _instance;
    ii.PhysicalDevice = _chosenGPU;
    ii.Device         = _device;
    ii.Queue          = _graphicsQueue;
    ii.DescriptorPool = imguiPool;
    ii.MinImageCount  = 2;
    ii.ImageCount     = 2;
    ii.RenderPass     = _imguiRenderPass;          // ← this instead of PipelineRenderingCreateInfo
    ii.MSAASamples    = VK_SAMPLE_COUNT_1_BIT;

    if (!ImGui_ImplVulkan_Init(&ii))
        fmt::print(stderr, "FATAL: ImGui_ImplVulkan_Init failed\n");

    ImGui_ImplVulkan_CreateFontsTexture();
    _mainDeletionQueue.push(DestroyType::DescriptorPool, (uint64_t)imguiPool);
    ImGui::StyleColorsLight();
    ImGui_ImplVulkan_DestroyFontsTexture();
    // ImGui_ImplVulkan_DestroyFontUploadObjects();
}

VkRenderingAttachmentInfo vkinit::attachment_info(VkImageView view, VkClearValue *clear, VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/){
    VkRenderingAttachmentInfo colorAttachment {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.pNext = nullptr;

    colorAttachment.imageView = view;
    colorAttachment.imageLayout = layout;
    colorAttachment.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    if(clear){
        colorAttachment.clearValue = *clear;
    }

    return colorAttachment;
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, uint32_t idx){
    VkRenderPassBeginInfo rpbi{};
    rpbi.sType      = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderPass = _imguiRenderPass;
    rpbi.framebuffer = _imguiFramebuffers[idx];
    rpbi.renderArea  = {{0,0}, _swapchainExtent};

    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRenderPass(cmd);
}

EntityId VulkanEngine::spawn_cube(glm::vec3 pos) {
    return _sceneManager.spawn(pos, glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                               glm::vec3(1.0f), _cubeMesh, _cubeMat);
}
void VulkanEngine::init_scene() {
    uint32_t w = _drawImage.imageExtent.width;
    uint32_t h = _drawImage.imageExtent.height;

    // ── depth image ──────────────────────────────────────
    {
        VkImageCreateInfo di = vkinit::image_create_info(
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            { w, h, 1 });

        VmaAllocationCreateInfo da{};
        da.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateImage(
            _allocator, &di, &da,
            &_sceneDepthImage.image, &_sceneDepthImage.allocation, nullptr));

        _sceneDepthImage.imageFormat = VK_FORMAT_D24_UNORM_S8_UINT;
        _sceneDepthImage.imageExtent = { w, h, 1 };

        VkImageViewCreateInfo dvi = vkinit::imageview_create_info(
            VK_FORMAT_D24_UNORM_S8_UINT,
            _sceneDepthImage.image,
            VK_IMAGE_ASPECT_DEPTH_BIT);
        VK_CHECK(vkCreateImageView(_device, &dvi, nullptr, &_sceneDepthImage.imageView));

        _mainDeletionQueue.push(DestroyType::Image,     (uint64_t)_sceneDepthImage.image, (uint64_t)_sceneDepthImage.allocation);
        _mainDeletionQueue.push(DestroyType::ImageView, (uint64_t)_sceneDepthImage.imageView);
    }

    // ── vertex buffer ────────────────────────────────────
    {
        size_t vsize = sizeof(CUBE_VERTS);

        VkBufferCreateInfo bci{};
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = vsize;
        bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        VmaAllocationCreateInfo aci{};
        aci.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VK_CHECK(vmaCreateBuffer(_allocator, &bci, &aci, &_vertexBuffer, &_vertexBufferAlloc, nullptr));

        void* data;
        VK_CHECK(vmaMapMemory(_allocator, _vertexBufferAlloc, &data));
        std::memcpy(data, CUBE_VERTS, vsize);
        vmaUnmapMemory(_allocator, _vertexBufferAlloc);

        _mainDeletionQueue.push(DestroyType::Buffer, (uint64_t)_vertexBuffer, (uint64_t)_vertexBufferAlloc);
    }

    // ── index buffer ─────────────────────────────────────
    {
        _indexCount = std::size(CUBE_INDICES);
        size_t isize = _indexCount * sizeof(uint32_t);

        VkBufferCreateInfo bci{};
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = isize;
        bci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

        VmaAllocationCreateInfo aci{};
        aci.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VK_CHECK(vmaCreateBuffer(_allocator, &bci,&aci, &_indexBuffer, &_indexBufferAlloc, nullptr));

        void* data;
        VK_CHECK(vmaMapMemory(_allocator, _indexBufferAlloc, &data));
        std::memcpy(data, CUBE_INDICES, isize);
        vmaUnmapMemory(_allocator, _indexBufferAlloc);

        _mainDeletionQueue.push(DestroyType::Buffer, (uint64_t)_indexBuffer, (uint64_t)_indexBufferAlloc);
    }

    // ── uniform buffer ───────────────────────────────────
    {
        VkBufferCreateInfo ubi{};
        ubi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ubi.size  = sizeof(SceneUniforms);
        ubi.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo aci{};
        aci.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VK_CHECK(vmaCreateBuffer(_allocator, &ubi,&aci, &_uniformBuffer, &_uniformBufferAlloc, nullptr));

        _mainDeletionQueue.push(DestroyType::Buffer, (uint64_t)_uniformBuffer, (uint64_t)_uniformBufferAlloc);
    }

    _cubeMesh = _sceneManager.add_mesh(Mesh{
        _vertexBuffer, _indexBuffer, _indexCount, VK_INDEX_TYPE_UINT32
    });

    _cubeMat = _sceneManager.add_material(Material{
        _scenePipeline, _sceneDescriptorSet
    });

    // ── descriptor set ───────────────────────────────────
    {
        std::vector<DescriptorAllocator::PoolSizeRatio> poolSizes{
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
        };
        _sceneDescriptorAllocator.init_pool(_device, 4, poolSizes);

        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        // _sceneDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT);
        _sceneDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

        _sceneDescriptorSet = _sceneDescriptorAllocator.allocate(_device, _sceneDescriptorLayout);

        // initial write (buffer range covers the whole struct)
        VkDescriptorBufferInfo bufInfo{ _uniformBuffer, 0, sizeof(SceneUniforms) };
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = _sceneDescriptorSet;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &bufInfo;
        vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);

        _mainDeletionQueue.push(DestroyType::DescriptorPool,   (uint64_t)_sceneDescriptorAllocator.pool);
        _mainDeletionQueue.push(DestroyType::DescriptorSetLayout, (uint64_t)_sceneDescriptorLayout);
    }

    // ── pipeline ─────────────────────────────────────────
    {
        VkPushConstantRange pc{};
        pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pc.offset = 0;
        pc.size = sizeof(glm::mat4);

        VkPipelineLayoutCreateInfo plci{};
        plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plci.setLayoutCount = 1;
        plci.pSetLayouts = &_sceneDescriptorLayout;
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges = &pc;

        VK_CHECK(vkCreatePipelineLayout(_device, &plci, nullptr, &_scenePipelineLayout));

        VkShaderModule vertMod, fragMod;
        if (!vkutil::load_shader_module("shaders/scene.vert.spv", _device, &vertMod)) { abort(); }
        if (!vkutil::load_shader_module("shaders/scene.frag.spv", _device, &fragMod)) { abort(); }

        VkPipelineShaderStageCreateInfo vs{}, fs{};
        vs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vs.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vs.module = vertMod;
        vs.pName = "main";

        fs.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fs.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fs.module = fragMod;
        fs.pName = "main";

        VkPipelineVertexInputStateCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.vertexBindingDescriptionCount = 1;
        static const VkVertexInputBindingDescription bindingDesc{ 0, sizeof(SceneVertex), VK_VERTEX_INPUT_RATE_VERTEX };
        vi.pVertexBindingDescriptions = &bindingDesc;
        vi.vertexAttributeDescriptionCount = 2;
        static const VkVertexInputAttributeDescription attrDescs[2] = {
            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SceneVertex, pos) },
            { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SceneVertex, nrm) },
        };
        vi.pVertexAttributeDescriptions = attrDescs;

        VkPipelineInputAssemblyStateCreateInfo ia{};
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo vp{};
        vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp.viewportCount = 1;
        vp.scissorCount = 1;

        VkPipelineDynamicStateCreateInfo dyn{};
        dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyn.dynamicStateCount = 2;
        static const VkDynamicState dynStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        dyn.pDynamicStates = dynStates;

        VkPipelineRasterizationStateCreateInfo rs{};
        rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.depthClampEnable = VK_FALSE;
        rs.rasterizerDiscardEnable = VK_FALSE;
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode = VK_CULL_MODE_BACK_BIT;
        rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.depthBiasEnable = VK_FALSE;
        rs.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo ds{};
        ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.depthTestEnable = VK_TRUE;
        ds.depthWriteEnable = VK_TRUE;
        ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        ds.depthBoundsTestEnable = VK_FALSE;
        ds.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState cb{};
        cb.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo cbs{};
        cbs.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cbs.attachmentCount = 1;
        cbs.pAttachments = &cb;

        VkPipelineRenderingCreateInfo prci{};
        prci.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        prci.colorAttachmentCount = 1;
        static const VkFormat colorFmt = VK_FORMAT_R16G16B16A16_SFLOAT; // match your _drawImage
        prci.pColorAttachmentFormats = &colorFmt;
        prci.depthAttachmentFormat   = VK_FORMAT_D24_UNORM_S8_UINT;     // match your depth image

        VkGraphicsPipelineCreateInfo gpci{};
        gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gpci.pNext = &prci;
        gpci.stageCount = 2;
        static const VkPipelineShaderStageCreateInfo stages[2] = { vs, fs };
        gpci.pStages = stages;
        gpci.pVertexInputState = &vi;
        gpci.pInputAssemblyState = &ia;
        gpci.pViewportState = &vp;
        gpci.pRasterizationState = &rs;
        gpci.pMultisampleState = &ms;
        gpci.pDepthStencilState = &ds;
        gpci.pColorBlendState = &cbs;
        gpci.layout = _scenePipelineLayout;
        gpci.renderPass = VK_NULL_HANDLE; // dynamic rendering
        gpci.pDynamicState = &dyn;

        VK_CHECK(vkCreateGraphicsPipelines(
            _device, VK_NULL_HANDLE, 1, &gpci, nullptr, &_scenePipeline));

        vkDestroyShaderModule(_device, vertMod, nullptr);
        vkDestroyShaderModule(_device, fragMod, nullptr);

        _mainDeletionQueue.push(DestroyType::PipelineLayout, (uint64_t)_scenePipelineLayout);
        _mainDeletionQueue.push(DestroyType::Pipeline,       (uint64_t)_scenePipeline);
    }

    // Retain stable handles even when their packed component slots move.
    _initialEntityHandles[0] = spawn_cube({0.0f,0.0f,0.0f});
    _initialEntityHandles[1] = spawn_cube({2.0f,0.0f,0.0f});
    _initialEntityHandles[2] = spawn_cube({-2.0f,1.0f,0.0f});
    _initialEntityHandles[3] = spawn_cube({1.0f,2.0f,-3.0f});
    _selectedEntity = _initialEntityHandles[0];
    immidiate_submit([&](VkCommandBuffer cmd) {
        vkutil::transition_image(cmd,_sceneDepthImage.image,
                                 VK_IMAGE_LAYOUT_UNDEFINED,
                                 // VK_IMAGE_LAYOUT_GENERAL,
                                 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                 VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
        vkutil::transition_image(cmd,_drawImage.image,
                                 VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_GENERAL,
                                 VK_IMAGE_ASPECT_COLOR_BIT);
    });
}

void VulkanEngine::draw_scene(VkCommandBuffer cmd) {
    VkViewport viewport{ 0, 0, (float)_drawExtent.width, (float)_drawExtent.height, 0.0f, 1.0f };
    VkRect2D   scissor  { { 0, 0 }, { _drawExtent.width, _drawExtent.height } };

    glm::vec3 fwd = glm::normalize(glm::vec3(
        glm::sin(_yaw) * glm::cos(_pitch),
        glm::sin(_pitch),
        glm::cos(_yaw) * glm::cos(_pitch)
    ));
    // ── ★ write valid matrices into the uniform buffer ★ ──
    SceneUniforms u{};
    // u.view      = glm::lookAt(_cameraPos, {0.0f,0.0f,0.0f}, {0.0f,1.0f,0.0f});
    u.view      = glm::lookAt(_cameraPos, _cameraPos + fwd, {0.0f,1.0f,0.0f});
    u.proj      = glm::perspective(
        glm::radians(60.0f),
        (float)_drawExtent.width / (float)_drawExtent.height,
        0.1f, 100.0f);
    // camera projection fix: correctly renders meshes (to avoid rendering faces inside out)
    u.proj *= glm::mat4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        0.0f, 0.0f, 0.5f, 1.0f
    );
    u.cameraPos = _cameraPos;

    vkCmdUpdateBuffer(cmd, _uniformBuffer, 0, sizeof(u), &u);
    VkBufferMemoryBarrier2 uniformBarrier{};
    uniformBarrier.sType =
        VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;

    uniformBarrier.srcStageMask =
        VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    uniformBarrier.srcAccessMask =
        VK_ACCESS_2_TRANSFER_WRITE_BIT;

    uniformBarrier.dstStageMask =
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    uniformBarrier.dstAccessMask =
        VK_ACCESS_2_UNIFORM_READ_BIT;

    uniformBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    uniformBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    uniformBarrier.buffer = _uniformBuffer;
    uniformBarrier.offset = 0;
    uniformBarrier.size = sizeof(SceneUniforms);

    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.bufferMemoryBarrierCount = 1;
    dependency.pBufferMemoryBarriers = &uniformBarrier;

    vkCmdPipelineBarrier2(cmd, &dependency);
    // ──────────────────────────────────────────────────────────

    VkRenderingAttachmentInfo color{};
    color.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    color.imageView   = _drawImage.imageView;
    color.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
    color.clearValue  = { 0.05f, 0.05f, 0.08f, 1.0f };

    VkRenderingAttachmentInfo depth{};
    depth.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    // depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    // depth.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.imageView   = _sceneDepthImage.imageView;
    depth.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp     = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.clearValue  = { 1.0f, 0, 0, 0 };

    VkRenderingInfo ri{};
    ri.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    ri.renderArea           = { { 0, 0 }, { _drawExtent.width, _drawExtent.height } };
    ri.layerCount           = 1;
    ri.viewMask             = 0;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments    = &color;
    ri.pDepthAttachment     = &depth;

    vkCmdBeginRendering(cmd, &ri);

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS, _scenePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,_scenePipelineLayout, 0,1,
                            &_sceneDescriptorSet,0, nullptr);

    _sceneManager.update_matrices(_dt);
    _sceneManager.render(cmd, _scenePipelineLayout);

    vkCmdEndRendering(cmd);
}

void VulkanEngine::destroy_scene() {
    // All resources are already in _mainDeletionQueue,
    // so this is only needed if you want to support
    // mid-session teardown (e.g. a "reset" button).
    // For now it's a no-op; cleanup() handles it.
}

void VulkanEngine::handle_input(float dt){
    const Uint8* keys = SDL_GetKeyboardState(nullptr);

    float speed = 5.0f * dt;
    if(keys[SDL_SCANCODE_LSHIFT]){
        speed *= 2.0f;
    }

    glm::vec3 forward = glm::normalize(
        glm::vec3(
            glm::sin(_yaw) * glm::cos(_pitch),
            glm::sin(_pitch),
            glm::cos(_yaw) * glm::cos(_pitch))
    );
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f,1.0f,0.0f)));

    if(keys[SDL_SCANCODE_W]) _cameraPos += forward * speed;
    if(keys[SDL_SCANCODE_S]) _cameraPos -= forward * speed;
    if(keys[SDL_SCANCODE_D]) _cameraPos += right * speed;
    if(keys[SDL_SCANCODE_A]) _cameraPos -= right * speed;

    // _view = glm::lookAt(_cameraPos, _cameraPos + forward, glm::vec3(0.0f,1.0f,0.0f));
}
