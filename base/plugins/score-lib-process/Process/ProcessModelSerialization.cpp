// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Process.hpp>

#include <QJsonObject>
#include <QJsonValue>

#include <Process/ProcessFactory.hpp>
#include <score/model/ModelMetadata.hpp>

#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>
#include <Process/TimeValueSerialization.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>

#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/plugins/customfactory/StringFactoryKeySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(
    const Process::ProcessModel& process)
{
  m_stream << process.m_duration
          << process.m_slotHeight;
}

// We only load the members of the process here.
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::ProcessModel& process)
{
  m_stream >> process.m_duration
           >> process.m_slotHeight;
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONObjectReader::read(
    const Process::ProcessModel& process)
{
  obj[strings.Duration] = toJsonValue(process.duration());
  obj[strings.Height] = process.getSlotHeight();
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write(Process::ProcessModel& process)
{
  process.m_duration = fromJsonValue<TimeVal>(obj[strings.Duration]);
  auto h_it = obj.constFind(strings.Height);
  if(h_it != obj.constEnd())
    process.m_slotHeight = obj[strings.Height].toDouble();
  else
    process.m_slotHeight = 300;

}
