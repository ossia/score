#pragma once

#if !defined(SCORE_ALL_UNITY) && !defined(__MINGW32__) && defined(__clang__)
#define SCORE_EXTERN_TEMPLATES_IN_SHARED_LIBRARIES 1
#endif

#if defined(__clang__)
#define SCORE_CLANG_NO_SANITIZE_INTEGER \
  __attribute__((no_sanitize("undefined", "integer")))
#else
#define SCORE_CLANG_NO_SANITIZE_INTEGER
#endif
