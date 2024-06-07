#version 460 core

layout(location = 0) in vec2 in_world_position;
layout(location = 1) in vec2 in_local_position;
layout(location = 2) in vec4 in_color;
layout(location = 3) in float in_thickness;
layout(location = 4) in float in_fade;

out vec2 vs_local_position;
out vec4 vs_color;
out float vs_thickness;
out float vs_fade;

uniform mat4 u_projection;

void main()
{
    vs_local_position = in_local_position;
    vs_color = in_color;
    vs_thickness = in_thickness;
    vs_fade = in_fade;
    gl_Position = u_projection * vec4(in_world_position, 0.0, 1.0);
}
