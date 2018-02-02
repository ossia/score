// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include "AutomationModel.hpp"
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Curve/CurveModel.hpp>
#include <State/Address.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

template <>
void DataStreamReader::read(
    const Automation::ProcessModel& autom)
{
  m_stream << *autom.outlet;
  readFrom(autom.curve());

  m_stream
      << autom.min()
      << autom.max()
      << autom.tween();

  insertDelimiter();
}


template <>
void DataStreamWriter::write(Automation::ProcessModel& autom)
{
  autom.outlet = Process::make_outlet(*this, &autom);

  autom.setCurve(new Curve::Model{*this, &autom});

  double min, max;
  bool tw;

  m_stream >> min >> max >> tw;

  autom.setMin(min);
  autom.setMax(max);
  autom.setTween(tw);

  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Automation::ProcessModel& autom)
{
  obj["Outlet"] = toJsonObject(*autom.outlet);
  obj["Curve"] = toJsonObject(autom.curve());
  obj[strings.Min] = autom.min();
  obj[strings.Max] = autom.max();
  obj["Tween"] = autom.tween();
}


template <>
void JSONObjectWriter::write(Automation::ProcessModel& autom)
{
  JSONObjectWriter writer{obj["Outlet"].toObject()};
  autom.outlet = Process::make_outlet(writer, &autom);
  if(!autom.outlet)
  {
    autom.outlet = Process::make_outlet(Id<Process::Port>(0), &autom);
    autom.outlet->type = Process::PortType::Message;
    autom.outlet->setAddress(fromJsonObject<State::AddressAccessor>(obj[strings.Address].toObject()));
  }

  JSONObject::Deserializer curve_deser{obj["Curve"].toObject()};
  autom.setCurve(new Curve::Model{curve_deser, &autom});

  autom.setMin(obj[strings.Min].toDouble());
  autom.setMax(obj[strings.Max].toDouble());
  autom.setTween(obj["Tween"].toBool());
}
