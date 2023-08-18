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
    int baseColorTextureIndex;
    int metallicRoughnessTextureIndex;
    int normalTextureIndex;
    int occlusionTextureIndex;
    int emissiveTextureIndex;
    int id;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord_0;

layout(location = 0) out vec2 outFragTexCoord;
layout(location = 1) out vec3 outPosition;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec3 outLightSpacePosition;
layout(location = 4) out mat3 outTBN;

void main() {
    vec4 posWorld = push.model * vec4(inPosition, 1.0);
    gl_Position = uboGeneral.proj * uboGeneral.view * posWorld;
    outFragTexCoord = inTexCoord_0;
    outNormal = normalize((push.model * vec4(inNormal, 0.0)).xyz);
    outPosition = posWorld.xyz;
    vec4 posInLightSpace = uboGeneral.globalLightProj * uboGeneral.globalLightView * posWorld;
    outLightSpacePosition = posInLightSpace.xyz / posInLightSpace.w;

    vec3 T = normalize(vec3((push.model * vec4(inTangent.rgb, 0.0))));
    vec3 N = normalize(vec3(push.model * vec4(inNormal, 0.0)));
    vec3 B = cross(N, T);
    outTBN = mat3(T, B, N);
}
