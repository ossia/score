#pragma once

#include <ossia/network/base/name_validation.hpp>
#include <iscore/model/Component.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/model/Component.hpp>
template <typename T>
class EntityMapInserter;

namespace iscore
{
/**
 * @brief Base for complex model objects.
 *
 * This class should be used by plug-in authors to provide model classes
 * with associated components.
 *
 * It has :
 * * Metadata : name, label, comments, color
 * * Components : a way to extend models through plug-ins.
 *
 * \see iscore::Component
 */
template <typename T>
class Entity : public IdentifiedObject<T>
{
public:
  static const constexpr bool entity_tag = true;

  Entity(Id<T> id, const QString& name, QObject* parent)
    : IdentifiedObject<T>{std::move(id), name, parent}
  {
    m_metadata.setParent(this);
  }

  Entity(const Entity& other, Id<T> id, const QString& name, QObject* parent)
    : IdentifiedObject<T>{std::move(id), name, parent}
    , m_metadata{other.metadata()}
  {
    m_metadata.setParent(this);
  }

  template<typename Visitor>
  Entity(Visitor& vis, QObject* parent)
    : IdentifiedObject<T>{vis, parent}
  {
    m_metadata.setParent(this);
    TSerializer<typename Visitor::type, Entity<T>>::writeTo(vis, *this);
  }

  const iscore::Components& components() const
  {
    return m_components;
  }
  iscore::Components& components()
  {
    return m_components;
  }
  const iscore::ModelMetadata& metadata() const
  {
    return m_metadata;
  }
  iscore::ModelMetadata& metadata()
  {
    return m_metadata;
  }

private:
  iscore::Components m_components;
  ModelMetadata m_metadata;
};


}
