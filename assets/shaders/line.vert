#version 460 core

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec4 in_color;

out vec4 vs_color;

uniform mat4 u_projection;

void main()
{
    vs_color = in_color;
    gl_Position = u_projection * vec4(in_position, 0.0, 1.0);
}
