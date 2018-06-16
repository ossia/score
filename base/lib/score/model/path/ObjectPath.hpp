#pragma once
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <algorithm>
#include <initializer_list>
#include <score/model/path/ObjectIdentifier.hpp>
#include <score_lib_base_export.h>
#include <type_traits>
#include <vector>
namespace score
{
struct DocumentContext;
}
class QObject;

/**
 * @brief The ObjectPath class
 *
 * A path in the QObject hierarchy, using names and identifiers.
 * See @c{IdentifiedObject}.
 *
 * If the object id is not known at compile time a path may be created using
 * ObjectPath::pathFromObject.
 *
 * Example:
 * @code
 *	ObjectPath p{ {"MyObject", 0}, {"SomeSubObjectInACollection", 27} };
 * @endcode
 *
 * Note : this class is mostly superseded by Path<T> which adds type-safety.
 */
class SCORE_LIB_BASE_EXPORT ObjectPath
{
  friend ObjectIdentifierVector::iterator begin(ObjectPath& path) noexcept
  {
    return path.m_objectIdentifiers.begin();
  }

  friend ObjectIdentifierVector::iterator end(ObjectPath& path) noexcept
  {
    return path.m_objectIdentifiers.end();
  }

  friend bool operator==(const ObjectPath& lhs, const ObjectPath& rhs) noexcept
  {
    return lhs.m_objectIdentifiers == rhs.m_objectIdentifiers;
  }

  friend bool operator!=(const ObjectPath& lhs, const ObjectPath& rhs) noexcept
  {
    return lhs.m_objectIdentifiers != rhs.m_objectIdentifiers;
  }

public:
  ObjectPath() noexcept { }
  ~ObjectPath() noexcept = default;
  QString toString() const noexcept;

  explicit ObjectPath(std::vector<ObjectIdentifier> vec) noexcept
      : m_objectIdentifiers{std::move(vec)}
  {
  }

  ObjectPath(std::initializer_list<ObjectIdentifier> lst) noexcept
      : m_objectIdentifiers(lst)
  {
  }

  ObjectPath(const ObjectPath& obj) noexcept
      : m_objectIdentifiers{obj.m_objectIdentifiers}
  {
  }

  ObjectPath(ObjectPath&& obj) noexcept
      : m_objectIdentifiers{std::move(obj.m_objectIdentifiers)}
  {
  }

  ObjectPath& operator=(ObjectPath&& obj) noexcept
  {
    m_objectIdentifiers = std::move(obj.m_objectIdentifiers);
    m_cache.clear();
    return *this;
  }

  ObjectPath& operator=(const ObjectPath& obj) noexcept
  {
    m_objectIdentifiers = obj.m_objectIdentifiers;
    m_cache.clear();
    return *this;
  }

  static ObjectPath pathBetweenObjects(
      const QObject* const parent_obj, const QObject* target_object);

  /**
   * @brief find the object described by the ObjectPath
   * @return A pointer to the object, or nullptr if it is not found
   *
   * This search starts from qApp.
   * @todo (maybe) a way to specify custom ways of finding an object
   * (for instance if obj->blurb() == Ding::someDing)
   * @todo search starting from another object, for more performance.
   */
  template <class T>
  T& find(const score::DocumentContext& ctx) const
  {
    // First see if the pointer is still loaded in the cache.
    if (!m_cache.isNull())
    {
      return *safe_cast<T*>(m_cache.data());
    }
    else // Load it by hand
    {
      auto ptr
          = safe_cast<typename std::remove_const<T>::type*>(find_impl(ctx));
      m_cache = ptr;
      return *ptr;
    }
  }

  /**
   * @brief Tries to find an object
   *
   * @return null if the object does not exist.
   */
  template <class T>
  T* try_find(const score::DocumentContext& ctx) const noexcept
  {
    try
    {
      if (!m_cache.isNull())
      {
        return safe_cast<T*>(m_cache.data());
      }
      else // Load it by hand
      {
        auto ptr = static_cast<typename std::remove_const<T>::type*>(
            find_impl_unsafe(ctx));
        m_cache = ptr;
        return ptr;
      }
    }
    catch (...)
    {
      return nullptr;
    }
  }

  const ObjectIdentifierVector& vec() const noexcept
  {
    return m_objectIdentifiers;
  }

  ObjectIdentifierVector& vec() noexcept
  {
    return m_objectIdentifiers;
  }

private:
  // Throws
  QObject* find_impl(const score::DocumentContext& ctx) const;

  // Returns nullptr
  QObject* find_impl_unsafe(const score::DocumentContext& ctx) const noexcept;

  ObjectIdentifierVector m_objectIdentifiers;
  mutable QPointer<QObject> m_cache;
};

SCORE_LIB_BASE_EXPORT void replacePathPart(const ObjectPath& src, const ObjectPath& target, ObjectPath& toChange);
inline uint qHash(const ObjectPath& obj, uint seed)
{
  return qHash(obj.toString(), seed);
}

namespace std
{
template <>
struct SCORE_LIB_BASE_EXPORT hash<ObjectIdentifier>
{
  std::size_t operator()(const ObjectIdentifier& path) const;
};
template <>
struct SCORE_LIB_BASE_EXPORT hash<ObjectPath>
{
  std::size_t operator()(const ObjectPath& path) const;
};
}

Q_DECLARE_METATYPE(ObjectPath)
W_REGISTER_ARGTYPE(ObjectPath)
