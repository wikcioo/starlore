#version 460 core

layout(location = 0) out vec4 out_color;

in vec4 vs_color;

void main()
{
    out_color = vs_color;
}
