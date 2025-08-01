#ifndef __RESIZE_PARAMS_H_INCLUDED__
#define __RESIZE_PARAMS_H_INCLUDED__

#include "sway_win.h"

#include <stdbool.h>
#include <stdint.h>

enum resize_direction {
    RESIZE_LEFT       = 0,
    RESIZE_RIGHT      = 1,
    RESIZE_TOP        = 2,
    RESIZE_BOTTOM     = 3,
    RESIZE_VERTICAL   = 4,
    RESIZE_HORIZONTAL = 5,
};

#define NUM_DIRECTIONS 6
#define NO_GUIDE       -1

struct resize_parameter {
    int32_t  value;
    uint32_t symbol;
    int32_t  guides[2];
    uint32_t size;
};

struct resize_parameters {
    struct resize_parameter *params;
    size_t                   count;
    bool                     relative;
    bool                     percentage;
    enum resize_direction    direction;
};

struct resize_guides_builder_ctx {
    enum resize_direction direction;
    int32_t               min_limit;
    int32_t               max_limit;
    int32_t               resizable_min_dir;
    int32_t               resizable_max_dir;
    int32_t               size;
    int32_t               pos;
    bool                  relative;
    bool                  percentage;

    struct resize_parameter *resize_params;
    size_t                   resize_params_count;
    size_t                   resize_params_cap;
};

const char *resize_direction_to_str(enum resize_direction direction);
int32_t     resize_direction_rect_to_size(
        enum resize_direction direction, struct rect *rect
    );

bool is_resize_direction_possible(
    enum resize_direction direction, bool left, bool right, bool top,
    bool bottom
);

int resize_guides_builder_add(
    struct resize_guides_builder_ctx *ctx, int32_t value, uint32_t symbol
);
void resize_guides_builder_init(
    struct resize_guides_builder_ctx *ctx, enum resize_direction direction,
    struct focused_window *fw, bool relative, bool percentage
);

struct resize_parameters      *
resize_guides_builder_make(struct resize_guides_builder_ctx *ctx);
void resize_parameters_free(struct resize_parameters *params);

void log_resize_params(struct resize_parameters *params, const char *name);

struct resize_parameter *
find_resize_param_by_symbol(struct resize_parameters *params, uint32_t rune);

#endif
