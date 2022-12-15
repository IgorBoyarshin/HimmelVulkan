#version 450

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform GeneralUbo {
    mat4 view;
    mat4 proj;
    mat4 globalLightView;
    mat4 globalLightProj;
    vec4 globalLightDir_ambientStrength;
    vec4 fogColor_density;
    vec3 cameraPos;
    float dayNightCycleT;
} uboGeneral;

// XXX sync with Fragment shader
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
    int textureIndex;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec2 outFragTexCoord;
layout(location = 1) out vec3 outPosition;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec3 outLightSpacePosition;

void main() {
    vec4 posWorld = push.model * vec4(inPosition, 1.0);
    gl_Position = uboGeneral.proj * uboGeneral.view * posWorld;
    outFragTexCoord = inTexCoord;
    outNormal = normalize((push.model * vec4(inNormal, 0.0)).xyz);
    outPosition = posWorld.xyz;
    vec4 posInLightSpace = uboGeneral.globalLightProj * uboGeneral.globalLightView * posWorld;
    outLightSpacePosition = posInLightSpace.xyz / posInLightSpace.w;
}
