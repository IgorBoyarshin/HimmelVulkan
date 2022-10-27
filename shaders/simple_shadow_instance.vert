#version 460

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
/* layout(push_constant) uniform PushConstants { */
/*     int textureIndex; */
/*     float time; */
/* } push; */

layout(std430, set = 2, binding = 0) readonly buffer Instances {
    mat4 data[];
} instances;

layout(location = 0) in vec3 inPosition;
/* layout(location = 1) in vec2 inTexCoord; */
/* layout(location = 2) in vec3 inNormal; */

void main() {
    // XXX It appears that gl_BaseInstance is automatically added to
    // gl_InstanceIndex, so gl_InstanceIndex does not restart at VkCmdDraw.
    /* mat4 model = instances.data[gl_BaseInstance + gl_InstanceIndex]; */
    mat4 model = instances.data[gl_InstanceIndex];
    vec4 posWorld = model * vec4(inPosition, 1.0);
    gl_Position = uboGeneral.globalLightProj * uboGeneral.globalLightView * posWorld;
}
