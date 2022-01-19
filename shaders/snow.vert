#version 450
#extension GL_ARB_separate_shader_objects : enable

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 globalLightDir_ambientStrength;
    vec4 fogColor_density;
    vec3 cameraPos;
} ubo;

layout(std140, set = 2, binding = 0) readonly buffer SnowInstancesUniformBuffers {
    vec4 positions[]; // vec3 pos + float rotation
} snowData;

layout(push_constant) uniform PushConstants {
    vec3 halfSize;
    float time;
    vec3 velocity;
    float snowMode;
} push;
const float rotationVelocity = 1.5f;

layout(location = 0)      out vec2  outTexCoord;
layout(location = 1) flat out int   outTextureIndex;
layout(location = 2)      out float outAmbient;


vec4 vertexFor(vec4 center, vec2 pos, float rotation) {
    const float scale = 0.04;
    float c = cos(rotation);
    float s = sin(rotation);
    vec2 posRotated = mat2(c, s, -s, c) * pos;
    return center + vec4(scale * posRotated, 0.0, 0.0);
}


const vec2 positionFor[6] = vec2[](
    vec2(-0.5, -0.5),
    vec2(+0.5, -0.5),
    vec2(+0.5, +0.5),
    vec2(+0.5, +0.5),
    vec2(-0.5, +0.5),
    vec2(-0.5, -0.5)
);
const vec2 texCoordFor[6] = vec2[](
    vec2(1.0, 0.0),
    vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0),
    vec2(1.0, 0.0)
);


#define TEXTURES_COUNT 2
void main() {
    vec3 position  = snowData.positions[gl_InstanceIndex].xyz;
    float rotation = snowData.positions[gl_InstanceIndex].w;
    if (push.snowMode > 0.0) { // SNOW_MODE_BOX

    } else { // SNOW_MODE_CAMERA
        position *= push.halfSize;
        position += push.time * push.velocity;
        /* position = mod(position, push.halfSize); */
        vec3 boundsUp   = ubo.cameraPos + push.halfSize;
        vec3 boundsDown = ubo.cameraPos - push.halfSize;
        while (position.x < boundsDown.x) position.x += 2.0 * push.halfSize.x;
        while (position.y < boundsDown.y) position.y += 2.0 * push.halfSize.y;
        while (position.z < boundsDown.z) position.z += 2.0 * push.halfSize.z;
        while (position.x > boundsUp.x)   position.x -= 2.0 * push.halfSize.x;
        while (position.y > boundsUp.y)   position.y -= 2.0 * push.halfSize.y;
        while (position.z > boundsUp.z)   position.z -= 2.0 * push.halfSize.z;
        rotation += push.time * rotationVelocity;
    }

    /* float ambientStrength = ubo.globalLightDir_ambientStrength.w; */
    float ambientStrength = 0.4;

    outTextureIndex = gl_InstanceIndex % TEXTURES_COUNT;
    outTexCoord = texCoordFor[gl_VertexIndex];
    outAmbient = max(abs(sin(rotation)), ambientStrength);

    vec4 centerInCameraSpace = ubo.view * vec4(position, 1.0);

    gl_Position = ubo.proj * vertexFor(centerInCameraSpace, positionFor[gl_VertexIndex], rotation);
}
