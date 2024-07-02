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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#include "macros.inc"
#include "libhidraw.h"

// Compatibility shims

void HIDRAW_open(const char *name, int32_t *fd, int32_t *error)
{
  HIDRAW_open1(name, fd, error);
}

void HIDRAW_open_id(int32_t VID, int32_t PID, int32_t *fd, int32_t *error)
{
  HIDRAW_open3(VID, PID, NULL, fd, error);
}

// Open the first raw HID device with matching vendor and product ID's

void HIDRAW_open2(int32_t VID, int32_t PID, int32_t *fd, int32_t *error)
{
  HIDRAW_open3(VID, PID, NULL, fd, error);
}

// Open the the raw HID device with matching vendor, product, and serial number

void HIDRAW_open3(int32_t VID, int32_t PID, const char *serial, int32_t *fd,
  int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (fd == NULL)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is NULL", *error, __LINE__ - 3);
    return;
  }

  int i;
  char name[MAXPATHLEN];
  int32_t b, v, p, e;
  char serialpath[MAXPATHLEN];
  char devserial[256];
  ssize_t len;

  // Search raw HID devices, looking for matching VID and PID

  for (i = 0; i < 255; i++)
  {
    snprintf(name, sizeof(name), "/dev/hidraw%d", i);

    // Open the candidate device node

    *fd = open(name, O_RDWR);
    if (*fd < 0) continue;

    // Try to get HID device info for the candidate device

    HIDRAW_get_info(*fd, &b, &v, &p, &e);
    if (e) continue;

    // Look for a matching device

    if ((VID != v) || (PID != p))
    {
      close(*fd);
      continue;
    }

    // If no serial number was specified, we are done

    if (serial == NULL)
    {
      *error = 0;
      return;
    }

    if (strlen(serial) == 0)
    {
      *error = 0;
      return;
    }

    // Fetch the serial number for this candidate device

    snprintf(serialpath, sizeof(serialpath),
      "/sys/class/hidraw/hidraw%d/../../../../serial", i);

    int serialfd = open(serialpath, O_RDONLY);

    if (serialfd < 0)
    {
      close(*fd);
      continue;
    }

    memset(devserial, 0, sizeof(0));
    len = read(serialfd, devserial, sizeof(devserial)-1);
    close(serialfd);

    // Check for successful read

    if (len < 1)
    {
      close(*fd);
      continue;
    }

    // Check whether we found a serial number

    if (strlen(devserial) == 0)
    {
      close(*fd);
      continue;
    }

    // Remove trailing LF, if any

    if (devserial[strlen(devserial)-1] == 10)
      devserial[strlen(devserial)-1] = 0;

    // Check for matching serial number

    if (!strcmp(serial, devserial))
    {
      *error = 0;
      return;
    }

    // Close the candidate device node

    close(*fd);
  }

  *fd = -1;
  *error = ENODEV;
  ERRORMSG("Cannot find matching raw HID device", *error, __LINE__ - 1);
}

// Get device information string (manufacturer + product)

void HIDRAW_get_name(int32_t fd, char *name, int32_t namesize, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (fd < 0)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (name == NULL)
  {
    *error = EINVAL;
    ERRORMSG("name argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (namesize < 16)
  {
    *error = EINVAL;
    ERRORMSG("namesize argument is too small", *error, __LINE__ - 3);
    return;
  }

  memset(name, 0, namesize);

  if (ioctl(fd, HIDIOCGRAWNAME(namesize), name) < 0)
  {
    *error = errno;
    ERRORMSG("ioctl() for HIDIOCGRAWNAME failed", *error, __LINE__ - 3);
    return;
  }

  *error = 0;
}

// Get device bus type, vendor and product information

void HIDRAW_get_info(int32_t fd, int32_t *bustype, int32_t *vendor, int32_t *product, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (fd < 0)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (bustype == NULL)
  {
    *error = EINVAL;
    ERRORMSG("bustype argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (vendor == NULL)
  {
    *error = EINVAL;
    ERRORMSG("vendor argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (product == NULL)
  {
    *error = EINVAL;
    ERRORMSG("product argument is NULL", *error, __LINE__ - 3);
    return;
  }

  struct hidraw_devinfo devinfo;

  if (ioctl(fd, HIDIOCGRAWINFO, &devinfo) < 0)
  {
    *error = errno;
    ERRORMSG("ioctl() for HIDIOCGRAWINFO failed", *error, __LINE__ - 3);
    return;
  }

  *bustype = devinfo.bustype;
  *vendor = devinfo.vendor;
  *product = devinfo.product;
  *error = 0;
}
