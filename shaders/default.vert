#version 450

/*
 * A triangle with nowhere else to come from yet: the positions and
 * the colours live in the shader, picked out by gl_VertexIndex.
 */
const vec2 positions[3] = vec2[](
    vec2( 0.0,  0.5),
    vec2(-0.5, -0.5),
    vec2( 0.5, -0.5)
);

const vec4 colors[3] = vec4[](
    vec4(0.90, 0.24, 0.24, 1.0),
    vec4(0.24, 0.78, 0.35, 1.0),
    vec4(0.24, 0.43, 0.90, 1.0)
);

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = colors[gl_VertexIndex];

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}