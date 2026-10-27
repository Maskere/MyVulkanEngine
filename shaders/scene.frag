#version 450
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vWorldPos;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform SceneUniforms {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
} U;

void main(){
    vec3 N = normalize(vNormal);
    vec3 V = normalize(U.cameraPos - vWorldPos);
    vec3 L = normalize(vec3(1.0,2.0,1.5));
    vec3 H = normalize(L + V);

    vec3 base = vec3(0.75, 0.35, 0.25);
    float diff = max(dot(N,L), 0.0);
    float spec = pow(max(dot(N,H), 0.0), 48.0);

    outColor = vec4(base * (0.15 + 0.85 * diff) + 0.5 * spec, 1.0);
}
