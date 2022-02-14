#version 450

layout(push_constant) uniform PushConstants {
    uint isVertical;
} push;

/* #define TEXTURES_COUNT 2 */
/* layout(set = 0, binding = 0) uniform sampler2D texSamplers[TEXTURES_COUNT]; */
layout(set = 0, binding = 0) uniform sampler2D tex;

layout(location = 0) in  vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

const float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    /* outColor = vec4(texture(tex, inTexCoord).rgb, 1.0); */
    vec2 tex_offset = 1.0 / textureSize(tex, 0);
    vec3 result = texture(tex, inTexCoord).rgb * weight[0];
    if (push.isVertical == 1) {
        for(int i = 1; i < 5; i++) {
            result += texture(tex, inTexCoord + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
            result += texture(tex, inTexCoord - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
        }
    } else {
        for(int i = 1; i < 5; i++) {
            result += texture(tex, inTexCoord + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
            result += texture(tex, inTexCoord - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
        }
    }
    outColor = vec4(result, 1.0);
}
