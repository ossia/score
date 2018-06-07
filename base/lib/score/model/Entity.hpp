#pragma once
#include <ossia-qt/name_utils.hpp>
#include <score/model/ComponentSerialization.hpp>
#include <score/model/EntityBase.hpp>

namespace score
{

template <typename T>
Id<score::Component> newId(const score::Entity<T>& e)
{
  return getStrongId(e.components());
}

template <typename T>
class EntityMapInserter<score::Entity<T>>
{
  void add(EntityMap<T>& map, T* obj)
  {
    SCORE_ASSERT(obj);

    std::vector<QString> bros_names;
    bros_names.reserve(map.size());
    std::transform(
        map.begin(), map.end(), std::back_inserter(bros_names),
        [&](const auto& res) {
          bros_names.push_back(res.metadata().getName());
        });

    auto new_name
        = ossia::net::sanitize_name(obj->metadata().getName(), bros_names);
    obj->metadata().setName(new_name);

    map.unsafe_map().insert(obj);

    map.mutable_added(*obj);
    map.added(*obj);

    // If there are serialized components, we try to deserialize them
    score::Components& comps = obj->components();
    deserializeRemainingComponents(comps, obj);
  }
};
}

template <typename T>
struct TSerializer<DataStream, score::Entity<T>>
{
  static void readFrom(DataStream::Serializer& s, const score::Entity<T>& obj)
  {
    TSerializer<DataStream, IdentifiedObject<T>>::readFrom(s, obj);
    s.readFrom(obj.metadata());

    // Save components
    score::DataStreamComponents vec;
    for (auto& comp : obj.components())
    {
      if (auto c = dynamic_cast<score::SerializableComponent*>(&comp))
      {
        vec[c->concreteKey()] = s.marshall(*c);
      }
    }

    s.readFrom(std::move(vec));

    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, score::Entity<T>& obj)
  {
    s.writeTo(obj.metadata());

    // Reload components
    score::DataStreamComponents vec;
    s.writeTo(vec);
    if (!vec.empty())
    {
      // TODO we use id -1, there should be a better way... for now it will
      // work since id's begin at 1.
      auto comp = new score::DataStreamSerializedComponents{
          Id<score::Component>{-1}, std::move(vec), &obj};
      obj.components().add(comp);
    }
    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename T>
struct TSerializer<JSONObject, score::Entity<T>>
{
  static void readFrom(JSONObject::Serializer& s, const score::Entity<T>& obj)
  {
    TSerializer<JSONObject, IdentifiedObject<T>>::readFrom(s, obj);
    s.obj[s.strings.Metadata] = toJsonObject(obj.metadata());

    // Save components
    QJsonArray json_components;
    for (auto& comp : obj.components())
    {
      if (auto c = dynamic_cast<score::SerializableComponent*>(&comp))
      {
        json_components.append(s.marshall(*c));
      }
    }

    s.obj[s.strings.Components] = std::move(json_components);
  }

  static void writeTo(JSONObject::Deserializer& s, score::Entity<T>& obj)
  {
    {
      JSONObjectWriter writer{s.obj[s.strings.Metadata].toObject()};
      writer.writeTo(obj.metadata());
    }

    const QJsonArray json_components = s.obj[s.strings.Components].toArray();
    if (!json_components.empty())
    {
      score::JSONComponents vec;
      for (const auto& comp : json_components)
      {
        // Since the component is a SerializableInterface, it has an uuid
        // attribute.
        auto obj = comp.toObject();

        UuidKey<score::SerializableComponent> k
            = fromJsonValue<score::uuid_t>(obj[s.strings.uuid]);
        vec.emplace(k, std::move(obj));
      }
      // TODO we use id -1, there should be a better way... for now it will
      // work since id's begin at 1.
      auto comp = new score::JSONSerializedComponents{Id<score::Component>{-1},
                                                      std::move(vec), &obj};
      obj.components().add(comp);
    }
  }
};
