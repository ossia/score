#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>

#include "Autom3DLayerModel.hpp"
#include "Autom3DModel.hpp"
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
void Visitor<Reader<DataStream>>::readFrom(const Autom3D::ProcessModel& autom)
{
    readFrom(*autom.pluginModelList);

    m_stream << autom.address();
    m_stream << autom.min();
    m_stream << autom.max();

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Autom3D::ProcessModel& autom)
{
    autom.pluginModelList = new iscore::ElementPluginModelList{*this, &autom};

    State::Address address;
    Autom3D::Point min, max;

    m_stream >> address >> min >> max;

    autom.setAddress(address);
    autom.setMin(min);
    autom.setMax(max);

    checkDelimiter();
}




template<>
void Visitor<Reader<JSONObject>>::readFrom(const Autom3D::ProcessModel& autom)
{
    /*
    m_obj["PluginsMetadata"] = toJsonValue(*autom.pluginModelList);

    m_obj["Address"] = toJsonObject(autom.address());
    m_obj["Min"] = autom.min();
    m_obj["Max"] = autom.max();
    */
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Autom3D::ProcessModel& autom)
{
    /*
    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    autom.pluginModelList = new iscore::ElementPluginModelList{elementPluginDeserializer, &autom};

    autom.setAddress(fromJsonObject<State::Address>(m_obj["Address"].toObject()));
    autom.setMin(m_obj["Min"].toDouble());
    autom.setMax(m_obj["Max"].toDouble());
    */
}


// Dynamic stuff
namespace Autom3D
{
void ProcessModel::serialize(const VisitorVariant& vis) const
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
