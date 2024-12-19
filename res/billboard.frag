#version 330 core

uniform sampler2D tex;
in vec2 vs_uv;
in vec3 vs_rgb;
out vec4 rgba;

void main() {
    vec4 rg = texture(tex, vs_uv);
    rgba = vec4(rg.r * vs_rgb, rg.g);
}
