// netsim.h
//
// Library interface for sending frames to netsim.
// Students include this header and link with netsim_lib.cc.
//
// Compile example:
//   g++ -O2 -o sender sender.cc netsim_lib.cc

#ifndef NETSIM_H
#define NETSIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Return values for send_frame()
#define NETSIM_ACK    0
#define NETSIM_NAK    1
#define NETSIM_ERROR -1

// Send one frame to netsim and wait for the ACK/NAK response.
//
// Parameters:
//   frame      - pointer to the entire frame bytes:
//                  [size: 2 bytes BE] [payload: P bytes] [CRC: 4 bytes BE]
//   frame_size - total frame size in bytes (= 2 + payload_size + 4)
//
// Returns:
//   NETSIM_ACK    - frame was received correctly; advance to next data
//   NETSIM_NAK    - frame was corrupted in transit; resend the same frame
//   NETSIM_ERROR  - unable to communicate with netsim
int send_frame(const uint8_t *frame, int frame_size);

#ifdef __cplusplus
}
#endif

#endif
