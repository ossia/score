/* SPI transaction services for Linux */

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

#ifndef LIBSPI_H
#define LIBSPI_H

#include <libsimpleio/cplusplus.h>
#include <stdint.h>
#include <linux/spi/spidev.h>

// Use hardware controlled chip select

#define SPI_CS_AUTO	-1

_BEGIN_STD_C

extern void SPI_open(const char *name, int32_t mode, int32_t wordsize,
  int32_t speed, int32_t *fd, int32_t *error);

extern void SPI_close(int32_t fd, int32_t *error);

extern void SPI_transaction(int32_t spifd, int32_t csfd, void *cmd,
  int32_t cmdlen, int32_t delayus, void *resp, int32_t resplen, int32_t *error);

_END_STD_C

#endif
