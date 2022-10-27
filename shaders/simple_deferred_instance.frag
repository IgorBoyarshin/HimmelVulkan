#version 450

#define MAX_TEXTURES_COUNT 32
layout(set = 2, binding = 1) uniform sampler2D texSamplers[MAX_TEXTURES_COUNT];

// XXX sync with Vertex shader
layout(push_constant) uniform PushConstants {
    int textureIndex;
    float time;
} push;

layout(location = 0) in vec2 inFragTexCoord;
layout(location = 1) in vec3 inPosition;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inLightSpacePosition;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gColor;
layout(location = 3) out vec4 gLightSpacePosition;

void main() {
    if (0 <= push.textureIndex) {
        gColor = texture(texSamplers[push.textureIndex], inFragTexCoord);
    } else {
        gColor = vec4(1.0, 0.0, 0.0, 1.0);
    }
    gPosition = vec4(inPosition, 1.0);
    gNormal = vec4(inNormal, 1.0);
    gLightSpacePosition = vec4(inLightSpacePosition, 1.0);
}
