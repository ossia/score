/* Serial port device wrapper services for Linux */

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

#ifndef LIBSERIAL_H
#define LIBSERIAL_H

#include <libsimpleio/cplusplus.h>
#include <stdint.h>

typedef enum
{
  SERIAL_PARITY_NONE,
  SERIAL_PARITY_EVEN,
  SERIAL_PARITY_ODD,
} SERIAL_PARITY_t;

_BEGIN_STD_C

extern void SERIAL_open(const char *name, int32_t baudrate, int32_t parity,
  int32_t databits, int32_t stopbits, int32_t *fd, int32_t *error);

extern void SERIAL_close(int32_t fd, int32_t *error);

extern void SERIAL_send(int32_t fd, void *buf, int32_t bufsize,
  int32_t *count, int32_t *error);

extern void SERIAL_receive(int32_t fd, void *buf, int32_t bufsize,
  int32_t *count, int32_t *error);

_END_STD_C

#endif
