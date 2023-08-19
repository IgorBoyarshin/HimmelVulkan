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
#define G_MATERIAL              5
#define G_COUNT                 6
layout(set = 1, binding = 0) uniform sampler2D texSamplers[G_COUNT];

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inSphereCoord;

layout(location = 0) out vec4 outColor;

float distToLine(vec3 v0, vec3 v1, vec3 p) {
    vec3 I = normalize(v1 - v0);
    vec3 P = p - v0;
    return sqrt(dot(P, P) - dot(P, I));
}

vec3 sunColor() {
    float t = uboGeneral.dayNightCycleT;
    const vec3 sunColorDay = vec3(1.0, 0.8, 0.2);
    const vec3 sunColorRise = vec3(1.0, 0.3, 0.0);
    /* float antiMiddleT = 2.0 * abs(0.5 - t); // night and day are 1, sunrise and sunset are 0 */
    float antiMiddleT = 2 * t - 1; // day is 1, sunrise and sunset are 0, night is -1
    return mix(sunColorRise, sunColorDay, smoothstep(0.0, 0.3, antiMiddleT));
}

vec3 atmosphere() {
    float t = uboGeneral.dayNightCycleT;
    vec3 sunPos = -uboGeneral.globalLightDir;
    vec3 r = normalize(inSphereCoord);

    const vec3 dayColor = vec3(0.5, 0.8, 1.0);
    const vec3 nightColor = vec3(0.0, 0.0, 0.0);
    vec3 skyColor = mix(nightColor, dayColor, smoothstep(0.35, 0.7, t));
    vec3 color = skyColor;

    // Horizon
    /* if (-0.2 < sunPos.y && sunPos.y < 0.2) { */
    /*     if (r.y < 0.2) { */
    /*         if (dot(sunPos.xz, r.xz) > 0.97) { */
    /*             color = vec3(0.7, 0.2, 0.2); */
    /*         } */
    /*     } */
    /* } */


    /* float antiMiddleT = 2.0 * abs(0.5 - t); // night and day are 1, sunrise and sunset are 0 */
    float antiMiddleT = 2 * t - 1; // day is 1, sunrise and sunset are 0, night is -1
    const float sunOutterDotSize = mix(0.941, 0.992, smoothstep(0.0, 0.25, antiMiddleT));
    const float sunInnerDotSize = 0.998;
    float dotSun = dot(sunPos, r);
    color = mix(color, sunColor(), smoothstep(sunOutterDotSize, sunInnerDotSize, dotSun));
    return color;
}

const float PI = 3.14159265359;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}


void main() {
    vec3 worldPos      = texture(texSamplers[G_POSITION], inTexCoord).rgb;
    vec3 lightSpacePos = texture(texSamplers[G_LIGHT_SPACE_POSITIONS], inTexCoord).rgb;
    vec3 normal    = texture(texSamplers[G_NORMAL], inTexCoord).rgb;
    vec3 albedo    = texture(texSamplers[G_COLOR], inTexCoord).rgb;
    // float specular = texture(texSamplers[G_COLOR], inTexCoord).a;
    float metallic = texture(texSamplers[G_MATERIAL], inTexCoord).r;
    float roughness = texture(texSamplers[G_MATERIAL], inTexCoord).g;
    float ao = texture(texSamplers[G_MATERIAL], inTexCoord).b;
    // const float ao = 1.0;

    // TODO simplify math (reuse; remove sqrt)
    float distToCamera = length(uboGeneral.cameraPos - worldPos);
    const float VERY_FAR = 1000.0;
    if (distToCamera > VERY_FAR) {
        /* outColor = vec4(uboGeneral.fogColor, 1.0); */
        outColor = vec4(atmosphere(), 1.0);
        return;
    }

    albedo.x = pow(albedo.x, 2.2); // convert from sRGB to normalized
    albedo.y = pow(albedo.y, 2.2); // convert from sRGB to normalized
    albedo.z = pow(albedo.z, 2.2); // convert from sRGB to normalized

    vec3 N = normalize(normal); // NOTE most likely redundant (already done elsewhere prior to writing to texture)
    vec3 V = normalize(uboGeneral.cameraPos - worldPos); // view
    float NdotV = max(dot(N, V), 0.0);

    vec3 F0 = vec3(0.04);
    F0      = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    for(int i = 0; i < uboLights.count + 1; i++) {
        bool isDirectionalLight = i == uboLights.count;
        if (isDirectionalLight && -uboGeneral.globalLightDir.y < -0.2f) continue;

        float lightIntensity;
        vec3 lightPosition;
        vec3 lightColor;
        if (isDirectionalLight) {
            lightIntensity = 20.0;
            lightPosition  = uboGeneral.globalLightDir;
            lightColor     = sunColor();
        } else {
            lightIntensity = uboLights.pointLights[i].intensity;
            lightPosition  = uboLights.pointLights[i].position;
            lightColor     = uboLights.pointLights[i].color;
        }

        // Per-light radiance (intensity)
        vec3 L = vec3(0.0); // light
        vec3 radiance = lightColor * lightIntensity;
        if (isDirectionalLight) {
            L = -lightPosition;
        } else {
            L = lightPosition - worldPos;
            float dist2 = dot(L, L);
            L = normalize(L);
            radiance /= dist2;

        }
        float NdotL = max(dot(N, L), 0.0);
        vec3 H = normalize(V + L); // half
        float HdotV = max(dot(H, V), 0.0);

        // Cook-Torrance BRDF
        // D: Normal Distribution Function. We use GGX (by Trowbridge & Reitz).
        // The number of microfacets that reflect light back at the viewer.
        float D = DistributionGGX(N, H, roughness);
        // G: Geometry Function. We use Schlick-GGX (by Schlick & Beckmann).
        // Probability that a microfacets with a given normal vector will be visible to
        // both the light source and the viewer (approximates microfacets self-shadowing).
        float G = GeometrySmith(N, V, L, roughness);
        // F: Fresnel Function. We use Schlick approximation.
        // Simulates the Fresnel reflection.
        vec3 F = fresnelSchlick(HdotV, F0);

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        // DiffuseBRDF
        vec3 diffuse = kD * albedo / PI;

        // SpecularBRDF
        vec3 numerator    = D * G * F;
        float denominator = 4.0 * NdotV * NdotL + 0.0001; // to avoid division by zero
        vec3 specular     = numerator / denominator;

        Lo += (diffuse + specular) * radiance * NdotL;
    }

    // Global diffuse from shadowmap
    float shadowFactor = 0.0;
    if (-uboGeneral.globalLightDir.y > -0.2f) {
        vec2 shadowmapSize = textureSize(texSamplers[G_SHADOWMAP], 0);
        vec2 shadowmapStep = 1.0 / shadowmapSize;
        vec2 shadowmapCoord = lightSpacePos.xy * 0.5 + 0.5;
        const int PCF = 1; // [0..]
        const float MIN_BIAS = 0.0005;
        const float MAX_BIAS = 0.002;
        float bias = max(MAX_BIAS * (1.0 - dot(normal, -uboGeneral.globalLightDir)), MIN_BIAS);
        for (int y = -PCF; y <= PCF; y++) {
            for (int x = -PCF; x <= PCF; x++) {
                float shadowDepth = texture(texSamplers[G_SHADOWMAP], shadowmapCoord + vec2(x, y) * shadowmapStep).r;
                shadowFactor += (lightSpacePos.z - bias > shadowDepth) ? 0.0 : 1.0; // reversed value to simplify clamp logic understanding
                shadowFactor *= dot(normal, -uboGeneral.globalLightDir) > 0 ? 1.0 : 0.0;
            }
        }
        shadowFactor /= (PCF * 2 + 1) * (PCF * 2 + 1);
    }
    shadowFactor = clamp(shadowFactor, 0.3, 1.0);
    Lo *= shadowFactor;

    // vec3 ambient = vec3(uboGeneral.ambientStrength) * albedo * ao;
    vec3 ambient = vec3(0.2) * albedo * ao;
    vec3 color   = ambient + Lo;

    outColor = vec4(color, 1.0);
}

void main2() {
    vec3 pos       = texture(texSamplers[G_POSITION], inTexCoord).rgb;
    vec3 lightSpacePos = texture(texSamplers[G_LIGHT_SPACE_POSITIONS], inTexCoord).rgb;
    vec3 normal    = texture(texSamplers[G_NORMAL], inTexCoord).rgb;
    vec3 albedo    = texture(texSamplers[G_COLOR], inTexCoord).rgb;
    float specular = texture(texSamplers[G_COLOR], inTexCoord).a;

    float distToCamera = length(uboGeneral.cameraPos - pos);
    const float VERY_FAR = 1000.0;
    if (distToCamera > VERY_FAR) {
        /* outColor = vec4(uboGeneral.fogColor, 1.0); */
        outColor = vec4(atmosphere(), 1.0);
        return;
    }

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
    if (-uboGeneral.globalLightDir.y > -0.2f) {
        vec2 shadowmapSize = textureSize(texSamplers[G_SHADOWMAP], 0);
        vec2 shadowmapStep = 1.0 / shadowmapSize;
        vec2 shadowmapCoord = lightSpacePos.xy * 0.5 + 0.5;
        const int PCF = 1; // [0..]
        const float MIN_BIAS = 0.0005;
        const float MAX_BIAS = 0.002;
        float bias = max(MAX_BIAS * (1.0 - dot(normal, -uboGeneral.globalLightDir)), MIN_BIAS);
        for (int y = -PCF; y <= PCF; y++) {
            for (int x = -PCF; x <= PCF; x++) {
                float shadowDepth = texture(texSamplers[G_SHADOWMAP], shadowmapCoord + vec2(x, y) * shadowmapStep).r;
                shadowFactor += (lightSpacePos.z - bias > shadowDepth) ? 0.0 : 1.0; // reversed value to simplify clamp logic understanding
                shadowFactor *= dot(normal, -uboGeneral.globalLightDir) > 0 ? 1.0 : 0.0;
            }
        }
        shadowFactor /= (PCF * 2 + 1) * (PCF * 2 + 1);
    }
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

    outColor = vec4(light, 1.0);
}
