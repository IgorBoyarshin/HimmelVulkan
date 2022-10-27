#version 450

layout(set = 1, binding = 0) uniform sampler2D heightmap;
layout(set = 1, binding = 1) uniform sampler2D grass;

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec4 inPosition_DepthFromLight;
layout(location = 2) in vec3 inNormal;
/* layout(location = 3) in vec3 inLightSpacePosition; */

layout(location = 0) out vec4 gPosition_DepthFromLight;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gColor;
// layout(location = 3) out vec4 gLightSpacePosition;

void main() {
    vec4 heightColor = vec4(texture(heightmap, inTexCoord).rrr, 1.0);
    float height = clamp(heightColor.r + 0.4, 0.0, 1.0);
    gColor = mix(texture(grass, inTexCoord), heightColor, height);
    gPosition_DepthFromLight = inPosition_DepthFromLight;
    /* gLightSpacePosition = vec4(inLightSpacePosition, 1.0); */
    gNormal = vec4(normalize(inNormal), 1.0);
}
