#version 450

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform GeneralUbo {
    mat4 view;
    mat4 proj;
    mat4 globalLightView;
    mat4 globalLightProj;
    vec4 globalLightDir_ambientStrength;
    vec4 fogColor_density;
    vec3 cameraPos;
} uboGeneral;

// XXX sync with Fragment shader
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
    int textureIndex;
} push;

layout(location = 0) in vec3 inPosition;
/* layout(location = 1) in vec2 inTexCoord; */
/* layout(location = 2) in vec3 inNormal; */

void main() {
    vec4 posWorld = push.model * vec4(inPosition, 1.0);
    gl_Position = uboGeneral.globalLightProj * uboGeneral.globalLightView * posWorld;
}
