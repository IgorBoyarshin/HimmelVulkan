#version 450

// XXX Sync with HmlMaterial::PlaceCount
#define MAX_TEXTURES_COUNT 5
layout(set = 1, binding = 0) uniform sampler2D texSamplers[MAX_TEXTURES_COUNT];

// XXX sync with Vertex shader
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

layout(location = 0) in vec2 inFragTexCoord;
layout(location = 1) in vec3 inPosition;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inLightSpacePosition;
layout(location = 4) in mat3 inTBN;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gColor;
layout(location = 3) out vec4 gLightSpacePosition;
layout(location = 4) out uint gId;
layout(location = 5) out vec4 gEmissive;

void main() {
    if (push.baseColorTextureIndex >= 0) {
        gColor = texture(texSamplers[push.baseColorTextureIndex], inFragTexCoord);
    } else {
        gColor = vec4(push.color.rgb, 1.0);
    }
    gPosition = vec4(inPosition, 1.0);
    vec3 normal = texture(texSamplers[push.normalTextureIndex], inFragTexCoord).rgb;
    normal = normal * 2.0 - 1.0;
    gNormal = vec4(normalize(inTBN * normal), 1.0);
    // gNormal = vec4(inNormal, 1.0);
    gLightSpacePosition = vec4(inLightSpacePosition, 1.0);
    gId = push.id;
    if (push.emissiveTextureIndex >= 0) {
        gEmissive = texture(texSamplers[push.emissiveTextureIndex], inFragTexCoord);
    } else {
        gEmissive = vec4(0);
    }
}
