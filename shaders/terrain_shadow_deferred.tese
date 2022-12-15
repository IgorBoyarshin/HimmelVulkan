#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(quads, equal_spacing, cw) in;

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform GeneralUbo {
    mat4 view;
    mat4 proj;
    mat4 globalLightView;
    mat4 globalLightProj;
    vec4 globalLightDir_ambientStrength;
    vec4 fogColor_density;
    vec3 cameraPos;
    float dayNightCycleT;
} uboGeneral;

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

    gl_Position = uboGeneral.globalLightProj * uboGeneral.globalLightView * v0;
}
