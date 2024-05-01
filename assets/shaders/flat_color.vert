#version 460 core

layout(location = 0) in vec2 in_position;

uniform mat4 u_projection;
uniform mat4 u_model;

void main()
{
    gl_Position = u_projection * u_model * vec4(in_position, 0.0, 1.0);
}
