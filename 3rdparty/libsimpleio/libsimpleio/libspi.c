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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#include "errmsg.inc"
#include "libgpio.h"
#include "libspi.h"

// Open and configure the SPI port

void SPI_open(const char *name, int32_t mode, int32_t wordsize,
  int32_t speed, int32_t *fd, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (fd == NULL)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (name == NULL)
  {
    *fd = -1;
    *error = EINVAL;
    ERRORMSG("name argument is NULL", *error, __LINE__ - 4);
    return;
  }

  if ((mode < 0) || (mode > 3))
  {
    *fd = -1;
    *error = EINVAL;
    ERRORMSG("mode argument is invalid", *error, __LINE__ - 4);
    return;
  }

  if ((wordsize != 0) && (wordsize != 8) && (wordsize != 16) &&
      (wordsize != 32))
  {
    *fd = -1;
    *error = EINVAL;
    ERRORMSG("wordsize argument is invalid", *error, __LINE__ - 5);
    return;
  }

  if (speed < 1)
  {
    *fd = -1;
    *error = EINVAL;
    ERRORMSG("speed argument is invalid", *error, __LINE__ - 4);
    return;
  }

  // Open the SPI device

  *fd = open(name, O_RDWR);
  if (*fd < 0)
  {
    *fd = -1;
    *error = errno;
    ERRORMSG("open() failed", *error, __LINE__ - 5);
    return;
  }

  // Configure SPI transfer mode (clock polarity and phase)

  if (ioctl(*fd, SPI_IOC_WR_MODE, &mode) < 0)
  {
    *error = errno;
    ERRORMSG("ioctl() for SPI_IOC_WR_MODE failed", *error, __LINE__ - 3);
    close(*fd);
    *fd = -1;
    return;
  }

  // Configure SPI transfer word size

  if (ioctl(*fd, SPI_IOC_WR_BITS_PER_WORD, &wordsize) < 0)
  {
    *error = errno;
    ERRORMSG("ioctl() for SPI_IOC_WR_BITS_PER_WORD failed", *error, __LINE__ - 3);
    close(*fd);
    *fd = -1;
    return;
  }

  // Configure (maximum) SPI transfer speed

  if (ioctl(*fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
  {
    *error = errno;
    ERRORMSG("ioctl() for SPI_IOC_WR_MAX_SPEED_HZ failed", *error, __LINE__ - 3);
    close(*fd);
    *fd = -1;
    return;
  }

  *error = 0;
}

// Perform an SPI I/O transaction (command and response)

void SPI_transaction(int32_t spifd, int32_t csfd, void *cmd,
  int32_t cmdlen, int32_t delayus, void *resp, int32_t resplen, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (spifd < 0)
  {
    *error = EINVAL;
    ERRORMSG("spifd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (csfd < SPI_CS_AUTO)
  {
    *error = EINVAL;
    ERRORMSG("csfd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (cmdlen < 0)
  {
    *error = EINVAL;
    ERRORMSG("cmdlen argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (delayus < 0)
  {
    *error = EINVAL;
    ERRORMSG("delayus argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (resplen < 0)
  {
    *error = EINVAL;
    ERRORMSG("resplen argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if ((cmd == NULL) && (cmdlen != 0))
  {
    *error = EINVAL;
    ERRORMSG("cmd and cmdlen arguments are inconsistent", *error,
      __LINE__ - 3);
    return;
  }

  if ((cmd != NULL) && (cmdlen == 0))
  {
    *error = EINVAL;
    ERRORMSG("cmd and cmdlen arguments are inconsistent", *error,
      __LINE__ - 3);
    return;
  }

  if ((resp == NULL) && (resplen != 0))
  {
    *error = EINVAL;
    ERRORMSG("resp and resplen arguments are inconsistent", *error,
      __LINE__ - 3);
    return;
  }

  if ((resp != NULL) && (resplen == 0))
  {
    *error = EINVAL;
    ERRORMSG("resp and resplen arguments are inconsistent", *error,
      __LINE__ - 3);
    return;
  }

  if ((cmd == NULL) && (resp == NULL))
  {
    *error = EINVAL;
    ERRORMSG("cmd and resp arguments are both NULL", *error, __LINE__ - 3);
    return;
  }

  struct spi_ioc_transfer xfer[2];

  // Prepare the SPI ioctl transfer structure
  //   xfer[0] is the outgoing command to the slave MCU
  //   xfer[1] is the incoming response from the slave MCU

  // The command and response transfers are executed back to back with
  // with a inter-transfer delay in microseconds specfied by xfer[0].delay
  // This delay determines the time available to the slave MCU to decode the
  // command and generate the response.

  memset(xfer, 0, sizeof(xfer));
  xfer[0].tx_buf = (typeof(xfer[0].tx_buf)) cmd;
  xfer[0].len = cmdlen;
  xfer[0].delay_usecs = delayus;
  xfer[1].rx_buf = (typeof(xfer[1].rx_buf)) resp;
  xfer[1].len = resplen;

  // Assert GPIO controlled chip select (if any)

  if (csfd > 0)
  {
    GPIO_line_write(csfd, 0, error);
    if (*error) return;
  }

  // Execute the SPI transfer operations

  if (ioctl(spifd, SPI_IOC_MESSAGE(2), xfer) < 0)
  {
    *error = errno;
    ERRORMSG("ioctl() for SPI_IOC_MESSAGE failed", *error, __LINE__ - 3);
    return;
  }

  // Deassert GPIO controlled chip select (if any)

  if (csfd > 0)
  {
    GPIO_line_write(csfd, 1, error);
    if (*error) return;
  }

  *error = 0;
}
