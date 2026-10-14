#version 450

layout(push_constant) uniform push_constants {
    mat4 view_projection;
} constants;

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
    gl_Position  = constants.view_projection * vec4(in_position, 1.0);
}