#include <QJsonObject>
#include <QJsonValue>

#include "ConstraintDurations.hpp"
#include <Process/TimeValue.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <typename T>
class Reader;
template <typename T>
class Writer;


template <>
void DataStreamReader::read(
    const Scenario::ConstraintDurations& durs)
{
  m_stream << durs.m_defaultDuration << durs.m_minDuration
           << durs.m_maxDuration << durs.m_rigidity << durs.m_isMinNull
           << durs.m_isMaxInfinite;
}


template <>
void DataStreamWriter::writeTo(Scenario::ConstraintDurations& durs)
{
  m_stream >> durs.m_defaultDuration >> durs.m_minDuration
      >> durs.m_maxDuration >> durs.m_rigidity >> durs.m_isMinNull
      >> durs.m_isMaxInfinite;
}


template <>
void JSONObjectReader::readFrom(
    const Scenario::ConstraintDurations& durs)
{
  obj["DefaultDuration"] = toJsonValue(durs.m_defaultDuration);
  obj["MinDuration"] = toJsonValue(durs.m_minDuration);
  obj["MaxDuration"] = toJsonValue(durs.m_maxDuration);
  obj["Rigidity"] = durs.m_rigidity;
  obj["MinNull"] = durs.m_isMinNull;
  obj["MaxInf"] = durs.m_isMaxInfinite;
}


template <>
void JSONObjectWriter::writeTo(Scenario::ConstraintDurations& durs)
{
  durs.m_defaultDuration = fromJsonValue<TimeValue>(obj["DefaultDuration"]);
  durs.m_minDuration = fromJsonValue<TimeValue>(obj["MinDuration"]);
  durs.m_maxDuration = fromJsonValue<TimeValue>(obj["MaxDuration"]);
  durs.m_rigidity = obj["Rigidity"].toBool();
  durs.m_isMinNull = obj["MinNull"].toBool();
  durs.m_isMaxInfinite = obj["MaxInf"].toBool();
}
