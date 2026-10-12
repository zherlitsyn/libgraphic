#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_texcoord;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_texcoord;

void main()
{
    out_color    = in_color;
    out_texcoord = in_texcoord;
    gl_Position  = vec4(in_position, 1.0);
}