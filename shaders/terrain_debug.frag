#version 450

layout(location = 0) in vec4  inColor;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outBloom;

void main() {
    outColor = inColor;
    outBloom = vec4(0.0);
}
