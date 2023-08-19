#version 450

layout(push_constant) uniform PushConstants {
    uint isHorizontal;
} push;

layout(set = 0, binding = 0) uniform sampler2D tex;

layout(location = 0) in  vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    const int R = 1;
    vec2 step;
    if (push.isHorizontal == 1) {
        step = vec2(1.0 / textureSize(tex, 0).x, 0);
    } else {
        step = vec2(0, 1.0 / textureSize(tex, 0).y);
    }

    vec3 result = vec3(0);
    for (int i = -R; i <= R; i++) {
        result += texture(tex, inTexCoord + i * step).rgb;
    }
    result /= 2 * R + 1;
    outColor = vec4(result, 1.0);
}
