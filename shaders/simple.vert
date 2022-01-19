#version 450
#extension GL_ARB_separate_shader_objects : enable

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 globalLightDir_ambientStrength;
    vec4 fogColor_density;
    vec3 cameraPos;
} ubo;
const float fogGradient = 1.8;

// XXX sync with Fragment shader
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
    int textureIndex;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out float outFragIllumIntensity;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out float outVisibility;
layout(location = 3) out vec4 outFogColor;

void main() {
    vec4 cameraPosition = ubo.view * push.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * cameraPosition;

    float ambientStrength = ubo.globalLightDir_ambientStrength.w;
    vec3 lightDir         = ubo.globalLightDir_ambientStrength.xyz;
    vec4 worldNormal = push.model * vec4(inNormal, 0.0);
    float diffuseStrength = max(dot(normalize(worldNormal.xyz), -lightDir), 0.0);
    outFragIllumIntensity = ambientStrength + diffuseStrength;
    fragTexCoord = inTexCoord;

    outFogColor = vec4(ubo.fogColor_density.rgb, 1.0);
    float fogDensity = ubo.fogColor_density.w;
    if (fogDensity > 0) {
        float dist = length(cameraPosition.xyz);
        outVisibility = clamp(exp(-pow((dist * fogDensity), fogGradient)), 0.0, 1.0);
    } else {
        outVisibility = 1.0;
    }
}
