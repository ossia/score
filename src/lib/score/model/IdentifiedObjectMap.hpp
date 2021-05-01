#pragma once
#include <score/model/IdentifiedObject.hpp>
#include <score/tools/std/IndirectContainer.hpp>

#include <tsl/hopscotch_map.h>

#include <list>
#include <vector>
// This file contains a fast map for items based on their identifier,
// based on boost's multi-index maps.

/**
 * @brief A map to access child objects through their id.
 */
template <class Element, class Model = Element, class Enable = void>
class IdContainer
{
};

// We have to write two implementations since const_mem_fun does not handle
// inheritance.

/** This map is for classes which inherit from
 * IdentifiedObject<T> and don't have an id() method by themselves, e.g. all
 * the model objects.
 *
 * Additionnally, items are ordered; iteration occurs on the ordered iterators.
 *
 * In the implementation :
 * * `get<0>()` gets the hashed (like std::unordered_map) iterator.
 * * `get<1>()` gets the ordered (like std::vector) iterator.
 */
template <typename Element, typename Model>
class IdContainer<
    Element,
    Model,
    std::enable_if_t<std::is_base_of<IdentifiedObject<Model>, Element>::value>>
{
public:
  using model_type = Model;
  using order_t = std::list<Element*>;
  using map_t = tsl::hopscotch_map<
      Id<Model>,
      std::pair<Element*, typename order_t::iterator>>;
  map_t m_map;
  order_t m_order;

  using value_type = Element;
  using iterator = score::indirect_iterator<typename order_t::iterator>;
  using const_iterator
      = score::indirect_iterator<typename order_t::const_iterator>;
  using const_reverse_iterator
      = score::indirect_iterator<typename order_t::const_reverse_iterator>;

  IdContainer() INLINE_EXPORT = default;
  IdContainer(const IdContainer& other) = delete;
  IdContainer(IdContainer&& other) noexcept = delete;
  IdContainer& operator=(const IdContainer& other) = delete;
  IdContainer& operator=(IdContainer&& other) = delete;

  ~IdContainer() INLINE_EXPORT
  {
    // To ensure that children are deleted before their parents
    for (auto elt : m_order)
    {
      delete elt;
    }
  }

  auto& ordered() INLINE_EXPORT { return m_order; }

  const_iterator begin() const INLINE_EXPORT
  {
    return score::make_indirect_iterator(this->m_order.begin());
  }
  const_reverse_iterator rbegin() const INLINE_EXPORT
  {
    return score::make_indirect_iterator(this->m_order.rbegin());
  }
  const_iterator cbegin() const INLINE_EXPORT
  {
    return score::make_indirect_iterator(this->m_order.cbegin());
  }
  const_iterator end() const INLINE_EXPORT
  {
    return score::make_indirect_iterator(this->m_order.end());
  }
  const_reverse_iterator rend() const INLINE_EXPORT
  {
    return score::make_indirect_iterator(this->m_order.rend());
  }
  const_iterator cend() const INLINE_EXPORT
  {
    return score::make_indirect_iterator(this->m_order.cend());
  }

  std::size_t size() const INLINE_EXPORT { return m_map.size(); }

  bool empty() const INLINE_EXPORT { return m_map.empty(); }

  std::vector<Element*> as_vec() const INLINE_EXPORT
  {
    return std::vector<Element*>(m_order.begin(), m_order.end());
  }

  score::IndirectContainer<Element> as_indirect_vec() const INLINE_EXPORT
  {
    return score::IndirectContainer<Element>(m_order.begin(), m_order.end());
  }

  void insert(value_type* t) INLINE_EXPORT
  {
    SCORE_ASSERT(m_map.find(t->id()) == m_map.end());
    m_order.push_front(t);
    m_map.insert({t->id(), {t, m_order.begin()}});
  }

  void remove(typename map_t::iterator it) INLINE_EXPORT
  {
    // No delete : it is done in EntityMap.

    if (it != this->m_map.end())
    {
      m_order.erase(it->second.second);
      m_map.erase(it);
    }
  }
  void remove(typename map_t::const_iterator it) INLINE_EXPORT
  {
    // No delete : it is done in EntityMap.

    if (it != this->m_map.end())
    {
      m_order.erase(it->second.second);
      m_map.erase(it);
    }
  }

  void remove(const Id<Model>& id) INLINE_EXPORT { remove(m_map.find(id)); }

  void clear() INLINE_EXPORT
  {
    m_map.clear();
    m_order.clear();
    // TODO why no delete ?!
    // e.g. in some cases (Curve::Model::clear()) it deletes afterwards
    // but not in Scenario destructor
  }

  const_iterator find(const Id<Model>& id) const INLINE_EXPORT
  {
    auto it = this->m_map.find(id);
    if (it != this->m_map.end())
    {
      return score::make_indirect_iterator(
          (typename order_t::const_iterator)it->second.second);
    }
    else
    {
      return score::make_indirect_iterator(this->m_order.end());
    }
  }

  Element& at(const Id<Model>& id) const INLINE_EXPORT
  {
    if (id.m_ptr)
    {
      SCORE_ASSERT(
          id.m_ptr->parent() == this->m_map.find(id)->second.first->parent());
      return safe_cast<Element&>(*id.m_ptr);
    }
    auto item = this->m_map.find(id);
    SCORE_ASSERT(item != this->m_map.end());

    id.m_ptr = item->second.first;
    return safe_cast<Element&>(*item->second.first);
  }
};

/** This specialization is for classes which directly have an id() method
 * like a Presenter whose id() would return its model's.
 */
template <typename Element, typename Model>
class IdContainer<
    Element,
    Model,
    std::enable_if_t<
        !std::is_base_of<IdentifiedObject<Model>, Element>::value>>
{
public:
  using model_type = Model;
  tsl::hopscotch_map<Id<Model>, Element*> m_map;

  std::vector<Element*> as_vec() const INLINE_EXPORT
  {
    std::vector<Element*> v;
    const auto N = m_map.size();
    v.reserve(N);
    for (auto& e : m_map)
    {
      v.push_back(e.second);
    }
    return v;
  }

  auto begin() const INLINE_EXPORT
  {
    return score::make_indirect_map_iterator(this->m_map.begin());
  }
  auto rbegin() const INLINE_EXPORT
  {
    return score::make_indirect_map_iterator(this->m_map.begin());
  }
  auto cbegin() const INLINE_EXPORT
  {
    return score::make_indirect_map_iterator(this->m_map.cbegin());
  }
  auto end() const INLINE_EXPORT
  {
    return score::make_indirect_map_iterator(this->m_map.end());
  }
  auto rend() const INLINE_EXPORT
  {
    return score::make_indirect_map_iterator(this->m_map.end());
  }
  auto cend() const INLINE_EXPORT
  {
    return score::make_indirect_map_iterator(this->m_map.cend());
  }

  auto find(const Id<Model>& id) const INLINE_EXPORT
  {
    return score::make_indirect_map_iterator(this->m_map.find(id));
  }

  void insert(Element* t) INLINE_EXPORT
  {
    SCORE_ASSERT(m_map.find(t->id()) == m_map.end());
    m_map.insert({t->id(), t});
  }

  void erase(const Id<Model>& id) INLINE_EXPORT
  {
    auto it = m_map.find(id);
    if (it != m_map.end())
    {
      auto ptr = it->second;
      m_map.erase(it);
      delete ptr;
    }
  }

  void remove_all() INLINE_EXPORT
  {
    for (auto& e : m_map)
    {
      delete e.second;
    }
    m_map.clear();
  }

  auto& at(const Id<Model>& id) const INLINE_EXPORT
  {
    auto item = this->m_map.find(id);
    SCORE_ASSERT(item != this->m_map.end());
    return *item->second;
  }
};
