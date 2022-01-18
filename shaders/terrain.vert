#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

// XXX sync with Fragment shader
layout(push_constant) uniform PushConstants {
    vec4 posStartFinish;
    uvec4 dimAndOffset;
} push;

layout(set = 1, binding = 0) uniform sampler2D heightmap;

layout(location = 0) out vec2 outTexCoord;

const vec2 posOffsetFor[6] = vec2[](
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    vec2(0, 0),
    vec2(1, 1),
    vec2(1, 0)
);

void main() {
    vec2 start = push.posStartFinish.xy;
    vec2 finish = push.posStartFinish.zw;
    vec2 mapSize = finish - start;

    vec2 mapIndices = posOffsetFor[gl_VertexIndex] + vec2(
        float(gl_InstanceIndex % (push.dimAndOffset.x - 1)),
        float(gl_InstanceIndex / (push.dimAndOffset.z - 1)));
    vec2 mapCoord = mapIndices / vec2(push.dimAndOffset.x - 1, push.dimAndOffset.z - 1);

    float offsetY = float(push.dimAndOffset.w);
    float maxHeight = float(push.dimAndOffset.y);
    float y = offsetY + maxHeight * texture(heightmap, mapCoord).r;
    vec2 pos = mapCoord * mapSize + start;

    gl_Position = ubo.proj * ubo.view * vec4(pos.x, y, pos.y, 1.0);
    outTexCoord = mapCoord;
}
