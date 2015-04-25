#include "ElementPluginModelList.hpp"
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelSerialization.hpp>


template<> void Visitor<Reader<DataStream>>::readFrom(const iscore::ElementPluginModelList& elts)
{
    m_stream << elts.list().size();
    for(auto& plug : elts.list())
    {
        readFrom(*plug);
    }

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(iscore::ElementPluginModelList& elts)
{
    int plugin_count;
    m_stream >> plugin_count;

    for(; plugin_count -- > 0;)
    {
        elts.add(deserializeElementPluginModel(*this, elts.parent(), &elts)); // Note : QObject::parent() is dangerous.
    }

    checkDelimiter();
}

template<> void Visitor<Reader<JSON>>::readFrom(const iscore::ElementPluginModelList& elts)
{
    m_obj["PluginsMetadata"] = toJsonArray(elts.list());
}

template<> void Visitor<Writer<JSON>>::writeTo(iscore::ElementPluginModelList& elts)
{
    QJsonArray plugin_array = m_obj["PluginsMetadata"].toArray();

    for(const auto& json_vref : plugin_array)
    {
        Deserializer<JSON> deserializer{json_vref.toObject()};

        elts.add(deserializeElementPluginModel(deserializer, elts.parent(), &elts));
    }
}
