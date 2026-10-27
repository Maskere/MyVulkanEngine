#include "systems.h"
#include "vk_engine.h"
#include <glm/gtc/quaternion.hpp>

namespace ecs {

void update_transforms(entt::registry& reg, float dt) {
    // Spin every cube a little each frame
    auto view = reg.view<Transform>();
    for (auto entity : view) {
        auto& t = reg.get<Transform>(entity);
        // rotate around Y-axis at 1 rad/s
        float angle = dt * 1.0f;
        t.rotation = glm::quat(glm::vec3(0.0f, angle, 0.0f)) * t.rotation;
        t.rotation = glm::normalize(t.rotation);
    }
}

void render(entt::registry& reg, VkCommandBuffer cmd, VulkanEngine& eng) {
    auto view = reg.view<Transform, Mesh, Material>();

    for (auto e : view) {
        auto& transform = reg.get<Transform>(e);
        auto& mesh  = reg.get<Mesh>(e);
        auto& mat = reg.get<Material>(e);

        glm::mat4 model =
            glm::translate(glm::mat4(1.0f), transform.position) *
            glm::mat4(transform.rotation) *
            glm::scale(glm::mat4(1.0f), transform.scale);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mat.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                eng._scenePipelineLayout, 0, 1,
                                &mat.descSet, 0, nullptr);

        vkCmdPushConstants(cmd, eng._scenePipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(glm::mat4), &model);   // ← per-cube model

        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer, &off);
        vkCmdBindIndexBuffer(cmd, mesh.indexBuffer, 0, mesh.indexType);

        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
    }
}

} // namespace ecs
