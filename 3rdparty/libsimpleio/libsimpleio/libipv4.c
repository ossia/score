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

#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "macros.inc"
#include "libipv4.h"

// Resolve host name to IPV4 address

void IPV4_resolve(const char *name, int32_t *addr, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (name == NULL)
  {
    *error = EINVAL;
    ERRORMSG("name argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (addr == NULL)
  {
    *error = EINVAL;
    ERRORMSG("addr argument is NULL", *error, __LINE__ - 3);
    return;
  }

  struct hostent *he = gethostbyname2(name, AF_INET);

  if (he == NULL)
  {
    // Map h_errno to errno

    switch(h_errno)
    {
      case HOST_NOT_FOUND :
      case NO_ADDRESS :
        *error = EIO;
        break;

      case TRY_AGAIN :
        *error = EAGAIN;
        ERRORMSG("gethostbyname2() failed", *error, __LINE__ - 15);
        break;

      default :
        *error = EIO;
        ERRORMSG("gethostbyname2() failed", *error, __LINE__ - 20);
        break;
    }

    return;
  }

  *addr = ntohl(*(int32_t *)he->h_addr);
  *error = 0;
}

// Convert IPV4 address to dotted decimal string

void IPV4_ntoa(int32_t addr, char *dst, int32_t dstsize, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (dst == NULL)
  {
    *error = EINVAL;
    ERRORMSG("dst argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (dstsize < 16)
  {
    *error = EINVAL;
    ERRORMSG("dstsize argument is too small", *error, __LINE__ - 3);
    return;
  }

  struct in_addr in;
  in.s_addr = htonl(addr);

  memset(dst, 0, dstsize);

  strncpy(dst, inet_ntoa(in), dstsize - 1);
  *error = 0;
}

// Connect to a TCP server

void TCP4_connect(int32_t addr, int32_t port, int32_t *fd, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if ((addr == 0x00000000) || (addr == 0xFFFFFFFF))
  {
    *error = EINVAL;
    ERRORMSG("addr argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if ((port < 1) || (port > 65535))
  {
    *error = EINVAL;
    ERRORMSG("port argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (fd == NULL)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is NULL", *error, __LINE__ - 3);
    return;
  }

  // Attempt to create a socket

  int s;
  struct sockaddr_in destaddr;

  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0)
  {
    *error = errno;
    ERRORMSG("socket() failed", *error, __LINE__ - 3);
    return;
  }

  // Build address structure for the specified server

  memset(&destaddr, 0, sizeof(destaddr));
  destaddr.sin_family = AF_INET;
  destaddr.sin_addr.s_addr = htonl(addr);
  destaddr.sin_port = htons(port);

  // Attempt to open connection to the server

  if (connect(s, (struct sockaddr *)&destaddr, sizeof(destaddr)))
  {
    *error = errno;
    ERRORMSG("connect() failed", *error, __LINE__ - 3);
    return;
  }

  // Prevent SIGPIPE

  signal(SIGPIPE, SIG_IGN);

  *fd = s;
  *error = 0;
}

// Wait (block) for exactly one connection from a TCP client, then
// return a file descriptor for the new connection

void TCP4_accept(int32_t addr, int32_t port, int32_t *fd, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (addr == 0xFFFFFFFF)
  {
    *error = EINVAL;
    ERRORMSG("addr argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if ((port < 1) || (port > 65535))
  {
    *error = EINVAL;
    ERRORMSG("port argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (fd == NULL)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is NULL", *error, __LINE__ - 3);
    return;
  }

  // Attempt to create a socket

  int s1, s2;
  struct sockaddr_in myaddr;

  s1 = socket(AF_INET, SOCK_STREAM, 0);
  if (s1 < 0)
  {
    *error = errno;
    ERRORMSG("socket() failed", *error, __LINE__ - 3);
    return;
  }

  // Attempt to bind socket

  memset(&myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(addr);
  myaddr.sin_port = htons(port);

  if (bind(s1, (struct sockaddr *)&myaddr, sizeof(myaddr)))
  {
    *error = errno;
    ERRORMSG("bind() failed", *error, __LINE__ - 3);
    return;
  }

  // Establish incoming connection queue

  if (listen(s1, 5))
  {
    *error = errno;
    ERRORMSG("listen() failed", *error, __LINE__ - 3);
    return;
  }

  // Wait for incoming connection

  s2 = accept(s1, NULL, NULL);
  if (s2 == -1)
  {
    *error = errno;
    ERRORMSG("accept() failed", *error, __LINE__ - 3);
    return;
  }

  close(s1);

  // Prevent SIGPIPE

  signal(SIGPIPE, SIG_IGN);

  *fd = s2;
  *error = 0;
}

// Wait (block) until a client connects, then fork and return a file
// descriptor to the child process

void TCP4_server(int32_t addr, int32_t port, int32_t *fd, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (addr == 0xFFFFFFFF)
  {
    *error = EINVAL;
    ERRORMSG("addr argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if ((port < 1) || (port > 65535))
  {
    *error = EINVAL;
    ERRORMSG("port argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (fd == NULL)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is NULL", *error, __LINE__ - 3);
    return;
  }

  // Attempt to create a socket

  int s1, s2;
  struct sockaddr_in myaddr;

  s1 = socket(AF_INET, SOCK_STREAM, 0);
  if (s1 < 0)
  {
    *error = errno;
    ERRORMSG("socket() failed", *error, __LINE__ - 3);
    return;
  }

  // Attempt to bind socket

  memset(&myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(addr);
  myaddr.sin_port = htons(port);

  if (bind(s1, (struct sockaddr *)&myaddr, sizeof(myaddr)))
  {
    *error = errno;
    ERRORMSG("bind() failed", *error, __LINE__ - 3);
    return;
  }

  // Establish incoming connection queue

  if (listen(s1, 5))
  {
    *error = errno;
    ERRORMSG("listen() failed", *error, __LINE__ - 3);
    return;
  }

  // Prevent zombie children

  signal(SIGCHLD, SIG_IGN);

  // Main service loop

  for (;;)
  {
    // Wait for incoming connection

    s2 = accept(s1, NULL, NULL);
    if (s2 == -1)
    {
      *error = errno;
      ERRORMSG("accept() failed", *error, __LINE__ - 3);
      return;
    }

    // Spawn child process for the new connection

    if (fork() == 0)
    {
      close(s1);

      // Prevent SIGPIPE

      signal(SIGPIPE, SIG_IGN);

      *error = 0;
      *fd = s2;
      return;
    }

    close(s2);
  }
}

// Open a UDP socket

void UDP4_open(int32_t addr, int32_t port, int32_t *fd, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (fd == NULL)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (port < 0)
  {
    *fd = -1;
    *error = EINVAL;
    ERRORMSG("port argument is invalid", *error, __LINE__ - 4);
    return;
  }

  // Create the UDP socket

  struct sockaddr_in srcaddr;

  *fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (*fd < 0)
  {
    *fd = -1;
    *error = errno;
    return;
  }

  // Bind the socket to the specified interface(s)

  memset(&srcaddr, 0, sizeof(srcaddr));

  srcaddr.sin_family = AF_INET;
  srcaddr.sin_addr.s_addr = htonl(addr);
  srcaddr.sin_port = htons(port);

  if (bind(*fd, (struct sockaddr *) &srcaddr, sizeof(srcaddr)) < 0)
  {
    close(*fd);
    *fd = -1;
    *error = errno;
    return;
  }

  *error = 0;
}

// Send a UDP datagram

void UDP4_send(int32_t fd, int32_t addr, int32_t port, void *buf,
  int32_t bufsize, int32_t flags, int32_t *count, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (fd < 0)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (port < 1)
  {
    *error = EINVAL;
    ERRORMSG("port argument is invalid", *error, __LINE__ - 3);
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

  // Build the destination address structure

  struct sockaddr_in dstaddr;

  memset(&dstaddr, 0, sizeof(dstaddr));

  dstaddr.sin_family = AF_INET;
  dstaddr.sin_addr.s_addr = htonl(addr);
  dstaddr.sin_port = htons(port);

  // Send the UDP datagram

  *count = sendto(fd, buf, bufsize, 0, (struct sockaddr *) &dstaddr,
    sizeof(dstaddr));

  if (*count < 0)
  {
    *count = 0;
    *error = errno;
    return;
  }

  *error = 0;
}

// Receive a UDP datagram

void UDP4_receive(int32_t fd, int32_t *addr, int32_t *port, void *buf,
  int32_t bufsize, int32_t flags, int32_t *count, int32_t *error)
{
  assert(error != NULL);

  // Validate parameters

  if (fd < 0)
  {
    *error = EINVAL;
    ERRORMSG("fd argument is invalid", *error, __LINE__ - 3);
    return;
  }

  if (addr == NULL)
  {
    *error = EINVAL;
    ERRORMSG("addr argument is NULL", *error, __LINE__ - 3);
    return;
  }

  if (port == NULL)
  {
    *error = EINVAL;
    ERRORMSG("port argument is NULL", *error, __LINE__ - 3);
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

  // Receive a UDP datagram

  struct sockaddr_in srcaddr;
  socklen_t addrlen = sizeof(srcaddr);

  memset(&srcaddr, 0, sizeof(srcaddr));

  *count = recvfrom(fd, buf, bufsize, flags, (struct sockaddr *) &srcaddr,
    &addrlen);

  if (*count < 0)
  {
    *count = 0;
    *error = errno;
    return;
  }

  // Retrieve sender IP address and UDP port number

  *addr = ntohl(srcaddr.sin_addr.s_addr);
  *port = ntohs(srcaddr.sin_port);
  *error = 0;
}
