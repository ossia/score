#include "Component.hpp"
#include "ComponentSerialization.hpp"
namespace iscore
{
Component::~Component() = default;
DataStreamSerializedComponents::~DataStreamSerializedComponents() = default;
JSONSerializedComponents::~JSONSerializedComponents() = default;


DataStreamSerializedComponents::DataStreamSerializedComponents(
    const Id<Component>& id,
    DataStreamComponents obj,
    QObject* parent):
  iscore::Component{id, "SerializedComponents", parent},
  data(std::move(obj))
{
  static_assert(is_identified_object<SerializableComponent>::value, "");
  static_assert(is_abstract_base<SerializableComponent>::value, "");
  static_assert(std::is_same<serialization_tag<SerializableComponent>::type, visitor_abstract_object_tag>::value, "");
}

JSONSerializedComponents::JSONSerializedComponents(
    const Id<Component>& id,
    JSONComponents obj,
    QObject* parent):
  iscore::Component{id, "SerializedComponents", parent},
  data(std::move(obj))
{

}

bool JSONSerializedComponents::deserializeRemaining(
    Components& comps,
    QObject* entity)
{
  /*
  auto& ctx = iscore::IDocument::documentContext(*entity);
  auto& comps = ctx.app.factories<iscore::SerializableComponentList>();
  for(auto it = v.begin(); it != v.end(); )
  {
    auto& arr = *it;
    JSONObject::Deserializer s{arr};
    auto res = deserialize_interface(comps, s, entity);
    if(res)
    {
      components.add(res);
      it = v.erase(it);
    }
    else
    {
      ++it;
    }
  }

*/
  return data.empty();
}

bool DataStreamSerializedComponents::deserializeRemaining(
    Components& components,
    QObject* entity)
{
  /*
  auto& ctx = iscore::IDocument::documentContext(*entity);
  auto& comps = ctx.app.factories<iscore::SerializableComponentList>();
  for(auto it = data.begin(); it != data.end(); )
  {
    DataStream::Deserializer s{it->second};
    auto res = deserialize_interface(comps, s, entity);
    if(res)
    {
      components.add(res);
      it = data.erase(it);
    }
    else
    {
      ++it;
    }
  }
*/
  return data.empty();
}


void deserializeRemainingComponents(iscore::Components& comps, QObject* obj)
{
    // First with datastream
    if(auto ser = findComponent<iscore::DataStreamSerializedComponents>(comps))
    {
      ser->deserializeRemaining(comps, obj);

      // If we deserialized everything, no point in keeping this one
      if(ser->finished())
        comps.remove(ser);
    }

    // Then with JSON
    if(auto ser = findComponent<iscore::JSONSerializedComponents>(comps))
    {
      ser->deserializeRemaining(comps, obj);

      // If we deserialized everything, no point in keeping this one
      if(ser->finished())
        comps.remove(ser);
    }
}

}

template<>
void DataStreamReader::read<iscore::SerializableComponent>(const iscore::SerializableComponent&)
{

}
template<>
void JSONObjectReader::read<iscore::SerializableComponent>(const iscore::SerializableComponent&)
{

}
