#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(quads, equal_spacing, cw) in;

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 globalLightDir_ambientStrength;
    vec4 fogColor_density;
    vec3 cameraPos;
} ubo;
const float fogGradient = 2.0;

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

layout(location = 0) in vec2  inTexCoord[];
layout(location = 1) in float inSegments[];

layout(location = 0) out vec3 outNormal;

void main() {
    vec2 t = mix(
        mix(inTexCoord[0], inTexCoord[1], gl_TessCoord.x),
        mix(inTexCoord[3], inTexCoord[2], gl_TessCoord.x),
        gl_TessCoord.y
    );
    vec4 v0 = mix(
        mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x),
        mix(gl_in[3].gl_Position, gl_in[2].gl_Position, gl_TessCoord.x),
        gl_TessCoord.y
    );
    v0.y = push.offsetY + push.maxHeight * texture(heightmap, t).r;

    // The other texCoords are generally the same, so 2 suffice
    float unit = length(inTexCoord[0] - inTexCoord[1]) / inSegments[0] / 2.0; // NOTE last division is imperical
    vec2 d = vec2(unit, 0.0); // just need the numbers, the vector has no real meaning
    float hL = texture(heightmap, t - d.xy).r;
    float hR = texture(heightmap, t + d.xy).r;
    float hD = texture(heightmap, t - d.yx).r;
    float hU = texture(heightmap, t + d.yx).r;
    vec3 normal = normalize(vec3(hL - hR, 16.0 * unit, hD - hU));

    gl_Position = v0;
    outNormal = normal;
}
