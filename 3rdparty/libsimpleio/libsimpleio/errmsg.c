// Define macros for display error messages

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

void PrintErrorMessage(const char *func, const char *file, int line, const char *msg, int err)
{
  char *slevel = getenv("DEBUGLEVEL");
  if (slevel == NULL) return;

  int ilevel = atoi(slevel);

  switch (ilevel)
  {
    case 1 :
      fprintf(stderr, "ERROR in %s(), at %s line %d: %s, %s\n", func, file, line, msg, strerror(err));
      break;

    case 2 :
      syslog(LOG_ERR, "ERROR in %s(), at %s line %d: %s, %s\n", func, file, line, msg, strerror(err));
      break;

    case 3 :
      fprintf(stderr, "ERROR in %s(), at %s line %d: %s, %s\n", func, file, line, msg, strerror(err));
      syslog(LOG_ERR, "ERROR in %s(), at %s line %d: %s, %s\n", func, file, line, msg, strerror(err));
      break;

    default :
      break;
  }
}
