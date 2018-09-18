#pragma once
#include <score/serialization/IsTemplate.hpp>

#include <ossia/detail/config.hpp>

#include <tsl/hopscotch_map.h>
namespace score
{
template <typename... Args>
using hash_map = tsl::hopscotch_map<Args...>;

template <typename Map>
void optimize_hash_map(Map& map)
{
  map.max_load_factor(0.1f);
  map.reserve(map.size());
}
}

template <typename T, typename U>
struct is_template<score::hash_map<T, U>> : std::true_type
{
};
