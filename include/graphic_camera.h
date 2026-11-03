#ifndef GRAPHIC_CAMERA_H
#define GRAPHIC_CAMERA_H

#include "graphic_linalg.h"

/*
 * Cameras, and the projection maths that goes with them.
 *
 * These live in their own header because the drawing code needs the
 * camera type (billboards face it) while the camera code needs
 * nothing from drawing: putting the types in graphic.h made the two
 * include each other.
 */

typedef struct graphic_camera2d {
    vec2_t offset;        /* where the target lands on screen */
    vec2_t target;        /* world point the camera looks at */
    float rotation;       /* degrees */
    float zoom;           /* 1.0 is one world unit per pixel */
} graphic_camera2d_t;

typedef struct graphic_camera3d {
    vec3_t position;
    vec3_t target;
    vec3_t up;
    float  fovy;          /* vertical field of view, degrees */
    int    projection;    /* enum graphic_camera_projection */
} graphic_camera3d_t;

enum graphic_camera_projection {
    GRAPHIC_CAMERA_PERSPECTIVE = 0,
    GRAPHIC_CAMERA_ORTHOGRAPHIC,
};

enum graphic_camera_mode {
    GRAPHIC_CAMERA_MODE_CUSTOM = 0,    /* nothing is updated for you */
    GRAPHIC_CAMERA_MODE_FREE,
    GRAPHIC_CAMERA_MODE_ORBITAL,
    GRAPHIC_CAMERA_MODE_FIRST_PERSON,
    GRAPHIC_CAMERA_MODE_THIRD_PERSON,
};

/* ------------------------------------------------------------------ */
/* cameras and projection                (src/graphic_camera.c)       */
/* ------------------------------------------------------------------ */

mat4_t graphic_camera_matrix_get(graphic_camera3d_t camera);
mat4_t graphic_camera2d_matrix_get(graphic_camera2d_t camera);

/*
 * The _ex variants take the viewport size explicitly, for rendering
 * into something other than the window.
 */
ray_t graphic_screen_to_world_ray_get(vec2_t             position, 
                                      graphic_camera3d_t camera);

ray_t graphic_screen_to_world_ray_get_ex(vec2_t             position,
                                         graphic_camera3d_t camera,
                                         int                width,
                                         int                height);

vec2_t graphic_world_to_screen_get(vec3_t             position,
                                   graphic_camera3d_t camera);

vec2_t graphic_world_to_screen_get_ex(vec3_t             position,
                                      graphic_camera3d_t camera,
                                      int                width,
                                      int                height);

vec2_t graphic_world_to_screen2d_get(vec2_t             position,
                                     graphic_camera2d_t camera);

vec2_t graphic_screen_to_world2d_get(vec2_t             position,
                                     graphic_camera2d_t camera);

void graphic_camera_update(graphic_camera3d_t *camera,
                           int                 mode);

void graphic_camera_update_ex(graphic_camera3d_t *camera,
                              vec3_t              movement,
                              vec3_t              rotation,
                              float               zoom);

float graphic_frame_time_get(void);     /* seconds, last frame */
int graphic_fps_get(void);
void graphic_fps_target_set(int fps);   /* 0 disables limiting */

#endif /* GRAPHIC_CAMERA_H */