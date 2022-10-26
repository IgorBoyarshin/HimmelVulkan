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

#define MAX_TEXTURES_COUNT 32
layout(set = 1, binding = 0) uniform sampler2D texSamplers[MAX_TEXTURES_COUNT];

// XXX sync with Vertex shader
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
    int textureIndex;
} push;

/* layout(location = 0) in float inFragIllumIntensity; */
layout(location = 0) in vec2  inFragTexCoord;
layout(location = 1) in float inVisibility;
layout(location = 2) in vec3  inFragColor;

layout(location = 0) out vec4 outColor;

void main() {
    if (0 <= push.textureIndex) {
        vec4 c = texture(texSamplers[push.textureIndex], inFragTexCoord);
        outColor = vec4(c.rgb, c.a);
    } else {
        outColor = vec4(push.color.rgb, 1.0);
    }
    outColor *= vec4(inFragColor, 1.0);
    vec4 fogColor = vec4(uboGeneral.fogColor_density.rgb, 1.0);
    outColor = mix(fogColor, outColor, inVisibility);
}
