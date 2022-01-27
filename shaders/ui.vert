#version 450

// XXX sync with Fragment shader
layout(push_constant) uniform PushConstants {
    int textureIndex;
    float shift;
} push;

layout(location = 0) out vec2 outTexCoord;

const vec2 positionFor[6] = vec2[](
    vec2(-0.0, -0.0),
    vec2(-0.0, +0.5),
    vec2(+0.5, +0.5),
    vec2(-0.0, -0.0),
    vec2(+0.5, +0.5),
    vec2(+0.5, -0.0)
);
const vec2 texCoordFor[6] = vec2[](
    vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0),
    vec2(0.0, 0.0),
    vec2(1.0, 1.0),
    vec2(1.0, 0.0)
);

void main() {
    outTexCoord = texCoordFor[gl_VertexIndex];
    gl_Position = vec4(vec2(push.shift) + positionFor[gl_VertexIndex], 0.0, 1.0);
}
