#pragma once

#if !defined(SCORE_ALL_UNITY) && !defined(__MINGW32__) && defined(__clang__)
#define SCORE_EXTERN_TEMPLATES_IN_SHARED_LIBRARIES 1
#endif
