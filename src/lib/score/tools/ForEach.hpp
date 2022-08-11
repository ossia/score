#pragma once
#include <ossia/detail/ssize.hpp>

/**
 * Used since it seems that
 * this is the fastest way to iterate over
 * a Qt container :
 * http://blog.qt.io/blog/2009/01/23/iterating-efficiently/
 */
template <typename Array, typename F>
void Foreach(Array&& arr, F fun)
{
  const int n = std::ssize(arr);
  for(int i = 0; i < n; ++i)
  {
    fun(arr.at(i));
  }
}
