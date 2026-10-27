#pragma once
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include <cstddef>
#include "vk_types.h"
#include "components.h"

constexpr uint32_t MAX_ENTITIES = 4096;
constexpr uint32_t MAX_MESHES = 16;
constexpr uint32_t MAX_MATERIALS = 16;

struct EntityId {
    uint32_t index = MAX_ENTITIES;
    uint32_t generation = 0;

    explicit operator bool() const { return index < MAX_ENTITIES && generation != 0; }
};

struct EntityTransform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

struct Scene {
    // Component data is packed by dense index for cache-friendly iteration.
    alignas(64) glm::vec3 positions[MAX_ENTITIES];
    alignas(64) glm::quat rotations[MAX_ENTITIES];
    alignas(64) glm::vec3 scales[MAX_ENTITIES];
    alignas(64) uint8_t meshIdx[MAX_ENTITIES];
    alignas(64) uint8_t matIdx[MAX_ENTITIES];
    uint32_t entityIndexByDense[MAX_ENTITIES]{};

    // Stable entity slots map to the current dense component-data slot.
    uint32_t denseIndexByEntity[MAX_ENTITIES]{};
    uint32_t generations[MAX_ENTITIES]{};
    uint32_t freeEntityIndices[MAX_ENTITIES]{};
    uint32_t nextEntityIndex = 0;
    uint32_t freeCount = 0;
    uint32_t count = 0;

    Mesh meshes[MAX_MESHES];
    Material materials[MAX_MATERIALS];
    uint32_t meshCount = 0;
    uint32_t matCount = 0;
};

class SceneManager {
public:
    SceneManager() : _scene(nullptr){}
    void init(Scene* s) {_scene = s; }
    explicit SceneManager(Scene* scene) : _scene(scene){}

    uint32_t add_mesh (Mesh mesh);
    uint32_t add_material (Material material);

    EntityId spawn(glm::vec3 pos, glm::quat rot = {1.0f,0.0f,0.0f,0.0f},
                   glm::vec3 scale = {1.0f,1.0f,1.0f},
                   uint32_t mesh = 0, uint32_t material = 0);
    bool is_alive(EntityId id) const;
    EntityId entity_at_dense(uint32_t denseIndex) const;
    bool get_transform(EntityId id, EntityTransform& outTransform) const;
    void despawn(EntityId id);

    void update_matrices(float dt);
    void render(VkCommandBuffer cmd, VkPipelineLayout layout);

    Scene* raw() {return _scene; };
private:
    Scene* _scene;
};
