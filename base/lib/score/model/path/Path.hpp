#pragma once
#include <QDataStream>
#include <score/document/DocumentInterface.hpp>
#include <score/tools/Metadata.hpp>

/**
 * @brief Used to know if two types are in an inheritance relationship.
 */
template <typename T, typename U>
struct in_relationship
{
  static const constexpr bool value
      = std::is_base_of<T, U>::value || std::is_base_of<U, T>::value;
};

// These forward declarations are required
// because Path<T>(Object&) calls score::IDocument::path()
// which in turns calls another constructor of Path<T>.
template <typename T>
class Path;
template <typename T>
class IdentifiedObject;

namespace score
{
namespace IDocument
{

template <typename T>
Path<T> path(const IdentifiedObject<T>& obj);
}
}

/**
 * @brief The Path class is a typesafe wrapper around ObjectPath.
 *
 * The wrapper is compatible through inheritance. Given the type tree :
 *
 * \code
 *      A
 *    /  \
 *   B    C
 *   |
 *  B1
 * \endcode
 *
 * A `Path<B1>` will be converted automatically in `Path<B>` or `Path<A>` but
 * not `Path<C>`.
 *
 */
template <typename Object>
class Path
{
  friend bool operator==(const Path& lhs, const Path& rhs)
  {
    return lhs.m_impl == rhs.m_impl;
  }

  friend bool operator!=(const Path& lhs, const Path& rhs)
  {
    return lhs.m_impl != rhs.m_impl;
  }

  friend uint qHash(const Path& obj, uint seed)
  {
    return qHash(obj.m_impl, seed);
  }

  template <typename U>
  friend class Path;
  friend class ObjectPath;

public:
  /**
   * @brief Use this if it is not possible to get a path
   *
   * (for instance because the object does not exist yet)
   */
  struct UnsafeDynamicCreation
  {
    UnsafeDynamicCreation() = default;
  };
  Path(const ObjectPath& obj, UnsafeDynamicCreation) : m_impl{obj.vec()}
  {
  }
  Path(ObjectPath&& obj, UnsafeDynamicCreation) : m_impl{std::move(obj.vec())}
  {
  }

  Path(const Object& obj) : Path(score::IDocument::path(obj))
  {
  }

  //! Add a new ObjectIdentifier at the end of the path and return a new path
  template <typename U>
  auto extend(const QString& name, const Id<U>& id) const&
  {
    Path<U> p{this->m_impl.vec()};
    p.m_impl.vec().push_back({name, id});
    return p;
  }

  //! Add a new ObjectIdentifier at the end of the path and return a new path.
  //! The previous path is now empty.
  template <typename U>
  auto extend(const QString& name, const Id<U>& id) &&
  {
    Path<U> p{std::move(this->m_impl.vec())};
    p.m_impl.vec().push_back({name, id});
    return p;
  }

  //! Add a new ObjectIdentifier at the end of the path and return a new path
  template <typename U>
  auto extend(const Id<U>& id) const&
  {
    Path<U> p{this->m_impl.vec()};
    p.m_impl.vec().push_back({Metadata<ObjectKey_k, U>::get(), id});
    return p;
  }

  //! Add a new ObjectIdentifier at the end of the path and return a new path.
  //! The previous path is now empty.
  template <typename U>
  auto extend(const Id<U>& id) &&
  {
    Path<U> p{std::move(this->m_impl.vec())};
    p.m_impl.vec().push_back({Metadata<ObjectKey_k, U>::get(), id});
    return p;
  }

  //! Return a new path without the last element of this one.
  template <typename U>
  auto splitLast() const&
  {
    SCORE_ASSERT(m_impl.vec().size() > 0);
    auto vec = m_impl.vec();
    auto last = vec.back();
    vec.pop_back();
    return std::make_pair(Path<U>{std::move(vec)}, std::move(last));
  }

  //! Return a new path without the last element of this one. The previous path
  //! is now empty.
  template <typename U>
  auto splitLast() &&
  {
    // Note : we *must not* move directly m_impl
    // because it carries a cache that becomes wrong.
    SCORE_ASSERT(!m_impl.vec().empty());
    auto last = m_impl.vec().back();
    m_impl.vec().pop_back();
    return std::make_pair(Path<U>{std::move(m_impl.vec())}, std::move(last));
  }

  // TODO do the same for ids
  // TODO make it work only for upcasts
  template <
      typename U,
      std::enable_if_t<in_relationship<U, Object>::value>* = nullptr>
  Path(const Path<U>& other) : m_impl{other.m_impl.vec()}
  {
  }

  template <
      typename U,
      std::enable_if_t<in_relationship<U, Object>::value>* = nullptr>
  Path(Path<U>&& other) : m_impl{std::move(other.m_impl.vec())}
  {
  }

  template <
      typename U,
      std::enable_if_t<in_relationship<U, Object>::value>* = nullptr>
  Path& operator=(const Path<U>& other)
  {
    m_impl = other.m_impl;
    return *this;
  }

  template <
      typename U,
      std::enable_if_t<in_relationship<U, Object>::value>* = nullptr>
  Path& operator=(Path<U>&& other)
  {
    m_impl = std::move(other.m_impl);
    return *this;
  }

  Path() = default;
  Path(const Path&) = default;
  Path(Path&&) = default;
  Path& operator=(const Path&) = default;
  Path& operator=(Path&&) = default;

  Object& find(const score::DocumentContext& ctx) const
  {
    SCORE_ASSERT(valid());
    return m_impl.find<Object>(ctx);
  }
  Object* try_find(const score::DocumentContext& ctx) const
  {
    if (!valid())
      return nullptr;
    return m_impl.try_find<Object>(ctx);
  }

  const auto& unsafePath() const&
  {
    return m_impl;
  }
  auto& unsafePath() &
  {
    return m_impl;
  }
  auto& unsafePath_ref()
  {
    // Due to a bug in unity build with gcc 5.3
    return m_impl;
  }
  auto&& unsafePath() &&
  {
    return std::move(m_impl);
  }

  bool valid() const
  {
    return !m_impl.vec().empty();
  }

private:
  Path(const ObjectPath& path) : m_impl{path.vec()}
  {
  }
  Path(ObjectPath&& path) : m_impl{std::move(path.vec())}
  {
  }
  Path(const std::vector<ObjectIdentifier>& vec) : m_impl{vec}
  {
  }
  Path(std::vector<ObjectIdentifier>&& vec) : m_impl{std::move(vec)}
  {
  }

  ObjectPath m_impl;
};

template <typename T>
Path<T> make_path(const T& t)
{
  return t;
}

template <typename T>
QDebug operator<<(QDebug d, Path<T> path)
{
  auto& unsafe = path.unsafePath();
  d << unsafe.toString();
  return d;
}

namespace std
{
template <typename tag>
struct hash<Path<tag>>
{
  std::size_t operator()(const Path<tag>& path) const
  {
    return boost::hash<ObjectPath>{}(path.unsafePath());
  }
};
}
