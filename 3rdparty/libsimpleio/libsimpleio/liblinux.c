/* Linux syscall wrappers.  These are primarily for the benefit of other */
/* programming languages, such as Ada, C#, Free Pascal, Go, etc.         */

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
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "errmsg.inc"
#include "liblinux.h"

// Detach process from the controlling terminal and run it in the background

void LINUX_detach(int32_t *error)
{
  assert(error != NULL);

  if (daemon(0, 0))
  {
    *error = errno;
    ERRORMSG("daemon() failed", *error, __LINE__ - 3);
    return;
  }

  *error = 0;
}

// Drop privileges from superuser to ordinary user

void LINUX_drop_privileges(const char *username, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (username == NULL)
  {
    *error = EINVAL;
    ERRORMSG("username argument is NULL", *error, __LINE__ - 3);
    return;
  }

  struct passwd *pwent;

  // Look up the user name

  pwent = getpwnam(username);
  if (pwent == NULL)
  {
    *error = errno;
    ERRORMSG("getpwnam() failed", *error, __LINE__ - 4);
    return;
  }

  // Set group membership

  if (initgroups(pwent->pw_name, pwent->pw_gid))
  {
    *error = errno;
    ERRORMSG("initgroups() failed", *error, __LINE__ - 3);
    return;
  }

  // Change gid

  if (setgid(pwent->pw_gid))
  {
    *error = errno;
    ERRORMSG("setgid() failed", *error, __LINE__ - 3);
    return;
  }

  // Change uid

  if (setuid(pwent->pw_uid))
  {
    *error = errno;
    ERRORMSG("setuid() failed", *error, __LINE__ - 3);
    return;
  }

  *error = 0;
}

// Open syslog connection

void LINUX_openlog(const char *id, int32_t options, int32_t facility,
  int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (id == NULL)
  {
    *error = EINVAL;
    ERRORMSG("id argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if ((facility >> 3) >= LOG_NFACILITIES)
  {
    *error = EINVAL;
    ERRORMSG("facility argument is invalid", *error, __LINE__ - 3);
    return;
  }

  // Save the identity string in a static buffer

  static char ident[256];
  memset(ident, 0, sizeof(ident));
  strncpy(ident, id, sizeof(ident) - 1);

  openlog(strlen(ident) ? ident : NULL, options, facility);
  *error = 0;
}

// Post syslog message

void LINUX_syslog(int32_t priority, const char *msg, int32_t *error)
{
  assert(error != NULL);

  if (msg == NULL)
  {
    *error = EINVAL;
    ERRORMSG("msg argument is NULL", *error, __LINE__ - 3);
    return;
  }

  syslog(priority, "%s", msg);
  *error = 0;
}

// Close syslog connection

void LINUX_closelog(int32_t *error)
{
  assert(error != NULL);

  closelog();
  *error = 0;
}

// Retrieve errno value

int32_t LINUX_errno(void)
{
  return errno;
}

// Retrieve errno message

void LINUX_strerror(int32_t error, char *buf, int32_t bufsize)
{
  // Validate parameters

  if (buf == NULL)
  {
    ERRORMSG("buf argument is NULL", EINVAL, __LINE__ - 2);
    return;
  }

  if (bufsize < 16)
  {
    ERRORMSG("bufsize argument is too small", EINVAL, __LINE__ - 2);
    return;
  }

  memset(buf, 0, bufsize);
  strerror_r(error, buf, bufsize);
}

// Wait for an event on one or more files

void LINUX_poll(int32_t numfiles, int32_t *files, int32_t *events,
  int32_t *results, int32_t timeout, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (numfiles < 1)
  {
    *error = EINVAL;
    ERRORMSG("numfiles argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (files == NULL)
  {
    *error = EINVAL;
    ERRORMSG("files argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (events == NULL)
  {
    *error = EINVAL;
    ERRORMSG("events argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (results == NULL)
  {
    *error = EINVAL;
    ERRORMSG("results argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (timeout < -1)
  {
    *error = EINVAL;
    ERRORMSG("timeout argument is out of range", *error, __LINE__ - 3);
    return;
  }

 // Prepare the poll request structure

  struct pollfd fds[10];

  int i;

  for (i = 0; i < numfiles; i++)
  {
    fds[i].fd = files[i];
    fds[i].events = events[i];
    fds[i].revents = 0;
  }

  // Wait for something to happen

  int count = poll(fds, numfiles, timeout);

  // Timeout occurred

  if (count == 0)
  {
    *error = EAGAIN;
    return;
  }

  // An error occurred

  if (count < 0)
  {
    *error = errno;
    ERRORMSG("poll() failed", *error, __LINE__ - 3);
    return;
  }

  // An event occurred

  for (i = 0; i < numfiles; i++)
    results[i] = fds[i].revents;

  *error = 0;
}

// Sleep for some number of microseconds

void LINUX_usleep(int32_t microseconds, int32_t *error)
{
  if (usleep(microseconds))
    *error = errno;
  else
    *error = 0;
}

// Execute a shell command string

void LINUX_command(const char *cmd, int32_t *status, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (status == NULL)
  {
    *error = EINVAL;
    ERRORMSG("status argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (cmd == NULL)
  {
    *error = EINVAL;
    *status = 0;
    ERRORMSG("cmd argument is NULL", *error, __LINE__ - 4);
    return;
  }

  int ret = system(cmd);

  if (ret < 0)
  {
    *error = errno;
    *status = 0;
    ERRORMSG("system() failed", *error, __LINE__ - 4);
    return;
  }

  *error = 0;
  *status = WEXITSTATUS(ret);
}

// Open a file descriptor

void LINUX_open(const char *name, int32_t flags, int32_t mode, int32_t *fd,
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

  if (name == NULL)
  {
    *fd = -1;
    *error = EINVAL;
    ERRORMSG("name argument is NULL", *error, __LINE__ - 4);
    return;
  }

  *fd = open(name, flags, mode);
  if (*fd < 0)
  {
    *fd = -1;
    *error = errno;
    ERRORMSG("open() failed", *error, __LINE__ - 5);
    return;
  }

  *error = 0;
}

// Open a file descriptor for read access

void LINUX_open_read(const char *name, int32_t *fd, int32_t *error)
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

  *fd = open(name, O_RDONLY);
  if (*fd < 0)
  {
    *fd = -1;
    *error = errno;
    ERRORMSG("open() failed", *error, __LINE__ - 5);
    return;
  }

  *error = 0;
}

// Open a file descriptor for write access

void LINUX_open_write(const char *name, int32_t *fd, int32_t *error)
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

  *fd = open(name, O_WRONLY);
  if (*fd < 0)
  {
    *fd = -1;
    *error = errno;
    ERRORMSG("open() failed", *error, __LINE__ - 5);
    return;
  }

  *error = 0;
}

// Open a file descriptor for read/write access

void LINUX_open_readwrite(const char *name, int32_t *fd, int32_t *error)
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

  *fd = open(name, O_RDWR);
  if (*fd < 0)
  {
    *fd = -1;
    *error = errno;
    ERRORMSG("open() failed", *error, __LINE__ - 5);
    return;
  }

  *error = 0;
}

// Open a file descriptor for create/overwrite access

void LINUX_open_create(const char *name, int32_t mode, int32_t *fd,
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

  if (name == NULL)
  {
    *fd = -1;
    *error = EINVAL;
    ERRORMSG("name argument is NULL", *error, __LINE__ - 4);
    return;
  }

  *fd = creat(name, mode);
  if (*fd < 0)
  {
    *fd = -1;
    *error = errno;
    ERRORMSG("creat() failed", *error, __LINE__ - 5);
    return;
  }

  *error = 0;
}

// Close a file descriptor

void LINUX_close(int32_t fd, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (fd < 0)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (close(fd))
  {
    *error = errno;
    ERRORMSG("close() failed", *error, __LINE__ - 3);
    return;
  }

  *error = 0;
}

// Read from a file descriptor

void LINUX_read(int32_t fd, void *buf, int32_t bufsize, int32_t *count,
  int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (fd < 0)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (buf == NULL)
  {
    *error = EINVAL;
    ERRORMSG("buf argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (bufsize < 1)
  {
    *error = EINVAL;
    ERRORMSG("bufsize argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (count == NULL)
  {
    *error = EINVAL;
    ERRORMSG("count argument is NULL", *error, __LINE__ - 3);
    return;
  }

  int32_t len = read(fd, buf, bufsize);
  if (len < 0)
  {
    *count = 0;
    *error = errno;
    ERRORMSG("read() failed", *error, __LINE__ - 5);
    return;
  }

  *count = len;
  *error = 0;
}

// Write to a file descriptor

void LINUX_write(int32_t fd, void *buf, int32_t bufsize, int32_t *count,
  int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (fd < 0)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (buf == NULL)
  {
    *error = EINVAL;
    ERRORMSG("buf argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (bufsize < 1)
  {
    *error = EINVAL;
    ERRORMSG("bufsize argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (count == NULL)
  {
    *error = EINVAL;
    ERRORMSG("count argument is NULL", *error, __LINE__ - 3);
    return;
  }

  int32_t len = write(fd, buf, bufsize);
  if (len < 0)
  {
    *count = 0;
    *error = errno;
    ERRORMSG("write() failed", *error, __LINE__ - 5);
    return;
  }

  *count = len;
  *error = 0;
}

// Open a pipe from another program

void LINUX_popen_read(const char *cmd, void **stream, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (cmd == NULL)
  {
    *error = EINVAL;
    ERRORMSG("cmd argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (strlen(cmd) == 0)
  {
    *error = EINVAL;
    ERRORMSG("cmd argument is empty", *error, __LINE__ - 3);
    return;
  }

  if (stream == NULL)
  {
    *error = EINVAL;
    ERRORMSG("stream argument is NULL", *error, __LINE__ - 3);
    return;
  }

  *stream = popen(cmd, "re");

  if (*stream == NULL)
  {
    *error = errno;
    ERRORMSG("popen() failed", *error, __LINE__ - 5);
    return;
  }

  *error = 0;
}

// Open a pipe to another program

void LINUX_popen_write(const char *cmd, void **stream, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (cmd == NULL)
  {
    *error = EINVAL;
    ERRORMSG("cmd argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (strlen(cmd) == 0)
  {
    *error = EINVAL;
    ERRORMSG("cmd argument is empty", *error, __LINE__ - 3);
    return;
  }

  if (stream == NULL)
  {
    *error = EINVAL;
    ERRORMSG("stream argument is NULL", *error, __LINE__ - 3);
    return;
  }

  *stream = popen(cmd, "we");

  if (*stream == NULL)
  {
    *error = errno;
    ERRORMSG("popen() failed", *error, __LINE__ - 5);
    return;
  }

  *error = 0;
}

// Close a pipe

void LINUX_pclose(void *stream, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (stream == NULL)
  {
    *error = EINVAL;
    ERRORMSG("stream argument is NULL", *error, __LINE__ - 3);
    return;
  }

  // Close the pipe

  if (pclose(stream) < 0)
  {
    *error = errno;
    ERRORMSG("pclose() failed", *error, __LINE__ - 3);
    return;
  }

  *error = 0;
}

// It is hard to index a C pointer to pointer(s) in Ada

void *LINUX_indexpp(void **p, int32_t i)
{
  if (p == NULL) return NULL;
  if (i < 0) return NULL;
  return p[i];
}

// Function aliases

#define ALIAS(orig) __attribute__((weak, alias(orig)))

void ADC_close(int32_t fd, int32_t *error) ALIAS("LINUX_close");
void DAC_close(int32_t fd, int32_t *error) ALIAS("LINUX_close");
void EVENT_close(int32_t fd, int32_t *error) ALIAS("LINUX_close");
void GPIO_close(int32_t fd, int32_t *error) ALIAS("LINUX_close");
void GPIO_line_close(int32_t fd, int32_t *error) ALIAS("LINUX_close");
void HIDRAW_close(int32_t fd, int32_t *error) ALIAS("LINUX_close");
void HIDRAW_open1(const char *name, int32_t *fd, int32_t *error) ALIAS("LINUX_open_readwrite");
void HIDRAW_receive(int32_t fd, void *buf, int32_t bufsize, int32_t *count, int32_t *error) ALIAS("LINUX_read");
void HIDRAW_send(int32_t fd, void *buf, int32_t bufsize, int32_t *count, int32_t *error) ALIAS("LINUX_write");
void I2C_close(int32_t fd, int32_t *error) ALIAS("LINUX_close");
void I2C_open(const char *name, int32_t *fd, int32_t *error) ALIAS("LINUX_open_readwrite");
void PWM_close(int32_t fd, int32_t *error) ALIAS("LINUX_close");
void SERIAL_close(int32_t fd, int32_t *error) ALIAS("LINUX_close");
void SERIAL_receive(int32_t fd, void *buf, int32_t bufsize, int32_t *count, int32_t *error) ALIAS("LINUX_read");
void SERIAL_send(int32_t fd, void *buf, int32_t bufsize, int32_t *count, int32_t *error) ALIAS("LINUX_write");
void SPI_close(int32_t fd, int32_t *error) ALIAS("LINUX_close");
void TCP4_close(int32_t fd, int32_t *error) ALIAS("LINUX_close");
void TCP4_receive(int32_t fd, void *buf, int32_t bufsize, int32_t *count, int32_t *error) ALIAS("LINUX_read");
void TCP4_send(int32_t fd, void *buf, int32_t bufsize, int32_t *count, int32_t *error) ALIAS("LINUX_write");
void UDP4_close(int32_t fd, int32_t *error) ALIAS("LINUX_close");
void WATCHDOG_close(int32_t fd, int32_t *error) ALIAS("LINUX_close");
void WATCHDOG_open(const char *name, int32_t *fd, int32_t *error) ALIAS("LINUX_open_readwrite");
