#pragma once
#include <ossia/detail/config.hpp>
#include <iscore/serialization/IsTemplate.hpp>
#include <hopscotch_map.h>
namespace iscore
{
template<typename... Args>
using hash_map = tsl::hopscotch_map<Args...>;

template<typename Map>
void optimize_hash_map(Map& map)
{
  map.max_load_factor(0.1f);
  map.reserve(map.size());
}
}

template <typename T, typename U>
struct is_template<iscore::hash_map<T, U>> : std::true_type
{
};
