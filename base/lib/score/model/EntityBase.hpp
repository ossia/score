#pragma once

#include <ossia/network/base/name_validation.hpp>

#include <score/model/Component.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/model/ModelMetadata.hpp>
template <typename T>
class EntityMapInserter;

namespace score
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
 * \see score::Component
 */
template <typename T>
class Entity : public IdentifiedObject<T>
{
public:
  static const constexpr bool entity_tag = true;

  Entity(Id<T> id, const QString& name, QObject* parent) noexcept
      : IdentifiedObject<T>{std::move(id), name, parent}
  {
    m_metadata.setParent(this);
  }

  template <typename Visitor>
  Entity(Visitor&& vis, QObject* parent) noexcept
    : IdentifiedObject<T>{std::forward<Visitor>(vis), parent}
  {
    using vis_type = typename std::remove_reference_t<Visitor>::type;
    m_metadata.setParent(this);
    TSerializer<vis_type, Entity<T>>::writeTo(vis, *this);
  }

  const score::Components& components() const noexcept
  {
    return m_components;
  }
  score::Components& components() noexcept
  {
    return m_components;
  }
  const score::ModelMetadata& metadata() const noexcept
  {
    return m_metadata;
  }
  score::ModelMetadata& metadata() noexcept
  {
    return m_metadata;
  }

private:
  score::Components m_components;
  ModelMetadata m_metadata;
};
}
