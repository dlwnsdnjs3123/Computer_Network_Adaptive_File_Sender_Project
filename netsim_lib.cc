// netsim_lib.cc
//
// Implementation of send_frame() declared in netsim.h.
// Internally uses stdin/stdout because the simulator runs the sender
// as a child process connected with pipes.

#include "netsim.h"

#include <errno.h>
#include <unistd.h>

int send_frame(const uint8_t *frame, int frame_size) {
    int written = 0;
    while (written < frame_size) {
        ssize_t w = write(STDOUT_FILENO, frame + written,
                          (size_t)(frame_size - written));
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return NETSIM_ERROR;
        }
        if (w == 0) {
            return NETSIM_ERROR;
        }
        written += (int)w;
    }

    unsigned char reply;
    ssize_t r;
    do {
        r = read(STDIN_FILENO, &reply, 1);
    } while (r < 0 && errno == EINTR);

    if (r != 1) {
        return NETSIM_ERROR;
    }

    if (reply == 'A') {
        return NETSIM_ACK;
    }
    if (reply == 'N') {
        return NETSIM_NAK;
    }
    return NETSIM_ERROR;
}
