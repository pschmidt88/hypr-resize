#ifndef __PHASE_H_INCLUDED__
#define __PHASE_H_INCLUDED__

#include "state.h"

#include <cairo.h>
#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

struct phase_interface {
    bool (*key)(struct state *, xkb_keysym_t, char *text);
    void (*render)(struct state *, cairo_t *);
};

extern struct phase_interface phase_resize_dir_interface;
extern struct phase_interface phase_resize_interface;

void phase_resize_enter(struct state *state, enum resize_direction direction);

#endif
