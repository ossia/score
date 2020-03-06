// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AutomationModel.hpp"

#include <Curve/CurveModel.hpp>
#include <Process/Dataflow/Port.hpp>
#include <State/Address.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <ossia/network/dataspace/dataspace_visitors.hpp>



template <>
void DataStreamReader::read(const Automation::ProcessModel& autom)
{
  m_stream << *autom.outlet;
  readFrom(autom.curve());

  m_stream << autom.tween();

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Automation::ProcessModel& autom)
{
  autom.outlet = Process::load_value_outlet(*this, &autom);

  autom.setCurve(new Curve::Model{*this, &autom});

  bool tw;

  m_stream >> tw;

  autom.setTween(tw);

  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Automation::ProcessModel& autom)
{
  obj["Outlet"] = toJsonObject(*autom.outlet);
  obj["Curve"] = toJsonObject(autom.curve());
  obj["Tween"] = autom.tween();
}

template <>
void JSONObjectWriter::write(Automation::ProcessModel& autom)
{
  JSONObjectWriter writer{obj["Outlet"].toObject()};
  autom.outlet = Process::load_value_outlet(writer, &autom);

  JSONObject::Deserializer curve_deser{obj["Curve"].toObject()};
  autom.setCurve(new Curve::Model{curve_deser, &autom});

  autom.setTween(obj["Tween"].toBool());
}
