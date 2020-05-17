#pragma once
#include <score/application/ApplicationComponents.hpp>
#include <score/model/EntityList.hpp>
#include <score/model/EntityMap.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

struct EntityMapSerializer
{

  template <typename T>
  static void readFrom(DataStream::Serializer& s, const T& obj)
  {
    s.m_stream << (int32_t)obj.size();
    for (const auto& child : obj)
      s.readFrom(child);
  }

  template <typename List, typename T>
  static void
  writeTo(DataStream::Deserializer& s, T& obj, const score::DocumentContext& ctx, QObject* parent)
  {
    int32_t sz;
    s.m_stream >> sz;
    auto& pl = s.components.template interfaces<List>();
    for (; sz-- > 0;)
    {
      auto proc = deserialize_interface(pl, s, ctx, parent);
      if (proc)
        obj.add(proc);
      else
        SCORE_TODO;
    }
  }

  template <typename T>
  static void readFrom(JSONObject::Serializer& s, const T& vec)
  {
    s.stream.StartArray();
    for (const auto& elt : vec)
      s.readFrom(elt);
    s.stream.EndArray();
  }
  template <typename List, typename T>
  static void
  writeTo(JSONObject::Deserializer&& s, T& obj, const score::DocumentContext& ctx, QObject* parent)
  {
    auto& pl = s.components.interfaces<List>();

    const auto& array = s.base.GetArray();
    for (const auto& json_vref : array)
    {
      JSONObject::Deserializer deserializer{json_vref};
      auto proc = deserialize_interface(pl, deserializer, ctx, parent);
      if (proc)
        obj.add(proc);
      else
        SCORE_TODO;
    }
  }
};

template <typename T>
struct TSerializer<DataStream, score::EntityMap<T>> : EntityMapSerializer
{
};

template <typename T>
struct TSerializer<JSONObject, score::EntityMap<T>> : EntityMapSerializer
{
};

template <typename T>
struct TSerializer<DataStream, score::EntityList<T>> : EntityMapSerializer
{
};

template <typename T>
struct TSerializer<JSONObject, score::EntityList<T>> : EntityMapSerializer
{
};
