#pragma once

#include "../third_party/SDL/src/video/khronos/vulkan/vulkan_core.h"
#include <cstdint>
// #include <deque>
#include <functional>
#include <vector>
#include <array>
#include <vk_types.h>
#include <vk_descriptors.h>
// #include <entt/entt.hpp>
#include <scene.h>
#include <components.h>

enum class DestroyType : uint8_t {
    Image,
    ImageView,
    Buffer,
    Pipeline,
    PipelineLayout,
    DescriptorPool,
    DescriptorSetLayout,
    CommandPool,
    Semaphore,
    Fence,
    ShaderModule,
    RenderPass,
    FrameBuffer,
};

struct DestroyRecord {
    uint8_t type;
    uint8_t _pad[7];
    uint64_t handle;
    uint64_t handle2;
};
static_assert(sizeof(DestroyRecord) == 24);

struct DeletionQueue {
    std::vector<DestroyRecord> _records;

    void push(DestroyType type, uint64_t handle, uint64_t handle2 = 0) {
        _records.push_back({ static_cast<uint8_t>(type), {}, handle, handle2 });
    }

    void flush(VkDevice dev, VmaAllocator alloc) {
        // Destroy in reverse order of creation (children before parents)
        for (auto it = _records.rbegin(); it != _records.rend(); ++it) {
            const auto& r = *it;
            switch (static_cast<DestroyType>(r.type)) {
                case DestroyType::ImageView:
                    vkDestroyImageView(dev, (VkImageView)r.handle, nullptr); break;
                case DestroyType::Image:
                    vmaDestroyImage(alloc, (VkImage)r.handle, (VmaAllocation)r.handle2); break;
                case DestroyType::Buffer:
                    vmaDestroyBuffer(alloc, (VkBuffer)r.handle, (VmaAllocation)r.handle2); break;
                case DestroyType::Pipeline:
                    vkDestroyPipeline(dev, (VkPipeline)r.handle, nullptr); break;
                case DestroyType::PipelineLayout:
                    vkDestroyPipelineLayout(dev, (VkPipelineLayout)r.handle, nullptr); break;
                case DestroyType::DescriptorPool:
                    vkDestroyDescriptorPool(dev, (VkDescriptorPool)r.handle, nullptr); break;
                case DestroyType::DescriptorSetLayout:
                    vkDestroyDescriptorSetLayout(dev, (VkDescriptorSetLayout)r.handle, nullptr); break;
                case DestroyType::CommandPool:
                    vkDestroyCommandPool(dev, (VkCommandPool)r.handle, nullptr); break;
                case DestroyType::Semaphore:
                    vkDestroySemaphore(dev, (VkSemaphore)r.handle, nullptr); break;
                case DestroyType::Fence:
                    vkDestroyFence(dev, (VkFence)r.handle, nullptr); break;
                case DestroyType::ShaderModule:
                    vkDestroyShaderModule(dev, (VkShaderModule)r.handle, nullptr); break;
                case DestroyType::RenderPass:
                    vkDestroyRenderPass(dev, (VkRenderPass)r.handle, nullptr); break;
                case DestroyType::FrameBuffer:
                    vkDestroyFramebuffer(dev, (VkFramebuffer)r.handle, nullptr); break;
                default:
                    assert(false && "Unhandled DestroyType in DeletionQueue::flush");
                    break;
            }
        }
        _records.clear();
    }
};

struct FrameData {
    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;
    // VkSemaphore _swapchainSemaphore, _renderSemaphore;
    VkSemaphore _swapchainSemaphore;
    // std::vector<VkSemaphore> _imageRenderSemaphores;
    VkFence _renderFence;
    DeletionQueue _deletionQueue;
};

struct SceneVertex{
    glm::vec3 pos;
    glm::vec3 nrm;
};

struct SceneUniforms {
    // glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 cameraPos;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:
    bool _isInitialized {false};
    int _frameNumber {0};
    bool stop_rendering {false};

    float _yaw = 0.0f;
    float _pitch = -0.3f;

    VkExtent2D _windowExtent { 1920, 1080 };
    DeletionQueue _mainDeletionQueue;
    VmaAllocator _allocator;

    FrameData _frames[FRAME_OVERLAP];

    FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    struct SDL_Window* _window {nullptr};

    static VulkanEngine& Get();

    std::vector<VkFramebuffer> _imguiFramebuffers;

    void init();
    void cleanup();
    void draw();
    void draw_background(VkCommandBuffer cmd);
    void draw_imgui(VkCommandBuffer cmd, uint32_t idx);

    VkBuffer get_uniform_buffer() const {return _uniformBuffer;}
    VkPipelineLayout get_scene_pipeline_layout() const {return _scenePipelineLayout;}

    EntityId spawn_cube(glm::vec3 pos);
// void VulkanEngine::draw_imgui(VkCommandBuffer cmd, uint32_t idx){
    void run();

    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debug_messenger;
    VkPhysicalDevice _chosenGPU;
    VkDevice _device;
    VkSurfaceKHR _surface;

    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;

    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;
    VkExtent2D _swapchainExtent;

    AllocatedImage _drawImage;
    AllocatedImage _sceneDepthImage;
    VkExtent2D _drawExtent;

    DescriptorAllocator globalDescriptorAllocator;
    DescriptorAllocator _sceneDescriptorAllocator;

    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSet _sceneDescriptorSet;
    VkDescriptorSetLayout _sceneDescriptorLayout;
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    VkPipeline _gradientPipeline;
    VkPipeline _scenePipeline;
    VkPipelineLayout _scenePipelineLayout;
    VkPipelineLayout _gradientPipelineLayout;

    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;

    VkBuffer _vertexBuffer;
    VkBuffer _indexBuffer;
    VkBuffer _uniformBuffer;
    VmaAllocation _vertexBufferAlloc;
    VmaAllocation _indexBufferAlloc;
    VmaAllocation _uniformBufferAlloc;
    uint32_t _indexCount { 0 };

    VkRenderPass _imguiRenderPass {VK_NULL_HANDLE};

    glm::mat4 _view { 1.0f };
    glm::mat4 _proj { 1.0f };
    glm::vec3 _cameraPos { 0.0f, 2.0f, -5.0f };
    float _dt = 1.0f / 60.0f;

    void immidiate_submit(std::function<void(VkCommandBuffer cmd)>&& function);
    void init_scene();
    void draw_scene(VkCommandBuffer cmd);
private:
    // entt::registry _reg;
    Scene scene;
    std::vector<VkSemaphore> _imageRenderSemaphores;
    SceneManager _sceneManager;
    std::array<EntityId, 4> _initialEntityHandles{};
    EntityId _selectedEntity{};
    uint32_t _cubeMesh;
    uint32_t _cubeMat;

    void init_vulkan();
    void init_swapchain();
    void init_commands();
    void init_sync_structures();
    void init_descriptors();
    void init_pipelines();
    void init_background_pipelines();
    void init_imgui();

    void handle_input(float dt);
    void create_swapchain(uint32_t width, uint32_t height);
    void destroy_swapchain();
    void destroy_scene();
};

