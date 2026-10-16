#include "hypr_ipc.h"

#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define BATCH_DELIMITER "\n\n\n"

static int _open_socket() {
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    const char *signature   = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (runtime_dir == NULL || signature == NULL) {
        LOG_ERR("XDG_RUNTIME_DIR or HYPRLAND_INSTANCE_SIGNATURE is not set.");
        return -1;
    }

    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    int                len  = snprintf(
        addr.sun_path, sizeof(addr.sun_path), "%s/hypr/%s/.socket.sock",
        runtime_dir, signature
    );
    if (len < 0 || len >= (int)sizeof(addr.sun_path)) {
        LOG_ERR("Hyprland socket path is too long.");
        return -1;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG_ERR("Could not create socket.");
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERR("Could not connect to Hyprland socket '%s'.", addr.sun_path);
        close(fd);
        return -1;
    }

    return fd;
}

char *hypr_ipc_request(const char *request) {
    int fd = _open_socket();
    if (fd < 0) {
        return NULL;
    }

    size_t len     = strlen(request);
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(fd, request + written, len - written);
        if (n <= 0) {
            LOG_ERR("Could not write to Hyprland socket.");
            close(fd);
            return NULL;
        }
        written += n;
    }

    // Hyprland closes the connection once the reply has been sent.
    size_t cap   = 4096;
    size_t size  = 0;
    char  *reply = malloc(cap);
    for (;;) {
        if (size + 1 >= cap) {
            cap   *= 2;
            reply  = realloc(reply, cap);
        }

        ssize_t n = read(fd, reply + size, cap - size - 1);
        if (n < 0) {
            LOG_ERR("Could not read from Hyprland socket.");
            free(reply);
            close(fd);
            return NULL;
        }
        if (n == 0) {
            break;
        }
        size += n;
    }
    reply[size] = '\0';

    close(fd);
    return reply;
}

size_t hypr_ipc_split_batch(char *reply, char **parts, size_t max_parts) {
    size_t count = 0;
    char  *start = reply;

    while (count < max_parts) {
        parts[count++] = start;

        char *end = strstr(start, BATCH_DELIMITER);
        if (end == NULL) {
            break;
        }
        *end  = '\0';
        start = end + strlen(BATCH_DELIMITER);
    }

    return count;
}
