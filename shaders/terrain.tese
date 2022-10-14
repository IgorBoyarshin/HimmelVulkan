/*
 * XXX UNUSED XXX UNUSED XXX UNUSED XXX
 * XXX UNUSED XXX UNUSED XXX UNUSED XXX
 * XXX UNUSED XXX UNUSED XXX UNUSED XXX
 * XXX UNUSED XXX UNUSED XXX UNUSED XXX
 * XXX UNUSED XXX UNUSED XXX UNUSED XXX
 * XXX UNUSED XXX UNUSED XXX UNUSED XXX
 * XXX UNUSED XXX UNUSED XXX UNUSED XXX
 * XXX UNUSED XXX UNUSED XXX UNUSED XXX
 */
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(quads, equal_spacing, cw) in;

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform GeneralUbo {
    mat4 view;
    mat4 proj;
    vec4 globalLightDir_ambientStrength;
    vec4 fogColor_density;
    vec3 cameraPos;
} uboGeneral;
const float fogGradient = 2.0;

struct PointLight {
    vec3 color;
    float intensity;
    vec3 position;
    float reserved;
};

// XXX Sync across all shaders
const uint MAX_POINT_LIGHTS = 64; // XXX sync with Himmel.h::LightUbo
layout(set = 0, binding = 1) uniform LightsUbo {
    PointLight pointLights[MAX_POINT_LIGHTS];
    uint count;
} uboLights;

// XXX Sync with other shaders
// Per patch
layout(push_constant) uniform PushConstants {
    vec2 center;
    vec2 size;
    vec2 texCoordStart;
    vec2 texCoordStep;
    vec4 tessLevelLeftDownRightUp;
    float offsetY;
    float maxHeight;
} push;

layout(set = 1, binding = 0) uniform sampler2D heightmap;

layout(location = 0) in vec2  inTexCoord[];
layout(location = 1) in float inSegments[];

layout(location = 0) out vec2  outTexCoord;
layout(location = 1) out float outVisibility;
layout(location = 2) out vec3  outFragColor;

void main() {
    vec2 t = mix(
        mix(inTexCoord[0], inTexCoord[1], gl_TessCoord.x),
        mix(inTexCoord[3], inTexCoord[2], gl_TessCoord.x),
        gl_TessCoord.y
    );
    vec4 v0 = mix(
        mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x),
        mix(gl_in[3].gl_Position, gl_in[2].gl_Position, gl_TessCoord.x),
        gl_TessCoord.y
    );
    v0.y = push.offsetY + push.maxHeight * texture(heightmap, t).r;

    // The other texCoords are generally the same, so 2 suffice
    float unit = length(inTexCoord[0] - inTexCoord[1]) / inSegments[0] / 2.0; // NOTE last division is imperical
    vec2 d = vec2(unit, 0.0); // just need the numbers, the vector has no real meaning
    float hL = texture(heightmap, t - d.xy).r;
    float hR = texture(heightmap, t + d.xy).r;
    float hD = texture(heightmap, t - d.yx).r;
    float hU = texture(heightmap, t + d.yx).r;
    vec3 normal = normalize(vec3(hL - hR, 16.0 * unit, hD - hU));

    vec4 cameraPosition = uboGeneral.view * v0;
    gl_Position = uboGeneral.proj * cameraPosition;

    float ambientStrength = uboGeneral.globalLightDir_ambientStrength.w;
    vec3 globalLightDir = uboGeneral.globalLightDir_ambientStrength.xyz;
    vec3 total = ambientStrength * vec3(1.0, 1.0, 1.0); // sun ??
    const float DIRECT_INTENSITY = 0.0;
    float globalDiffuseStrength = max(dot(normal, -globalLightDir), 0.0);
    total += DIRECT_INTENSITY * globalDiffuseStrength * vec3(1.0, 1.0, 1.0); // sun ??
    for (uint i = 0; i < uboLights.count; i++) {
        float intensity = uboLights.pointLights[i].intensity;
        vec3 position   = uboLights.pointLights[i].position;
        vec3 color      = uboLights.pointLights[i].color;
        vec3 lightDir = position - v0.xyz;
        float attenuation = 1.0 / dot(lightDir, lightDir);
        float diffuseStrength = max(dot(normal, normalize(lightDir)), 0.0);
        intensity *= attenuation;
        intensity *= diffuseStrength;
        total += intensity * color;
    }
    outFragColor = total;

    outTexCoord = t;
    float fogDensity = uboGeneral.fogColor_density.w;
    if (fogDensity > 0) {
        float dist = length(cameraPosition.xyz);
        outVisibility = clamp(exp(-pow((dist * fogDensity), fogGradient)), 0.0, 1.0);
    } else {
        outVisibility = 1.0;
    }
}
