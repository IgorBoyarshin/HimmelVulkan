#version 450

layout(set = 0, binding = 0) uniform sampler2D mainSampler;
layout(set = 0, binding = 1) uniform sampler2D brightnessSampler;

layout(location = 0) in  vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

/* vec3 blurred(vec2 coord) { */
/*     vec2 texelSize = 1.0 / vec2(textureSize(brightnessSampler, 0)); */
/*     vec3 result = vec3(0.0); */
/*     const int SIZE = 5; */
/*     for (int x = -SIZE; x <= SIZE; x++) { */
/*         for (int y = -SIZE; y <= SIZE; y++) { */
/*             vec2 off = vec2(float(x), float(y)) * texelSize; */
/*             result += texture(brightnessSampler, coord + off).rgb; */
/*         } */
/*     } */
/*     const float EDGE = (2 * SIZE + 1); */
/*     return result / (EDGE * EDGE); */
/* } */

void main() {
    vec3 color = texture(mainSampler, inTexCoord).rgb;
    // vec2 strength = texture(brightnessSampler, inTexCoord).ra;

    // Apply brightness
    // color *= (1.0 + pow(strength.x * strength.y, 4));
    const float emissiveFactor = 4.0;
    vec4 emissive = texture(brightnessSampler, inTexCoord);
    color += emissiveFactor * emissive.rgb * emissive.a;

    // Tone mapping and gamma correcton
    const float exposure = 1.7;
    const float gamma = 0.6;
    vec3 mapped = vec3(1.0) - exp(-color * exposure);
    color = pow(mapped, vec3(1.0 / gamma));

    outColor = vec4(color, 1.0);
    // outColor.rgb += texture(brightnessSampler, inTexCoord).rgb;
}
