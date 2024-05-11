#version 460 core

out vec4 out_color;

uniform vec4 u_color;

void main()
{
    out_color = u_color;
}
