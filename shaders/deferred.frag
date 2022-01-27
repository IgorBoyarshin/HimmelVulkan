#version 450

#define G_POSITION 0
#define G_NORMAL   1
#define G_COLOR    2
#define G_COUNT    3
layout(set = 0, binding = 0) uniform sampler2D texSamplers[G_COUNT];

// XXX sync with Vertex shader
/* layout(push_constant) uniform PushConstants { */
/*     int textureIndex; */
/*     float shift; */
/* } push; */

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(texSamplers[G_COLOR], inTexCoord);
}
