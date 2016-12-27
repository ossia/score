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


template <>
void DataStreamReader::read(
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
void DataStreamWriter::writeTo(Automation::ProcessModel& autom)
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
void JSONObjectReader::read(
    const Automation::ProcessModel& autom)
{
  obj["Curve"] = toJsonObject(autom.curve());
  obj[strings.Address] = toJsonObject(autom.address());
  obj[strings.Min] = autom.min();
  obj[strings.Max] = autom.max();
  obj["Tween"] = autom.tween();
}


template <>
void JSONObjectWriter::writeTo(Automation::ProcessModel& autom)
{
  JSONObject::Deserializer curve_deser{obj["Curve"].toObject()};
  autom.setCurve(new Curve::Model{curve_deser, &autom});

  autom.setAddress(
      fromJsonObject<State::AddressAccessor>(obj[strings.Address]));
  autom.setMin(obj[strings.Min].toDouble());
  autom.setMax(obj[strings.Max].toDouble());
  autom.setTween(obj["Tween"].toBool());
}
