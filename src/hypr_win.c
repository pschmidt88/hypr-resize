#include "hypr_win.h"

#include "hypr_ipc.h"
#include "log.h"

#include <jansson.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Distance in pixels used to find out which edges of the window move when it
// is resized. The probe is undone within the same batch request, so it is
// never rendered.
#define PROBE_DISTANCE 10

struct hypr_window {
    char        address[32];
    struct rect rect;
    int         monitor;
    bool        floating;
    int         fullscreen;
};

static int _get_int(json_t *node, const char *key, int *value) {
    json_t *field = json_object_get(node, key);
    if (!json_is_number(field)) {
        LOG_ERR("Object field '%s' not found or not a number.", key);
        return -1;
    }
    *value = (int)lround(json_number_value(field));
    return 0;
}

static int _get_int_pair(json_t *node, const char *key, int *a, int *b) {
    json_t *field = json_object_get(node, key);
    if (!json_is_array(field) || json_array_size(field) != 2) {
        LOG_ERR("Object field '%s' not found or not a pair.", key);
        return -1;
    }
    *a = (int)lround(json_number_value(json_array_get(field, 0)));
    *b = (int)lround(json_number_value(json_array_get(field, 1)));
    return 0;
}

static int _parse_window(const char *payload, struct hypr_window *win) {
    json_error_t error;
    json_t      *root = json_loads(payload, 0, &error);
    if (!json_is_object(root)) {
        LOG_ERR("Could not parse active window: %s", payload);
        json_decref(root);
        return -1;
    }

    json_t *address = json_object_get(root, "address");
    if (!json_is_string(address) || json_string_length(address) == 0) {
        LOG_ERR("No focused window.");
        json_decref(root);
        return -1;
    }
    snprintf(
        win->address, sizeof(win->address), "%s", json_string_value(address)
    );

    win->floating = json_is_true(json_object_get(root, "floating"));

    if (_get_int_pair(root, "at", &win->rect.x, &win->rect.y) ||
        _get_int_pair(root, "size", &win->rect.w, &win->rect.h) ||
        _get_int(root, "monitor", &win->monitor) ||
        _get_int(root, "fullscreen", &win->fullscreen)) {
        json_decref(root);
        return -1;
    }

    json_decref(root);
    return 0;
}

static void _format_resize_cmd(
    char *buf, size_t len, const char *address, int32_t dx, int32_t dy
) {
    snprintf(
        buf, len,
        "dispatch hl.dsp.window.resize({x=%d, y=%d, relative=true, "
        "window=\"address:%s\"})",
        dx, dy, address
    );
}

// Resize the window by `distance` along one axis, read back its geometry and
// undo the resize, all in a single batch request.
static int _probe(
    const char *address, bool vertical, int32_t distance,
    struct hypr_window *probed
) {
    char grow[256], shrink[256], request[640];
    _format_resize_cmd(
        grow, sizeof(grow), address, vertical ? 0 : distance,
        vertical ? distance : 0
    );
    _format_resize_cmd(
        shrink, sizeof(shrink), address, vertical ? 0 : -distance,
        vertical ? -distance : 0
    );
    snprintf(
        request, sizeof(request), "[[BATCH]]%s ; j/activewindow ; %s", grow,
        shrink
    );

    char *reply = hypr_ipc_request(request);
    if (reply == NULL) {
        return -1;
    }

    char  *parts[3];
    size_t count = hypr_ipc_split_batch(reply, parts, 3);
    int    err   = count == 3 ? _parse_window(parts[1], probed) : -1;
    if (count != 3) {
        LOG_ERR("Unexpected reply to probe request.");
    }

    free(reply);
    return err;
}

static bool _moved(int32_t a, int32_t b) {
    return abs(a - b) >= PROBE_DISTANCE / 2;
}

// Find out which edges move along one axis. The layout decides which edge
// moves (e.g. on dwindle, the split closest to the window), so we just try.
static int _probe_axis(
    const struct hypr_window *orig, bool vertical, bool *min_edge,
    bool *max_edge
) {
    *min_edge = false;
    *max_edge = false;

    // If the split is already at its limit, growing does nothing, so we also
    // try the other way.
    static const int32_t signs[] = {1, -1};
    for (int i = 0; i < 2; i++) {
        struct hypr_window probed;
        if (_probe(
                orig->address, vertical, signs[i] * PROBE_DISTANCE, &probed
            )) {
            return -1;
        }

        if (vertical) {
            *min_edge = _moved(probed.rect.y, orig->rect.y);
            *max_edge = _moved(
                probed.rect.y + probed.rect.h, orig->rect.y + orig->rect.h
            );
        } else {
            *min_edge = _moved(probed.rect.x, orig->rect.x);
            *max_edge = _moved(
                probed.rect.x + probed.rect.w, orig->rect.x + orig->rect.w
            );
        }

        if (*min_edge || *max_edge) {
            return 0;
        }
    }

    return 0;
}

static int _get_active_window(struct hypr_window *win) {
    char *reply = hypr_ipc_request("j/activewindow");
    if (reply == NULL) {
        return -1;
    }
    int err = _parse_window(reply, win);
    free(reply);
    return err;
}

static int _get_monitor(int id, struct focused_window *fw) {
    char *reply = hypr_ipc_request("j/monitors");
    if (reply == NULL) {
        return -1;
    }

    json_error_t error;
    json_t      *monitors = json_loads(reply, 0, &error);
    free(reply);
    if (!json_is_array(monitors)) {
        LOG_ERR("Could not parse monitors.");
        json_decref(monitors);
        return -1;
    }

    int     err = -1;
    size_t  i;
    json_t *monitor;
    json_array_foreach(monitors, i, monitor) {
        int monitor_id;
        if (_get_int(monitor, "id", &monitor_id) || monitor_id != id) {
            continue;
        }

        int    width, height, transform;
        json_t *name     = json_object_get(monitor, "name");
        json_t *scale    = json_object_get(monitor, "scale");
        json_t *reserved = json_object_get(monitor, "reserved");
        if (!json_is_string(name) || !json_is_number(scale) ||
            !json_is_array(reserved) || json_array_size(reserved) != 4 ||
            _get_int(monitor, "x", &fw->output_rect.x) ||
            _get_int(monitor, "y", &fw->output_rect.y) ||
            _get_int(monitor, "width", &width) ||
            _get_int(monitor, "height", &height) ||
            _get_int(monitor, "transform", &transform)) {
            LOG_ERR("Could not parse monitor %d.", id);
            break;
        }

        // Width and height are in physical pixels, everything else is in
        // logical pixels.
        double s = json_number_value(scale);
        if (transform % 2 == 1) {
            int tmp = width;
            width   = height;
            height  = tmp;
        }
        fw->output        = strdup(json_string_value(name));
        fw->output_rect.w = (int)lround(width / s);
        fw->output_rect.h = (int)lround(height / s);

        int reserved_left   = json_integer_value(json_array_get(reserved, 0));
        int reserved_top    = json_integer_value(json_array_get(reserved, 1));
        int reserved_right  = json_integer_value(json_array_get(reserved, 2));
        int reserved_bottom = json_integer_value(json_array_get(reserved, 3));

        fw->resize_left_limit   = reserved_left;
        fw->resize_right_limit  = fw->output_rect.w - reserved_right;
        fw->resize_top_limit    = reserved_top;
        fw->resize_bottom_limit = fw->output_rect.h - reserved_bottom;

        err = 0;
        break;
    }

    if (err && fw->output == NULL) {
        LOG_ERR("Could not find monitor %d.", id);
    }

    json_decref(monitors);
    return err;
}

int find_focused_window(struct focused_window *fw) {
    memset(fw, 0, sizeof(*fw));

    struct hypr_window win;
    if (_get_active_window(&win)) {
        return -1;
    }

    if (win.fullscreen != 0) {
        LOG_ERR("The focused window is fullscreen.");
        return -1;
    }

    // Only horizontal resizing is offered, so the top and bottom edges are
    // never probed and stay fixed.
    if (_probe_axis(&win, false, &fw->resize_left, &fw->resize_right)) {
        return -1;
    }

    // The probes are undone, but re-read the geometry in case a split ratio
    // got clamped along the way.
    if (_get_active_window(&win)) {
        return -1;
    }

    if (_get_monitor(win.monitor, fw)) {
        return -1;
    }

    snprintf(fw->address, sizeof(fw->address), "%s", win.address);
    fw->floating = win.floating;
    fw->rect     = win.rect;
    fw->rect.x  -= fw->output_rect.x;
    fw->rect.y  -= fw->output_rect.y;

    return 0;
}

void log_focused_window(struct focused_window *fw) {
    LOG_INFO("focused_window");
    LOG_INFO(" .address = %s", fw->address);
    LOG_INFO(" .output = %s", fw->output);
    LOG_INFO(" .floating = %s", fw->floating ? "true" : "false");
    LOG_INFO(
        " .resize_top_limit = %d (%s)", fw->resize_top_limit,
        fw->resize_top ? "resizable" : "fixed"
    );
    LOG_INFO(
        " .resize_bottom_limit = %d (%s)", fw->resize_bottom_limit,
        fw->resize_bottom ? "resizable" : "fixed"
    );
    LOG_INFO(
        " .resize_right_limit = %d (%s)", fw->resize_right_limit,
        fw->resize_right ? "resizable" : "fixed"
    );
    LOG_INFO(
        " .resize_left_limit = %d (%s)", fw->resize_left_limit,
        fw->resize_left ? "resizable" : "fixed"
    );
    LOG_INFO(
        " .rect = %dx%d+%d+%d", fw->rect.w, fw->rect.h, fw->rect.x, fw->rect.y
    );
}

int hypr_send_resize(const char *address, int32_t dx, int32_t dy) {
    char request[256];
    _format_resize_cmd(request, sizeof(request), address, dx, dy);

    char *reply = hypr_ipc_request(request);
    if (reply == NULL) {
        return -1;
    }

    int err = strncmp(reply, "ok", 2) == 0 ? 0 : -1;
    if (err) {
        LOG_ERR("Resize failed: %s", reply);
    }

    free(reply);
    return err;
}
