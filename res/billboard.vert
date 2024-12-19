#version 460 core

#define RADIUS 0.4f

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;

layout(binding = 2, std430) readonly buffer balls {
    vec4 positions[4096];
    uint colors[4096];
};

const int indices[6] = int[6] (0, 1, 2, 2, 3, 0); 

const vec2 uvs[] = vec2[](
    vec2(1.0f, 1.0f),
    vec2(0.0f, 1.0f),
    vec2(0.0f, 0.0f),
    vec2(1.0f, 0.0f)
);

out vec2 vs_uv;
out vec3 vs_rgb;

void main() {
    int i = gl_VertexID / 6;
    int j = gl_VertexID % 6; 
    vec2 uv = uvs[indices[j]];
    vs_uv = uv;
    uint color = colors[i];
    float r = color >> 16u;
    float g = (color >> 8u) & 255u;
    float b = color & 255u;
    vs_rgb = vec3(r, g, b) / 255.0;
    mat4 model = mat4(1.0);
    model[3] = positions[i];
    mat4 mv = view * model;
    mv[0] = vec4(RADIUS, 0.0f, 0.0f, 0.0f);
    mv[1] = vec4(0.0f, RADIUS, 0.0f, 0.0f);
    mv[2] = vec4(0.0f, RADIUS, RADIUS, 0.0f);
    vec2 pos = uv * 2.0f - 1.0f; 
    gl_Position = proj * mv * vec4(pos, 0.0f, 1.0f); 
}
