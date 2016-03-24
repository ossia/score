#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>

#include "AutomationLayerModel.hpp"
#include "AutomationModel.hpp"
#include <Curve/CurveModel.hpp>
#include <State/Address.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

namespace Process { class LayerModel; }
class QObject;
struct VisitorVariant;
template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom_impl(
        const Automation::ProcessModel& autom)
{
    readFrom(*autom.pluginModelList);

    readFrom(autom.curve());

    m_stream << autom.address();
    m_stream << autom.min();
    m_stream << autom.max();

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Automation::ProcessModel& autom)
{
    autom.pluginModelList = new iscore::ElementPluginModelList{*this, &autom};

    autom.setCurve(new Curve::Model{*this, &autom});

    State::Address address;
    double min, max;

    m_stream >> address >> min >> max;

    autom.setAddress(address);
    autom.setMin(min);
    autom.setMax(max);

    checkDelimiter();
}




template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(
        const Automation::ProcessModel& autom)
{
    m_obj["PluginsMetadata"] = toJsonValue(*autom.pluginModelList);

    m_obj["Curve"] = toJsonObject(autom.curve());
    m_obj["Address"] = toJsonObject(autom.address());
    m_obj["Min"] = autom.min();
    m_obj["Max"] = autom.max();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Automation::ProcessModel& autom)
{
    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    autom.pluginModelList = new iscore::ElementPluginModelList{elementPluginDeserializer, &autom};

    Deserializer<JSONObject> curve_deser{m_obj["Curve"].toObject()};
    autom.setCurve(new Curve::Model{curve_deser, &autom});

    autom.setAddress(fromJsonObject<State::Address>(m_obj["Address"]));
    autom.setMin(m_obj["Min"].toDouble());
    autom.setMax(m_obj["Max"].toDouble());
}


// Dynamic stuff
namespace Automation
{
void ProcessModel::serialize_impl(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

Process::LayerModel* ProcessModel::loadLayer_impl(
        const VisitorVariant& vis,
        QObject* parent)
{
    return deserialize_dyn(vis, [&] (auto&& deserializer)
    {
        auto autom = new LayerModel{
                        deserializer, *this, parent};

        return autom;
    });
}
}
