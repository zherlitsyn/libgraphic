#ifndef GRAPHIC_CAMERA_H
#define GRAPHIC_CAMERA_H

/*
 * Cameras, and the projection maths that goes with them.
 *
 * These live in their own header because the drawing code needs the
 * camera type (billboards face it) while the camera code needs
 * nothing from drawing: putting the types in graphic.h made the two
 * include each other.
 */

float graphic_frame_time_get(void);     /* seconds, last frame */
int graphic_fps_get(void);
void graphic_fps_target_set(int fps);   /* 0 disables limiting */

#endif /* GRAPHIC_CAMERA_H */