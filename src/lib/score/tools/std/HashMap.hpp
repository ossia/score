#pragma once
#include <score/serialization/IsTemplate.hpp>

#include <ossia/detail/config.hpp>

#include <ossia/detail/hash_map.hpp>
namespace score
{
template <class Key, class T>
using hash_map = ossia::hash_map<Key, T>;

template <typename Map>
void optimize_hash_map(Map& map)
{
  map.max_load_factor(0.1f);
  map.reserve(map.size());
}
}

template <typename T, typename U>
struct is_template<ossia::hash_map<T, U>> : std::true_type
{
};
