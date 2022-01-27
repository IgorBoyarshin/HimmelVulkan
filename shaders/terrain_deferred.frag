#version 450

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform GeneralUbo {
    mat4 view;
    mat4 proj;
    vec4 globalLightDir_ambientStrength;
    vec4 fogColor_density;
    vec3 cameraPos;
} uboGeneral;

layout(set = 1, binding = 0) uniform sampler2D heightmap;
layout(set = 1, binding = 1) uniform sampler2D grass;

layout(location = 0) in vec2  inTexCoord;
layout(location = 1) in float inVisibility;
layout(location = 2) in vec3  inFragColor;
layout(location = 3) in vec3  inPosition;
layout(location = 4) in vec3  inNormal;

// TODO vec3
/* layout(location = 0) out vec4 outColor; */
layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gColor;

void main() {
    vec4 heightColor = texture(heightmap, inTexCoord);
    float height = heightColor.r + 0.4;
    if (height > 1.0) height = 1.0;
    gColor = mix(texture(grass, inTexCoord), heightColor, height); // grass

    gColor *= vec4(inFragColor, 1.0);
    vec4 fogColor = vec4(uboGeneral.fogColor_density.rgb, 1.0);
    gColor = mix(fogColor, gColor, inVisibility);
    gPosition = vec4(inPosition, 1.0);
    gNormal = vec4(normalize(inNormal), 1.0);
}
