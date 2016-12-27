#pragma once

#include <ossia/network/base/name_validation.hpp>
#include <iscore/model/Component.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/model/IdentifiedObject.hpp>

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
    vis.writeTo(*this);
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

template <typename T>
using enable_if_object = typename std::enable_if_t<T::identified_object_tag>;

template <class, class Enable = void>
struct is_object : std::false_type
{
};

template <class T>
struct is_object<T, enable_if_object<T>> : std::true_type
{
};

template <typename T>
using enable_if_entity = typename std::enable_if_t<T::entity_tag>;

template <class, class Enable = void>
struct is_entity : std::false_type
{
};

template <class T>
struct is_entity<T, enable_if_entity<T>> : std::true_type
{
};
}

template <typename T>
class EntityMapInserter<iscore::Entity<T>>
{
  void add(EntityMap<T>& map, T* t)
  {
    ISCORE_ASSERT(t);

    std::vector<std::string> bros_names;
    bros_names.reserve(map.size());
    std::transform(
        map.begin(), map.end(), std::back_inserter(bros_names),
        [&](const auto& res) {
          bros_names.push_back(res.metadata().getName().toStdString());
        });

    auto new_name = ossia::net::sanitize_name(
        t->metadata().getName().toStdString(), bros_names);
    t->metadata().setName(QString::fromStdString(new_name));

    map.unsafe_map().insert(t);

    map.mutable_added(*t);
    map.added(*t);
  }
};

class DataStream;
template <typename T>
struct TSerializer<DataStream, iscore::Entity<T>>;
