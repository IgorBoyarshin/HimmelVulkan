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
    float offsetY;
    float maxHeight;
    int level;
} push;

layout(set = 1, binding = 0) uniform sampler2D heightmap;


layout(location = 0) in vec2 inTexCoord[];

layout(location = 0) out vec2  outTexCoord[];
layout(location = 1) out float outSegments[];


float distToPower(float dist, float edge) {
    const float RATIO = 70.0; // proportional to LOD
    return ceil(RATIO * inversesqrt(dist));
}


float tessFor(vec3 p1, vec3 p2) {
    float dist = max(distance(ubo.cameraPos, p1), distance(ubo.cameraPos, p2));
    const float MAX_P = 6;
    /* float p = distToPower(dist, distance(p1, p2)) - push.level; */
    float p = distToPower(dist, distance(p1, p2));
    p = clamp(p, 0, MAX_P);
    return pow(2, p);
}


void main() {
    if (gl_InvocationID == 0) {
        vec3 p0 = gl_in[0].gl_Position.xyz;
        vec3 p1 = gl_in[1].gl_Position.xyz;
        vec3 p2 = gl_in[2].gl_Position.xyz;
        vec3 p3 = gl_in[3].gl_Position.xyz;
        gl_TessLevelOuter[1] = tessFor(p0, p1);
        gl_TessLevelOuter[0] = tessFor(p0, p3);
        gl_TessLevelOuter[3] = tessFor(p3, p2);
        gl_TessLevelOuter[2] = tessFor(p1, p2);

        gl_TessLevelInner[0] = 0.5 * (gl_TessLevelOuter[0] + gl_TessLevelOuter[3]);
        gl_TessLevelInner[1] = 0.5 * (gl_TessLevelOuter[2] + gl_TessLevelOuter[1]);
        outSegments[gl_InvocationID] = 0.5 * (gl_TessLevelInner[0] + gl_TessLevelInner[1]);
    }

    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    outTexCoord[gl_InvocationID] = inTexCoord[gl_InvocationID];
}
