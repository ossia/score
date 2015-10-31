#include "MappingModel.hpp"
#include "MappingFactory.hpp"
#include "MappingLayerModel.hpp"
#include <iscore/serialization/VisitorCommon.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const MappingModel& autom)
{
    readFrom(*autom.pluginModelList); // TODO it is unbearable to save / load these every time

    readFrom(autom.curve());

    m_stream << autom.sourceAddress()
             << autom.sourceMin()
             << autom.sourceMax();

    m_stream << autom.targetAddress()
             << autom.targetMin()
             << autom.targetMax();

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(MappingModel& autom)
{
    autom.pluginModelList = new iscore::ElementPluginModelList{*this, &autom};

    autom.setCurve(new CurveModel{*this, &autom});
    { // Source
        iscore::Address address;
        double min, max;

        m_stream >> address >> min >> max;

        autom.setSourceAddress(address);
        autom.setSourceMin(min);
        autom.setSourceMax(max);
    }
    { // Target
        iscore::Address address;
        double min, max;

        m_stream >> address >> min >> max;

        autom.setTargetAddress(address);
        autom.setTargetMin(min);
        autom.setTargetMax(max);
    }
    checkDelimiter();
}




template<>
void Visitor<Reader<JSONObject>>::readFrom(const MappingModel& autom)
{
    m_obj["PluginsMetadata"] = toJsonValue(*autom.pluginModelList);

    m_obj["Curve"] = toJsonObject(autom.curve());

    m_obj["SourceAddress"] = toJsonObject(autom.sourceAddress());
    m_obj["SourceMin"] = autom.sourceMin();
    m_obj["SourceMax"] = autom.sourceMax();

    m_obj["TargetAddress"] = toJsonObject(autom.targetAddress());
    m_obj["TargetMin"] = autom.targetMin();
    m_obj["TargetMax"] = autom.targetMax();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(MappingModel& autom)
{
    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    autom.pluginModelList = new iscore::ElementPluginModelList{elementPluginDeserializer, &autom};

    Deserializer<JSONObject> curve_deser{m_obj["Curve"].toObject()};
    autom.setCurve(new CurveModel{curve_deser, &autom});

    autom.setSourceAddress(fromJsonObject<iscore::Address>(m_obj["SourceAddress"].toObject()));
    autom.setSourceMin(m_obj["SourceMin"].toDouble());
    autom.setSourceMax(m_obj["SourceMax"].toDouble());

    autom.setTargetAddress(fromJsonObject<iscore::Address>(m_obj["TargetAddress"].toObject()));
    autom.setTargetMin(m_obj["TargetMin"].toDouble());
    autom.setTargetMax(m_obj["TargetMax"].toDouble());
}




void MappingModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

Process* MappingFactory::loadModel(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    { return new MappingModel{deserializer, parent};});
}

LayerModel* MappingModel::loadLayer_impl(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto autom = new MappingLayerModel{
                        deserializer, *this, parent};

        return autom;
    });
}
