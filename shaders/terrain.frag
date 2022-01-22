#version 450

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 globalLightDir_ambientStrength;
    vec4 fogColor_density;
    vec3 cameraPos;
} ubo;

layout(set = 1, binding = 0) uniform sampler2D heightmap;
layout(set = 1, binding = 1) uniform sampler2D grass;

layout(location = 0) in vec2  inTexCoord;
layout(location = 1) in float inFragIllumIntensity;
layout(location = 2) in float inVisibility;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 heightColor = texture(heightmap, inTexCoord);
    float height = heightColor.r + 0.4;
    if (height > 1.0) height = 1.0;
    outColor = mix(texture(grass, inTexCoord), heightColor, height); // grass

    outColor.rgb *= inFragIllumIntensity;

    vec4 fogColor = vec4(ubo.fogColor_density.rgb, 1.0);
    outColor = mix(fogColor, outColor, inVisibility);

    /* outColor = vec4(1.0, 0.0, 0.0, 1.0); */
}
