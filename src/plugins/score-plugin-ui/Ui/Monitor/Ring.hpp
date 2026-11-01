#pragma once
#include <boost/circular_buffer.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

namespace Ui::Monitor
{
/**
 * Appends to a history kept as a ring, oldest first. The ring grows as
 * needed up to `limit` items; past it, the oldest item gives way to the new
 * one.
 */
template <typename T, typename V>
void pushRing(
    boost::circular_buffer<T>& ring, V&& v,
    std::size_t limit = std::numeric_limits<std::size_t>::max())
{
  if(ring.full() && ring.capacity() < limit)
    ring.set_capacity(std::min(limit, std::max<std::size_t>(16, ring.capacity() * 2)));
  ring.push_back(std::forward<V>(v));
}
}
