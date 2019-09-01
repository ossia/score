// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>
#include <Process/TimeValueSerialization.hpp>

#include <score/model/ModelMetadata.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/plugins/StringFactoryKeySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>


template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamReader::read(const Process::ProcessModel& process)
{
  m_stream << process.m_duration << process.m_slotHeight << process.m_startOffset << process.m_loopDuration << process.m_position << process.m_size << process.m_loops;
}

// We only load the members of the process here.
template <>
SCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::ProcessModel& process)
{
  m_stream >> process.m_duration >> process.m_slotHeight >> process.m_startOffset >> process.m_loopDuration >> process.m_position >> process.m_size >> process.m_loops;
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectReader::read(const Process::ProcessModel& process)
{
  obj[strings.Duration] = toJsonValue(process.duration());
  obj[strings.Height] = process.getSlotHeight();
  obj["StartOffset"] = toJsonValue(process.startOffset());
  obj["LoopDuration"] = toJsonValue(process.loopDuration());
  obj["Pos"] = toJsonValue(process.m_position);
  obj["Size"] = toJsonValue(process.m_size);
  obj["Loops"] = process.loops();
}

template <>
SCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write(Process::ProcessModel& process)
{
  process.m_duration = fromJsonValue<TimeVal>(obj[strings.Duration]);
  auto h_it = obj.constFind(strings.Height);
  if (h_it != obj.constEnd())
    process.m_slotHeight = obj[strings.Height].toDouble();
  else
    process.m_slotHeight = 300;

  process.m_startOffset = fromJsonValue<TimeVal>(obj["StartOffset"]);
  process.m_loopDuration = fromJsonValue<TimeVal>(obj["LoopDuration"]);
  process.m_position = fromJsonValue<QPointF>(obj["Pos"]);
  process.m_size = fromJsonValue<QSizeF>(obj["Size"]);
  process.m_loops = obj["Loops"].toBool();
}
