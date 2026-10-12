#ifndef GRAPHIC_INTERNAL_H
#define GRAPHIC_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "graphic.h"

#define GRAPHIC_BATCH_VERTICES_DEFAULT    (8 * 1024)
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

#endif /* GRAPHIC_INTERNAL_H */