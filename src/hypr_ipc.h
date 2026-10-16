#ifndef __HYPR_IPC_H_INCLUDED__
#define __HYPR_IPC_H_INCLUDED__

#include <stddef.h>

// Send a request to the Hyprland command socket and return the reply.
// The returned string must be freed by the caller. Returns NULL on error.
char *hypr_ipc_request(const char *request);

// Split a `[[BATCH]]` reply into its individual replies. The reply string is
// modified in place. Returns the number of parts written to `parts`.
size_t hypr_ipc_split_batch(char *reply, char **parts, size_t max_parts);

#endif
