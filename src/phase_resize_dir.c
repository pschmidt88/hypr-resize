#include "consts.h"
#include "phase.h"
#include "resize_params.h"
#include "state.h"
#include "utils_cairo.h"

#include <cairo.h>
#include <string.h>

static const char *direction_to_label[] = {
    [RESIZE_VERTICAL] = "u", [RESIZE_HORIZONTAL] = "e", [RESIZE_LEFT] = "h",
    [RESIZE_RIGHT] = "l",    [RESIZE_TOP] = "k",        [RESIZE_BOTTOM] = "j"
};

static void
render_edge_direction_option(cairo_t *cairo, int x, int y, const char *label) {
    cairo_arc(cairo, x + .5, y + .5, DIRECTION_OPTION_RADIUS, 0, 2 * M_PI);
    cairo_set_source_u32(cairo, DIRECTION_OPTION_BG_COLOR);
    cairo_fill(cairo);

    cairo_arc(cairo, x + .5, y + .5, DIRECTION_OPTION_RADIUS, 0, 2 * M_PI);
    cairo_set_source_u32(cairo, DIRECTION_OPTION_BORDER_COLOR);
    cairo_set_line_width(cairo, 1);
    cairo_stroke(cairo);

    cairo_set_font_size(cairo, 15);
    cairo_text_extents_t te;
    cairo_text_extents(cairo, label, &te);
    cairo_move_to(
        cairo, x - te.width / 2. - te.x_bearing + .5,
        y - te.height / 2. - te.y_bearing + .5
    );
    cairo_set_source_u32(cairo, DIRECTION_OPTION_LABEL_COLOR);
    cairo_show_text(cairo, label);
}

static void phase_resize_dir_render(struct state *state, cairo_t *cairo) {
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

    if (state->focused_window.resize_left) {
        render_edge_direction_option(
            cairo, state->focused_window.rect.x,
            state->focused_window.rect.y + state->focused_window.rect.h / 2,
            direction_to_label[RESIZE_LEFT]
        );
    }
    if (state->focused_window.resize_right) {
        render_edge_direction_option(
            cairo, state->focused_window.rect.x + state->focused_window.rect.w,
            state->focused_window.rect.y + state->focused_window.rect.h / 2,
            direction_to_label[RESIZE_RIGHT]
        );
    }

    if (state->focused_window.resize_top) {
        render_edge_direction_option(
            cairo,
            state->focused_window.rect.x + state->focused_window.rect.w / 2,
            state->focused_window.rect.y, direction_to_label[RESIZE_TOP]
        );
    }
    if (state->focused_window.resize_bottom) {
        render_edge_direction_option(
            cairo,
            state->focused_window.rect.x + state->focused_window.rect.w / 2,
            state->focused_window.rect.y + state->focused_window.rect.h,
            direction_to_label[RESIZE_BOTTOM]
        );
    }

    bool both_hv_directions = state->focused_window.resize_left &&
                              state->focused_window.resize_right &&
                              state->focused_window.resize_top &&
                              state->focused_window.resize_bottom;

    if (state->focused_window.resize_left &&
        state->focused_window.resize_right) {
        int x = state->focused_window.rect.x +
                state->focused_window.rect.w / 2. -
                DIRECTION_OPTION_RADIUS * 2.5 * both_hv_directions;
        int y = state->focused_window.rect.y + state->focused_window.rect.h / 2;
        render_edge_direction_option(
            cairo, x, y, direction_to_label[RESIZE_HORIZONTAL]
        );

        cairo_set_source_u32(cairo, DIRECTION_OPTION_BORDER_COLOR);
        cairo_set_line_join(cairo, CAIRO_LINE_JOIN_ROUND);
        cairo_set_line_width(cairo, 1);
        cairo_move_to(
            cairo, x + DIRECTION_OPTION_RADIUS * 1.5 + .5,
            y - DIRECTION_OPTION_RADIUS / 2. + .5
        );
        cairo_line_to(cairo, x + DIRECTION_OPTION_RADIUS * 2 + .5, y + .5);
        cairo_line_to(
            cairo, x + DIRECTION_OPTION_RADIUS * 1.5 + .5,
            y + DIRECTION_OPTION_RADIUS / 2. + .5
        );
        cairo_stroke(cairo);

        cairo_move_to(
            cairo, x - DIRECTION_OPTION_RADIUS * 1.5 + .5,
            y - DIRECTION_OPTION_RADIUS / 2. + .5
        );
        cairo_line_to(cairo, x - DIRECTION_OPTION_RADIUS * 2 + .5, y + .5);
        cairo_line_to(
            cairo, x - DIRECTION_OPTION_RADIUS * 1.5 + .5,
            y + DIRECTION_OPTION_RADIUS / 2. + .5
        );
        cairo_stroke(cairo);
    }

    if (state->focused_window.resize_bottom &&
        state->focused_window.resize_top) {
        int x = state->focused_window.rect.x +
                state->focused_window.rect.w / 2. +
                DIRECTION_OPTION_RADIUS * 2.5 * both_hv_directions;
        int y = state->focused_window.rect.y + state->focused_window.rect.h / 2;
        render_edge_direction_option(
            cairo, x, y, direction_to_label[RESIZE_VERTICAL]
        );

        cairo_set_source_u32(cairo, DIRECTION_OPTION_BORDER_COLOR);
        cairo_set_line_join(cairo, CAIRO_LINE_JOIN_ROUND);
        cairo_set_line_width(cairo, 1);
        cairo_move_to(
            cairo, x + DIRECTION_OPTION_RADIUS / 2. + .5,
            y + DIRECTION_OPTION_RADIUS * 1.5 + .5
        );
        cairo_line_to(cairo, x, y + DIRECTION_OPTION_RADIUS * 2 + .5);
        cairo_line_to(
            cairo, x - DIRECTION_OPTION_RADIUS / 2. + .5,
            y + DIRECTION_OPTION_RADIUS * 1.5 + .5
        );
        cairo_stroke(cairo);

        cairo_move_to(
            cairo, x + DIRECTION_OPTION_RADIUS / 2. + .5,
            y - DIRECTION_OPTION_RADIUS * 1.5 + .5
        );
        cairo_line_to(cairo, x, y - DIRECTION_OPTION_RADIUS * 2 + .5);
        cairo_line_to(
            cairo, x - DIRECTION_OPTION_RADIUS / 2. + .5,
            y - DIRECTION_OPTION_RADIUS * 1.5 + .5
        );
        cairo_stroke(cairo);
    }
}

static bool
phase_resize_dir_key(struct state *state, xkb_keysym_t keysym, char *text) {
    for (int direction = 0; direction < NUM_DIRECTIONS; direction++) {
        if (strcmp(direction_to_label[direction], text) == 0 &&
            is_resize_direction_possible(
                direction, state->focused_window.resize_left,
                state->focused_window.resize_right,
                state->focused_window.resize_top,
                state->focused_window.resize_bottom
            )) {
            state->resize_direction = direction;
            phase_resize_enter(state, direction);
            return true;
        }
    }

    return false;
}

struct phase_interface phase_resize_dir_interface = {
    .render = phase_resize_dir_render,
    .key    = phase_resize_dir_key,
};
