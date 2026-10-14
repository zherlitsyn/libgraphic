#include <math.h>

#include "graphic_internal.h"

/*
 * Matrices are column-major, laid out exactly like a GLSL mat4:
 * element at row r, column c lives at m[c * 4 + r]. This means a
 * mat4_t can be pushed to the GPU with a plain memcpy.
 *
 * Conventions:
 *   - right handed world space, camera looks down -Z
 *   - clip space depth range is [0, 1]   (Vulkan native, not OpenGL)
 *   - the Y flip of Vulkan NDC is NOT done here; it is applied by the
 *     backend through a negative viewport height
 *   - column vector convention: v' = M * v, so a composition is
 *     written projection * view * model
 */

/* ------------------------------------------------------------------ */
/* vec3                                                               */
/* ------------------------------------------------------------------ */

vec3_t vec3_add(vec3_t a,
                vec3_t b)
{
    vec3_t result = { a.x + b.x, a.y + b.y, a.z + b.z };
    return result;
}

vec3_t vec3_subtract(vec3_t a,
                     vec3_t b)
{
    vec3_t result = { a.x - b.x, a.y - b.y, a.z - b.z };
    return result;
}

vec3_t vec3_scale(vec3_t v,
                  float  scale)
{
    vec3_t result = { v.x * scale, v.y * scale, v.z * scale };
    return result;
}

vec3_t vec3_cross(vec3_t a,
                  vec3_t b)
{
    vec3_t result = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
    return result;
}

float vec3_dot(vec3_t a,
               vec3_t b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float vec3_length(vec3_t v)
{
    return sqrtf(vec3_dot(v, v));
}

vec3_t vec3_normalize(vec3_t v)
{
    float length = vec3_length(v);
    vec3_t zero = { 0.0f, 0.0f, 0.0f };

    if (length < MATH_EPSILON)
        return zero;

    return vec3_scale(v, 1.0f / length);
}

/*
 * Transform a point (implicit w = 1). No perspective divide: this is
 * used for model space transforms only, where the matrix is affine.
 */
vec3_t vec3_transform(vec3_t v,
                      mat4_t matrix)
{
    const float *m = matrix.m;
    vec3_t result;

    result.x = m[0] * v.x + m[4] * v.y + m[8]  * v.z + m[12];
    result.y = m[1] * v.x + m[5] * v.y + m[9]  * v.z + m[13];
    result.z = m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14];

    return result;
}

/* ------------------------------------------------------------------ */
/* mat4                                                               */
/* ------------------------------------------------------------------ */

mat4_t mat4_identity_get(void)
{
    mat4_t result = { { 0 } };
    result.m[0]   = 1.0f;
    result.m[5]   = 1.0f;
    result.m[10]  = 1.0f;
    result.m[15]  = 1.0f;

    return result;
}

/* result = A * B */
mat4_t mat4_multiply_get(mat4_t A,
                         mat4_t B)
{
    mat4_t result = {
        A.m[0] * B.m[0]  + A.m[1] * B.m[4]  + A.m[2] * B.m[8]   + A.m[3] * B.m[12],
        A.m[0] * B.m[1]  + A.m[1] * B.m[5]  + A.m[2] * B.m[9]   + A.m[3] * B.m[13],
        A.m[0] * B.m[2]  + A.m[1] * B.m[6]  + A.m[2] * B.m[10]  + A.m[3] * B.m[14],
        A.m[0] * B.m[3]  + A.m[1] * B.m[7]  + A.m[2] * B.m[11]  + A.m[3] * B.m[15],

        A.m[4] * B.m[0]  + A.m[5] * B.m[4]  + A.m[6] * B.m[8]   + A.m[7] * B.m[12],
        A.m[4] * B.m[1]  + A.m[5] * B.m[5]  + A.m[6] * B.m[9]   + A.m[7] * B.m[13],
        A.m[4] * B.m[2]  + A.m[5] * B.m[6]  + A.m[6] * B.m[10]  + A.m[7] * B.m[14],
        A.m[4] * B.m[3]  + A.m[5] * B.m[7]  + A.m[6] * B.m[11]  + A.m[7] * B.m[15],

        A.m[8] * B.m[0]  + A.m[9] * B.m[4]  + A.m[10] * B.m[8]  + A.m[11] * B.m[12],
        A.m[8] * B.m[1]  + A.m[9] * B.m[5]  + A.m[10] * B.m[9]  + A.m[11] * B.m[13],
        A.m[8] * B.m[2]  + A.m[9] * B.m[6]  + A.m[10] * B.m[10] + A.m[11] * B.m[14],
        A.m[8] * B.m[3]  + A.m[9] * B.m[7]  + A.m[10] * B.m[11] + A.m[11] * B.m[15],

        A.m[12] * B.m[0] + A.m[13] * B.m[4] + A.m[14] * B.m[8]  + A.m[15] * B.m[12],
        A.m[12] * B.m[1] + A.m[13] * B.m[5] + A.m[14] * B.m[9]  + A.m[15] * B.m[13],
        A.m[12] * B.m[2] + A.m[13] * B.m[6] + A.m[14] * B.m[10] + A.m[15] * B.m[14],
        A.m[12] * B.m[3] + A.m[13] * B.m[7] + A.m[14] * B.m[11] + A.m[15] * B.m[15]
    };

    return result;
}

mat4_t mat4_translate_get(float x,
                          float y,
                          float z)
{
    mat4_t result = mat4_identity_get();
    result.m[12]  = x;
    result.m[13]  = y;
    result.m[14]  = z;

    return result;
}

mat4_t mat4_scale_get(float x,
                      float y,
                      float z)
{
    mat4_t result = { { 0 } };
    result.m[0]   = x;
    result.m[5]   = y;
    result.m[10]  = z;
    result.m[15]  = 1.0f;

    return result;
}

/* Rodrigues rotation around an arbitrary axis */
mat4_t mat4_rotate_get(vec3_t axis,
                       float  angle_radians)
{
    float s = sinf(angle_radians);
    float c = cosf(angle_radians);
    float t = 1.0f - c;

    mat4_t result = mat4_identity_get();
    vec3_t n      = vec3_normalize(axis);

    result.m[0]  = t * n.x * n.x + c;
    result.m[1]  = t * n.x * n.y + s * n.z;
    result.m[2]  = t * n.x * n.z - s * n.y;

    result.m[4]  = t * n.x * n.y - s * n.z;
    result.m[5]  = t * n.y * n.y + c;
    result.m[6]  = t * n.y * n.z + s * n.x;

    result.m[8]  = t * n.x * n.z + s * n.y;
    result.m[9]  = t * n.y * n.z - s * n.x;
    result.m[10] = t * n.z * n.z + c;

    return result;
}

/*
 * Right handed perspective projection mapping depth to [0, 1].
 * m[11] = -1 puts view space Z into W, which drives the divide.
 */
mat4_t mat4_perspective_get(float fovy_radians,
                            float aspect,
                            float near_plane,
                            float far_plane)
{
    float focal = 1.0f / tanf(fovy_radians * 0.5f);

    mat4_t result = { { 0 } };
    result.m[0]   = focal / aspect;
    result.m[5]   = focal;
    result.m[10]  = far_plane / (near_plane - far_plane);
    result.m[11]  = -1.0f;
    result.m[14]  = (far_plane * near_plane) / (near_plane - far_plane);

    return result;
}

/* Right handed orthographic projection mapping depth to [0, 1] */
mat4_t mat4_ortho_get(float left,
                      float right,
                      float bottom,
                      float top,
                      float near_plane,
                      float far_plane)
{
    mat4_t result = { { 0 } };

    result.m[0]  = 2.0f / (right - left);
    result.m[5]  = 2.0f / (top - bottom);
    result.m[10] = 1.0f / (near_plane - far_plane);

    result.m[12] = -(right + left) / (right - left);
    result.m[13] = -(top + bottom) / (top - bottom);
    result.m[14] = near_plane / (near_plane - far_plane);
    result.m[15] = 1.0f;

    return result;
}

mat4_t mat4_look_at_get(vec3_t eye,
                        vec3_t target,
                        vec3_t up)
{
    mat4_t result  = mat4_identity_get();
    vec3_t forward = vec3_normalize(vec3_subtract(target, eye));
    vec3_t side    = vec3_normalize(vec3_cross(forward, up));
    vec3_t upward  = vec3_cross(side, forward);

    result.m[0]  =  side.x;
    result.m[4]  =  side.y;
    result.m[8]  =  side.z;

    result.m[1]  =  upward.x;
    result.m[5]  =  upward.y;
    result.m[9]  =  upward.z;

    result.m[2]  = -forward.x;
    result.m[6]  = -forward.y;
    result.m[10] = -forward.z;

    result.m[12] = -vec3_dot(side, eye);
    result.m[13] = -vec3_dot(upward, eye);
    result.m[14] =  vec3_dot(forward, eye);

    return result;
}

/* ------------------------------------------------------------------ */
/* vec2                                                               */
/* ------------------------------------------------------------------ */

vec2_t vec2_add(vec2_t a,
                vec2_t b)
{
    vec2_t result = { a.x + b.x, a.y + b.y };
    return result;
}

vec2_t vec2_subtract(vec2_t a,
                     vec2_t b)
{
    vec2_t result = { a.x - b.x, a.y - b.y };
    return result;
}

vec2_t vec2_scale(vec2_t v,
                  float  scale)
{
    vec2_t result = { v.x * scale, v.y * scale };
    return result;
}