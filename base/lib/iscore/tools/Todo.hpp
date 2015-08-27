#pragma once
#include <exception>
#include <QDebug>

#ifdef _MSC_VER
#define DEBUG_BREAK __debugbreak()
#else
#include <csignal>
#define DEBUG_BREAK std::raise(SIGTRAP)
#endif

#define ISCORE_TODO do { qDebug() << "TODO"; } while (0)
#if defined(ISCORE_DEBUG)
#define ISCORE_BREAKPOINT do { DEBUG_BREAK; } while (0)
#else
#define ISCORE_BREAKPOINT do {} while (0)
#endif

#ifdef ISCORE_DEBUG
#define ISCORE_ASSERT(arg) do { \
        bool b = (arg); \
        if(!b) { DEBUG_BREAK; Q_ASSERT( b ); } \
    } while (0)
#else
#define ISCORE_ASSERT(arg) ((void)(0))
#endif

#define ISCORE_ABORT do { DEBUG_BREAK; std::terminate(); } while(0)
