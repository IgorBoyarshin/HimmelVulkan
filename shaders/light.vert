#version 450

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec3 globalLightDir;
    float ambientStrength;
    vec3 fogColor;
    float fogDensity;
    vec3 cameraPos;
} uboGeneral;
const float fogGradient = 2.0;

struct PointLight {
    vec3 color;
    float intensity;
    vec3 position;
    float radius;
};

// XXX Sync across all shaders
const uint MAX_POINT_LIGHTS = 32;
layout(set = 0, binding = 1) uniform LightsUbo {
    PointLight pointLights[MAX_POINT_LIGHTS];
    uint count;
} uboLights;

layout(location = 0) out vec2  outCoord;
layout(location = 1) out vec4  outColor;
layout(location = 2) out float outIntensity;

#define UNIT 1.0
const vec2 positionFor[6] = vec2[](
    vec2(-UNIT, -UNIT),
    vec2(-UNIT, +UNIT),
    vec2(+UNIT, +UNIT),
    vec2(-UNIT, -UNIT),
    vec2(+UNIT, +UNIT),
    vec2(+UNIT, -UNIT)
);

void main() {
    PointLight light = uboLights.pointLights[gl_InstanceIndex];
    vec4 centerViewPos = uboGeneral.view * vec4(light.position, 1.0);
    vec4 viewPos = centerViewPos + vec4(light.radius * positionFor[gl_VertexIndex], 0.0, 0.0);
    gl_Position = uboGeneral.proj * viewPos;
    outCoord = positionFor[gl_VertexIndex];
    outColor.rgb = light.color;
    outIntensity = light.intensity;

    // Apply simple fog
    if (uboGeneral.fogDensity > 0) {
        // To make the light for fog-resistant
        const float densityMult = 0.7;
        const float gradientMult = 0.5;
        float distToCamera = length(uboGeneral.cameraPos - light.position);
        float visibility = clamp(exp(-pow((densityMult * distToCamera * uboGeneral.fogDensity), gradientMult * fogGradient)), 0.0, 1.0);
        outColor = vec4(mix(uboGeneral.fogColor, outColor.rgb, visibility), visibility);
    }
}
