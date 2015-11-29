#include <qjsonobject.h>
#include <qjsonvalue.h>

#include "DummyModel.hpp"
#include "iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp"
#include "iscore/serialization/DataStreamVisitor.hpp"
#include "iscore/serialization/JSONValueVisitor.hpp"
#include "iscore/serialization/JSONVisitor.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom(const DummyModel& proc)
{
    readFrom(*proc.pluginModelList);

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(DummyModel& proc)
{
    proc.pluginModelList = new iscore::ElementPluginModelList{*this, &proc};

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const DummyModel& proc)
{
    m_obj["PluginsMetadata"] = toJsonValue(*proc.pluginModelList);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(DummyModel& proc)
{
    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    proc.pluginModelList = new iscore::ElementPluginModelList{elementPluginDeserializer, &proc};
}
