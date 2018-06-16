#pragma once
#include <ossia/detail/config.hpp>
#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <wobjectdefs.h>

/**
 * @brief The ObjectIdentifier class
 *
 * A mean to identify an object without a pointer. The id is useful
 * if the object is inside a collection.
 *
 * Example:
 * @code
 *	ObjectIdentifier ob{"TheObjectName", 34};
 * @endcode
 */
class ObjectIdentifier
{
  friend bool
  operator==(const ObjectIdentifier& lhs, const ObjectIdentifier& rhs)
  {
    return (lhs.m_objectName == rhs.m_objectName) && (lhs.m_id == rhs.m_id);
  }

public:
  ObjectIdentifier() = default;
  explicit ObjectIdentifier(const char* name) : m_objectName{name}
  {
  }

  ObjectIdentifier(QString name, int32_t id)
      : m_objectName{std::move(name)}, m_id{id}
  {
  }

  template <typename T>
  ObjectIdentifier(QString name, Id<T> id)
      : m_objectName{std::move(name)}, m_id{id.val()}
  {
  }

  const QString& objectName() const
  {
    return m_objectName;
  }

  int32_t id() const
  {
    return m_id;
  }

private:
  QString m_objectName;
  int32_t m_id{};
};

Q_DECLARE_METATYPE(ObjectIdentifier)
W_REGISTER_ARGTYPE(ObjectIdentifier)

using ObjectIdentifierVector = std::vector<ObjectIdentifier>;
