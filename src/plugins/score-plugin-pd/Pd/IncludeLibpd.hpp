#pragma once

// We do this to prevent picking up a potential system /usr/inclde/m_pd.h
#include <libpd/pure-data/src/m_pd.h>

// Needed under Win32 as lld does not seem to be able to link to stuff declared "__declspec(dllimport) extern"
#if defined(_WIN32)
#if defined(EXTERN)
#undef EXTERN
#define EXTERN
#endif
#endif

#include <z_libpd.h>
