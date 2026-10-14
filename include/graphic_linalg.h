#ifndef LIBLINALG_H
#define LIBLINALG_H

#include <stdbool.h>

/*
 * Math layer. Names here carry no graphic_ prefix on purpose: these
 * calls are composed inline all over the place, often several per
 * line, and the prefix would drown the expression. Everything outside
 * this header is prefixed.
 *
 * Conventions:
 *   - right handed world space, camera looks down -Z
 *   - clip space depth range is [0, 1]   (Vulkan native, not OpenGL)
 *   - the Y flip of Vulkan NDC is NOT done here; it is applied by the
 *     backend through a negative viewport height
 *   - column vector convention: v' = M * v, so a composition is
 *     written projection * view * model
 */

#define MATH_PI         3.14159265358979323846f
#define DEG2RAD         (MATH_PI / 180.0f)
#define MATH_EPSILON    1e-6f
#define RAD2DEG         (180.0f / MATH_PI)

typedef struct vec2 {
    float x;
    float y;
} vec2_t;

typedef struct vec3 {
    float x;
    float y;
    float z;
} vec3_t;

typedef struct vec4 {
    float x;
    float y;
    float z;
    float w;
} vec4_t;

/*
 * 4x4 matrix, column-major, memory layout identical to GLSL mat4:
 * element at row r, column c lives at m[c * 4 + r]. This means a
 * mat4_t can be pushed to the GPU with a plain memcpy.
 */
typedef struct mat4 {
    float m[16];
} mat4_t;

/* ------------------------------------------------------------------ */
/* vec2                                                               */
/* ------------------------------------------------------------------ */

vec2_t vec2_add(vec2_t a, vec2_t b);
vec2_t vec2_subtract(vec2_t a, vec2_t b);
vec2_t vec2_scale(vec2_t v, float scale);

/* ------------------------------------------------------------------ */
/* vec3                                                               */
/* ------------------------------------------------------------------ */

vec3_t vec3_add(vec3_t a,
                vec3_t b);

vec3_t vec3_subtract(vec3_t a,
                     vec3_t b);

vec3_t vec3_scale(vec3_t v,
                  float  scale);

vec3_t vec3_cross(vec3_t a,
                  vec3_t b);

vec3_t vec3_normalize(vec3_t v);

vec3_t vec3_transform(vec3_t v,
                      mat4_t matrix);

float vec3_dot(vec3_t a,
               vec3_t b);

float vec3_length(vec3_t v);

/* ------------------------------------------------------------------ */
/* mat                                                                */
/* ------------------------------------------------------------------ */

mat4_t mat4_identity_get(void);

mat4_t mat4_multiply_get(mat4_t A,
                         mat4_t B);

mat4_t mat4_translate_get(float x,
                          float y,
                          float z);

mat4_t mat4_rotate_get(vec3_t axis,
                       float  angle_radians);

mat4_t mat4_scale_get(float x,
                      float y,
                      float z);

mat4_t mat4_perspective_get(float fovy_radians,
                            float aspect,
                            float near_plane,
                            float far_plane);

mat4_t mat4_ortho_get(float left,
                      float right,
                      float bottom,
                      float top,
                      float near_plane,
                      float far_plane);

mat4_t mat4_look_at_get(vec3_t eye,
                        vec3_t target,
                        vec3_t up);

#endif /* LIBLINALG_H */