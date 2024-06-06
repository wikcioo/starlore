#version 460 core

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_tex_coords;
layout(location = 3) in float in_tex_index;

out vec4 vs_color;
out vec2 vs_tex_coords;
out float vs_tex_index;

uniform mat4 u_projection;

void main()
{
    vs_color = in_color;
    vs_tex_coords = in_tex_coords;
    vs_tex_index = in_tex_index;
    gl_Position = u_projection * vec4(in_position);
}
