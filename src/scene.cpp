#include "scene.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "vk_engine.h"
#include <cstdint>

uint32_t SceneManager::add_mesh(Mesh mesh){
    uint32_t id = _scene->meshCount++;
    _scene->meshes[id] = mesh;
    return id;
}

uint32_t SceneManager::add_material(Material material){
    uint32_t id = _scene->matCount++;
    _scene->materials[id] = material;
    return id;
}

EntityId SceneManager::spawn(glm::vec3 pos, glm::quat rot,
                             glm::vec3 scale, uint32_t mesh, uint32_t material){
    if (!_scene || _scene->count >= MAX_ENTITIES ||
        mesh >= _scene->meshCount || material >= _scene->matCount) {
        return {};
    }

    uint32_t entityIndex;
    if (_scene->freeCount > 0) {
        entityIndex = _scene->freeEntityIndices[--_scene->freeCount];
    } else {
        if (_scene->nextEntityIndex >= MAX_ENTITIES) return {};
        entityIndex = _scene->nextEntityIndex++;
    }

    const uint32_t denseIndex = _scene->count++;
    if (_scene->generations[entityIndex] == 0) {
        _scene->generations[entityIndex] = 1;
    }

    _scene->positions[denseIndex] = pos;
    _scene->rotations[denseIndex] = rot;
    _scene->scales[denseIndex] = scale;
    _scene->meshIdx[denseIndex] = static_cast<uint8_t>(mesh);
    _scene->matIdx[denseIndex] = static_cast<uint8_t>(material);
    _scene->entityIndexByDense[denseIndex] = entityIndex;
    _scene->denseIndexByEntity[entityIndex] = denseIndex;

    return {entityIndex, _scene->generations[entityIndex]};
}

bool SceneManager::is_alive(EntityId id) const {
    if (!_scene || !id || id.index >= _scene->nextEntityIndex ||
        _scene->generations[id.index] != id.generation) {
        return false;
    }

    const uint32_t denseIndex = _scene->denseIndexByEntity[id.index];
    return denseIndex < _scene->count &&
           _scene->entityIndexByDense[denseIndex] == id.index;
}

EntityId SceneManager::entity_at_dense(uint32_t denseIndex) const {
    if (!_scene || denseIndex >= _scene->count) return {};
    const uint32_t entityIndex = _scene->entityIndexByDense[denseIndex];
    return {entityIndex, _scene->generations[entityIndex]};
}

bool SceneManager::get_transform(EntityId id, EntityTransform& outTransform) const {
    if (!is_alive(id)) return false;
    const uint32_t denseIndex = _scene->denseIndexByEntity[id.index];
    outTransform.position = _scene->positions[denseIndex];
    outTransform.rotation = _scene->rotations[denseIndex];
    outTransform.scale = _scene->scales[denseIndex];
    return true;
}

void SceneManager::despawn(EntityId id){
    if (!is_alive(id)) return;

    const uint32_t denseIndex = _scene->denseIndexByEntity[id.index];
    const uint32_t lastDenseIndex = _scene->count - 1;

    if (denseIndex != lastDenseIndex) {
        const uint32_t movedEntityIndex = _scene->entityIndexByDense[lastDenseIndex];
        _scene->positions[denseIndex] = _scene->positions[lastDenseIndex];
        _scene->rotations[denseIndex] = _scene->rotations[lastDenseIndex];
        _scene->scales[denseIndex] = _scene->scales[lastDenseIndex];
        _scene->meshIdx[denseIndex] = _scene->meshIdx[lastDenseIndex];
        _scene->matIdx[denseIndex] = _scene->matIdx[lastDenseIndex];
        _scene->entityIndexByDense[denseIndex] = movedEntityIndex;
        _scene->denseIndexByEntity[movedEntityIndex] = denseIndex;
    }

    --_scene->count;
    ++_scene->generations[id.index];
    if (_scene->generations[id.index] == 0) {
        ++_scene->generations[id.index];
    }
    _scene->freeEntityIndices[_scene->freeCount++] = id.index;
}

void SceneManager::update_matrices(float dt){
    glm::quat spin = glm::angleAxis(1.5f * dt, glm::vec3(0.0f,1.0f,0.0f));
    for(uint32_t i = 0; i < _scene->count; ++i){
        _scene->rotations[i] = spin * _scene->rotations[i];
    }
}

void SceneManager::render(VkCommandBuffer cmd, VkPipelineLayout layout){
    auto& scene = *_scene;

    for(uint32_t i = 0; i < scene.count; ++i){
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::scale(model, scene.scales[i]);
        model = model * glm::mat4_cast(scene.rotations[i]);
        model = glm::translate(model, scene.positions[i]);

        vkCmdPushConstants(cmd,
                           layout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(glm::mat4),&model);

        const Mesh& mesh = scene.meshes[scene.meshIdx[i]];
        VkDeviceSize zero = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer, &zero);
        vkCmdBindIndexBuffer(cmd, mesh.indexBuffer,0, mesh.indexType);

        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0,0);
    }
}
