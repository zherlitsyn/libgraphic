#include <math.h>
#include <stddef.h>

#include "graphic_internal.h"

/*
 * Camera matrices, projection and unprojection, and the built in
 * camera controllers.
 *
 * This file has no Vulkan in it. It only needs the render size and the
 * input state, so the same code serves any backend.
 */

#define CAMERA_MOVE_SPEED           5.4f      /* units per second */
#define CAMERA_ROTATION_SPEED       90.0f     /* degrees per second */
#define CAMERA_PAN_SPEED            2.0f
#define CAMERA_MOUSE_SENSITIVITY    0.003f    /* radians per pixel */
#define CAMERA_ORBITAL_SPEED        0.5f      /* radians per second */
#define CAMERA_WHEEL_ZOOM           1.5f
#define CAMERA_MIN_CLIP_DISTANCE    0.3f

/* ------------------------------------------------------------------ */
/* matrices                                                           */
/* ------------------------------------------------------------------ */

mat4_t graphic_camera_matrix_get(graphic_camera3d_t camera)
{
    return mat4_look_at_get(camera.position, camera.target, camera.up);
}

/*
 * Read right to left: move the target to the origin, rotate and scale
 * about it, then place it at the offset on screen. That order is what
 * makes zoom and rotation happen around the target rather than around
 * the top left corner.
 */
mat4_t graphic_camera2d_matrix_get(graphic_camera2d_t camera)
{
    vec3_t axis        = { 0.0f, 0.0f, 1.0f };
    mat4_t origin      = mat4_translate_get(-camera.target.x, -camera.target.y, 0.0f);
    mat4_t rotation    = mat4_rotate_get(axis, camera.rotation * DEG2RAD);
    mat4_t scale       = mat4_scale_get(camera.zoom, camera.zoom, 1.0f);
    mat4_t translation = mat4_translate_get(camera.offset.x, camera.offset.y, 0.0f);

    return mat4_multiply_get(mat4_multiply_get(translation, mat4_multiply_get(rotation, scale)), origin);
}

static mat4_t camera_projection_get(graphic_camera3d_t camera,
                                    float              aspect)
{
    if (camera.projection == GRAPHIC_CAMERA_ORTHOGRAPHIC) {
        float top   = camera.fovy / 2.0f;
        float right = top * aspect;

        return mat4_ortho_get(-right, right, -top, top, 0.01f, 1000.0f);
    }

    return mat4_perspective_get(camera.fovy * DEG2RAD, aspect, 0.01f, 1000.0f);
}

/* ------------------------------------------------------------------ */
/* projection                                                         */
/* ------------------------------------------------------------------ */

vec2_t graphic_world_to_screen_get_ex(vec3_t             position,
                                      graphic_camera3d_t camera,
                                      int                width,
                                      int                height)
{
    vec2_t screen = { 0.0f, 0.0f };

    if (width <= 0 || height <= 0)
        return screen;

    mat4_t view       = graphic_camera_matrix_get(camera);
    mat4_t projection = camera_projection_get(camera, (float)width / (float)height);

    vec4_t point;
    point.x = position.x;
    point.y = position.y;
    point.z = position.z;
    point.w = 1.0f;

    vec4_t clip = vec4_transform(point, mat4_multiply_get(projection, view));

    if (clip.w > -MATH_EPSILON && clip.w < MATH_EPSILON)
        return screen;    /* exactly on the eye plane */

    clip.x /= clip.w;
    clip.y /= clip.w;

    /*
     * The viewport flip lives in the backend, so clip space here is
     * still Y up and the Y term is inverted by hand.
     */
    screen.x = (clip.x + 1.0f) * 0.5f * (float)width;
    screen.y = (1.0f - clip.y) * 0.5f * (float)height;

    return screen;
}

vec2_t graphic_world_to_screen_get(vec3_t             position,
                                   graphic_camera3d_t camera)
{
    return graphic_world_to_screen_get_ex(position, 
                                          camera,
                                          graphic_render_width_get(),
                                          graphic_render_height_get());
}

ray_t graphic_screen_to_world_ray_get_ex(vec2_t             position,
                                         graphic_camera3d_t camera,
                                         int                width,
                                         int                height)
{
    ray_t ray;
    ray.position  = camera.position;
    ray.direction = vec3_normalize(vec3_subtract(camera.target, camera.position));

    if (width <= 0 || height <= 0)
        return ray;

    float ndc_x = position.x / (float)width * 2.0f - 1.0f;
    float ndc_y = 1.0f - position.y / (float)height * 2.0f;

    mat4_t inverse = mat4_invert_get(mat4_multiply_get(
        camera_projection_get(camera, (float)width / (float)height),
        graphic_camera_matrix_get(camera)));

    /* depth is [0, 1] here, not [-1, 1] as it would be in OpenGL */
    vec4_t near_point;
    near_point.x = ndc_x;
    near_point.y = ndc_y;
    near_point.z = 0.0f;
    near_point.w = 1.0f;

    vec4_t far_point;
    far_point   = near_point;
    far_point.z = 1.0f;

    near_point = vec4_transform(near_point, inverse);
    far_point  = vec4_transform(far_point,  inverse);

    if (near_point.w > -MATH_EPSILON && near_point.w < MATH_EPSILON)
        return ray;

    if (far_point.w > -MATH_EPSILON && far_point.w < MATH_EPSILON)
        return ray;

    vec3_t near_world = (vec3_t){ near_point.x / near_point.w,
                                  near_point.y / near_point.w,
                                  near_point.z / near_point.w };

    vec3_t far_world = (vec3_t){ far_point.x / far_point.w,
                                 far_point.y / far_point.w,
                                 far_point.z / far_point.w };

    if (camera.projection == GRAPHIC_CAMERA_ORTHOGRAPHIC) {
        /* parallel rays: the origin moves, the direction does not */
        ray.position = near_world;
    } else {
        ray.direction = vec3_normalize(vec3_subtract(far_world, near_world));
    }

    return ray;
}

ray_t graphic_screen_to_world_ray_get(vec2_t             position,
                                      graphic_camera3d_t camera)
{
    return graphic_screen_to_world_ray_get_ex(position,
                                              camera,
                                              graphic_render_width_get(),
                                              graphic_render_height_get());
}

vec2_t graphic_world_to_screen2d_get(vec2_t             position,
                                     graphic_camera2d_t camera)
{
    vec3_t point       = { position.x, position.y, 0.0f };
    vec3_t transformed = vec3_transform(point, graphic_camera2d_matrix_get(camera));
    vec2_t screen      = { transformed.x, transformed.y };

    return screen;
}

vec2_t graphic_screen_to_world2d_get(vec2_t             position,
                                     graphic_camera2d_t camera)
{
    mat4_t inverse     = mat4_invert_get(graphic_camera2d_matrix_get(camera));
    vec3_t point       = { position.x, position.y, 0.0f };
    vec3_t transformed = vec3_transform(point, inverse);
    vec2_t world       = { transformed.x, transformed.y };

    return world;
}

/* ------------------------------------------------------------------ */
/* camera frame                                                        */
/* ------------------------------------------------------------------ */

static vec3_t camera_forward_get(const graphic_camera3d_t *camera)
{
    return vec3_normalize(vec3_subtract(camera->target, camera->position));
}

static vec3_t camera_right_get(const graphic_camera3d_t *camera)
{
    return vec3_normalize(vec3_cross(camera_forward_get(camera),
                                     vec3_normalize(camera->up)));
}

/*
 * move_in_world_plane keeps a walking camera on the ground when it is
 * looking up or down, which is what a first person controller wants
 * and a free flying one does not.
 */
static void camera_move_forward(graphic_camera3d_t *camera,
                                float               distance,
                                bool                move_in_world_plane)
{
    vec3_t forward = camera_forward_get(camera);

    if (move_in_world_plane) {
        forward.y = 0.0f;
        forward   = vec3_normalize(forward);
    }

    forward = vec3_scale(forward, distance);

    camera->position = vec3_add(camera->position, forward);
    camera->target   = vec3_add(camera->target, forward);
}

static void camera_move_right(graphic_camera3d_t *camera,
                              float               distance,
                              bool                move_in_world_plane)
{
    vec3_t right = camera_right_get(camera);

    if (move_in_world_plane) {
        right.y = 0.0f;
        right   = vec3_normalize(right);
    }

    right = vec3_scale(right, distance);

    camera->position = vec3_add(camera->position, right);
    camera->target   = vec3_add(camera->target, right);
}

static void camera_move_up(graphic_camera3d_t *camera,
                           float               distance)
{
    vec3_t up = vec3_scale(vec3_normalize(camera->up), distance);

    camera->position = vec3_add(camera->position, up);
    camera->target   = vec3_add(camera->target, up);
}

/* positive distance moves towards the target, without passing it */
static void camera_move_to_target(graphic_camera3d_t *camera,
                                  float               delta)
{
    float distance = vec3_distance(camera->position, camera->target) - delta;

    if (distance < CAMERA_MIN_CLIP_DISTANCE)
        distance = CAMERA_MIN_CLIP_DISTANCE;

    camera->position = vec3_add(camera->target, 
                                vec3_scale(camera_forward_get(camera), -distance));
}

static void camera_yaw(graphic_camera3d_t *camera,
                       float               angle,
                       bool                rotate_around_target)
{
    vec3_t up   = vec3_normalize(camera->up);
    vec3_t view = vec3_subtract(camera->target, camera->position);

    view = vec3_rotate_by_axis_angle(view, up, angle);

    if (rotate_around_target)
        camera->position = vec3_subtract(camera->target, view);
    else
        camera->target = vec3_add(camera->position, view);
}

/*
 * lock_view stops the camera from tipping past straight up or straight
 * down, which would flip the world over.
 */
static void camera_pitch(graphic_camera3d_t *camera,
                         float               angle,
                         bool                lock_view,
                         bool                rotate_around_target)
{
    vec3_t up    = vec3_normalize(camera->up);
    vec3_t view  = vec3_subtract(camera->target, camera->position);
    vec3_t right = camera_right_get(camera);

    if (lock_view) {
        float max_angle;

        max_angle = acosf(vec3_dot(up, vec3_normalize(view)));
        max_angle -= 0.001f;

        if (angle > max_angle)
            angle = max_angle;

        max_angle = -acosf(vec3_dot(vec3_scale(up, -1.0f), vec3_normalize(view)));
        max_angle += 0.001f;

        if (angle < max_angle)
            angle = max_angle;
    }

    view = vec3_rotate_by_axis_angle(view, right, angle);

    if (rotate_around_target)
        camera->position = vec3_subtract(camera->target, view);
    else
        camera->target = vec3_add(camera->position, view);
}

/* ------------------------------------------------------------------ */
/* controllers                                                        */
/* ------------------------------------------------------------------ */

void graphic_camera_update(graphic_camera3d_t *camera,
                           int                 mode)
{
    if (camera == NULL || mode == GRAPHIC_CAMERA_MODE_CUSTOM)
        return;

    float delta_time   = graphic_frame_time_get();
    vec2_t mouse_delta = graphic_mouse_delta_get();

    float move_speed     = CAMERA_MOVE_SPEED * delta_time;
    float rotation_speed = CAMERA_ROTATION_SPEED * DEG2RAD * delta_time;

    bool move_in_world_plane = (mode == GRAPHIC_CAMERA_MODE_FIRST_PERSON ||
                                mode == GRAPHIC_CAMERA_MODE_THIRD_PERSON);

    bool rotate_around_target = (mode == GRAPHIC_CAMERA_MODE_THIRD_PERSON ||
                                 mode == GRAPHIC_CAMERA_MODE_ORBITAL);

    bool lock_view = (mode != GRAPHIC_CAMERA_MODE_FREE);

    if (mode == GRAPHIC_CAMERA_MODE_ORBITAL) {
        /* a slow automatic turn, plus zoom on the wheel */
        camera_yaw(camera, CAMERA_ORBITAL_SPEED * delta_time, true);
        camera_move_to_target(camera,
                              graphic_mouse_wheel_move_get() * CAMERA_WHEEL_ZOOM);
        return;
    }

    if (graphic_is_key_down(GRAPHIC_KEY_W))
        camera_move_forward(camera, move_speed, move_in_world_plane);

    if (graphic_is_key_down(GRAPHIC_KEY_S))
        camera_move_forward(camera, -move_speed, move_in_world_plane);

    if (graphic_is_key_down(GRAPHIC_KEY_A))
        camera_move_right(camera, -move_speed, move_in_world_plane);

    if (graphic_is_key_down(GRAPHIC_KEY_D))
        camera_move_right(camera, move_speed, move_in_world_plane);

    if (mode == GRAPHIC_CAMERA_MODE_FREE) {
        if (graphic_is_key_down(GRAPHIC_KEY_E))
            camera_move_up(camera, move_speed);

        if (graphic_is_key_down(GRAPHIC_KEY_Q))
            camera_move_up(camera, -move_speed);
    }

    /*
     * The mouse only steers while it is captured. Otherwise the
     * camera would spin whenever the pointer crossed the window,
     * so the arrow keys stand in.
     */
    if (graphic_cursor_is_hidden()) {
        camera_yaw(camera,
                   -mouse_delta.x * CAMERA_MOUSE_SENSITIVITY,
                   rotate_around_target);

        camera_pitch(camera,
                     -mouse_delta.y * CAMERA_MOUSE_SENSITIVITY,
                     lock_view, rotate_around_target);
    } else {
        if (graphic_is_key_down(GRAPHIC_KEY_LEFT))
            camera_yaw(camera, rotation_speed, rotate_around_target);
        if (graphic_is_key_down(GRAPHIC_KEY_RIGHT))
            camera_yaw(camera, -rotation_speed, rotate_around_target);
        if (graphic_is_key_down(GRAPHIC_KEY_UP))
            camera_pitch(camera, rotation_speed, lock_view, rotate_around_target);
        if (graphic_is_key_down(GRAPHIC_KEY_DOWN))
            camera_pitch(camera, -rotation_speed, lock_view, rotate_around_target);
    }

    camera_move_to_target(camera, graphic_mouse_wheel_move_get() * CAMERA_WHEEL_ZOOM);
}

/*
 * The explicit version: movement is forward, right and up in world
 * units, rotation is yaw, pitch and roll in degrees, zoom moves along
 * the view axis. Nothing is read from the input devices, so this is
 * the one to drive from a script or a simulation.
 */
void graphic_camera_update_ex(graphic_camera3d_t *camera,
                              vec3_t              movement,
                              vec3_t              rotation,
                              float               zoom)
{
    if (camera == NULL)
        return;

    camera_yaw(camera, -rotation.x * DEG2RAD, false);
    camera_pitch(camera, rotation.y * DEG2RAD, true, false);
    /* roll is not applied: the up vector is kept world aligned */

    camera_move_forward(camera, movement.x, true);
    camera_move_right(camera, movement.y, true);
    camera_move_up(camera, movement.z);

    camera_move_to_target(camera, zoom);
}