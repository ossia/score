/* Watchdog timer services for Linux */

// Copyright (C)2017-2023, Philip Munts dba Munts Technologies.
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
#include <linux/watchdog.h>
#include <sys/ioctl.h>

#include "errmsg.inc"
#include "libwatchdog.h"

void WATCHDOG_get_timeout(int32_t fd, int32_t *timeout, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (fd < 0)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (timeout == NULL)
  {
    *error = EINVAL;
    ERRORMSG("timeout argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (ioctl(fd, WDIOC_GETTIMEOUT, timeout) < 0)
  {
    *error = errno;
    ERRORMSG("ioctl() for WDIOC_GETTIMEOUT failed", *error, __LINE__ - 3);
    return;
  }

  *error = 0;
}

void WATCHDOG_set_timeout(int32_t fd, int32_t newtimeout,
  int32_t *timeout, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (fd < 0)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (newtimeout < 0)
  {
    *error = EINVAL;
    ERRORMSG("newtimeout argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (timeout == NULL)
  {
    *error = EINVAL;
    ERRORMSG("timeout argument is NULL", *error, __LINE__ - 3);
    return;
  }

  *timeout = newtimeout;

  if (ioctl(fd, WDIOC_SETTIMEOUT, timeout) < 0)
  {
    *error = errno;
    ERRORMSG("ioctl() for WDIOC_SETTIMEOUT failed", *error, __LINE__ - 3);
    return;
  }

  *error = 0;
}

void WATCHDOG_kick(int32_t fd, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (fd < 0)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (ioctl(fd, WDIOC_KEEPALIVE, 0) < 0)
  {
    *error = errno;
    ERRORMSG("ioctl() for WDIOC_KEEPALIVE failed", *error, __LINE__ - 3);
    return;
  }

  *error = 0;
}
