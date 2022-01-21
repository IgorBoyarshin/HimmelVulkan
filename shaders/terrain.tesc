#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(vertices = 4) out;

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 globalLightDir_ambientStrength;
    vec4 fogColor_density;
    vec3 cameraPos;
} ubo;

// XXX Sync with other shaders
// Per patch
layout(push_constant) uniform PushConstants {
    vec2 center;
    vec2 size;
    vec2 texCoordStart;
    vec2 texCoordStep;
    vec4 tessLevelLeftDownRightUp;
    float offsetY;
    float maxHeight;
} push;

layout(set = 1, binding = 0) uniform sampler2D heightmap;


layout(location = 0) in vec2 inTexCoord[];

layout(location = 0) out vec2  outTexCoord[];
layout(location = 1) out float outSegments[];


void main() {
    if (gl_InvocationID == 0) {
        gl_TessLevelOuter[0] = push.tessLevelLeftDownRightUp.y; // down
        gl_TessLevelOuter[1] = push.tessLevelLeftDownRightUp.x; // left
        gl_TessLevelOuter[2] = push.tessLevelLeftDownRightUp.w; // up
        gl_TessLevelOuter[3] = push.tessLevelLeftDownRightUp.z; // right

        gl_TessLevelInner[0] = 0.5 * (gl_TessLevelOuter[0] + gl_TessLevelOuter[3]);
        gl_TessLevelInner[1] = 0.5 * (gl_TessLevelOuter[2] + gl_TessLevelOuter[1]);
        outSegments[gl_InvocationID] = 0.5 * (gl_TessLevelInner[0] + gl_TessLevelInner[1]);
    }

    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    outTexCoord    [gl_InvocationID] = inTexCoord[gl_InvocationID];
}
