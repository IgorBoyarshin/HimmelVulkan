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
} uboGeneral;
const float fogGradient = 2.0;

struct PointLight {
    vec3 color;
    float intensity;
    vec3 position;
    float radius;
};

// XXX Sync across all shaders
const uint MAX_POINT_LIGHTS = 64; // XXX sync with Himmel.h::LightUbo
layout(set = 0, binding = 1) uniform LightsUbo {
    PointLight pointLights[MAX_POINT_LIGHTS];
    uint count;
} uboLights;

#define G_POSITION              0
#define G_NORMAL                1
#define G_COLOR                 2
#define G_LIGHT_SPACE_POSITIONS 3
#define G_SHADOWMAP             4
#define G_COUNT                 5
layout(set = 1, binding = 0) uniform sampler2D texSamplers[G_COUNT];

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

float distToLine(vec3 v0, vec3 v1, vec3 p) {
    vec3 I = normalize(v1 - v0);
    vec3 P = p - v0;
    return sqrt(dot(P, P) - dot(P, I));
}

void main() {
    vec3 pos       = texture(texSamplers[G_POSITION], inTexCoord).rgb;
    vec3 lightSpacePos = texture(texSamplers[G_LIGHT_SPACE_POSITIONS], inTexCoord).rgb;
    vec3 normal    = texture(texSamplers[G_NORMAL], inTexCoord).rgb;
    vec3 albedo    = texture(texSamplers[G_COLOR], inTexCoord).rgb;
    float specular = texture(texSamplers[G_COLOR], inTexCoord).a;
    float distToCamera = length(uboGeneral.cameraPos - pos);

    const vec3 GLOBAL_COLOR = vec3(1.0, 1.0, 1.0);

    // Global ambient
    float ambientStrength = uboGeneral.ambientStrength;
    vec3 light = ambientStrength * albedo * GLOBAL_COLOR;

    // Global diffuse
    /* const float DIRECT_INTENSITY = 0.0; */
    /* vec3 globalLightDir = -uboGeneral.globalLightDir; */
    /* float diffuseStrength = max(dot(normal, globalLightDir), 0.0); */
    /* light += DIRECT_INTENSITY * diffuseStrength * albedo * GLOBAL_COLOR; */

    // Diffuse from point lights; fog color
    vec3 fogColor = vec3(0.0);
    for(int i = 0; i < uboLights.count; i++) {
        float intensity = uboLights.pointLights[i].intensity;
        vec3 position   = uboLights.pointLights[i].position;
        vec3 color      = uboLights.pointLights[i].color;

        // Diffuse
        vec3 lightDir = position - pos;
        float dist2 = dot(lightDir, lightDir);
        float strength = max(dot(normal, normalize(lightDir)), 0.0);
        float attenuation = strength * intensity / dist2;
        light += attenuation * albedo * color;

        // Fog
        /* float h = distToLine(uboGeneral.cameraPos, pos, position); */
        /* fogColor += sqrt(intensity) * color * atan(distToCamera / h) / h; */
    }

    // Global diffuse from shadowmap
    float shadowFactor = 0.0;
    vec2 shadowmapSize = textureSize(texSamplers[G_SHADOWMAP], 0);
    vec2 shadowmapStep = 1.0 / shadowmapSize;
    vec2 shadowmapCoord = lightSpacePos.xy * 0.5 + 0.5;
    const int PCF = 1; // [0..]
    const float MIN_BIAS = 0.0005;
    const float MAX_BIAS = 0.003;
    float bias = max(MAX_BIAS * (1.0 - dot(normal, -uboGeneral.globalLightDir)), MIN_BIAS);
    for (int y = -PCF; y <= PCF; y++) {
        for (int x = -PCF; x <= PCF; x++) {
            float shadowDepth = texture(texSamplers[G_SHADOWMAP], shadowmapCoord + vec2(x, y) * shadowmapStep).r;
            shadowFactor += (lightSpacePos.z - bias > shadowDepth) ? 0.0 : 1.0; // reversed value to simplify clamp logic understanding
        }
    }
    shadowFactor /= (PCF * 2 + 1) * (PCF * 2 + 1);
    shadowFactor = clamp(shadowFactor, 0.3, 1.0);
    light *= shadowFactor;

    // Apply simple fog
    if (uboGeneral.fogDensity > 0) {
        float visibility = clamp(exp(-pow((distToCamera * uboGeneral.fogDensity), fogGradient)), 0.0, 1.0);
        light = mix(uboGeneral.fogColor, light, visibility);
    }

    // Tone mapping and gamma correcton
    /* const float exposure = 0.2; */
    /* const float gamma = 1.2; */
    /* vec3 mapped = vec3(1.0) - exp(-light * exposure); */
    /* light = pow(mapped, vec3(1.0 / gamma)); */

    const float VERY_FAR = 1000.0;
    if (distToCamera > VERY_FAR) light = uboGeneral.fogColor;

    outColor = vec4(light, 1.0);
}
