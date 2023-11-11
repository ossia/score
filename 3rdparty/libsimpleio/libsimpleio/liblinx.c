// LabView LINX device firmware definitions

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
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "liblinx.h"

void LINX_transmit_command(int32_t fd, LINX_command_t *cmd, int32_t *error)
{
  assert(error != NULL);

  uint8_t *p = (uint8_t *) cmd;
  uint8_t checksum = 0;
  int i;
  int status;

  // Validate parameters

  if (fd < 0)
  {
    *error = EINVAL;
    return;
  }

  if (cmd == NULL)
  {
    *error = EINVAL;
    return;
  }

  // Validate frame structure

  if (cmd->SoF != LINX_SOF)
  {
    *error = EINVAL;
    return;
  }

  if ((cmd->PacketSize < 7) || (cmd->PacketSize > sizeof(LINX_command_t)))
  {
    *error = EINVAL;
    return;
  }

  // Convert PacketNum and Command fields to network byte order

  cmd->PacketNum = htons(cmd->PacketNum);
  cmd->Command = htons(cmd->Command);

  // Calculate packet checksum

  for (i = 0; i < cmd->PacketSize - 1; i++)
    checksum += *p++;

  cmd->Args[cmd->PacketSize-7] = checksum;

  // Transmit the frame

  status = write(fd, cmd, cmd->PacketSize);
  if (status < 0)
  {
    *error = errno;
    return;
  }

  *error = 0;
}

void LINX_receive_command(int32_t fd, LINX_command_t *cmd, int32_t *count, int32_t *error)
{
  assert(error != NULL);

  int status;
  uint8_t b;
  uint8_t checksum;
  uint8_t *p;
  int i;

  // Validate parameters

  if (fd < 0)
  {
    *error = EINVAL;
    return;
  }

  if (cmd == NULL)
  {
    *error = EINVAL;
    return;
  }

  if (count == NULL)
  {
    *error = EINVAL;
    return;
  }

  // Check for buffer overrun

  if (*count >= sizeof(LINX_command_t))
  {
    *count = 0;
    *error = EINVAL;
    return;
  }

  // Read next incoming byte

  status = read(fd, &b, 1);

  // read() failed

  if (status < 0)
  {
    *count = 0;
    *error = errno;
    return;
  }

  // No data available, connection was closed

  if (status == 0)
  {
    *count = 0;
    *error = EPIPE;
    return;
  }

  // Assemble frame

  switch (*count)
  {
    case 0 :
      if (b != LINX_SOF)
      {
        *count = 0;
        *error = EINVAL;
        return;
      }
      cmd->SoF = b;
      break;

    case 1 :
      if ((b < 7) || (b > sizeof(LINX_command_t)))
      {
        *count = 0;
        *error = EINVAL;
        return;
      }
      cmd->PacketSize = b;
      break;

    case 2 :
      cmd->PacketNum = b << 8;
      break;

    case 3 :
      cmd->PacketNum += b;
      break;

    case 4 :
      cmd->Command = b << 8;
      break;

    case 5 :
      cmd->Command += b;
      break;

    default :
      cmd->Args[*count-6] = b;
      break;
  }

  // Increment byte counter

  *count += 1;

  // Check for complete frame

  if ((*count < 6) || (*count < cmd->PacketSize))
  {
    *error = EAGAIN;
    return;
  }

  // Calculate frame checksum

  p = (uint8_t *) cmd;

  checksum = 0;

  for (i = 0; i < *count-1; i++)
    checksum += p[i];

  // Validate frame checksum

  if (checksum != cmd->Args[*count-7])
  {
    *count = 0;
    *error = EINVAL;
    return;
  }

  // Frame is complete and valid

  *count = 0;
  *error = 0;
}

void LINX_transmit_response(int32_t fd, LINX_response_t *resp, int32_t *error)
{
  assert(error != NULL);

  uint8_t *p = (uint8_t *) resp;
  uint8_t checksum = 0;
  int i;
  int status;

  // Validate parameters

  if (fd < 0)
  {
    *error = EINVAL;
    return;
  }

  if (resp == NULL)
  {
    *error = EINVAL;
    return;
  }

  // Validate frame structure

  if (resp->SoF != LINX_SOF)
  {
    *error = EINVAL;
    return;
  }

  if ((resp->PacketSize < 6) || (resp->PacketSize > sizeof(LINX_response_t)))
  {
    *error = EINVAL;
    return;
  }

  // Convert PacketSize and Command fields to network byte order

  resp->PacketNum = htons(resp->PacketNum);

  // Calculate packet checksum

  for (i = 0; i < resp->PacketSize - 1; i++)
    checksum += *p++;

  resp->Data[resp->PacketSize-6] = checksum;

  // Transmit the frame

  status = write(fd, resp, resp->PacketSize);
  if (status < 0)
  {
    *error = errno;
    return;
  }

  *error = 0;
}

void LINX_receive_response(int32_t fd, LINX_response_t *resp, int32_t *count, int32_t *error)
{
  assert(error != NULL);

  int status;
  uint8_t b;
  uint8_t checksum;
  uint8_t *p;
  int i;

  // Validate parameters

  if (fd < 0)
  {
    *error = EINVAL;
    return;
  }

  if (resp == NULL)
  {
    *error = EINVAL;
    return;
  }

  if (count == NULL)
  {
    *error = EINVAL;
    return;
  }

  // Check for buffer overrun

  if (*count >= sizeof(LINX_response_t))
  {
    *count = 0;
    *error = EINVAL;
    return;
  }

  // Read next incoming byte

  status = read(fd, &b, 1);

  // read() failed

  if (status < 0)
  {
    *count = 0;
    *error = errno;
    return;
  }

  // No data available, connection was closed

  if (status == 0)
  {
    *count = 0;
    *error = EPIPE;
    return;
  }

  // Assemble frame

  switch (*count)
  {
    case 0 :
      if (b != LINX_SOF)
      {
        *count = 0;
        *error = EINVAL;
        return;
      }
      resp->SoF = b;
      break;

    case 1 :
      if ((b < 6) || (b > sizeof(LINX_response_t)))
      {
        *count = 0;
        *error = EINVAL;
        return;
      }
      resp->PacketSize = b;
      break;

    case 2 :
      resp->PacketNum = b << 8;
      break;

    case 3 :
      resp->PacketNum += b;
      break;

    case 4 :
      resp->Status = b;
      break;

    default :
      resp->Data[*count-5] = b;
      break;
  }

  // Increment byte counter

  *count += 1;

  // Check for complete frame

  if ((*count < 5) || (*count < resp->PacketSize))
  {
    *error = EAGAIN;
    return;
  }

  // Calculate frame checksum

  p = (uint8_t *) resp;

  checksum = 0;

  for (i = 0; i < *count-1; i++)
    checksum += p[i];

  // Validate frame checksum

  if (checksum != resp->Data[*count-6])
  {
    *count = 0;
    *error = EINVAL;
    return;
  }

  *count = 0;
  *error = 0;
}

uint16_t LINX_makeu16(uint8_t b0, uint8_t b1)
{
  return (b0 << 8) + b1;
}

uint32_t LINX_makeu32(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)
{
  return (b0 << 24) + (b1 << 16) + (b2 << 8) + b3;
}

uint8_t LINX_splitu16(uint16_t u, int32_t bn)
{
  return (u >> ((1-bn)*8)) & 0xFF;
}

uint8_t LINX_splitu32(uint32_t u, int32_t bn)
{
  return (u >> ((3-bn)*8)) & 0xFF;
}
