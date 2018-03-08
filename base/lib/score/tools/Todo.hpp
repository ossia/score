#pragma once
#include <QDebug>
#include <QObject>
#include <stdexcept>
#include <score_compiler_detection.hpp>
#include <score_lib_base_export.h>
#include <typeinfo>

#ifdef _WIN32
#include <Windows.h>
#define DEBUG_BREAK DebugBreak()
#else
#include <csignal>
#define DEBUG_BREAK std::raise(SIGTRAP)
#endif

#define SCORE_TODO                    \
  do                                   \
  {                                    \
    static bool score_todo_b = false; \
    if (!score_todo_b)                \
    {                                  \
      qDebug() << "TODO";              \
      score_todo_b = true;            \
    }                                  \
  } while (0)
#define SCORE_TODO_(Str)              \
  do                                   \
  {                                    \
    static bool score_todo_b = false; \
    if (!score_todo_b)                \
    {                                  \
      qDebug() << "TODO: " << (Str);   \
      score_todo_b = true;            \
    }                                  \
  } while (0)
#if defined(SCORE_DEBUG)
#define SCORE_BREAKPOINT \
  do                      \
  {                       \
    DEBUG_BREAK;          \
  } while (0)
#else
#define SCORE_BREAKPOINT \
  do                      \
  {                       \
  } while (0)
#endif

#ifdef SCORE_DEBUG
#define SCORE_ASSERT(arg)          \
  do                                \
  {                                 \
    bool score_assert_b = !!(arg); \
    if (!score_assert_b)           \
    {                               \
      DEBUG_BREAK;                  \
      Q_ASSERT(score_assert_b);    \
    }                               \
  } while (false)
#else
#define SCORE_ASSERT(arg)          \
  do                                \
  {                                 \
    bool score_assert_b = !!(arg); \
    if (!score_assert_b)           \
    {                               \
      throw std::runtime_error("Error: " #arg );    \
    }                               \
  } while (false)
#endif

#define SCORE_ABORT  \
  do                  \
  {                   \
    DEBUG_BREAK;      \
    std::terminate(); \
  } while (0)

#define SCORE_XSTR(s) SCORE_STR(s)
#define SCORE_STR(s) #s

#if SCORE_COMPILER_CXX_RELAXED_CONSTEXPR
#define SCORE_RELAXED_CONSTEXPR constexpr
#else
#define SCORE_RELAXED_CONSTEXPR
#endif

template <typename T>
using remove_qualifs_t = std::decay_t<std::remove_pointer_t<std::decay_t<T>>>;

template <typename T>
using add_cref_t = std::add_lvalue_reference_t<std::add_const_t<T>>;

#ifdef SCORE_DEBUG
template <typename T, typename U>
T safe_cast(U* other)
{
  auto res = dynamic_cast<T>(other);
  SCORE_ASSERT(res);
  return res;
}

template <typename T, typename U>
T safe_cast(U&& other) try
{
  auto&& res = dynamic_cast<T>(other);
  return res;
}
catch (std::bad_cast& e)
{
  qDebug() << e.what();
  SCORE_ABORT;
}

#else
#define safe_cast static_cast
#endif

/**
 * @brief The ptr struct
 * Reduces the chances of UB
 */
template <typename T>
struct ptr
{
  T* impl{};
  ptr() = default;
  ptr(const ptr&) = default;
  ptr(ptr&&) = default;
  ptr& operator=(const ptr&) = default;
  ptr& operator=(ptr&&) = default;

  ptr(T* p) : impl{p}
  {
  }

  auto operator=(T* other)
  {
    impl = other;
  }

  operator bool() const
  {
    return impl;
  }

  operator T*() const
  {
    return impl;
  }

  auto operator*() const -> decltype(auto)
  {
    SCORE_ASSERT(impl);
    return *impl;
  }

  T* operator->() const
  {
    SCORE_ASSERT(impl);
    return impl;
  }

  void free()
  {
    ::delete impl;
    impl = nullptr;
  }
};

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

template <typename T, typename... Args>
QMetaObject::Connection con(ptr<T> t, Args&&... args)
{
  return QObject::connect(&*t, std::forward<Args>(args)...);
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

/**
 * Macros to define a property type, e.g. an
 * abstraction over foo.getBar(); foo.setBar();
 * etc..
 */
#define SCORE_PARAMETER_TYPE(ModelType, Name)                          \
  struct ModelType##Name##Parameter                                     \
  {                                                                     \
    using model_type = ModelType;                                       \
    using param_type = decltype(std::declval<ModelType>().get##Name()); \
    static constexpr auto get()                                         \
    {                                                                   \
      return &model_type::get##Name;                                    \
    }                                                                   \
    static constexpr auto set()                                         \
    {                                                                   \
      return &model_type::set##Name;                                    \
    }                                                                   \
    static constexpr auto notify()                                      \
    {                                                                   \
      return &model_type::Name##Changed;                                \
    }                                                                   \
  };

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
