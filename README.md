# Computer Network Adaptive File Sender Project

This repository contains a C++ sender implementation for a computer networks project focused on reliable file transfer over a noisy channel. The sender builds framed packets, computes CRC values, reacts to ACK/NAK feedback, and adapts payload size to improve transmission efficiency under changing error rates.

## Overview

- Implemented a reliable sender on top of a provided network simulator interface
- Constructed frames with a 2-byte payload length header and 4-byte CRC trailer
- Re-sent data when the simulator returned `NAK`
- Estimated channel quality from observed ACK/NAK outcomes
- Adaptively selected payload sizes to balance retransmission risk and throughput

## Repository Structure

- `sender.cc`: main sender implementation
- `netsim.h`: simulator interface used by the sender
- `netsim_lib.cc`: helper library for communicating with the simulator process
- `Makefile`: build commands for the sender binary

## How It Works

The sender reads the input file in binary mode, stages pending bytes, and packages each transmission into a frame:

1. 2-byte big-endian payload size
2. payload bytes
3. 4-byte CRC

After each transmission, the sender waits for an `ACK` or `NAK` response from the simulator. Instead of using a fixed payload size, the implementation updates an internal belief about the channel error rate and chooses the next payload from a predefined ladder of candidate sizes. This allows the sender to move toward larger frames on clean channels and become more conservative when corruption becomes more frequent.

## Technical Highlights

- Custom CRC32-style frame validation logic
- Adaptive payload sizing using posterior updates from delivery outcomes
- Clean separation between file I/O, frame construction, and payload selection
- Robust handling for partial writes, read failures, and simulator communication errors

## Build

```bash
make
```

This produces a `sender` executable.

## Run

This project was originally designed to run with a course-provided simulator process. The repository includes the interface and support code required by the sender, but not the external simulator executable used in the original lab environment.

A typical build command is:

```bash
g++ -O2 -std=c++17 -o sender sender.cc netsim_lib.cc
```

## Portfolio Note

This public repository is a cleaned portfolio version of the original coursework submission. Large sample media, bundled test assets, and course-distributed executables were intentionally excluded so the repository stays focused on the implemented networking logic.
