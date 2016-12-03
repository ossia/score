#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "AutomationLayerModel.hpp"
#include "AutomationModel.hpp"
#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <Curve/CurveModel.hpp>
#include <State/Address.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

namespace Process
{
class LayerModel;
}
class QObject;
struct VisitorVariant;
template <typename T>
class Reader;
template <typename T>
class Writer;

template <>
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

template <>
void Visitor<Writer<DataStream>>::writeTo(Automation::ProcessModel& autom)
{
  autom.setCurve(new Curve::Model{*this, &autom});

  State::AddressAccessor address;
  double min, max;
  bool tw;

  m_stream >> address >> min >> max >> tw;

  autom.setAddress(address);
  autom.setMin(min);
  autom.setMax(max);
  autom.setTween(tw);

  checkDelimiter();
}

template <>
void Visitor<Reader<JSONObject>>::readFrom_impl(
    const Automation::ProcessModel& autom)
{
  m_obj["Curve"] = toJsonObject(autom.curve());
  m_obj[strings.Address] = toJsonObject(autom.address());
  m_obj[strings.Min] = autom.min();
  m_obj[strings.Max] = autom.max();
  m_obj["Tween"] = autom.tween();
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(Automation::ProcessModel& autom)
{
  Deserializer<JSONObject> curve_deser{m_obj["Curve"].toObject()};
  autom.setCurve(new Curve::Model{curve_deser, &autom});

  autom.setAddress(
      fromJsonObject<State::AddressAccessor>(m_obj[strings.Address]));
  autom.setMin(m_obj[strings.Min].toDouble());
  autom.setMax(m_obj[strings.Max].toDouble());
  autom.setTween(m_obj["Tween"].toBool());
}
