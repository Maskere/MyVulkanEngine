// systems.h
#pragma once
#include "glm/ext/matrix_transform.hpp"
#include "glm/matrix.hpp"
#include "vk_engine.h"
#include <entt/entt.hpp>
#include <vulkan/vulkan.h>
#include <components.h>

class VulkanEngine;

namespace ecs {
void update_transforms(entt::registry& reg, float dt);
void render(entt::registry& reg, VkCommandBuffer cmd, VulkanEngine& eng);
}

