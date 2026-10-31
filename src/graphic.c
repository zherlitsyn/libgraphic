#include <string.h>

#include "graphic_internal.h"

/*
 * Immediate mode
 *
 * The model matrix is applied to every vertex on the CPU as it is
 * written. Only the view projection matrix reaches the GPU, as a push
 * constant. That is what lets thousands of independently transformed
 * objects collapse into a single draw call.
 */

struct graphic_batch_state {
    /* model matrix stack */
    mat4_t                   stack[GRAPHIC_MAX_MATRIX_STACK];
    int                      stack_counter;

    mat4_t                   model;
    bool                     model_required; /* false while model is identity */

    /* camera */
    mat4_t                   view;
    mat4_t                   projection;
    mat4_t                   view_projection;
    uint32_t                 view_projection_generation;
    bool                     depth_test;
    int blend_mode;

    bool                     scissor;
    int                      scissor_x;
    int                      scissor_y;
    int                      scissor_width;
    int                      scissor_height;

    int                      matrix_mode;

    /* sticky vertex attributes */
    uint8_t                  color[4];
    float                    normal[3];
    float                    texcoord[2];

    /* vertex storage: points straight into host visible GPU memory */
    struct graphic_vertex   *vertices;
    uint32_t                 vertex_capacity;
    uint32_t                 vertex_count;
    uint32_t                 vertex_required;

    /* recorded commands */
    struct graphic_draw_call draw_calls[GRAPHIC_MAX_DRAW_CALLS];
    uint32_t                 draw_call_count;
    uint32_t                 draw_call_generation;

    bool                     inside_begin;
    int                      mode;
};

static struct graphic_batch_state batch;

/* ------------------------------------------------------------------ */
/* internal helpers                                                    */
/* ------------------------------------------------------------------ */

static mat4_t *matrix_current_get(void)
{
    if (batch.matrix_mode == GRAPHIC_MATRIX_PROJECTION)
        return &batch.projection;

    return &batch.model;
}

static void view_projection_update(void)
{
    batch.view_projection = mat4_multiply_get(batch.projection,
                                              batch.view);
    batch.view_projection_generation++;
}

static struct graphic_draw_call *draw_call_open(int mode)
{
    struct graphic_draw_call *call;

    /*
     * Extend the previous command instead of opening a new one when
     * nothing that the GPU cares about has changed. This is what
     * merges many graphic_begin/graphic_end pairs into one vkCmdDraw.
     */
    if (batch.draw_call_count > 0) {
        call = &batch.draw_calls[batch.draw_call_count - 1];

        if (call->mode                 == mode                 &&
            call->depth_test           == batch.depth_test     &&
            call->blend_mode           == batch.blend_mode     &&
            call->scissor              == batch.scissor        &&
            call->scissor_x            == batch.scissor_x      &&
            call->scissor_y            == batch.scissor_y      &&
            call->scissor_width        == batch.scissor_width  &&
            call->scissor_height       == batch.scissor_height &&
            batch.draw_call_generation == batch.view_projection_generation)
            return call;
    }

    if (batch.draw_call_count >= GRAPHIC_MAX_DRAW_CALLS)
        return NULL;

    call                  = &batch.draw_calls[batch.draw_call_count++];
    call->mode            = mode;
    call->vertex_offset   = batch.vertex_count;
    call->vertex_count    = 0;
    call->index_count     = 0;
    call->depth_test      = batch.depth_test;
    call->blend_mode      = batch.blend_mode;
    call->scissor         = batch.scissor;
    call->scissor_x       = batch.scissor_x;
    call->scissor_y       = batch.scissor_y;
    call->scissor_width   = batch.scissor_width;
    call->scissor_height  = batch.scissor_height;
    call->view_projection = batch.view_projection;

    batch.draw_call_generation = batch.view_projection_generation;

    return call;
}

/* ------------------------------------------------------------------ */
/* frame control (called from graphic_core.c)                         */
/* ------------------------------------------------------------------ */

void graphic_batch_frame_begin(struct graphic_vertex *mapped, uint32_t capacity)
{
    batch.vertices        = mapped;
    batch.vertex_capacity = capacity;
    batch.vertex_count    = 0;
    batch.vertex_required = 0;
    batch.draw_call_count = 0;
    batch.inside_begin    = false;

    batch.stack_counter  = 0;
    batch.model          = mat4_identity_get();
    batch.model_required = false;
    batch.matrix_mode    = GRAPHIC_MATRIX_MODELVIEW;

    batch.color[0] = 255;
    batch.color[1] = 255;
    batch.color[2] = 255;
    batch.color[3] = 255;

    batch.normal[0] = 0.0f;
    batch.normal[1] = 0.0f;
    batch.normal[2] = 1.0f;

    batch.texcoord[0] = 0.0f;
    batch.texcoord[1] = 0.0f;

    batch.blend_mode = GRAPHIC_BLEND_ALPHA;
    batch.scissor    = false;
}

void graphic_batch_frame_end(void)
{
    if (batch.inside_begin)
        graphic_end();

    graphic_backend_draw_calls_record(batch.draw_calls,
                                      batch.draw_call_count);

    batch.vertices = NULL;
}

void graphic_batch_view_set(mat4_t view)
{
    batch.view = view;
    view_projection_update();
}

void graphic_batch_projection_set(mat4_t projection)
{
    batch.projection = projection;
    view_projection_update();
}

void graphic_batch_depth_test_set(bool enabled)
{
    batch.depth_test = enabled;
}

mat4_t graphic_batch_view_projection_get(void)
{
    return batch.view_projection;
}

bool graphic_batch_depth_test_get(void)
{
    return batch.depth_test;
}

/* ------------------------------------------------------------------ */
/* render state                                                        */
/*                                                                     */
/* None of this flushes anything. A state change only closes the open  */
/* draw call, so the vertices keep piling into the same buffer and the */
/* whole frame is still one submit.                                    */
/* ------------------------------------------------------------------ */

void graphic_mode_blend_begin(int mode)
{
    if (mode < 0 || mode >= GRAPHIC_BLEND_MODE_COUNT)
        return;

    if (batch.inside_begin)
        graphic_end();

    /* compile now, outside command recording */
    graphic_backend_blend_mode_prepare(mode);

    batch.blend_mode = mode;
}

void graphic_mode_blend_end(void)
{
    if (batch.inside_begin)
        graphic_end();

    batch.blend_mode = GRAPHIC_BLEND_ALPHA;
}

/*
 * Coordinates are in pixels with the origin at the top left, matching
 * the framebuffer, so no Y flip happens here even though the viewport
 * is flipped.
 */
void graphic_mode_scissor_begin(int x, int y, int width, int height)
{
    if (batch.inside_begin)
        graphic_end();

    batch.scissor        = true;
    batch.scissor_x      = x;
    batch.scissor_y      = y;
    batch.scissor_width  = (width > 0) ? width : 0;
    batch.scissor_height = (height > 0) ? height : 0;
}

void graphic_mode_scissor_end(void)
{
    if (batch.inside_begin)
        graphic_end();

    batch.scissor = false;
}

/*
 * Number of vertices the last frame actually wanted. The backend uses
 * it to grow the per frame buffer between frames instead of stalling
 * the pipeline in the middle of one.
 */
uint32_t graphic_batch_capacity_required_get(void)
{
    return batch.vertex_required;
}

/* ------------------------------------------------------------------ */
/* matrix stack                                                        */
/* ------------------------------------------------------------------ */

void graphic_matrix_mode(int mode)
{
    batch.matrix_mode = mode;
}

void graphic_matrix_push(void)
{
    if (batch.stack_counter >= GRAPHIC_MAX_MATRIX_STACK)
        return;

    batch.stack[batch.stack_counter++] = batch.model;
}

void graphic_matrix_pop(void)
{
    if (batch.stack_counter <= 0)
        return;

    batch.model = batch.stack[--batch.stack_counter];
}

void graphic_matrix_identity(void)
{
    *matrix_current_get() = mat4_identity_get();

    if (batch.matrix_mode == GRAPHIC_MATRIX_PROJECTION)
        view_projection_update();
    else if (batch.stack_counter == 0)
        batch.model_required = false;
}

void graphic_matrix_load(mat4_t matrix)
{
    *matrix_current_get() = matrix;

    if (batch.matrix_mode == GRAPHIC_MATRIX_PROJECTION)
        view_projection_update();
    else
        batch.model_required = true;
}

void graphic_matrix_multiply(mat4_t matrix)
{
    mat4_t *current = matrix_current_get();

    /* post multiply: new transforms act in the local frame */
    *current = mat4_multiply_get(*current, matrix);

    if (batch.matrix_mode == GRAPHIC_MATRIX_PROJECTION)
        view_projection_update();
    else
        batch.model_required = true;
}

void graphic_translatef(float x,
                        float y,
                        float z)
{
    graphic_matrix_multiply(mat4_translate_get(x, y, z));
}

void graphic_rotatef(float angle,
                     float x,
                     float y,
                     float z)
{
    vec3_t axis = { x, y, z };
    graphic_matrix_multiply(mat4_rotate_get(axis, angle * DEG2RAD));
}

void graphic_scalef(float x,
                    float y,
                    float z)
{
    graphic_matrix_multiply(mat4_scale_get(x, y, z));
}

/* ------------------------------------------------------------------ */
/* immediate mode geometry                                             */
/* ------------------------------------------------------------------ */

void graphic_begin(int mode)
{
    if (batch.inside_begin)
        graphic_end();

    batch.mode = mode;
    batch.inside_begin = true;

    draw_call_open(mode);
}

void graphic_end(void)
{
    if (!batch.inside_begin)
        return;

    batch.inside_begin = false;

    if (batch.draw_call_count == 0)
        return;

    struct graphic_draw_call *call;
    call = &batch.draw_calls[batch.draw_call_count - 1];

    uint32_t vertices_per_primitive;
    switch (batch.mode) {
        case GRAPHIC_DRAW_LINES:
            vertices_per_primitive = 2;
            break;
        case GRAPHIC_DRAW_QUADS:
            vertices_per_primitive = 4;
            break;
        default:
            vertices_per_primitive = 3;
            break;
    }

    /* drop an unfinished primitive rather than feeding garbage in */
    uint32_t remainder = call->vertex_count % vertices_per_primitive;
    if (remainder != 0) {
        call->vertex_count -= remainder;
        batch.vertex_count -= remainder;
    }

    if (batch.mode == GRAPHIC_DRAW_QUADS)
        call->index_count = call->vertex_count / 4 * 6;
}

void graphic_color4ub(uint8_t r,
                      uint8_t g,
                      uint8_t b,
                      uint8_t a)
{
    batch.color[0] = r;
    batch.color[1] = g;
    batch.color[2] = b;
    batch.color[3] = a;
}

void graphic_color4f(float r,
                     float g,
                     float b,
                     float a)
{
    graphic_color4ub((uint8_t)(r * 255.0f),
                     (uint8_t)(g * 255.0f),
                     (uint8_t)(b * 255.0f),
                     (uint8_t)(a * 255.0f));
}

void graphic_normal3f(float x,
                      float y,
                      float z)
{
    batch.normal[0] = x;
    batch.normal[1] = y;
    batch.normal[2] = z;
}

void graphic_texcoord2f(float u,
                        float v)
{
    batch.texcoord[0] = u;
    batch.texcoord[1] = v;
}

void graphic_vertex3f(float x,
                      float y,
                      float z)
{
    batch.vertex_required++;

    if (!batch.inside_begin || batch.vertices == NULL)
        return;

    if (batch.draw_call_count == 0)
        return;

    /*
     * Overflow is not fatal: the excess is dropped for this frame
     * and the backend grows the buffer before the next one, using
     * graphic_batch_capacity_required_get().
     */
    if (batch.vertex_count >= batch.vertex_capacity)
        return;

    vec3_t position = { x, y, z };
    if (batch.model_required)
        position = vec3_transform(position, batch.model);

    struct graphic_vertex *vertex;
    vertex = &batch.vertices[batch.vertex_count++];

    vertex->position[0] = position.x;
    vertex->position[1] = position.y;
    vertex->position[2] = position.z;

    vertex->normal[0]   = batch.normal[0];
    vertex->normal[1]   = batch.normal[1];
    vertex->normal[2]   = batch.normal[2];

    vertex->color[0]    = batch.color[0];
    vertex->color[1]    = batch.color[1];
    vertex->color[2]    = batch.color[2];
    vertex->color[3]    = batch.color[3];

    vertex->texcoord[0] = batch.texcoord[0];
    vertex->texcoord[1] = batch.texcoord[1];

    batch.draw_calls[batch.draw_call_count - 1].vertex_count++;
}

void graphic_vertex2f(float x,
                      float y)
{
    graphic_vertex3f(x, y, 0.0f);
}