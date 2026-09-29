#ifndef GRAPHIC_BACKEND_H
#define GRAPHIC_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

#include "graphic_internal.h"

bool graphic_backend_init(int width, int height, const char *title, uint32_t flags);
void graphic_backend_shutdown(void);

#endif /* GRAPHIC_BACKEND_H */