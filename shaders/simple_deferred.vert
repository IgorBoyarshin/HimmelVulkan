#version 450

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform GeneralUbo {
    mat4 view;
    mat4 proj;
    vec4 globalLightDir_ambientStrength;
    vec4 fogColor_density;
    vec3 cameraPos;
} uboGeneral;
const float fogGradient = 1.8;

struct PointLight {
    vec3 color;
    float intensity;
    vec3 position;
    float reserved;
};

// XXX Sync across all shaders
const uint MAX_POINT_LIGHTS = 32;
layout(set = 0, binding = 1) uniform LightsUbo {
    PointLight pointLights[MAX_POINT_LIGHTS];
    uint count;
} uboLights;

// XXX sync with Fragment shader
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
    int textureIndex;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

/* layout(location = 0) out float outFragIllumIntensity; */
layout(location = 0) out vec2  outFragTexCoord;
layout(location = 1) out float outVisibility;
layout(location = 2) out vec3  outFragColor;
layout(location = 3) out vec3  outPosition;
layout(location = 4) out vec3  outNormal;

void main() {
    vec4 cameraPosition = uboGeneral.view * push.model * vec4(inPosition, 1.0);
    gl_Position = uboGeneral.proj * cameraPosition;
    vec3 normal = normalize((push.model * vec4(inNormal, 0.0)).xyz);

    float ambientStrength = uboGeneral.globalLightDir_ambientStrength.w;
    vec3 globalLightDir = uboGeneral.globalLightDir_ambientStrength.xyz;
    vec3 total = ambientStrength * vec3(1.0, 1.0, 1.0); // sun ??
    const float DIRECT_INTENSITY = 0.5;
    float globalDiffuseStrength = max(dot(normal, -globalLightDir), 0.0);
    total += DIRECT_INTENSITY * globalDiffuseStrength * vec3(1.0, 1.0, 1.0); // sun ??
    for (uint i = 0; i < uboLights.count; i++) {
        float intensity = uboLights.pointLights[i].intensity;
        vec3 position   = uboLights.pointLights[i].position;
        vec3 color      = uboLights.pointLights[i].color;
        vec3 lightDir = position - inPosition;
        float attenuation = 1.0 / dot(lightDir, lightDir);
        float diffuseStrength = max(dot(normal, normalize(lightDir)), 0.0);
        intensity *= attenuation;
        intensity *= diffuseStrength;
        total += intensity * color;
    }
    outFragColor = total;

    outFragTexCoord = inTexCoord;

    float fogDensity = uboGeneral.fogColor_density.w;
    if (fogDensity > 0) {
        float dist = length(cameraPosition.xyz);
        outVisibility = clamp(exp(-pow((dist * fogDensity), fogGradient)), 0.0, 1.0);
    } else {
        outVisibility = 1.0;
    }

    /* outNormal = (uboGeneral.proj * uboGeneral.view * vec4(normal, 0.0)).xyz; */
    outNormal = normal;
    outPosition = (push.model * vec4(inPosition, 1.0)).xyz;
}
