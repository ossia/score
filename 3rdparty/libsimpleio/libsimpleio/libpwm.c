/* PWM (Pulse Width Modulated) output services for Linux */

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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>

#include "macros.inc"
#include "libpwm.h"

// Device nodes

#define DIR_CHIP	"/sys/class/pwm/pwmchip%d"
#define FILE_EXPORT	DIR_CHIP  "/export"
#define FILE_UNEXPORT	DIR_CHIP  "/unexport"
#define DIR_CHAN	DIR_CHIP  "/pwm%d"
#define FILE_ENABLE	DIR_CHAN  "/enable"
#define FILE_ONTIME	DIR_CHAN  "/duty_cycle"	// nanoseconds
#define FILE_PERIOD	DIR_CHAN  "/period"	// nanoseconds
#define FILE_POLARITY	DIR_CHAN  "/polarity"	// "normal" or "inversed" [sic]

static uint64_t milliseconds(void)
{
  struct timespec t;

  clock_gettime(CLOCK_REALTIME, &t);
  return t.tv_sec*1000LL + (1LL*t.tv_nsec)/1000000LL;
}

// Configure PWM output device

void PWM_configure(int32_t chip, int32_t channel, int32_t period,
  int32_t ontime, int32_t polarity, int32_t *error)
{
  assert(error != NULL);

  char filename_export[MAXPATHLEN];
  char filename_enable[MAXPATHLEN];
  char filename_ontime[MAXPATHLEN];
  char filename_period[MAXPATHLEN];
  char filename_polarity[MAXPATHLEN];
  char buf[16];
  int fd;
  int len;

  // Validate parameters

  if (chip < 0)
  {
    *error = EINVAL;
    ERRORMSG("chip argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (channel < 0)
  {
    *error = EINVAL;
    ERRORMSG("channel argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (period < 0)
  {
    *error = EINVAL;
    ERRORMSG("period argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (ontime < 0)
  {
    *error = EINVAL;
    ERRORMSG("ontime argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if ((polarity < PWM_POLARITY_ACTIVELOW) || (polarity > PWM_POLARITY_ACTIVEHIGH))
  {
    *error = EINVAL;
    ERRORMSG("polarity argument is invalid", *error, __LINE__ - 3);
    return;
  }

  snprintf(filename_export,   sizeof(filename_export),   FILE_EXPORT,   chip);
  snprintf(filename_enable,   sizeof(filename_enable),   FILE_ENABLE,   chip, channel);
  snprintf(filename_ontime,   sizeof(filename_ontime),   FILE_ONTIME,   chip, channel);
  snprintf(filename_period,   sizeof(filename_period),   FILE_PERIOD,   chip, channel);
  snprintf(filename_polarity, sizeof(filename_polarity), FILE_POLARITY, chip, channel);

  // Export the PWM channel, if necessary

  if (access(filename_ontime, W_OK))
  {

    // Open the PWM chip export file

    fd = open(filename_export, O_WRONLY);
    if (fd < 0)
    {
      *error = errno;
      ERRORMSG("Cannot open export", *error, __LINE__ - 4);
      return;
    }

    // Export the PWM output channel

    len = snprintf(buf, sizeof(buf), "%d\n", channel);

    if (write(fd, buf, len) < len)
    {
      *error = errno;
      ERRORMSG("Cannot write to export", *error, __LINE__ - 3);
      close(fd);
      return;
    }

    // Close the PWM chip export file

    close(fd);

    // Wait for the PWM output channel device to be created

    uint64_t start = milliseconds();

    while (access(filename_enable,   W_OK) ||
           access(filename_ontime,   W_OK) ||
           access(filename_period,   W_OK) ||
           access(filename_polarity, W_OK))
    {
      if (milliseconds() - start > 1000)
      {
        *error = EIO;
        ERRORMSG("Timed out waiting for PWM output export", *error,
          __LINE__ - 3);
        return;
      }

      usleep(100000);
    }
  }

  // Try to write initial 0 to duty_cycle, so we can change the period
  // if this output has previously been configured.

  fd = open(filename_ontime, O_WRONLY);
  if (fd < 0)
  {
    *error = errno;
    ERRORMSG("Cannot open duty_cycle", *error, __LINE__ - 4);
    return;
  }

  len = write(fd, "0\n", 2); // Don't care if write() fails here...
  close(fd);

  // Write to period

  fd = open(filename_period, O_WRONLY);
  if (fd < 0)
  {
    *error = errno;
    ERRORMSG("Cannot open period", *error, __LINE__ - 4);
    return;
  }

  len = snprintf(buf, sizeof(buf), "%d\n", period);

  if (write(fd, buf, len) < len)
  {
    *error = errno;
    ERRORMSG("Cannot write to period", *error, __LINE__ - 3);
    close(fd);
    return;
  }

  close(fd);

 // Disable the PWM output

  fd = open(filename_enable, O_WRONLY);
  if (fd < 0)
  {
    *error = errno;
    ERRORMSG("Cannot open enable", *error, __LINE__ - 4);
    return;
  }

  if (write(fd, "0\n", 2) < 2)
  {
    *error = errno;
    ERRORMSG("Cannot write to enable", *error, __LINE__ - 3);
    close(fd);
    return;
  }

  close(fd);

  // Write to polarity

  fd = open(filename_polarity, O_WRONLY);
  if (fd < 0)
  {
    *error = errno;
    ERRORMSG("Cannot open polarity", *error, __LINE__ - 4);
    return;
  }

  if (polarity == PWM_POLARITY_ACTIVEHIGH)
    len = snprintf(buf, sizeof(buf), "normal\n");
  else
    len = snprintf(buf, sizeof(buf), "inversed\n");

  if (write(fd, buf, len) < len)
  {
    *error = errno;
    ERRORMSG("Cannot write to polarity", *error, __LINE__ - 3);
    close(fd);
    return;
  }

  close(fd);

  // Reenable the PWM output

  fd = open(filename_enable, O_WRONLY);
  if (fd < 0)
  {
    *error = errno;
    ERRORMSG("Cannot open enable", *error, __LINE__ - 4);
    return;
  }

  if (write(fd, "1\n", 2) < 2)
  {
    *error = errno;
    ERRORMSG("Cannot write to enable", *error, __LINE__ - 3);
    close(fd);
    return;
  }

  close(fd);

  // Write to duty_cycle (which is actually the on time in nanosecods...)

  fd = open(filename_ontime, O_WRONLY);
  if (fd < 0)
  {
    *error = errno;
    ERRORMSG("Cannot open duty_cycle", *error, __LINE__ - 4);
    return;
  }

  len = snprintf(buf, sizeof(buf), "%d\n", ontime);

  if (write(fd, buf, len) < len)
  {
    *error = errno;
    ERRORMSG("Cannot write to duty_cycle", *error, __LINE__ - 3);
    close(fd);
    return;
  }

  close(fd);

  *error = 0;
}

// Open PWM output device

void PWM_open(int32_t chip, int32_t channel, int32_t *fd, int32_t *error)
{
  char filename[MAXPATHLEN];

  // Validate parameters

  assert(error != NULL);

  if (fd == NULL)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (chip < 0)
  {
    *fd = -1;
    *error = EINVAL;
    ERRORMSG("chip argument is invalid", *error, __LINE__ - 4);
    return;
  }

  if (channel < 0)
  {
    *fd = -1;
    *error = EINVAL;
    ERRORMSG("channel argument is invalid", *error, __LINE__ - 4);
    return;
  }

  snprintf(filename, sizeof(filename), FILE_ONTIME, chip, channel);

  *fd = open(filename, O_WRONLY);
  if (*fd < 0)
  {
    *fd = -1;
    *error = errno;
    ERRORMSG("open() failed", *error, __LINE__ - 4);
    return;
  }

  *error = 0;
}

// Write to PWM output device

void PWM_write(int32_t fd, int32_t ontime, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (fd < 0)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (ontime < 0)
  {
    *error = EINVAL;
    ERRORMSG("ontime argument is invalid", *error, __LINE__ - 3);
    return;
  }

  char buf[16];
  int len;

  // Convert on time to string

  len = snprintf(buf, sizeof(buf), "%d\n", ontime);

  // Write string to the device

  if (write(fd, buf, len) < len)
  {
    *error = errno;
    ERRORMSG("write() failed", *error, __LINE__ - 3);
    return;
  }

  *error = 0;
}
