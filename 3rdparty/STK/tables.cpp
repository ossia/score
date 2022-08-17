
#if defined(_MSC_VER)
#define STK_EXPORT __declspec(dllexport)
#else
#define STK_EXPORT extern "C" [[gnu::visibility("default")]]
#endif



#include "bass.h"
#include "harpsichord.h"
#include "instrument.h"
#include "modalBar.h"
#include "phonemes.h"
#include "piano.h"
