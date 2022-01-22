#version 450

layout(triangles) in;
layout(line_strip, max_vertices = 10) out;

// XXX Sync across all shaders
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 globalLightDir_ambientStrength;
    vec4 fogColor_density;
    vec3 cameraPos;
} ubo;

layout(location = 0) in vec3 inNormal[];

layout(location = 0) out vec4 outColor;

void generateLine(vec4 p1, vec4 p2) {
    gl_Position = p2;
    EmitVertex();
    EndPrimitive();
}

void main() {
    const vec4 normalColor = vec4(1.0, 0.0, 0.0, 1.0);
    const vec4 triangleColor = vec4(1.0, 1.0, 0.0, 1.0);
    const vec4 off = vec4(0.0, 0.001, 0.0, 0.0);
    vec4 p0 = gl_in[0].gl_Position + off;
    vec4 p1 = gl_in[1].gl_Position + off;
    vec4 p2 = gl_in[2].gl_Position + off;
    gl_Position = ubo.proj * ubo.view * p0;
    outColor = triangleColor; EmitVertex();
    gl_Position = ubo.proj * ubo.view * p1;
    outColor = triangleColor; EmitVertex();
    gl_Position = ubo.proj * ubo.view * p2;
    outColor = triangleColor; EmitVertex();
    gl_Position = ubo.proj * ubo.view * p0;
    outColor = triangleColor; EmitVertex();
    EndPrimitive();

    const float NORMAL_MAGNITUDE = 2.0;
    vec4 n0 = p0 + NORMAL_MAGNITUDE * vec4(inNormal[0], 0.0);
    vec4 n1 = p1 + NORMAL_MAGNITUDE * vec4(inNormal[1], 0.0);
    vec4 n2 = p2 + NORMAL_MAGNITUDE * vec4(inNormal[2], 0.0);
    gl_Position = ubo.proj * ubo.view * p0;
    outColor = normalColor; EmitVertex();
    gl_Position = ubo.proj * ubo.view * n0;
    outColor = normalColor; EmitVertex();
    EndPrimitive();
    gl_Position = ubo.proj * ubo.view * p1;
    outColor = normalColor; EmitVertex();
    gl_Position = ubo.proj * ubo.view * n1;
    outColor = normalColor; EmitVertex();
    EndPrimitive();
    gl_Position = ubo.proj * ubo.view * p2;
    outColor = normalColor; EmitVertex();
    gl_Position = ubo.proj * ubo.view * n2;
    outColor = normalColor; EmitVertex();
    EndPrimitive();

    /* vec4 m0 = mix(mix(p0, p1, 0.5), p2, 0.5); */
    /* vec4 m1 = m0 + inNormal; */
    /* gl_Position = ubo.proj * ubo.view * m0; EmitVertex(); */
    /* gl_Position = ubo.proj * ubo.view * m1; EmitVertex(); */
    /* EndPrimitive(); */
}
