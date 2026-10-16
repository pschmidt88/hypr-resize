#ifndef __HYPR_WIN_H_INCLUDED__
#define __HYPR_WIN_H_INCLUDED__

#include "utils.h"

#include <stdbool.h>
#include <stdint.h>

struct focused_window {
    char        address[32];
    const char *output;
    struct rect rect;
    struct rect output_rect;
    int32_t     resize_top_limit;
    int32_t     resize_bottom_limit;
    int32_t     resize_left_limit;
    int32_t     resize_right_limit;
    bool        resize_top;
    bool        resize_bottom;
    bool        resize_left;
    bool        resize_right;
    bool        floating;
};

// Query the focused window from Hyprland and find out which of its edges can
// be moved. Coordinates are relative to the window's monitor.
int  find_focused_window(struct focused_window *fw);
void log_focused_window(struct focused_window *fw);

// Resize the window by the given amount of pixels. `dx` and `dy` are the
// distances the movable edges are moved by (positive is right/down).
int hypr_send_resize(const char *address, int32_t dx, int32_t dy);

#endif
