#version 460 core

layout(location = 0) out vec4 out_color;

in vec2 vs_local_position;
in vec4 vs_color;
in float vs_thickness;
in float vs_fade;

void main()
{
    float distance = 1.0 - length(vs_local_position);
    float circle = smoothstep(0.0, vs_fade, distance);
    circle *= smoothstep(vs_thickness + vs_fade, vs_thickness, distance);

    if (circle == 0.0) {
        discard;
    }

    out_color = vs_color;
    out_color.a *= circle;
}
