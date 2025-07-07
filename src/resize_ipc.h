#ifndef __RESIZE_IPC_H_INCLUDED__
#define __RESIZE_IPC_H_INCLUDED__

#include "resize_params.h"

int sway_ipc_send_resize(
    int socket, enum resize_direction direction, int32_t increase
);

#endif
