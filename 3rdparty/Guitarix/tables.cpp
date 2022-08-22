#define __rt_data
#define __rt_func

#if defined(_MSC_VER)
#define GUITARIX_EXPORT extern "C" __declspec(dllexport)
#define always_inline
#elif defined(_WIN32)
#define GUITARIX_EXPORT extern "C" __attribute__((visibility("default"))) __declspec(dllexport)
#define always_inline
#else
#define GUITARIX_EXPORT  extern "C"  __attribute__((visibility("default")))
#define always_inline  __attribute__((always_inline))
#endif

#include "clipping.h"
#include "epiphone_jr_out_neg_table.h"
#include "epiphone_jr_out_table.h"
#include "orangedarkterrorp3_neg_table.h"
#include "orangedarkterrorp3_table.h"
#include "plexipowerampel34_neg_table.h" 
#include "plexipowerampel34_table.h"
#include "princeton_table.h"
#include "supersonic_neg_table.h"
#include "supersonic_table.h"
#include "trany.h"
#include "tweedchamp_neg_table.h"
#include "tweedchamp_table.h"
#include "valve.h"
#undef always_inline
