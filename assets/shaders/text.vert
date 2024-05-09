#version 460 core

layout(location = 0) in vec4 in_coord;

out vec2 vs_tex_coord;

uniform mat4 u_projection;

void main()
{
    gl_Position = u_projection * vec4(in_coord.xy, 0.0, 1.0);
    vs_tex_coord = vec2(in_coord.z, in_coord.w);
}
