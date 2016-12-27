#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>
#include <iscore/serialization/VisitorCommon.hpp>

#include "MappingLayerModel.hpp"
#include "MappingModel.hpp"
#include <Curve/CurveModel.hpp>
#include <State/Address.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

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
void DataStreamReader::read(
    const Mapping::ProcessModel& autom)
{
  readFrom(autom.curve());

  m_stream << autom.sourceAddress() << autom.sourceMin() << autom.sourceMax();

  m_stream << autom.targetAddress() << autom.targetMin() << autom.targetMax();

  insertDelimiter();
}


template <>
void DataStreamWriter::writeTo(Mapping::ProcessModel& autom)
{
  autom.setCurve(new Curve::Model{*this, &autom});
  { // Source
    State::AddressAccessor address;
    double min, max;

    m_stream >> address >> min >> max;

    autom.setSourceAddress(address);
    autom.setSourceMin(min);
    autom.setSourceMax(max);
  }
  { // Target
    State::AddressAccessor address;
    double min, max;

    m_stream >> address >> min >> max;

    autom.setTargetAddress(address);
    autom.setTargetMin(min);
    autom.setTargetMax(max);
  }
  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Mapping::ProcessModel& autom)
{
  obj["Curve"] = toJsonObject(autom.curve());

  obj["SourceAddress"] = toJsonObject(autom.sourceAddress());
  obj["SourceMin"] = autom.sourceMin();
  obj["SourceMax"] = autom.sourceMax();

  obj["TargetAddress"] = toJsonObject(autom.targetAddress());
  obj["TargetMin"] = autom.targetMin();
  obj["TargetMax"] = autom.targetMax();
}


template <>
void JSONObjectWriter::writeTo(Mapping::ProcessModel& autom)
{
  JSONObject::Deserializer curve_deser{obj["Curve"].toObject()};
  autom.setCurve(new Curve::Model{curve_deser, &autom});

  autom.setSourceAddress(
      fromJsonObject<State::AddressAccessor>(obj["SourceAddress"]));
  autom.setSourceMin(obj["SourceMin"].toDouble());
  autom.setSourceMax(obj["SourceMax"].toDouble());

  autom.setTargetAddress(
      fromJsonObject<State::AddressAccessor>(obj["TargetAddress"]));
  autom.setTargetMin(obj["TargetMin"].toDouble());
  autom.setTargetMax(obj["TargetMax"].toDouble());
}
