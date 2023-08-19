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
layout(location = 6) out vec4 gMaterial;

void main() {
    if (push.baseColorTextureIndex >= 0) {
        gColor = texture(texSamplers[push.baseColorTextureIndex], inFragTexCoord);
    } else {
        gColor = vec4(push.color.rgb, 1.0);
    }
    gPosition = vec4(inPosition, 1.0);

    vec3 normal = inNormal;
    if (push.normalTextureIndex >= 0) {
        normal = texture(texSamplers[push.normalTextureIndex], inFragTexCoord).rgb;
        normal = normal * 2.0 - 1.0;
        normal = normalize(inTBN * normal);
    }
    gNormal = vec4(normal, 1.0);

    gLightSpacePosition = vec4(inLightSpacePosition, 1.0);
    gId = push.id;

    vec4 emissive = vec4(0.0);
    if (push.emissiveTextureIndex >= 0) {
        emissive = texture(texSamplers[push.emissiveTextureIndex], inFragTexCoord);
    }
    gEmissive = emissive;

    vec2 metallicRoughness = vec2(0.0, 1.0);
    if (push.metallicRoughnessTextureIndex >= 0) {
        metallicRoughness = texture(texSamplers[push.metallicRoughnessTextureIndex], inFragTexCoord).bg;
    }
    float ao = 1.0;
    if (push.occlusionTextureIndex >= 0) {
        ao = texture(texSamplers[push.occlusionTextureIndex], inFragTexCoord).r;
    }
    gMaterial = vec4(metallicRoughness, ao, 0.0);
}
