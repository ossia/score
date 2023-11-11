// Simple IPv4 TCP client and server routines

// Copyright (C)2016-2023, Philip Munts dba Munts Technologies.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef _LIBIPV4_H_
#define _LIBIPV4_H_

#include <libsimpleio/cplusplus.h>
#include <stdint.h>

_BEGIN_STD_C

// Resolve host name to IPV4 address

extern void IPV4_resolve(const char *name, int32_t *addr, int32_t *error);

// Convert IPV4 address to dotted decimal string

extern void IPV4_ntoa(int32_t addr, char *dst, int32_t dstsize,
  int32_t *error);

// Connect to a TCP server

extern void TCP4_connect(int32_t addr, int32_t port,
  int32_t *fd, int32_t *error);

// Wait (block) for exactly one connection from a TCP client, then
// return a file descriptor for the new connection

extern void TCP4_accept(int32_t addr, int32_t port,
  int32_t *fd, int32_t *error);

// Wait (block) until a client connects, then fork and return a file
// descriptor for the new connection to the child process

extern void TCP4_server(int32_t addr, int32_t port,
  int32_t *fd, int32_t *error);

// Close connection

extern void TCP4_close(int32_t fd, int32_t *error);

// Send data

extern void TCP4_send(int32_t fd, void *buf, int32_t bufsize, int32_t *count,
  int32_t *error);

// Receive data

extern void TCP4_receive(int32_t fd, void *buf, int32_t bufsize,
  int32_t *count, int32_t *error);

// Open a UDP socket

extern void UDP4_open(int32_t addr, int32_t port, int32_t *fd, int32_t *error);

// Close a UDP socket

extern void UDP4_close(int32_t fd, int32_t *error);

// Send a UDP datagram

extern void UDP4_send(int32_t fd, int32_t addr, int32_t port, void *buf,
  int32_t bufsize, int32_t flags, int32_t *count, int32_t *error);

// Receive a UDP datagram

extern void UDP4_receive(int32_t fd, int32_t *addr, int32_t *port, void *buf,
  int32_t bufsize, int32_t flags, int32_t *count, int32_t *error);

_END_STD_C

#endif
