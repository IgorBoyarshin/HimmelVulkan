#version 460

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform GeneralUbo {
    mat4 view;
    mat4 proj;
    vec4 globalLightDir_ambientStrength;
    vec4 fogColor_density;
    vec3 cameraPos;
} uboGeneral;

// XXX sync with Fragment shader
layout(push_constant) uniform PushConstants {
    /* mat4 model; */
    /* vec4 color; */
    int textureIndex;
    float time;
} push;

layout(std430, set = 2, binding = 0) readonly buffer Instances {
    mat4 data[];
} instances;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec2 outFragTexCoord;
layout(location = 1) out vec3 outPosition;
layout(location = 2) out vec3 outNormal;

void main() {
    mat4 model = instances.data[gl_BaseInstance + gl_InstanceIndex];
    vec4 posWorld = model * vec4(inPosition, 1.0);
    gl_Position = uboGeneral.proj * uboGeneral.view * posWorld;
    outFragTexCoord = inTexCoord;
    outNormal = normalize((model * vec4(inNormal, 0.0)).xyz);
    outPosition = posWorld.xyz;
}