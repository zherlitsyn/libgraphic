#ifndef GRAPHIC_INTERNAL_H
#define GRAPHIC_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "graphic.h"

#define GRAPHIC_MAX_MATRIX_STACK          32
#define GRAPHIC_BATCH_VERTICES_DEFAULT    (8 * 1024)
#define GRAPHIC_MAX_DRAW_CALLS            256
#define GRAPHIC_FRAMES_IN_FLIGHT          2

/*
 * 36 bytes. The layout must stay in sync with the vertex input
 * description built in graphic_pipeline.c.
 */
struct graphic_vertex {
    float   position[3];
    float   normal[3];
    uint8_t color[4];
    float   texcoord[2];
};

/*
 * One recorded draw command. The view projection matrix is stored per
 * draw call: since it travels as a push constant, the camera can change
 * mid frame without ever flushing the vertex buffer.
 */
struct graphic_draw_call {
    int      mode;           /* enum graphic_draw_mode */
    uint32_t vertex_offset;
    uint32_t vertex_count;
    uint32_t index_count;    /* non zero only for GRAPHIC_DRAW_QUADS */
    bool     depth_test;
    mat4_t   view_projection;
};

/* implemented in graphic.c, driven by graphic_core.c */
void graphic_batch_frame_begin(struct graphic_vertex *mapped,
                               uint32_t               capacity);

void graphic_batch_frame_end(void);

void graphic_batch_view_set(mat4_t view);

void graphic_batch_projection_set(mat4_t projection);

void graphic_batch_depth_test_set(bool enabled);

mat4_t graphic_batch_view_projection_get(void);

bool graphic_batch_depth_test_get(void);

uint32_t graphic_batch_capacity_required_get(void);

/* implemented in graphic_backend.c, called once per frame from graphic.c */
void graphic_backend_draw_calls_record(const struct graphic_draw_call *calls,
                                       uint32_t                        count);

#endif /* GRAPHIC_INTERNAL_H */