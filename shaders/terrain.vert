#version 450
#extension GL_ARB_separate_shader_objects : enable

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 globalLightDir_ambientStrength;
    vec4 fogColor_density;
} ubo;
const float fogGradient = 1.8;

// XXX sync with Fragment shader
layout(push_constant) uniform PushConstants {
    vec4 posStartFinish;
    uvec4 dimAndOffset;
} push;

layout(set = 1, binding = 0) uniform sampler2D heightmap;

layout(location = 0) out vec2  outTexCoord;
layout(location = 1) out float outFragIllumIntensity;
layout(location = 2) out float outVisibility;
layout(location = 3) out vec4  outFogColor;

const vec2 posOffsetFor[6] = vec2[](
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1),
    vec2(0, 0),
    vec2(1, 1),
    vec2(1, 0)
);


vec3 positionFor(vec2 mapCoord) {
    vec2 start = push.posStartFinish.xy;
    vec2 finish = push.posStartFinish.zw;
    vec2 mapSize = finish - start;

    float offsetY   = float(push.dimAndOffset.w);
    float maxHeight = float(push.dimAndOffset.y);
    float y = offsetY + maxHeight * texture(heightmap, mapCoord).r;
    vec2 pos = mapCoord * mapSize + start;
    return vec3(pos.x, y, pos.y);
}


const int friend1[6] = int[](
    1, 2, 0, 4, 5, 3
);
const int friend2[6] = int[](
    2, 0, 1, 5, 3, 4
);


void main() {
    vec2 baseIndices = vec2(
        float(gl_InstanceIndex % (push.dimAndOffset.x - 1)),
        float(gl_InstanceIndex / (push.dimAndOffset.z - 1))
    );

    vec2 mapSizeWorld = vec2(push.dimAndOffset.x - 1, push.dimAndOffset.z - 1);
    vec2 mapCoord0 = (baseIndices + posOffsetFor[gl_VertexIndex]) / mapSizeWorld;
    vec2 mapCoord1 = (baseIndices + posOffsetFor[friend1[gl_VertexIndex]]) / mapSizeWorld;
    vec2 mapCoord2 = (baseIndices + posOffsetFor[friend2[gl_VertexIndex]]) / mapSizeWorld;
    vec3 v0 = positionFor(mapCoord0);
    vec3 v1 = positionFor(mapCoord1);
    vec3 v2 = positionFor(mapCoord2);

    vec3 normal = -normalize(cross(v2 - v0, v1 - v0));

    float ambientStrength = ubo.globalLightDir_ambientStrength.w;
    vec3 lightDir         = ubo.globalLightDir_ambientStrength.xyz;
    float diffuseStrength = max(dot(normal, -lightDir), 0.0);

    vec4 cameraPosition = ubo.view * vec4(v0, 1.0);
    gl_Position = ubo.proj * cameraPosition;
    outTexCoord = mapCoord0;
    outFragIllumIntensity = ambientStrength + diffuseStrength;

    outFogColor = vec4(ubo.fogColor_density.rgb, 1.0);
    float fogDensity = ubo.fogColor_density.w;
    if (fogDensity > 0) {
        float dist = length(cameraPosition.xyz);
        outVisibility = clamp(exp(-pow((dist * fogDensity), fogGradient)), 0.0, 1.0);
    } else {
        outVisibility = 1.0;
    }
}
