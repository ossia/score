// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MappingModel.hpp"

#include <Curve/CurveModel.hpp>
#include <Process/Dataflow/Port.hpp>
#include <State/Address.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>



template <>
void DataStreamReader::read(const Mapping::ProcessModel& autom)
{
  m_stream << *autom.inlet << *autom.outlet;
  readFrom(autom.curve());

  m_stream << autom.sourceMin() << autom.sourceMax() << autom.targetMin()
           << autom.targetMax();

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Mapping::ProcessModel& autom)
{
  autom.inlet = Process::load_value_inlet(*this, &autom);
  autom.outlet = Process::load_value_outlet(*this, &autom);
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
void JSONReader::read(const Mapping::ProcessModel& autom)
{
  obj["Inlet"] = *autom.inlet;
  obj["Outlet"] = *autom.outlet;
  obj["Curve"] = autom.curve();

  obj["SourceMin"] = autom.sourceMin();
  obj["SourceMax"] = autom.sourceMax();

  obj["TargetMin"] = autom.targetMin();
  obj["TargetMax"] = autom.targetMax();
}

template <>
void JSONWriter::write(Mapping::ProcessModel& autom)
{
  {
    JSONWriter writer{obj["Inlet"]};
    autom.inlet = Process::load_value_inlet(writer, &autom);
  }
  {
    JSONWriter writer{obj["Outlet"]};
    autom.outlet = Process::load_value_outlet(writer, &autom);
  }

  JSONObject::Deserializer curve_deser{obj["Curve"]};
  autom.setCurve(new Curve::Model{curve_deser, &autom});

  autom.setSourceMin(obj["SourceMin"].toDouble());
  autom.setSourceMax(obj["SourceMax"].toDouble());

  autom.setTargetMin(obj["TargetMin"].toDouble());
  autom.setTargetMax(obj["TargetMax"].toDouble());
}
