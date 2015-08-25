#pragma once
#include <QDebug>
#define ISCORE_TODO do { qDebug() << "TODO"; } while (0)
#if defined(ISCORE_DEBUG)
#define ISCORE_BREAKPOINT do { __asm__("int3"); } while (0)
#else
#define ISCORE_BREAKPOINT do {} while (0)
#endif

#ifdef ISCORE_DEBUG
#define ISCORE_ASSERT(arg) do { \
        bool b = (arg); \
        if(!b) { __asm__("int3"); Q_ASSERT( b ); } \
    } while (0)
#else
#define ISCORE_ASSERT(arg) ((void)(0))
#endif

#define ISCORE_ABORT ISCORE_ASSERT(false)
