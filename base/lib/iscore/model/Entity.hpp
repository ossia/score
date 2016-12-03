#pragma once

#include <ossia/network/base/name_validation.hpp>
#include <iscore/component/Component.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

template <typename T>
class EntityMapInserter;

namespace iscore
{
template <typename T>
class Entity : public IdentifiedObject<T>
{
public:
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

  Entity(Deserializer<DataStream>& vis, QObject* parent)
      : IdentifiedObject<T>{vis, parent}
  {
    m_metadata.setParent(this);
    vis.writeTo(*this);
  }

  Entity(Deserializer<JSONObject>& vis, QObject* parent)
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
}

template <typename T>
struct TSerializer<DataStream, void, iscore::Entity<T>>
{
  static void readFrom(DataStream::Serializer& s, const iscore::Entity<T>& obj)
  {
    s.readFrom(static_cast<const IdentifiedObject<T>&>(obj));
    s.readFrom(obj.metadata());
  }

  static void writeTo(DataStream::Deserializer& s, iscore::Entity<T>& obj)
  {
    s.writeTo(obj.metadata());
  }
};

template <typename T>
struct TSerializer<JSONObject, iscore::Entity<T>>
{
  static void readFrom(JSONObject::Serializer& s, const iscore::Entity<T>& obj)
  {
    s.readFrom(static_cast<const IdentifiedObject<T>&>(obj));
    s.m_obj[s.strings.Metadata] = toJsonObject(obj.metadata());
  }

  static void writeTo(JSONObject::Deserializer& s, iscore::Entity<T>& obj)
  {
    obj.metadata()
        = fromJsonObject<iscore::ModelMetadata>(s.m_obj[s.strings.Metadata]);
  }
};

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
