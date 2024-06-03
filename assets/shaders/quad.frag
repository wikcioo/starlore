#version 460 core

layout(location = 0) out vec4 out_color;

in vec4 vs_color;
in vec2 vs_tex_coords;
in float vs_tex_index;

uniform sampler2D u_textures[32];

void main()
{
    int index = int(vs_tex_index);
    out_color = texture(u_textures[index], vs_tex_coords) * vs_color;
}
