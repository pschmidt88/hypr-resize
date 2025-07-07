#include "resize_ipc.h"

#include "sway_ipc.h"

#include <string.h>

int sway_ipc_send_resize(
    int socket, enum resize_direction direction, int32_t increase
) {
    if (direction < 0 || direction >= NUM_DIRECTIONS) {
        return -1;
    }

    char cmd[256];

    static const char *cmd_direction[NUM_DIRECTIONS] = {
        [RESIZE_VERTICAL] = "height", [RESIZE_HORIZONTAL] = "width",
        [RESIZE_LEFT] = "left",       [RESIZE_RIGHT] = "right",
        [RESIZE_TOP] = "up",          [RESIZE_BOTTOM] = "down",
    };

    snprintf(
        cmd, sizeof(cmd) - 1, "resize grow %s %dpx", cmd_direction[direction],
        increase
    );
    cmd[sizeof(cmd) - 1] = '\0';

    sway_ipc_send(socket, SWAY_MSG_RUN_COMMAND, cmd, strlen(cmd));
    struct sway_ipc_msg *res = sway_ipc_recv(socket);
    puts(res->payload);
    free(res);

    return 0;
}
