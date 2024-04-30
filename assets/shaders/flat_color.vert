#version 460 core

layout(location = 0) in vec2 in_position;

uniform vec2 u_position_offset;

void main()
{
    gl_Position = vec4(in_position + u_position_offset, 0.0, 1.0);
}
