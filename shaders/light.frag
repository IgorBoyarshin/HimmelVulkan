#version 450

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform GeneralUbo {
    mat4 view;
    mat4 proj;
    mat4 globalLightView;
    mat4 globalLightProj;
    vec3 globalLightDir;
    float ambientStrength;
    vec3 fogColor;
    float fogDensity;
    vec3 cameraPos;
    float dayNightCycleT;
} uboGeneral;

layout(location = 0) in vec2  inCoord;
layout(location = 1) in vec4  inColor;
layout(location = 2) in float inIntensity;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outBloom;

void main() {
    float d = distance(inCoord, vec2(0.0, 0.0));
    float strength = 1.0 - clamp(d, 0.0, 1.0);
    outColor = vec4(inColor.rgb, inColor.a * smoothstep(0.0, 0.4, strength));
    outBloom = vec4(vec3(outColor * inIntensity), outColor.a);
    // outBloom = vec4(vec3(inIntensity), outColor.a);
    /* outBloom = vec4(inIntensity * outColor.rgb, outColor.a); */
    if (strength < 0.2) discard;
}
