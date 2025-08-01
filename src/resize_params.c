#include "resize_params.h"

#include "log.h"
#include "utils.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void resize_guides_builder_init(
    struct resize_guides_builder_ctx *ctx, enum resize_direction direction,
    struct focused_window *fw, bool relative, bool percentage
) {
    ctx->direction  = direction;
    ctx->relative   = relative;
    ctx->percentage = percentage;

    ctx->resize_params_cap   = 16;
    ctx->resize_params_count = 0;
    ctx->resize_params =
        malloc(ctx->resize_params_cap * sizeof(*ctx->resize_params));

    switch (direction) {
    case RESIZE_HORIZONTAL:
        ctx->min_limit         = fw->resize_left_limit;
        ctx->max_limit         = fw->resize_right_limit;
        ctx->resizable_min_dir = fw->resize_left;
        ctx->resizable_max_dir = fw->resize_right;
        ctx->size              = fw->rect.w;
        ctx->pos               = fw->rect.x;
        break;

    case RESIZE_LEFT:
        ctx->min_limit         = fw->resize_left_limit;
        ctx->max_limit         = fw->rect.x + fw->rect.w;
        ctx->resizable_min_dir = fw->resize_left;
        ctx->resizable_max_dir = false;
        ctx->size              = fw->rect.w;
        ctx->pos               = fw->rect.x;
        break;

    case RESIZE_RIGHT:
        ctx->min_limit         = fw->rect.x;
        ctx->max_limit         = fw->resize_right_limit;
        ctx->resizable_min_dir = false;
        ctx->resizable_max_dir = fw->resize_right;
        ctx->size              = fw->rect.w;
        ctx->pos               = fw->rect.x;
        break;

    case RESIZE_TOP:
        ctx->min_limit         = fw->resize_top_limit;
        ctx->max_limit         = fw->rect.y + fw->rect.h;
        ctx->resizable_min_dir = fw->resize_top;
        ctx->resizable_max_dir = false;
        ctx->size              = fw->rect.h;
        ctx->pos               = fw->rect.y;
        break;

    case RESIZE_BOTTOM:
        ctx->min_limit         = fw->rect.y;
        ctx->max_limit         = fw->resize_bottom_limit;
        ctx->resizable_min_dir = false;
        ctx->resizable_max_dir = fw->resize_bottom;
        ctx->size              = fw->rect.h;
        ctx->pos               = fw->rect.y;
        break;

    case RESIZE_VERTICAL:
        ctx->min_limit         = fw->resize_top_limit;
        ctx->max_limit         = fw->resize_bottom_limit;
        ctx->resizable_min_dir = fw->resize_top;
        ctx->resizable_max_dir = fw->resize_bottom;
        ctx->size              = fw->rect.h;
        ctx->pos               = fw->rect.y;
        break;
    }
}

static int _resize_guides_builder_compute(
    struct resize_guides_builder_ctx *ctx, struct resize_parameter *param
) {
    if (!ctx->resizable_min_dir && !ctx->resizable_max_dir) {
        return 0;
    }

    int32_t new_size;

    if (ctx->relative) {
        new_size = ctx->percentage ? (100 + param->value) * ctx->size / 100
                                   : ctx->size + param->value;
    } else {
        new_size = ctx->percentage
                       ? (ctx->max_limit - ctx->min_limit) * param->value / 100
                       : param->value;
    }

    if (abs(new_size - ctx->size) < 2) {
        return 0;
    }

    int32_t guide_pos[2];

    if (new_size < 15) {
        return 0;
    }

    if (!ctx->resizable_min_dir) {
        /*
            pos
            min_limit                    max_limit
            |                            |
        ----|--------|-------|-----------|------->
            ^________^       ^
               size          guide_pos[0]
            ^________________^
                 new_size

         */

        guide_pos[0] = new_size + ctx->pos;
        guide_pos[1] = NO_GUIDE;

        if (guide_pos[0] > ctx->max_limit) {
            return 0;
        }

    } else if (!ctx->resizable_max_dir) {
        /*
            min_limit                    max_limit
            |                pos         |
        ----|--------|-------|-----------|------->
                     .       ^___________^
                     .           size    .
                     ^___________________^
                            new_size
         */

        guide_pos[0] = ctx->pos + ctx->size - new_size;
        guide_pos[1] = NO_GUIDE;

        if (guide_pos[0] < ctx->min_limit) {
            return 0;
        }
    } else {
        /*
            min_limit                                           max_limit
            |                pos                                |
        ----|--------|-------|-----------|-------|--------------|------->
                     .       ^___________^       .
                     .           size            .
                     ^___________________________^
                               new_size
         */

        int32_t grow = (new_size - ctx->size) / 2;
        guide_pos[0] = ctx->pos - grow;

        if (guide_pos[0] < ctx->min_limit) {
            return 0;
        }

        guide_pos[1] = ctx->pos + ctx->size + grow;
        if (guide_pos[1] > ctx->max_limit) {
            return 0;
        }

        new_size = guide_pos[1] - guide_pos[0];
    }

    param->size      = new_size;
    param->guides[0] = guide_pos[0];
    param->guides[1] = guide_pos[1];

    return 1;
}

int resize_guides_builder_add(
    struct resize_guides_builder_ctx *ctx, int32_t value, uint32_t symbol
) {
    if (ctx->resize_params_count + 1 >= ctx->resize_params_cap) {
        ctx->resize_params_cap *= 1.5;
        ctx->resize_params      = realloc(
            ctx->resize_params,
            sizeof(*ctx->resize_params) * ctx->resize_params_cap
        );
    }

    struct resize_parameter *param =
        &ctx->resize_params[ctx->resize_params_count];
    param->value  = value;
    param->symbol = symbol;

    if (_resize_guides_builder_compute(ctx, param)) {
        ctx->resize_params_count++;
        return 1;
    }

    return 0;
}

struct resize_parameters *
resize_guides_builder_make(struct resize_guides_builder_ctx *ctx) {
    struct resize_parameters *params = malloc(sizeof(*params));
    params->relative                 = ctx->relative;
    params->percentage               = ctx->percentage;
    params->direction                = ctx->direction;
    params->count                    = ctx->resize_params_count;
    params->params                   = realloc(
        ctx->resize_params, ctx->resize_params_count * sizeof(*params->params)
    );

    ctx->resize_params_count = 0;
    ctx->resize_params_cap   = 0;
    ctx->resize_params       = NULL;

    return params;
}

void log_resize_params(struct resize_parameters *params, const char *name) {
    LOG_INFO("params[%s]", name);
    LOG_INFO(" .relative = %s", params->relative ? "true" : "false");
    LOG_INFO(" .percentage = %s", params->percentage ? "true" : "false");

    if (params->count == 0) {
        LOG_INFO(" .[]");
        return;
    }

    for (int i = 0; i < params->count; i++) {
        char symbol[5];
        rune_to_str(params->params[i].symbol, symbol);
        LOG_INFO(" .[%d]", i);
        LOG_INFO("  .symbol = %s (%d)", symbol, params->params[i].symbol);
        LOG_INFO("  .value = %d", params->params[i].value);
        LOG_INFO("  .size = %d", params->params[i].size);
        LOG_INFO("  .guides[0] = %d", params->params[i].guides[0]);
        LOG_INFO("  .guides[1] = %d", params->params[i].guides[1]);
    }
}

const char *resize_direction_to_str(enum resize_direction direction) {
    const static char *invalid_str = "RESIZE_DIRECTION_INVALID";
    if (direction < 0 || direction >= NUM_DIRECTIONS) {
        return invalid_str;
    }

#define DIR_STR(dir) [dir] = #dir

    const static char *direction_strs[NUM_DIRECTIONS] = {
        DIR_STR(RESIZE_TOP),        DIR_STR(RESIZE_BOTTOM),
        DIR_STR(RESIZE_LEFT),       DIR_STR(RESIZE_RIGHT),
        DIR_STR(RESIZE_HORIZONTAL), DIR_STR(RESIZE_VERTICAL),
    };

    return direction_strs[direction];
}

bool is_resize_direction_possible(
    enum resize_direction direction, bool left, bool right, bool top,
    bool bottom
) {

#define DIR_MASK(left, right, top, bottom) \
    ((left != 0) | (right != 0) << 1 | (top != 0) << 2 | (bottom != 0) << 3)

    const static uint8_t direction_to_mask[NUM_DIRECTIONS] = {
        [RESIZE_LEFT]       = DIR_MASK(1, 0, 0, 0),
        [RESIZE_RIGHT]      = DIR_MASK(0, 1, 0, 0),
        [RESIZE_TOP]        = DIR_MASK(0, 0, 1, 0),
        [RESIZE_BOTTOM]     = DIR_MASK(0, 0, 0, 1),
        [RESIZE_HORIZONTAL] = DIR_MASK(1, 1, 0, 0),
        [RESIZE_VERTICAL]   = DIR_MASK(0, 0, 1, 1),
    };

    return (DIR_MASK(left, right, top, bottom) &
            direction_to_mask[direction]) != 0;
}

int32_t resize_direction_rect_to_size(
    enum resize_direction direction, struct rect *rect
) {
    switch (direction) {
    case RESIZE_HORIZONTAL:
    case RESIZE_LEFT:
    case RESIZE_RIGHT:
        return rect->w;
    case RESIZE_VERTICAL:
    case RESIZE_TOP:
    case RESIZE_BOTTOM:
        return rect->h;
    }

    return 0;
}

struct resize_parameter *
find_resize_param_by_symbol(struct resize_parameters *params, uint32_t rune) {
    for (int i = 0; i < params->count; i++) {
        if (params->params[i].symbol == rune) {
            return &params->params[i];
        }
    }

    return NULL;
}

void resize_parameters_free(struct resize_parameters *params) {
    if (params->params != NULL) {
        free(params->params);
    }
    free(params);
}
