#pragma once

#if defined(SCORE_DEBUG)
#include <QDebug>

#ifdef _WIN32
#if defined(_MSC_VER)
#include <windows.h>
#endif
#include <debugapi.h>
#define DEBUG_BREAK DebugBreak()
#else
#include <csignal>
#define DEBUG_BREAK std::raise(SIGTRAP)
#endif
#else
#include <stdexcept>
#endif

#if defined(SCORE_DEBUG)
#define SCORE_TODO                    \
  do                                  \
  {                                   \
    static bool score_todo_b = false; \
    if (!score_todo_b)                \
    {                                 \
      qDebug() << "TODO";             \
      score_todo_b = true;            \
    }                                 \
  } while (0)

#define SCORE_TODO_(Str)              \
  do                                  \
  {                                   \
    static bool score_todo_b = false; \
    if (!score_todo_b)                \
    {                                 \
      qDebug() << "TODO: " << (Str);  \
      score_todo_b = true;            \
    }                                 \
  } while (0)

#define SCORE_BREAKPOINT \
  do                     \
  {                      \
    DEBUG_BREAK;         \
  } while (0)

#else
#define SCORE_TODO \
  do               \
  {                \
  } while (0)
#define SCORE_TODO_(Str) \
  do                     \
  {                      \
  } while (0)
#define SCORE_BREAKPOINT \
  do                     \
  {                      \
  } while (0)
#endif

#ifdef SCORE_DEBUG
#define SCORE_ASSERT(arg)                               \
  do                                                    \
  {                                                     \
    if (bool score_assert_b = !!(arg); !score_assert_b) \
    {                                                   \
      DEBUG_BREAK;                                      \
      Q_ASSERT((#arg, false));                          \
    }                                                   \
  } while (false)
#else
#define SCORE_ASSERT(arg)                               \
  do                                                    \
  {                                                     \
    if (bool score_assert_b = !!(arg); !score_assert_b) \
    {                                                   \
      throw std::runtime_error("Error: " #arg);         \
    }                                                   \
  } while (false)
#endif

#define SCORE_SOFT_ASSERT(arg)                          \
  do                                                    \
  {                                                     \
    if (bool score_assert_b = !!(arg); !score_assert_b) \
    {                                                   \
      SCORE_BREAKPOINT;                                 \
      qDebug() << "Error: " #arg;                       \
    }                                                   \
  } while (false)

#define SCORE_ABORT \
  do                \
  {                 \
    std::abort();   \
  } while (0)

#define SCORE_XSTR(s) SCORE_STR(s)
#define SCORE_STR(s) #s

// TODO reconsider this
#if defined(Q_CC_MSVC)
#define INLINE_EXPORT
#else
#if defined(SCORE_STATIC_PLUGINS)
#define INLINE_EXPORT
#else
#define INLINE_EXPORT Q_DECL_EXPORT
#endif
#endif
