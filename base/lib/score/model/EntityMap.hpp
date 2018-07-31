#pragma once
#include <array>
#include <iostream>
#include <nano_signal_slot.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <utility>

namespace score
{
template <typename T>
class Entity;
template <typename T>
class EntityMap;
template <typename T>
class EntityMapInserter
{
public:
  void add(EntityMap<T>& map, T* t);
};

/**
 * @brief The EntityMap class
 *
 * This class is a wrapper over IdContainer.
 * Differences :
 *  - Deletes objects when they are removed ("ownership")
 *  - Sends signals after adding and before deleting.
 *
 * Tthe parent of the childs are the parents of the map.
 * Hence the objects shall not be deleted upon deletion of the map
 * itself, to prevent a double-free.
 *
 */
template <typename T>
class EntityMap
{
public:
  // The real interface starts here
  using value_type = T;
  auto begin() const INLINE_EXPORT
  {
    return m_map.begin();
  }
  auto rbegin() const INLINE_EXPORT
  {
    return m_map.rbegin();
  }
  auto cbegin() const INLINE_EXPORT
  {
    return m_map.cbegin();
  }
  auto end() const INLINE_EXPORT
  {
    return m_map.end();
  }
  auto rend() const INLINE_EXPORT
  {
    return m_map.rend();
  }
  auto cend() const INLINE_EXPORT
  {
    return m_map.cend();
  }
  auto size() const INLINE_EXPORT
  {
    return m_map.size();
  }
  bool empty() const INLINE_EXPORT
  {
    return m_map.empty();
  }
  auto& unsafe_map() INLINE_EXPORT
  {
    return m_map;
  }
  const auto& map() const INLINE_EXPORT
  {
    return m_map;
  }
  const auto& get() const INLINE_EXPORT
  {
    return m_map.m_map;
  }
  T& at(const Id<T>& id) INLINE_EXPORT
  {
    return m_map.at(id);
  }
  T& at(const Id<T>& id) const INLINE_EXPORT
  {
    return m_map.at(id);
  }
  auto find(const Id<T>& id) const INLINE_EXPORT
  {
    return m_map.find(id);
  }

  // public:
  mutable Nano::Signal<void(T&)> mutable_added;
  mutable Nano::Signal<void(const T&)> added;
  mutable Nano::Signal<void(const T&)> removing;
  mutable Nano::Signal<void(const T&)> removed;
  mutable Nano::Signal<void()> orderChanged;

  void add(T* t) INLINE_EXPORT
  {
    EntityMapInserter<T>{}.add(*this, t);
  }

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
    auto& elt = *it->second.first;

    removing(elt);
    m_map.remove(it);
    removed(elt);
    delete &elt;
  }

  void clear() INLINE_EXPORT
  {
    while (!m_map.empty())
    {
      remove(*m_map.begin());
    }
  }

private:
  IdContainer<T> m_map;
};

template <typename T>
void EntityMapInserter<T>::add(EntityMap<T>& map, T* t)
{
  SCORE_ASSERT(t);
  map.unsafe_map().insert(t);

  map.mutable_added(*t);
  map.added(*t);
}
}
