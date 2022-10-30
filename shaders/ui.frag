#version 450

// XXX sync with Vertex shader
#define MAX_TEXTURES_COUNT 5
layout(push_constant) uniform PushConstants {
    int textureIndex;
    float shift;
} push;

layout(set = 0, binding = 0) uniform sampler2D texSamplers[MAX_TEXTURES_COUNT];

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(texSamplers[push.textureIndex], inTexCoord);
    if (push.textureIndex == 4) {
        outColor = vec4(1.0 - (1.0 - outColor.r) * 1000.0);
    } else if (push.textureIndex == 3) {
        outColor = vec4(1.0 - (1.0 - outColor.z) * 1000.0);
    }
}
