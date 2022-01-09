#version 450
#extension GL_ARB_separate_shader_objects : enable

#define MAX_TEXTURES_COUNT 32
layout(set = 1, binding = 0) uniform sampler2D texSamplers[MAX_TEXTURES_COUNT];

layout(push_constant) uniform PushConstants {
    mat4 model;
    int textureIndex;
} push;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    if (0 <= push.textureIndex) {
        outColor = vec4(texture(texSamplers[push.textureIndex], fragTexCoord).rgb, 1.0);
    } else {
        outColor = vec4(fragColor, 1.0);
    }
}
