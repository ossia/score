#include <iscore/serialization/VisitorCommon.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>

#include <Curve/CurveModel.hpp>
#include "MappingLayerModel.hpp"
#include "MappingModel.hpp"
#include <State/Address.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

namespace Process { class LayerModel; }
class QObject;
struct VisitorVariant;
template <typename T> class Reader;
template <typename T> class Writer;

template<>
void Visitor<Reader<DataStream>>::readFrom_impl(
        const Mapping::ProcessModel& autom)
{
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
void Visitor<Writer<DataStream>>::writeTo(
        Mapping::ProcessModel& autom)
{
    autom.setCurve(new Curve::Model{*this, &autom});
    { // Source
        State::Address address;
        double min, max;

        m_stream >> address >> min >> max;

        autom.setSourceAddress(address);
        autom.setSourceMin(min);
        autom.setSourceMax(max);
    }
    { // Target
        State::Address address;
        double min, max;

        m_stream >> address >> min >> max;

        autom.setTargetAddress(address);
        autom.setTargetMin(min);
        autom.setTargetMax(max);
    }
    checkDelimiter();
}




template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(
        const Mapping::ProcessModel& autom)
{
    m_obj["Curve"] = toJsonObject(autom.curve());

    m_obj["SourceAddress"] = toJsonObject(autom.sourceAddress());
    m_obj["SourceMin"] = autom.sourceMin();
    m_obj["SourceMax"] = autom.sourceMax();

    m_obj["TargetAddress"] = toJsonObject(autom.targetAddress());
    m_obj["TargetMin"] = autom.targetMin();
    m_obj["TargetMax"] = autom.targetMax();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Mapping::ProcessModel& autom)
{
    Deserializer<JSONObject> curve_deser{m_obj["Curve"].toObject()};
    autom.setCurve(new Curve::Model{curve_deser, &autom});

    autom.setSourceAddress(fromJsonObject<State::Address>(m_obj["SourceAddress"]));
    autom.setSourceMin(m_obj["SourceMin"].toDouble());
    autom.setSourceMax(m_obj["SourceMax"].toDouble());

    autom.setTargetAddress(fromJsonObject<State::Address>(m_obj["TargetAddress"]));
    autom.setTargetMin(m_obj["TargetMin"].toDouble());
    autom.setTargetMax(m_obj["TargetMax"].toDouble());
}
