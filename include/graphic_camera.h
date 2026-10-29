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

float graphic_frame_time_get(void);     /* seconds, last frame */
int graphic_fps_get(void);
void graphic_fps_target_set(int fps);   /* 0 disables limiting */

#endif /* GRAPHIC_CAMERA_H */