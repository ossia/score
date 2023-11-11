/* Raw HID device wrapper services for Linux */

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

#ifndef LIBHIDRAW_H
#define LIBHIDRAW_H

#include <libsimpleio/cplusplus.h>
#include <stdint.h>

_BEGIN_STD_C

extern void HIDRAW_open1(const char *name, int32_t *fd, int32_t *error);

extern void HIDRAW_open2(int32_t VID, int32_t PID, int32_t *fd, int32_t *error);

extern void HIDRAW_open3(int32_t VID, int32_t PID, const char *serial,
  int32_t *fd, int32_t *error);

extern void HIDRAW_close(int32_t fd, int32_t *error);

extern void HIDRAW_get_name(int32_t fd, char *name, int32_t namesize,
  int32_t *error);

extern void HIDRAW_get_info(int32_t fd, int32_t *bustype, int32_t *vendor,
  int32_t *product, int32_t *error);

extern void HIDRAW_send(int32_t fd, void *buf, int32_t bufsize, int32_t *count,
  int32_t *error);

extern void HIDRAW_receive(int32_t fd, void *buf, int32_t bufsize, int32_t *count,
  int32_t *error);

_END_STD_C

#endif
