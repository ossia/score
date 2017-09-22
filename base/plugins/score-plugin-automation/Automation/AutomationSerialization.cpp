// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include "AutomationModel.hpp"
#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <Curve/CurveModel.hpp>
#include <State/Address.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

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
void DataStreamWriter::write(Automation::ProcessModel& autom)
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
void JSONObjectWriter::write(Automation::ProcessModel& autom)
{
  JSONObject::Deserializer curve_deser{obj["Curve"].toObject()};
  autom.setCurve(new Curve::Model{curve_deser, &autom});

  auto adrObj = obj[strings.Address].toObject();
  if(adrObj.contains("Path"))
  {
      autom.setAddress(State::AddressAccessor{
                  fromJsonObject<State::Address>(adrObj)});
  }
  else
  {
      autom.setAddress(
          fromJsonObject<State::AddressAccessor>(adrObj));
  }
  autom.setMin(obj[strings.Min].toDouble());
  autom.setMax(obj[strings.Max].toDouble());
  autom.setTween(obj["Tween"].toBool());
}
