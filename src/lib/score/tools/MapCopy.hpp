#pragma once
#include <score/model/EntityMap.hpp>

#include <vector>

template <typename T, typename M, bool B>
std::vector<T*> shallow_copy(const IdContainer<T, M, B>& m)
{
  return m.as_vec();
}

template <typename T, bool B>
std::vector<T*> shallow_copy(const score::EntityMap<T, B>& m)
{
  return shallow_copy(m.map());
}
