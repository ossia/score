
#if defined(_MSC_VER)
#define STK_EXPORT extern "C" __declspec(dllexport)
#elif defined(_WIN32)
#define STK_EXPORT extern "C" __attribute__((visibility("default"))) __declspec(dllexport)
#else
#define STK_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#include "bass.h"
#include "harpsichord.h"
#include "instrument.h"
#include "modalBar.h"
#include "phonemes.h"
#include "piano.h"
