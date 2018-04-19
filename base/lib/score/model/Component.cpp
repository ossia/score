// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Component.hpp"

#include "ComponentSerialization.hpp"

#include <score/document/DocumentContext.hpp>
#include <wobjectimpl.h>

template class SCORE_LIB_BASE_EXPORT score::EntityMap<score::Component>;
template class SCORE_LIB_BASE_EXPORT
    tsl::hopscotch_map<UuidKey<score::SerializableComponent>, QByteArray>;
template class SCORE_LIB_BASE_EXPORT
    tsl::hopscotch_map<UuidKey<score::SerializableComponent>, QJsonObject>;
W_OBJECT_IMPL(score::Component)
namespace score
{
Component::~Component() = default;
DataStreamSerializedComponents::~DataStreamSerializedComponents() = default;
JSONSerializedComponents::~JSONSerializedComponents() = default;

DataStreamSerializedComponents::DataStreamSerializedComponents(
    const Id<Component>& id, DataStreamComponents obj, QObject* parent)
    : score::Component{id, "SerializedComponents", parent}
    , data(std::move(obj))
{
  static_assert(is_identified_object<SerializableComponent>::value, "");
  static_assert(is_abstract_base<SerializableComponent>::value, "");
  static_assert(
      std::is_same<
          serialization_tag<SerializableComponent>::type,
          visitor_abstract_object_tag>::value,
      "");
}

JSONSerializedComponents::JSONSerializedComponents(
    const Id<Component>& id, JSONComponents obj, QObject* parent)
    : score::Component{id, "SerializedComponents", parent}
    , data(std::move(obj))
{
}

bool JSONSerializedComponents::deserializeRemaining(
    Components& comps, QObject* entity)
{
  auto& ctx = score::IDocument::documentContext(*entity);
  auto& comp_factory
      = ctx.app.interfaces<score::SerializableComponentFactoryList>();
  for (auto it = data.begin(); it != data.end();)
  {
    JSONObject::Deserializer s{it->second};
    auto res = deserialize_interface(comp_factory, s, ctx, entity);
    if (res)
    {
      comps.add(res);
      it = data.erase(it);
    }
    else
    {
      ++it;
    }
  }

  return data.empty();
}

bool DataStreamSerializedComponents::deserializeRemaining(
    Components& components, QObject* entity)
{
  auto& ctx = score::IDocument::documentContext(*entity);
  auto& comp_factory
      = ctx.app.interfaces<score::SerializableComponentFactoryList>();
  for (auto it = data.begin(); it != data.end();)
  {
    DataStream::Deserializer s{it->second};
    auto res = deserialize_interface(comp_factory, s, ctx, entity);
    if (res)
    {
      components.add(res);
      it = data.erase(it);
    }
    else
    {
      ++it;
    }
  }
  return data.empty();
}

SerializableComponentFactory::~SerializableComponentFactory()
{
}

SerializableComponentFactoryList::~SerializableComponentFactoryList()
{
}

score::SerializableComponent* SerializableComponentFactoryList::loadMissing(
    const VisitorVariant& vis,
    const DocumentContext& ctx,
    QObject* parent) const
{
  SCORE_TODO;
  return nullptr;
}

void deserializeRemainingComponents(score::Components& comps, QObject* obj)
{
  // First with datastream
  if (auto ser = findComponent<score::DataStreamSerializedComponents>(comps))
  {
    ser->deserializeRemaining(comps, obj);

    // If we deserialized everything, no point in keeping this one
    if (ser->finished())
      comps.remove(ser);
  }

  // Then with JSON
  if (auto ser = findComponent<score::JSONSerializedComponents>(comps))
  {
    ser->deserializeRemaining(comps, obj);

    // If we deserialized everything, no point in keeping this one
    if (ser->finished())
      comps.remove(ser);
  }
}
}

template <>
SCORE_LIB_BASE_EXPORT void
DataStreamReader::read<score::SerializableComponent>(
    const score::SerializableComponent&)
{
}
template <>
SCORE_LIB_BASE_EXPORT void
JSONObjectReader::read<score::SerializableComponent>(
    const score::SerializableComponent&)
{
}
