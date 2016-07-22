#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>

#include "AutomationLayerModel.hpp"
#include "AutomationModel.hpp"
#include <Curve/CurveModel.hpp>
#include <State/Address.hpp>
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
    readFrom(autom.curve());

    m_stream << autom.address();
    m_stream << autom.min();
    m_stream << autom.max();
    m_stream << autom.tween();

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Automation::ProcessModel& autom)
{
    autom.setCurve(new Curve::Model{*this, &autom});

    State::Address address;
    double min, max;
    bool tw;

    m_stream >> address >> min >> max >> tw;

    autom.setAddress(address);
    autom.setMin(min);
    autom.setMax(max);
    autom.setTween(tw);

    checkDelimiter();
}




template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(
        const Automation::ProcessModel& autom)
{
    m_obj["Curve"] = toJsonObject(autom.curve());
    m_obj[iscore::StringConstant().Address] = toJsonObject(autom.address());
    m_obj[iscore::StringConstant().Min] = autom.min();
    m_obj[iscore::StringConstant().Max] = autom.max();
    m_obj["Tween"] = autom.tween();
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Automation::ProcessModel& autom)
{
    Deserializer<JSONObject> curve_deser{m_obj["Curve"].toObject()};
    autom.setCurve(new Curve::Model{curve_deser, &autom});

    autom.setAddress(fromJsonObject<State::Address>(m_obj[iscore::StringConstant().Address]));
    autom.setMin(m_obj[iscore::StringConstant().Min].toDouble());
    autom.setMax(m_obj[iscore::StringConstant().Max].toDouble());
    autom.setTween(m_obj["Tween"].toBool());
}
