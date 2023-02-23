#pragma once
#include <score/model/ComponentSerialization.hpp>
#include <score/model/EntityBase.hpp>

#include <ossia-qt/name_utils.hpp>

namespace score
{
template <typename T, bool Ordered>
class EntityMapInserter<score::Entity<T>, Ordered>
{
  void add(EntityMap<T, Ordered>& map, T* obj)
  {
    SCORE_ASSERT(obj);

    std::vector<QString> bros_names;
    bros_names.reserve(map.size());
    std::transform(
        map.begin(), map.end(), std::back_inserter(bros_names),
        [&](const auto& res) { bros_names.push_back(res.metadata().getName()); });

    auto new_name = ossia::net::sanitize_name(obj->metadata().getName(), bros_names);
    obj->metadata().setName(new_name);

    map.unsafe_map().insert(obj);

    map.mutable_added(*obj);
    map.added(*obj);

    // If there are serialized components, we try to deserialize them
    score::Components& comps = obj->components();
    deserializeRemainingComponents(comps, obj);
  }
};
}
