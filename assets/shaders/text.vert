#version 460 core

layout(location = 0) in vec4 in_coord;
layout(location = 1) in vec4 in_color;
layout(location = 2) in float in_tex_index;

out vec4 vs_color;
out vec2 vs_tex_coord;
out float vs_tex_index;

uniform mat4 u_projection;

void main()
{
    vs_color = in_color;
    vs_tex_coord = vec2(in_coord.z, in_coord.w);
    vs_tex_index = in_tex_index;
    gl_Position = u_projection * vec4(in_coord.xy, 0.0, 1.0);
}
