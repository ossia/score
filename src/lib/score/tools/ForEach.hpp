#pragma once

/**
 * Used since it seems that
 * this is the fastest way to iterate over
 * a Qt container :
 * http://blog.qt.io/blog/2009/01/23/iterating-efficiently/
 */
template <typename Array, typename F>
void Foreach(Array&& arr, F fun)
{
  const int n = arr.size();
  for (int i = 0; i < n; ++i)
  {
    fun(arr.at(i));
  }
}
