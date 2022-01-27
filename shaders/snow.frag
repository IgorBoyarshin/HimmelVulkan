#version 450

#define TEXTURES_COUNT 2
layout(set = 1, binding = 0) uniform sampler2D texSamplers[TEXTURES_COUNT];

layout(location = 0)      in vec2  fragTexCoord;
layout(location = 1) flat in int   textureIndex;
layout(location = 2)      in float inAmbient;

layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gColor;

void main() {
    gColor = texture(texSamplers[textureIndex], fragTexCoord);
    if (gColor.a < 0.1) discard;
    if (gColor.r < 0.1) discard;
    gColor.rgb *= inAmbient;
    gPosition = vec4(0.0);
    gNormal = vec4(0.0);
}
