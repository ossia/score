#include "AutomationModel.hpp"
#include "AutomationLayerModel.hpp"
#include "AutomationFactory.hpp"
#include "Curve/CurveModel.hpp"

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const AutomationModel& autom)
{
    readFrom(*autom.pluginModelList);

    m_stream << autom.address();
    m_stream << autom.min();
    m_stream << autom.max();
    readFrom(autom.curve());

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(AutomationModel& autom)
{
    autom.pluginModelList = new iscore::ElementPluginModelList{*this, &autom};

    iscore::Address address;
    double min, max;

    m_stream >> address >> min >> max;

    autom.setAddress(address);
    autom.setMin(min);
    autom.setMax(max);

    autom.setCurve(new CurveModel{*this, &autom});

    checkDelimiter();
}




template<>
void Visitor<Reader<JSONObject>>::readFrom(const AutomationModel& autom)
{
    m_obj["PluginsMetadata"] = toJsonValue(*autom.pluginModelList);

    m_obj["Address"] = toJsonObject(autom.address());
    m_obj["Min"] = autom.min();
    m_obj["Max"] = autom.max();
    m_obj["Curve"] = toJsonObject(autom.curve());
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(AutomationModel& autom)
{
    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    autom.pluginModelList = new iscore::ElementPluginModelList{elementPluginDeserializer, &autom};

    autom.setAddress(fromJsonObject<iscore::Address>(m_obj["Address"].toObject()));
    autom.setMin(m_obj["Min"].toDouble());
    autom.setMax(m_obj["Max"].toDouble());

    Deserializer<JSONObject> curve_deser{m_obj["Curve"].toObject()};
    autom.setCurve(new CurveModel{curve_deser, &autom});

}


// Dynamic stuff
#include <iscore/serialization/VisitorCommon.hpp>
void AutomationModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

Process* AutomationFactory::loadModel(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    { return new AutomationModel{deserializer, parent};});
}

LayerModel* AutomationModel::loadLayer_impl(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto autom = new AutomationLayerModel{
                        deserializer, *this, parent};

        return autom;
    });
}



/////// ViewModel
// TODO in its own file.
// Also id's should be saved.
template<>
void Visitor<Reader<DataStream>>::readFrom(const AutomationLayerModel& lm)
{
}

template<>
void Visitor<Writer<DataStream>>::writeTo(AutomationLayerModel& lm)
{
}



template<>
void Visitor<Reader<JSONObject>>::readFrom(const AutomationLayerModel& lm)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(AutomationLayerModel& lm)
{
}
