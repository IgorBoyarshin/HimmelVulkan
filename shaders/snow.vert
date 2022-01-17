#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(std140, set = 2, binding = 0) readonly buffer SnowInstancesUniformBuffers {
    vec4 positions[]; // vec3 pos + float rotation
} snowData;

layout(location = 0)      out vec2 outTexCoord;
layout(location = 1) flat out int  outTextureIndex;



mat4 model(vec3 pos, float rotation) {
    /* mat4 res = mat4(1.0); */
    /* res[3] = vec4(pos, 1.0 + rotation); */
    const float scale = 0.04;
    /* const float scale = 1.0; */
    /* res[0][0] = scale; */
    /* res[1][1] = scale; */
    /* res[2][2] = scale; */
    /* return res; */
    float c = cos(rotation);
    float s = sin(rotation);
    return mat4(
        c, s, 0, 0,
        -s, c, 0, 0,
        0, 0, 1, 0,
        pos, 1
    ) * mat4(
        scale, 0, 0, 0,
        0, scale, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    );
    /* return mat4( */
    /*     scale, 0.0, 0.0, 0.0, */
    /*     0.0, scale * c, s, 0.0, */
    /*     0.0, -s, scale * c, 0.0, */
    /*     pos, 1.0 */
    /* ); */
}


const vec2 positionFor[6] = vec2[](
    vec2(-0.5, -0.5),
    vec2(+0.5, -0.5),
    vec2(+0.5, +0.5),
    vec2(+0.5, +0.5),
    vec2(-0.5, +0.5),
    vec2(-0.5, -0.5)
);
const vec2 texCoordFor[6] = vec2[](
    vec2(1.0, 0.0),
    vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0),
    vec2(1.0, 0.0)
);


#define TEXTURES_COUNT 2
void main() {
    vec3 position  = snowData.positions[gl_InstanceIndex].xyz;
    float rotation = snowData.positions[gl_InstanceIndex].w;
    outTextureIndex = gl_InstanceIndex % TEXTURES_COUNT;
    outTexCoord = texCoordFor[gl_VertexIndex];
    gl_Position = ubo.proj * ubo.view * model(position, rotation) * vec4(positionFor[gl_VertexIndex], 0.0, 1.0);
}
