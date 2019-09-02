#pragma once
#include <score/tools/Debug.hpp>

#include <exception>

#ifdef SCORE_DEBUG
template <typename T, typename U>
T safe_cast(U* other)
{ // there is also a static_cast since compilers
  // must ensure that the downcast is possible, which it
  // does not with dynamic_cast
  [[maybe_unused]] auto check = static_cast<T>(other);
  auto res = dynamic_cast<T>(other);
  SCORE_ASSERT(res);
  return res;
}

template <typename T, typename U>
T safe_cast(U&& other)
try
{
  [[maybe_unused]] auto&& check = static_cast<T>(other);
  auto&& res = dynamic_cast<T>(other);
  return res;
}
catch (const std::exception& e)
{
  qDebug() << e.what();
  SCORE_ABORT;
}
#else
#define safe_cast static_cast
#endif
