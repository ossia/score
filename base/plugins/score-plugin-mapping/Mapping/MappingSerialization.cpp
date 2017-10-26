// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>
#include <score/serialization/VisitorCommon.hpp>

#include <Process/Dataflow/Port.hpp>
#include "MappingModel.hpp"
#include <Curve/CurveModel.hpp>
#include <State/Address.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(
    const Mapping::ProcessModel& autom)
{
  m_stream << *autom.inlet << *autom.outlet;
  readFrom(autom.curve());

  m_stream << autom.sourceMin() << autom.sourceMax()
           << autom.targetMin() << autom.targetMax();

  insertDelimiter();
}


template <>
void DataStreamWriter::write(Mapping::ProcessModel& autom)
{
  autom.inlet = std::make_unique<Process::Port>(*this, &autom);
  autom.outlet = std::make_unique<Process::Port>(*this, &autom);
  autom.setCurve(new Curve::Model{*this, &autom});

  { // Source
    double min, max;

    m_stream >> min >> max;

    autom.setSourceMin(min);
    autom.setSourceMax(max);
  }
  { // Target
    double min, max;

    m_stream >> min >> max;

    autom.setTargetMin(min);
    autom.setTargetMax(max);
  }
  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Mapping::ProcessModel& autom)
{
  obj["Inlet"] = toJsonObject(*autom.inlet);
  obj["Outlet"] = toJsonObject(*autom.outlet);
  obj["Curve"] = toJsonObject(autom.curve());

  obj["SourceMin"] = autom.sourceMin();
  obj["SourceMax"] = autom.sourceMax();

  obj["TargetMin"] = autom.targetMin();
  obj["TargetMax"] = autom.targetMax();
}


template <>
void JSONObjectWriter::write(Mapping::ProcessModel& autom)
{
  {
    JSONObjectWriter writer{obj["Inlet"].toObject()};
    autom.inlet = std::make_unique<Process::Port>(writer, &autom);
  }
  {
    JSONObjectWriter writer{obj["Outlet"].toObject()};
    autom.outlet = std::make_unique<Process::Port>(writer, &autom);
  }

  JSONObject::Deserializer curve_deser{obj["Curve"].toObject()};
  autom.setCurve(new Curve::Model{curve_deser, &autom});

  autom.setSourceMin(obj["SourceMin"].toDouble());
  autom.setSourceMax(obj["SourceMax"].toDouble());

  autom.setTargetMin(obj["TargetMin"].toDouble());
  autom.setTargetMax(obj["TargetMax"].toDouble());
}
