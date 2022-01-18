#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform sampler2D heightmap;
layout(set = 1, binding = 1) uniform sampler2D grass;

layout(location = 0) in vec2  inTexCoord;
layout(location = 1) in float inFragIllumIntensity;
layout(location = 2) in float inVisibility;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 heightColor = texture(heightmap, inTexCoord);
    float height = heightColor.r + 0.3;
    if (height > 1.0) height = 1.0;
    outColor = mix(texture(grass, inTexCoord), heightColor, height);
    outColor.rgb *= inFragIllumIntensity;
    const vec4 fogColor = vec4(0.7, 0.7, 0.7, 1.0); // XXX
    outColor = mix(fogColor, outColor, inVisibility);
}
