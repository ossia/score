#pragma once
#include <array>
#include <iostream>
#include <score/model/IdentifiedObjectMap.hpp>
#include <nano_signal_slot.hpp>
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
  auto begin() const
  {
    return m_map.begin();
  }
  auto cbegin() const
  {
    return m_map.cbegin();
  }
  auto end() const
  {
    return m_map.end();
  }
  auto cend() const
  {
    return m_map.cend();
  }
  auto size() const
  {
    return m_map.size();
  }
  bool empty() const
  {
    return m_map.empty();
  }
  auto& unsafe_map()
  {
    return m_map;
  }
  const auto& map() const
  {
    return m_map;
  }
  const auto& get() const
  {
    return m_map.get();
  }
  T& at(const Id<T>& id)
  {
    return m_map.at(id);
  }
  T& at(const Id<T>& id) const
  {
    return m_map.at(id);
  }
  auto find(const Id<T>& id) const
  {
    return m_map.find(id);
  }

  // Q_SIGNALS:
  mutable Nano::Signal<void(T&)> mutable_added;
  mutable Nano::Signal<void(const T&)> added;
  mutable Nano::Signal<void(const T&)> removing;
  mutable Nano::Signal<void(const T&)> removed;
  mutable Nano::Signal<void()> orderChanged;

  void add(T* t)
  {
    EntityMapInserter<T>{}.add(*this, t);
  }

  void erase(T& elt)
  {
    removing(elt);
    m_map.remove(elt.id());
    removed(elt);
  }

  void remove(T& elt)
  {
    erase(elt);
    delete &elt;
  }

  void remove(T* elt)
  {
    remove(*elt);
  }

  void remove(const Id<T>& id)
  {
    auto it = m_map.get().find(id);
    auto& elt = **it;

    removing(elt);
    m_map.remove(it);
    removed(elt);
    delete &elt;
  }

  void clear()
  {
    while (!m_map.empty())
    {
      remove(*m_map.begin());
    }
  }

  void swap(const Id<T>& id1, const Id<T>& id2)
  {
    if (id1 != id2)
    {
      m_map.swap(id1, id2);
      orderChanged();
    }
  }

  // Will put id2 before id1
  void relocate(const Id<T>& id1, const Id<T>& id2)
  {
    if (id1 != id2)
    {
      m_map.relocate(id1, id2);
      orderChanged();
    }
  }

  void putToEnd(const Id<T>& id1)
  {
    m_map.putToEnd(id1);
    orderChanged();
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
