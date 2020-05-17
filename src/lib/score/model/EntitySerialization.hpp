#pragma once
#include <score/model/EntityBase.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
template <typename T>
struct TSerializer<DataStream, score::Entity<T>>
{
  static void readFrom(DataStream::Serializer& s, const score::Entity<T>& obj)
  {
    TSerializer<DataStream, IdentifiedObject<T>>::readFrom(s, obj);
    s.readFrom(obj.metadata());

#if defined(SCORE_SERIALIZABLE_COMPONENTS)
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
#endif
    SCORE_DEBUG_INSERT_DELIMITER2(s);
  }

  static void writeTo(DataStream::Deserializer& s, score::Entity<T>& obj)
  {
    s.writeTo(obj.metadata());
#if defined(SCORE_SERIALIZABLE_COMPONENTS)

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
#endif
    SCORE_DEBUG_CHECK_DELIMITER2(s);
  }
};

template <typename T>
struct TSerializer<JSONObject, score::Entity<T>>
{
  static void readFrom(JSONObject::Serializer& s, const score::Entity<T>& obj)
  {
    TSerializer<JSONObject, IdentifiedObject<T>>::readFrom(s, obj);
    s.obj[s.strings.Metadata] = obj.metadata();

#if defined(SCORE_SERIALIZABLE_COMPONENTS)
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
#endif
  }

  static void writeTo(JSONObject::Deserializer& s, score::Entity<T>& obj)
  {
    {
      JSONWriter writer{s.obj[s.strings.Metadata]};
      writer.writeTo(obj.metadata());
    }

#if defined(SCORE_SERIALIZABLE_COMPONENTS)
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
      auto comp
          = new score::JSONSerializedComponents{Id<score::Component>{-1}, std::move(vec), &obj};
      obj.components().add(comp);
    }
#endif
  }
};

struct ArrayEntitySerializer
{
  /// Arrays of pointed-to objects
  template <typename T>
  static void readFrom(DataStream::Serializer& s, const T& vec)
  {
    s.m_stream << (int32_t)vec.size();
    for (auto* v : vec)
      s.readFrom(*v);
  }

  template <typename List, typename OnSucces, typename OnFailure>
  static void writeTo(
      DataStream::Deserializer& s,
      const List& lst,
      QObject* parent,
      const OnSucces& success,
      const OnFailure& fail)
  {
    int32_t count;
    s.m_stream >> count;
    for (; count-- > 0;)
    {
      auto proc = deserialize_interface(lst, s, parent);
      if (proc)
      {
        success(proc);
      }
      else
      {
        fail();
      }
    }
  }

  template <typename T>
  static void readFrom(JSONObject::Serializer& s, const T& vec)
  {
    s.stream.StartArray();
    for (const auto* elt : vec)
      s.readFrom(*elt);
    s.stream.EndArray();
  }

  template <typename List, typename OnSucces, typename OnFailure>
  static void writeTo(
      const JSONObject::Deserializer& s,
      const List& lst,
      QObject* parent,
      const OnSucces& success,
      const OnFailure& fail)
  {
    for (const auto& json_vref : s.base.GetArray())
    {
      auto proc = deserialize_interface(lst, JSONObject::Deserializer{json_vref}, parent);
      if (proc)
      {
        success(proc);
      }
      else
      {
        fail(json_vref);
      }
    }
  }
};

template <typename T, typename Alloc>
struct TSerializer<DataStream, std::vector<T*, Alloc>> : ArrayEntitySerializer
{
};
template <typename T, typename Alloc>
struct TSerializer<JSONObject, std::vector<T*, Alloc>> : ArrayEntitySerializer
{
};

template <typename T, std::size_t N>
struct TSerializer<DataStream, boost::container::small_vector<T*, N>> : ArrayEntitySerializer
{
};
template <typename T, std::size_t N>
struct TSerializer<JSONObject, boost::container::small_vector<T*, N>> : ArrayEntitySerializer
{
};
