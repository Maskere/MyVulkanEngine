#pragma once

#include "../third_party/SDL/src/video/khronos/vulkan/vulkan_core.h"
#include <vulkan/vulkan.h>

namespace vkutil{
void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destionation, VkExtent2D srcSize, VkExtent2D dstSize);
};

