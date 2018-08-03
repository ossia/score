#pragma once
#include <QDebug>
#include <QObject>
#include <score_compiler_detection.hpp>
#include <score_lib_base_export.h>
#include <stdexcept>

#ifdef _WIN32
#  include <Windows.h>
#  define DEBUG_BREAK DebugBreak()
#else
#  include <csignal>
#  define DEBUG_BREAK std::raise(SIGTRAP)
#endif

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
#if defined(SCORE_DEBUG)
#  define SCORE_BREAKPOINT \
    do                     \
    {                      \
      DEBUG_BREAK;         \
    } while (0)
#else
#  define SCORE_BREAKPOINT \
    do                     \
    {                      \
    } while (0)
#endif

#ifdef SCORE_DEBUG
#  define SCORE_ASSERT(arg)          \
    do                               \
    {                                \
      bool score_assert_b = !!(arg); \
      if (!score_assert_b)           \
      {                              \
        DEBUG_BREAK;                 \
        Q_ASSERT(score_assert_b);    \
      }                              \
    } while (false)
#else
#  define SCORE_ASSERT(arg)                       \
    do                                            \
    {                                             \
      bool score_assert_b = !!(arg);              \
      if (!score_assert_b)                        \
      {                                           \
        throw std::runtime_error("Error: " #arg); \
      }                                           \
    } while (false)
#endif

#define SCORE_ABORT   \
  do                  \
  {                   \
    DEBUG_BREAK;      \
    std::terminate(); \
  } while (0)

#define SCORE_XSTR(s) SCORE_STR(s)
#define SCORE_STR(s) #s

#if SCORE_COMPILER_CXX_RELAXED_CONSTEXPR
#  define SCORE_RELAXED_CONSTEXPR constexpr
#else
#  define SCORE_RELAXED_CONSTEXPR
#endif

#if defined(Q_CC_MSVC)
  #define INLINE_EXPORT
#else
  #if defined(SCORE_STATIC_PLUGINS)
    #define INLINE_EXPORT
  #else
    #define INLINE_EXPORT Q_DECL_EXPORT
  #endif
#endif

template <typename T>
using remove_qualifs_t = std::decay_t<std::remove_pointer_t<std::decay_t<T>>>;

template <typename T>
using add_cref_t = std::add_lvalue_reference_t<std::add_const_t<T>>;

#ifdef SCORE_DEBUG
template <typename T, typename U>
T safe_cast(U* other)
{ // there is also a static_cast since compilers
  // must ensure that the downcast is possible, which it
  // does not with dynamic_cast
  auto check = static_cast<T>(other);
  auto res = dynamic_cast<T>(other);
  SCORE_ASSERT(res);
  return res;
}

template <typename T, typename U>
T safe_cast(U&& other) try
{
  auto&& check = static_cast<T>(other);
  auto&& res = dynamic_cast<T>(other);
  return res;
}
catch (const std::exception& e)
{
  qDebug() << e.what();
  SCORE_ABORT;
}
#else
#  define safe_cast static_cast
#endif

/**
 * @brief con A wrapper around Qt's connect
 *
 * Allows the first argument to be a reference
 */
template <typename T, typename... Args>
QMetaObject::Connection con(const T& t, Args&&... args)
{
  return QObject::connect(&t, std::forward<Args>(args)...);
}

template <typename T, typename Property, typename U, typename Slot, typename... Args>
QMetaObject::Connection bind(T& t, const Property&, const U* tgt, Slot&& slt, Args&&... args)
{
  slt((t.*(Property::get()))());

  return QObject::connect(&t, Property::notify(), tgt, std::forward<Slot>(slt), std::forward<Args>(args)...);
}


/**
 * Used since it seems that
 * this is the fastest way to iterate over
 * a Qt container :
 * http://blog.qt.io/blog/2009/01/23/iterating-efficiently/
 */
template <typename Array, typename F>
void Foreach(Array&& arr, F fun)
{
  int n = arr.size();
  for (int i = 0; i < n; i++)
  {
    fun(arr.at(i));
  }
}

struct unused_t
{
  template <typename... Args>
  unused_t(Args&&...)
  {
  }
};

template <typename T>
struct matches
{
  bool operator()(const QObject* obj)
  {
    return dynamic_cast<const T*>(obj);
  }
};

/**
 * @brief matches
 * @return <= 0 : does not match
 * > 0 : matches. The highest priority should be taken.
 */
using Priority = int32_t;
