#include "JSProcessModel.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom(const JSProcessModel& proc)
{
    readFrom(*proc.pluginModelList);

    m_stream << proc.m_script;

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(JSProcessModel& proc)
{
    proc.pluginModelList = new iscore::ElementPluginModelList{*this, &proc};

    QString str;
    m_stream >> str;
    proc.setScript(str);

    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const JSProcessModel& proc)
{
    m_obj["PluginsMetadata"] = toJsonValue(*proc.pluginModelList);
    m_obj["Script"] = proc.script();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(JSProcessModel& proc)
{
    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    proc.pluginModelList = new iscore::ElementPluginModelList{elementPluginDeserializer, &proc};

    proc.setScript(m_obj["Script"].toString());
}
