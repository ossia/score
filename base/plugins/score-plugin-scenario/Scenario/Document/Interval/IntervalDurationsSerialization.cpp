// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QJsonObject>
#include <QJsonValue>

#include "IntervalDurations.hpp"
#include <Process/TimeValue.hpp>
#include <Process/TimeValueSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
void DataStreamReader::read(
    const Scenario::IntervalDurations& durs)
{
  m_stream << durs.m_defaultDuration << durs.m_minDuration
           << durs.m_maxDuration << durs.m_guiDuration << durs.m_rigidity << durs.m_isMinNull
           << durs.m_isMaxInfinite;
}


template <>
void DataStreamWriter::write(Scenario::IntervalDurations& durs)
{
  m_stream >> durs.m_defaultDuration >> durs.m_minDuration
      >> durs.m_maxDuration >> durs.m_guiDuration
      >> durs.m_rigidity >> durs.m_isMinNull
      >> durs.m_isMaxInfinite;
}


template <>
void JSONObjectReader::read(
    const Scenario::IntervalDurations& durs)
{
  obj[strings.DefaultDuration] = toJsonValue(durs.m_defaultDuration);
  obj[strings.MinDuration] = toJsonValue(durs.m_minDuration);
  obj[strings.MaxDuration] = toJsonValue(durs.m_maxDuration);
  obj[strings.GuiDuration] = toJsonValue(durs.m_guiDuration);
  obj[strings.Rigidity] = durs.m_rigidity;
  obj[strings.MinNull] = durs.m_isMinNull;
  obj[strings.MaxInf] = durs.m_isMaxInfinite;
}


template <>
void JSONObjectWriter::write(Scenario::IntervalDurations& durs)
{
  durs.m_defaultDuration = fromJsonValue<TimeVal>(obj[strings.DefaultDuration]);
  durs.m_minDuration = fromJsonValue<TimeVal>(obj[strings.MinDuration]);
  durs.m_maxDuration = fromJsonValue<TimeVal>(obj[strings.MaxDuration]);

  durs.m_rigidity = obj[strings.Rigidity].toBool();
  durs.m_isMinNull = obj[strings.MinNull].toBool();
  durs.m_isMaxInfinite = obj[strings.MaxInf].toBool();

  auto guidur = obj.find(strings.GuiDuration);
  if(guidur != obj.end())
    durs.m_guiDuration = fromJsonValue<TimeVal>(*guidur);
  else
  {
    if(durs.m_isMaxInfinite)
      durs.m_guiDuration = durs.m_defaultDuration;
    else
      durs.m_guiDuration = durs.m_maxDuration;
  }
}
