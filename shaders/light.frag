#version 450

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform GeneralUbo {
    mat4 view;
    mat4 proj;
    vec3 globalLightDir;
    float ambientStrength;
    vec3 fogColor;
    float fogDensity;
    vec3 cameraPos;
} uboGeneral;

layout(location = 0) in vec2  inCoord;
layout(location = 1) in vec3  inColor;
layout(location = 2) in float inIntensity;
layout(location = 0) out vec4 outColor;

void main() {
    const float sqrt2 = sqrt(2.0);
    float d = distance(inCoord, vec2(0.0, 0.0));
    float strength = sqrt2 - d;
    float colorStrength = smoothstep(0.2, 0.9, strength);
    float alphaStrength = smoothstep(0.6, 1.0, strength);
    outColor = vec4(inColor * colorStrength, alphaStrength);
    /* if (alphaStrength < 0.1) discard; */
}
