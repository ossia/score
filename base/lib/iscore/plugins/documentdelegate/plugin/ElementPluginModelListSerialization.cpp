#include "ElementPluginModelList.hpp"
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelSerialization.hpp>


template<> void Visitor<Reader<DataStream>>::readFrom(const iscore::ElementPluginModelList& elts)
{
    m_stream << elts.list().size();
    for(const iscore::ElementPluginModel* plug : elts.list())
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
        auto elt = deserializeElementPluginModel(*this, elts.parent(), elts.parent());
        if(elt)
            elts.add(elt);
        else
            ISCORE_ABORT;
    }

    checkDelimiter();
}

template<> void Visitor<Reader<JSONObject>>::readFrom(const iscore::ElementPluginModelList& elts)
{
    m_obj["PluginsMetadata"] = toJsonArray(elts.list());
}

template<> void Visitor<Writer<JSONObject>>::writeTo(iscore::ElementPluginModelList& elts)
{
    QJsonArray plugin_array = m_obj["PluginsMetadata"].toArray();

    for(const auto& json_vref : plugin_array)
    {
        Deserializer<JSONObject> deserializer{json_vref.toObject()};

        auto elt = deserializeElementPluginModel(deserializer, elts.parent(), elts.parent());
        if(elt)
            elts.add(elt);
        else
            ISCORE_ABORT;
    }
}


template<> void Visitor<Reader<JSONValue>>::readFrom(const iscore::ElementPluginModelList& elts)
{
    val = toJsonArray(elts.list());
}

template<> void Visitor<Writer<JSONValue>>::writeTo(iscore::ElementPluginModelList& elts)
{
    QJsonArray plugin_array = val.toArray();

    for(const auto& json_vref : plugin_array)
    {
        Deserializer<JSONObject> deserializer{json_vref.toObject()};

        auto elt = deserializeElementPluginModel(deserializer, elts.parent(), elts.parent());
        if(elt)
            elts.add(elt);
        else
            ISCORE_ABORT;
    }
}
