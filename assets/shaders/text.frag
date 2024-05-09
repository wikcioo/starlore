#version 460 core

out vec4 out_color;
in vec2 vs_tex_coord;

uniform vec4 u_color;
uniform sampler2D u_texture;

void main()
{
    out_color = vec4(1.0, 1.0, 1.0, texture(u_texture, vs_tex_coord).r) * u_color;
}
