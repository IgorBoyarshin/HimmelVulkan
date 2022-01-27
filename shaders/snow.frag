#version 450

#define TEXTURES_COUNT 2
layout(set = 1, binding = 0) uniform sampler2D texSamplers[TEXTURES_COUNT];

layout(location = 0)      in vec2  fragTexCoord;
layout(location = 1) flat in int   textureIndex;
layout(location = 2)      in float inAmbient;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outColor1; // NOTE temporary

void main() {
    outColor = texture(texSamplers[textureIndex], fragTexCoord);
    if (outColor.a < 0.1) discard;
    if (outColor.r < 0.1) discard;
    outColor.rgb *= inAmbient;
    outColor1 = outColor; // NOTE temporary
}
