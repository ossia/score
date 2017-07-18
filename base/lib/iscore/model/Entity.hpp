#pragma once
#include <iscore/model/EntityBase.hpp>
#include <iscore/model/ComponentSerialization.hpp>
#include <ossia-qt/name_utils.hpp>

namespace iscore
{

template<typename T>
Id<iscore::Component> newId(const iscore::Entity<T>& e)
{
  return getStrongId(e.components());
}

template <typename T>
class EntityMapInserter<iscore::Entity<T>>
{
  void add(EntityMap<T>& map, T* obj)
  {
    ISCORE_ASSERT(obj);

    std::vector<QString> bros_names;
    bros_names.reserve(map.size());
    std::transform(
          map.begin(), map.end(), std::back_inserter(bros_names),
          [&](const auto& res) {
      bros_names.push_back(res.metadata().getName());
    });

    auto new_name = ossia::net::sanitize_name(
          obj->metadata().getName(), bros_names);
    obj->metadata().setName(new_name);

    map.unsafe_map().insert(obj);

    map.mutable_added(*obj);
    map.added(*obj);

    // If there are serialized components, we try to deserialize them
    iscore::Components& comps = obj->components();
    deserializeRemainingComponents(comps, obj);
  }
};
}

template <typename T>
struct TSerializer<DataStream, iscore::Entity<T>>
{
  static void readFrom(DataStream::Serializer& s, const iscore::Entity<T>& obj)
  {
    TSerializer<DataStream, IdentifiedObject<T>>::readFrom(s, obj);
    s.readFrom(obj.metadata());

    // Save components
    iscore::DataStreamComponents vec;
    for(auto& comp : obj.components())
    {
      if(auto c = dynamic_cast<iscore::SerializableComponent*>(&comp))
      {
        vec[c->concreteKey()] = s.marshall(*c);
      }
    }

    s.readFrom(std::move(vec));

    ISCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, iscore::Entity<T>& obj)
  {
    s.writeTo(obj.metadata());

    // Reload components
    iscore::DataStreamComponents vec;
    s.writeTo(vec);
    if(!vec.empty())
    {
      // TODO we use id -1, there should be a better way... for now it will work since id's begin at 1.
      auto comp = new iscore::DataStreamSerializedComponents{
          Id<iscore::Component>{-1}, std::move(vec), &obj};
      obj.components().add(comp);
    }
    ISCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};


template <typename T>
struct TSerializer<JSONObject, iscore::Entity<T>>
{
  static void readFrom(JSONObject::Serializer& s, const iscore::Entity<T>& obj)
  {
    TSerializer<JSONObject, IdentifiedObject<T>>::readFrom(s, obj);
    s.obj[s.strings.Metadata] = toJsonObject(obj.metadata());

    // Save components
    QJsonArray json_components;
    for(auto& comp : obj.components())
    {
      if(auto c = dynamic_cast<iscore::SerializableComponent*>(&comp))
      {
        json_components.append(s.marshall(*c));
      }
    }

    s.obj[s.strings.Components] = std::move(json_components);
  }

  static void writeTo(JSONObject::Deserializer& s, iscore::Entity<T>& obj)
  {
    obj.metadata()
        = fromJsonObject<iscore::ModelMetadata>(s.obj[s.strings.Metadata]);

    QJsonArray json_components = s.obj[s.strings.Components].toArray();
    if(!json_components.empty())
    {
      iscore::JSONComponents vec;
      for(const auto& comp : json_components)
      {
        // Since the component is a SerializableInterface, it has an uuid attribute.
        auto obj = comp.toObject();

        UuidKey<iscore::SerializableComponent> k = fromJsonValue<iscore::uuid_t>(obj[s.strings.uuid]);
        vec.emplace(k, std::move(obj));
      }
      // TODO we use id -1, there should be a better way... for now it will work since id's begin at 1.
      auto comp = new iscore::JSONSerializedComponents{
          Id<iscore::Component>{-1}, std::move(vec), &obj};
      obj.components().add(comp);
    }

  }
};

