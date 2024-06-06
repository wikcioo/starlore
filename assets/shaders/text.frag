#version 460 core

out vec4 out_color;

in vec4 vs_color;
in vec2 vs_tex_coord;
in float vs_tex_index;

uniform sampler2D u_textures[4];

void main()
{
    int index = int(vs_tex_index);
    out_color = vec4(1.0, 1.0, 1.0, texture(u_textures[index], vs_tex_coord).r) * vs_color;
}
