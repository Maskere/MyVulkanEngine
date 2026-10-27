#version 450
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

layout(binding = 0) uniform SceneUniforms {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
} U;

layout(push_constant) uniform PushConstant {
    mat4 model;
} PC;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec3 vWorldPos;

void main() {
    vWorldPos = (PC.model * vec4(aPos, 1.0)).xyz;
    vNormal   = mat3(PC.model) * aNormal;
    gl_Position = U.proj * U.view * vec4(vWorldPos, 1.0);
}
