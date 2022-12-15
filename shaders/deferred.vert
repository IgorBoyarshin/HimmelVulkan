#version 450

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outSphereCoord;

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform GeneralUbo {
    mat4 view;
    mat4 proj;
    mat4 globalLightView;
    mat4 globalLightProj;
    vec3 globalLightDir;
    float ambientStrength;
    vec3 fogColor;
    float fogDensity;
    vec3 cameraPos;
    float dayNightCycleT;
} uboGeneral;

#define UNIT 1.0
const vec2 positionFor[6] = vec2[](
    vec2(-UNIT, -UNIT),
    vec2(-UNIT, +UNIT),
    vec2(+UNIT, +UNIT),
    vec2(-UNIT, -UNIT),
    vec2(+UNIT, +UNIT),
    vec2(+UNIT, -UNIT)
);
const vec2 texCoordFor[6] = vec2[](
    vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0),
    vec2(0.0, 0.0),
    vec2(1.0, 1.0),
    vec2(1.0, 0.0)
);

layout (constant_id = 0) const float SPHERE_X = 1.0f;
layout (constant_id = 1) const float SPHERE_Y = 1.0f;
layout (constant_id = 2) const float SPHERE_Z = 1.0f;

void main() {
    vec3 sphereCoord;
    switch (gl_VertexIndex) {
        case 0:
        case 3:
            sphereCoord = vec3(-SPHERE_X, -SPHERE_Y, -SPHERE_Z); break;
        case 1:
            sphereCoord = vec3(-SPHERE_X, +SPHERE_Y, -SPHERE_Z); break;
        case 2:
        case 4:
            sphereCoord = vec3(+SPHERE_X, +SPHERE_Y, -SPHERE_Z); break;
        case 5:
            sphereCoord = vec3(+SPHERE_X, -SPHERE_Y, -SPHERE_Z); break;
    }
    outTexCoord = texCoordFor[gl_VertexIndex];
    outSphereCoord = (transpose(uboGeneral.view) * vec4(sphereCoord, 0.0)).xyz;
    gl_Position = vec4(positionFor[gl_VertexIndex], 0.0, 1.0);
}
