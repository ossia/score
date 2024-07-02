/* epoll wrapper services for Linux */

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
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "macros.inc"
#include "libevent.h"

void EVENT_open(int32_t *epfd, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (epfd == NULL)
  {
    *error = EINVAL;
    ERRORMSG("epfd argument is NULL", *error, __LINE__ - 3);
    return;
  }

  *epfd = epoll_create(256);
  if (*epfd < 0)
  {
    *epfd = -1;
    *error = errno;
    ERRORMSG("epoll_create() failed", *error, __LINE__ - 5);
    return;
  }

  *error = 0;
}

void EVENT_register_fd(int32_t epfd, int32_t fd, int32_t events,
  int32_t handle, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (epfd < 0)
  {
    *error = EINVAL;
    ERRORMSG("epfd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (fd < 0)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  struct epoll_event ev;

  memset(&ev, 0, sizeof(ev));
  ev.events = events;
  ev.data.fd = fd;
  ev.data.u32 = handle;

  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev))
  {
    *error = errno;
    ERRORMSG("epoll_ctl() failed", *error, __LINE__ - 3);
    return;
  }

  *error = 0;
}

void EVENT_modify_fd(int32_t epfd, int32_t fd, int32_t events,
  int32_t handle, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (epfd < 0)
  {
    *error = EINVAL;
    ERRORMSG("epfd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (fd < 0)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  struct epoll_event ev;

  memset(&ev, 0, sizeof(ev));
  ev.events = events;
  ev.data.fd = fd;
  ev.data.u32 = handle;

  if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev))
  {
    *error = errno;
    ERRORMSG("epoll_ctl() failed", *error, __LINE__ - 3);
    return;
  }

  *error = 0;
}

void EVENT_unregister_fd(int32_t epfd, int32_t fd, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (epfd < 0)
  {
    *error = EINVAL;
    ERRORMSG("epfd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (fd < 0)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL))
  {
    *error = errno;
    ERRORMSG("epoll_ctl() failed", *error, __LINE__ - 3);
    return;
  }

  *error = 0;
}

void EVENT_wait(int32_t epfd, int32_t *fd, int32_t *event,
  int32_t *handle, int32_t timeoutms, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (epfd < 0)
  {
    *error = EINVAL;
    ERRORMSG("epfd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (fd == NULL)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (event == NULL)
  {
    *error = EINVAL;
    ERRORMSG("event argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (handle == NULL)
  {
    *error = EINVAL;
    ERRORMSG("handle argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (timeoutms < -1)
  {
    *error = EINVAL;
    ERRORMSG("timeut argument is invalid", *error, __LINE__ - 3);
    return;
  }

  int status;
  struct epoll_event ev;

  memset(&ev, 0, sizeof(ev));

  status = epoll_wait(epfd, &ev, 1, timeoutms);

  // An error occurred:

  if (status < 0)
  {
    *fd = 0;
    *event = 0;
    *handle = 0;
    *error = errno;
    ERRORMSG("epoll_wait() failed", *error, __LINE__ - 3);
    return;
  }

  // No events are available:

  if (status == 0)
  {
    *fd = 0;
    *event = 0;
    *handle = 0;
    *error = EAGAIN;
    return;
  }

  // An event occurred:

  *fd  = ev.data.fd;
  *event = ev.events;
  *handle = ev.data.u32;
  *error = 0;
}
