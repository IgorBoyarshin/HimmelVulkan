#version 450

#define MAX_TEXTURES_COUNT 32
layout(set = 1, binding = 0) uniform sampler2D texSamplers[MAX_TEXTURES_COUNT];

// XXX sync with Vertex shader
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
    int textureIndex;
} push;

layout(location = 0) in float inFragIllumIntensity;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in float inVisibility;
layout(location = 3) in vec4 inFogColor; // NOTE pass through UBO in future

layout(location = 0) out vec4 outColor;

void main() {
    if (0 <= push.textureIndex) {
        vec4 c = texture(texSamplers[push.textureIndex], fragTexCoord);
        outColor = vec4(c.rgb * inFragIllumIntensity, c.a);
    } else {
        outColor = vec4(push.color.rgb * inFragIllumIntensity, 1.0);
    }
    outColor = mix(inFogColor, outColor, inVisibility);
    // if (outColor.a < 0.1) discard;
}
