#pragma once
#include <QDebug>
#include <QPointer>
#include <score/tools/Todo.hpp>
#include <score/tools/std/Optional.hpp>

namespace score
{
template <typename T>
class EntityMap;
}
template <typename Element, typename Model, typename Map>
class IdContainer;
template <typename T>
class IdentifiedObject;
/**
 * @brief The id_base_t class
 * @tparam tag Type of the object identified.
 * @tparam impl Underlying implementation of id_base_t.
 *
 * The base identifier type. Classes should not have
 * to use this directly; instead the identifier type is Id<T>.
 *
 * A class wishing to be identified like this should inherit from
 * IdentifiedObject<T> or Entity<T>, like this :
 *
 * \code
 * class MyModel :
 *  public score::Entity<MyModel>
 * {
 * };
 * \endcode
 *
 * This ensures that one cannot mistakenly use the identifier of an object
 * in another object, e.g. one cannot do :
 *
 * \code
 * void myFunction(Id<OtherModel> id);
 * // ...
 * myFunction(myModel.id());
 * \endcode
 *
 * The compiler will rightfully prevent this incorrect code from working.
 *
 * The class has a cache to allow for fast access to the object it is
 * identifying.
 *
 * @see Id
 * @see IdentifiedObject
 * @see IdentifiedObjectMap
 * @see Entity
 * @see EntityMap
 */
template <typename tag, typename impl>
class id_base_t
{
  friend tag;
  friend class score::EntityMap<tag>;
  friend class IdentifiedObject<tag>;

  // TODO Try to only have Map as a template type here
  template <typename Element, typename Model, typename Map>
  friend class IdContainer;

public:
  using value_type = impl;
  explicit id_base_t() noexcept { }

  id_base_t(const id_base_t& other) noexcept : m_id{other.m_id}
  {
  }

  id_base_t(id_base_t&& other) noexcept : m_id{std::move(other.m_id)}
  {
  }
  template <typename other>
  id_base_t(
      const id_base_t<other, impl>& oid,
      typename std::enable_if<std::is_base_of_v<tag, other>>::type* = 0) noexcept
      : m_id{oid.val()}
  {
  }

  template <typename other>
  id_base_t(
      id_base_t&& oid,
      typename std::enable_if<std::is_base_of_v<tag, other>>::type* = 0) noexcept
      : m_id{oid.val()}
  {
  }

  id_base_t& operator=(const id_base_t& other) noexcept
  {
    m_id = other.m_id;
    m_ptr.clear();
    return *this;
  }

  id_base_t& operator=(id_base_t&& other) noexcept
  {
    m_id = other.m_id;
    m_ptr.clear();
    return *this;
  }

  // TODO check if when an id is returned by value,
  // the pointer gets copied correctly
  explicit id_base_t(value_type val) noexcept : m_id{std::move(val)}
  {
  }

  explicit id_base_t(tag& element) noexcept : m_ptr{&element}, m_id{element.id()}
  {
  }

  id_base_t& operator=(tag& element) noexcept
  {
    m_ptr = &element;
    m_id = element.id();

    return *this;
  }

  friend bool operator==(const id_base_t& lhs, const id_base_t& rhs) noexcept
  {
    return lhs.m_id == rhs.m_id;
  }

  friend bool operator!=(const id_base_t& lhs, const id_base_t& rhs) noexcept
  {
    return lhs.m_id != rhs.m_id;
  }

  friend bool operator<(const id_base_t& lhs, const id_base_t& rhs) noexcept
  {
    return lhs.val() < rhs.val();
  }

  explicit operator value_type() const noexcept
  {
    return m_id;
  }

  const value_type& val() const noexcept
  {
    return m_id;
  }

  void setVal(value_type val) noexcept
  {
    m_id = val;
  }

private:
  mutable QPointer<QObject> m_ptr;
  value_type m_id{};
};

/**
 * @typedef Id identifier for an object
 */
template <typename tag>
using Id = id_base_t<tag, int32_t>;

/**
 * @typedef OptionalId identifier for an object that may not exist
 */
template <typename tag>
using OptionalId = optional<Id<tag>>;

namespace std
{
template <typename tag>
struct hash<Id<tag>>
{
  std::size_t operator()(const Id<tag>& id) const
  {
    return std::hash<int32_t>{}(id.val());
  }
};
}
template <typename T>
uint qHash(const Id<T>& id, uint seed)
{
  return qHash(*id.val(), seed);
}

template <typename tag>
QDebug operator<<(QDebug dbg, const Id<tag>& c)
{
  dbg.nospace() << c.val();

  return dbg.space();
}
