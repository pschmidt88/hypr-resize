#include "consts.h"
#include "phase.h"
#include "resize_params.h"
#include "utils.h"
#include "utils_cairo.h"

#include <cairo.h>

void phase_resize_enter(struct state *state, enum resize_direction direction) {
    state->phase = &phase_resize_interface;
    struct resize_guides_builder_ctx ctx;
    resize_guides_builder_init(
        &ctx, direction, &state->focused_window, true, false
    );

    static const int values[] = {-800, -600, -400, -200, -100, -50, -20, -10,
                                 10,   20,   50,   100,  200,  400, 600, 800};

    for (int i = 0; i < ARRAY_LEN(values); i++) {
        resize_guides_builder_add(&ctx, values[i], 'a' + i);
    }

    state->resize_params = resize_guides_builder_make(&ctx);
    log_resize_params(state->resize_params, resize_direction_to_str(direction));
}

static void _render_vertical_guide(
    cairo_t *cairo, uint32_t x, uint32_t start_y, uint32_t end_y, char *label
) {
    cairo_set_font_size(cairo, GUIDE_LABEL_FONT_SIZE);
    cairo_text_extents_t te;
    cairo_text_extents(cairo, label, &te);

    uint32_t label_x = x - te.x_advance / 2;
    uint32_t label_y =
        end_y + (start_y < end_y ? GUIDE_LABEL_FONT_SIZE + 3.5 : -5);

    static const double dashes[] = {1., 1.};
    cairo_set_dash(cairo, dashes, 2, 0);

    cairo_set_source_u32(cairo, GUIDE_LINE_COLOR);
    cairo_move_to(cairo, x + .5, start_y + .5);
    cairo_line_to(cairo, x + .5, end_y + .5);
    cairo_set_line_width(cairo, 1);
    cairo_stroke(cairo);

    cairo_set_dash(cairo, NULL, 0, 0);

    cairo_set_source_u32(cairo, GUIDE_LINE_COLOR);
    cairo_set_line_width(cairo, 2);
    cairo_move_to(cairo, x - 4.5, end_y);
    cairo_line_to(cairo, x + 5.5, end_y);
    cairo_stroke(cairo);

    cairo_set_source_u32(cairo, GUIDE_LABEL_COLOR);
    cairo_move_to(cairo, label_x, label_y);
    cairo_show_text(cairo, label);
}

static void _render_horizontal_guide(
    cairo_t *cairo, uint32_t y, uint32_t start_x, uint32_t end_x, char *label
) {
    cairo_set_font_size(cairo, GUIDE_LABEL_FONT_SIZE);
    cairo_text_extents_t te;
    cairo_text_extents(cairo, label, &te);

    uint32_t label_x = end_x + (start_x < end_x ? 3.5 : -te.x_advance - 3.5);
    uint32_t label_y = y + te.height / 2;

    static const double dashes[] = {1., 1.};
    cairo_set_dash(cairo, dashes, 2, 0);

    cairo_set_source_u32(cairo, GUIDE_LINE_COLOR);
    cairo_move_to(cairo, start_x + .5, y + .5);
    cairo_line_to(cairo, end_x + .5, y + .5);
    cairo_set_line_width(cairo, 1);
    cairo_stroke(cairo);

    cairo_set_dash(cairo, NULL, 0, 0);

    cairo_set_source_u32(cairo, GUIDE_LINE_COLOR);
    cairo_set_line_width(cairo, 2);
    cairo_move_to(cairo, end_x + .5, y - 4.5);
    cairo_line_to(cairo, end_x + .5, y + 5.5);
    cairo_stroke(cairo);

    cairo_set_source_u32(cairo, GUIDE_LABEL_COLOR);
    cairo_move_to(cairo, label_x, label_y);
    cairo_show_text(cairo, label);
}

#define GUIDE_PADDING 10

static void _render_guides(
    cairo_t *cairo, struct resize_parameter *params, size_t num_params,
    struct focused_window *focused_window, enum resize_direction direction
) {
    int      y = 0;
    uint32_t start_pos =
        direction == RESIZE_VERTICAL || direction == RESIZE_BOTTOM ||
                direction == RESIZE_TOP
            ? focused_window->rect.x +
                  (focused_window->rect.w - GUIDE_PADDING * num_params) / 2
            : focused_window->rect.y +
                  (focused_window->rect.h - GUIDE_PADDING * num_params) / 2;

    for (int i = 0; i < num_params; i++) {
        struct resize_parameter *param = &params[i];

        char label[5];
        rune_to_str(param->symbol, label);

        uint32_t pos = start_pos + GUIDE_PADDING * (y + 1);
        switch (direction) {
        case RESIZE_TOP:
        case RESIZE_BOTTOM:
        case RESIZE_VERTICAL:
            if (param->guides[0] != NO_GUIDE) {
                _render_vertical_guide(
                    cairo, pos, focused_window->rect.y, param->guides[0], label
                );
            }
            if (param->guides[1] != NO_GUIDE) {
                _render_vertical_guide(
                    cairo, pos,
                    focused_window->rect.y + focused_window->rect.h - 1,
                    param->guides[1], label
                );
            }
            break;
        case RESIZE_LEFT:
        case RESIZE_RIGHT:
        case RESIZE_HORIZONTAL:
            if (param->guides[0] != NO_GUIDE) {
                _render_horizontal_guide(
                    cairo, pos, focused_window->rect.x, param->guides[0], label
                );
            }
            if (param->guides[1] != NO_GUIDE) {
                _render_horizontal_guide(
                    cairo, pos,
                    focused_window->rect.x + focused_window->rect.w - 1,
                    param->guides[1], label
                );
            }
        }
        y++;
    }
}

static void _get_guide_min_max(
    struct resize_parameters *params, int32_t *guide_min, int32_t *guide_max
) {
    *guide_min = params->params[0].guides[0];
    *guide_max = params->params[0].guides[0];

    for (int i = 1; i < params->count; i++) {
        int32_t guide = params->params[i].guides[0];

        if (*guide_max == NO_GUIDE ||
            (guide != NO_GUIDE && guide > *guide_max)) {
            *guide_max = guide;
        }

        if (*guide_min == NO_GUIDE ||
            (guide != NO_GUIDE && guide < *guide_min)) {
            *guide_min = guide;
        }
    }
}

static void _render_horizontal_guide_bar(
    struct resize_parameters *params, cairo_t *cairo, struct focused_window *fw
) {
    if (params->count == 0) {
        return;
    }

    int32_t guide_min = NO_GUIDE;
    int32_t guide_max = NO_GUIDE;
    _get_guide_min_max(params, &guide_min, &guide_max);

    int32_t y = fw->rect.y + fw->rect.h / 2;

    cairo_select_font_face(
        cairo, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL
    );
    cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
    cairo_set_font_size(cairo, GUIDE_LABEL_FONT_SIZE);

    static const double dashes[] = {1., 1.};
    cairo_set_dash(cairo, dashes, 2, 0);

    cairo_set_source_u32(cairo, GUIDE_LINE_COLOR);
    cairo_move_to(cairo, guide_min + .5, y + .5);
    cairo_line_to(cairo, guide_max + .5, y + .5);
    cairo_set_line_width(cairo, 1);
    cairo_stroke(cairo);

    cairo_set_dash(cairo, NULL, 0, 0);

    for (int i = 0; i < params->count; i++) {
        struct resize_parameter *param = &params->params[i];

        cairo_set_source_u32(cairo, GUIDE_LINE_COLOR);
        cairo_set_line_width(cairo, 2);
        cairo_move_to(cairo, param->guides[0] + .5, y - 4.5);
        cairo_line_to(cairo, param->guides[0] + .5, y + 5.5);
        cairo_stroke(cairo);

        cairo_set_source_u32(cairo, GUIDE_LINE_COLOR);
        char label[64];
        rune_to_str(param->symbol, label);

        cairo_text_extents_t te;
        cairo_text_extents(cairo, label, &te);
        cairo_set_source_u32(cairo, GUIDE_LABEL_COLOR);
        cairo_move_to(
            cairo, param->guides[0] - (te.width / 2. + te.x_bearing) + .5,
            y - 10.5
        );
        cairo_show_text(cairo, label);
    }
}

static void _render_vertical_guide_bar(
    struct resize_parameters *params, cairo_t *cairo, struct focused_window *fw
) {
    if (params->count == 0) {
        return;
    }

    int32_t guide_min = NO_GUIDE;
    int32_t guide_max = NO_GUIDE;
    _get_guide_min_max(params, &guide_min, &guide_max);

    int32_t x = fw->rect.x + fw->rect.w / 2;

    cairo_select_font_face(
        cairo, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL
    );
    cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
    cairo_set_font_size(cairo, GUIDE_LABEL_FONT_SIZE);

    static const double dashes[] = {1., 1.};
    cairo_set_dash(cairo, dashes, 2, 0);

    cairo_set_source_u32(cairo, GUIDE_LINE_COLOR);
    cairo_move_to(cairo, x + .5, guide_min + .5);
    cairo_line_to(cairo, x + .5, guide_max + .5);
    cairo_set_line_width(cairo, 1);
    cairo_stroke(cairo);

    cairo_set_dash(cairo, NULL, 0, 0);

    for (int i = 0; i < params->count; i++) {
        struct resize_parameter *param = &params->params[i];

        cairo_set_source_u32(cairo, GUIDE_LINE_COLOR);
        cairo_set_line_width(cairo, 2);
        cairo_move_to(cairo, x - 4.5, param->guides[0] + .5);
        cairo_line_to(cairo, x + 5.5, param->guides[0] + .5);
        cairo_stroke(cairo);

        cairo_set_source_u32(cairo, GUIDE_LINE_COLOR);
        char label[64];
        rune_to_str(param->symbol, label);

        cairo_text_extents_t te;
        cairo_text_extents(cairo, label, &te);
        cairo_set_source_u32(cairo, GUIDE_LABEL_COLOR);
        cairo_move_to(
            cairo, x + 10.5,
            param->guides[0] - (te.height / 2. + te.y_bearing) + .5
        );
        cairo_show_text(cairo, label);
    }
}

static void phase_resize_render(struct state *state, cairo_t *cairo) {
    cairo_select_font_face(
        cairo, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL
    );

    cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_u32(cairo, BG_COLOR);
    cairo_paint(cairo);

    cairo_rectangle(
        cairo, state->focused_window.rect.x + .5,
        state->focused_window.rect.y + .5, state->focused_window.rect.w,
        state->focused_window.rect.h
    );
    cairo_set_source_u32(cairo, WIN_BG_COLOR);
    cairo_fill(cairo);

    switch (state->resize_params->direction) {
    case RESIZE_LEFT:
    case RESIZE_RIGHT:
        _render_horizontal_guide_bar(
            state->resize_params, cairo, &state->focused_window
        );
        break;

    case RESIZE_TOP:
    case RESIZE_BOTTOM:
        _render_vertical_guide_bar(
            state->resize_params, cairo, &state->focused_window
        );
        break;

    case RESIZE_HORIZONTAL:
    case RESIZE_VERTICAL:
        _render_guides(
            cairo, state->resize_params->params, state->resize_params->count,
            &state->focused_window, state->resize_params->direction
        );
        break;
    }
}

static bool
phase_resize_key(struct state *state, xkb_keysym_t keysym, char *text) {
    if (keysym == XKB_KEY_BackSpace) {
        resize_parameters_free(state->resize_params);
        state->resize_params = NULL;
        state->phase         = &phase_resize_dir_interface;
        return true;
    }

    if (text[0] == '\0') {
        return false;
    }

    uint32_t rune;
    str_to_rune(text, &rune);

    struct resize_parameter *resize_param =
        find_resize_param_by_symbol(state->resize_params, rune);
    if (resize_param != NULL) {
        state->selected_resize = resize_param;
        state->running         = false;
        return true;
    }

    return false;
}

struct phase_interface phase_resize_interface = {
    .render = phase_resize_render,
    .key    = phase_resize_key,
};
