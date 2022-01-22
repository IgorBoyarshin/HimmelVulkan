#version 450

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 globalLightDir_ambientStrength;
    vec4 fogColor_density;
    vec3 cameraPos;
} ubo;

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

layout(location = 0) out vec2 outTexCoord;


void main() {
    // TODO can make a look-up table
    vec2 halfSize = 0.5 * push.size;
    if (gl_VertexIndex == 0) {
        gl_Position = vec4(push.center.x - halfSize.x, 0.0, push.center.y - halfSize.y, 1.0);
        outTexCoord = push.texCoordStart;
    } else if (gl_VertexIndex == 1) {
        gl_Position = vec4(push.center.x - halfSize.x, 0.0, push.center.y + halfSize.y, 1.0);
        outTexCoord = push.texCoordStart;
        outTexCoord.y += push.texCoordStep.y;
    } else if (gl_VertexIndex == 2) {
        gl_Position = vec4(push.center.x + halfSize.x, 0.0, push.center.y + halfSize.y, 1.0);
        outTexCoord = push.texCoordStart + push.texCoordStep;
    } else if (gl_VertexIndex == 3) {
        gl_Position = vec4(push.center.x + halfSize.x, 0.0, push.center.y - halfSize.y, 1.0);
        outTexCoord = push.texCoordStart;
        outTexCoord.x += push.texCoordStep.x;
    }
}
