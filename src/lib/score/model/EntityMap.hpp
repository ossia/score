#pragma once
#include <score/model/IdentifiedObjectMap.hpp>

#include <nano_signal_slot.hpp>

#include <utility>

namespace score
{
template <typename T>
class Entity;
template <typename T, bool Ordered>
class EntityMap;
template <typename T, bool Ordered>
class EntityMapInserter
{
public:
  void add(EntityMap<T, Ordered>& map, T* t);
};

/**
 * @brief The EntityMap class
 *
 * This class is a wrapper over IdContainer.
 * Differences :
 *  - Deletes objects when they are removed ("ownership")
 *  - Sends signals after adding and before deleting.
 *
 * The parent of the childs are the parents of the map.
 * Hence the objects shall not be deleted upon deletion of the map
 * itself, to prevent a double-free.
 *
 */
template <typename T, bool Ordered = false>
class EntityMap
{
public:
  // The real interface starts here
  using value_type = T;
  OSSIA_INLINE auto begin() const INLINE_EXPORT { return m_map.begin(); }
  // OSSIA_INLINE auto rbegin() const INLINE_EXPORT { return m_map.rbegin(); }
  OSSIA_INLINE auto cbegin() const INLINE_EXPORT { return m_map.cbegin(); }
  OSSIA_INLINE auto end() const INLINE_EXPORT { return m_map.end(); }
  // OSSIA_INLINE auto rend() const INLINE_EXPORT { return m_map.rend(); }
  OSSIA_INLINE auto cend() const INLINE_EXPORT { return m_map.cend(); }
  OSSIA_INLINE auto size() const INLINE_EXPORT { return m_map.size(); }
  OSSIA_INLINE bool empty() const INLINE_EXPORT { return m_map.empty(); }
  OSSIA_INLINE auto& unsafe_map() INLINE_EXPORT { return m_map; }
  OSSIA_INLINE const auto& map() const INLINE_EXPORT { return m_map; }
  OSSIA_INLINE const auto& get() const INLINE_EXPORT { return m_map.m_map; }
  T& at(const Id<T>& id) INLINE_EXPORT { return m_map.at(id); }
  T& at(const Id<T>& id) const INLINE_EXPORT { return m_map.at(id); }
  auto find(const Id<T>& id) const INLINE_EXPORT { return m_map.find(id); }

  // public:
  mutable Nano::Signal<void(T&)> mutable_added;
  mutable Nano::Signal<void(const T&)> added;
  mutable Nano::Signal<void(const T&)> removing;
  mutable Nano::Signal<void(const T&)> removed;
  mutable Nano::Signal<void()> orderChanged;
  mutable Nano::Signal<void()> replaced;

  void add(T* t) INLINE_EXPORT { EntityMapInserter<T, Ordered>{}.add(*this, t); }

  void erase(T& elt) INLINE_EXPORT
  {
    removing(elt);
    m_map.remove(elt.id());
    removed(elt);
  }

  void remove(T& elt) INLINE_EXPORT
  {
    removing(elt);
    m_map.remove(elt.id());
    removed(elt);
    delete &elt;
  }

  void remove(T* elt) INLINE_EXPORT
  {
    removing(*elt);
    m_map.remove(elt->id());
    removed(*elt);
    delete elt;
  }

  void remove(const Id<T>& id) INLINE_EXPORT
  {
    auto it = m_map.m_map.find(id);
    if constexpr(Ordered)
    {
      auto& elt = *it->second.first;

      removing(elt);
      m_map.remove(it);
      removed(elt);
      delete &elt;
    }
    else
    {
      auto& elt = *it->second;

      removing(elt);
      m_map.remove(it);
      removed(elt);
      delete &elt;
    }
  }

  void clear() INLINE_EXPORT
  {
    while(!m_map.empty())
    {
      remove(*m_map.begin());
    }
  }

  void replace(IdContainer<T, T, Ordered>&& new_map)
  {
    for(T& m : m_map)
      delete &m;

    m_map.clear();
    m_map.m_map = std::move(new_map.m_map);
    if constexpr(Ordered)
    {
      m_map.m_order = std::move(new_map.m_order);
    }

    replaced();
  }

private:
  IdContainer<T, T, Ordered> m_map;
};

template <typename T, bool O>
void EntityMapInserter<T, O>::add(EntityMap<T, O>& map, T* t)
{
  SCORE_ASSERT(t);
  map.unsafe_map().insert(t);

  map.mutable_added(*t);
  map.added(*t);
}
}
