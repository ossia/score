#include <QJsonObject>
#include <QJsonValue>

#include "DummyModel.hpp"
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom_impl(
        const Dummy::DummyModel& proc)
{
    readFrom(*proc.pluginModelList);

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Dummy::DummyModel& proc)
{
    proc.pluginModelList = new iscore::ElementPluginModelList{*this, &proc};

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(
        const Dummy::DummyModel& proc)
{
    m_obj["PluginsMetadata"] = toJsonValue(*proc.pluginModelList);
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Dummy::DummyModel& proc)
{
    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    proc.pluginModelList = new iscore::ElementPluginModelList{elementPluginDeserializer, &proc};
}
