// components.h
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vulkan/vulkan.h>

struct Transform {
    glm::vec3 position {0.0f};
    glm::quat rotation {1.0f,0.0f,0.0f,0.0f};
    glm::vec3 scale {1.0f};
};

struct Mesh {
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
    uint32_t indexCount;
    VkIndexType indexType;
};

struct Material {
    VkPipeline pipeline;
    VkDescriptorSet descSet;
};
