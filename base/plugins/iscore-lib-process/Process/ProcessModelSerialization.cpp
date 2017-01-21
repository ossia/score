#include <Process/Process.hpp>

#include <QJsonObject>
#include <QJsonValue>

#include <Process/ProcessFactory.hpp>
#include <iscore/model/ModelMetadata.hpp>

#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <>
ISCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(
    const Process::ProcessModel& process)
{
  readFrom(process.duration());
  // m_stream << process.useParentDuration();
}

// We only load the members of the process here.
template <>
ISCORE_LIB_PROCESS_EXPORT void
DataStreamWriter::write(Process::ProcessModel& process)
{
  writeTo(process.m_duration);
  // m_stream >> process.m_useParentDuration;

  // Delimiter checked on createProcess
}

template <>
ISCORE_LIB_PROCESS_EXPORT void JSONObjectReader::read(
    const Process::ProcessModel& process)
{
  obj[strings.Duration] = toJsonValue(process.duration());
  // obj["UseParentDuration"] = process.useParentDuration();
}

template <>
ISCORE_LIB_PROCESS_EXPORT void
JSONObjectWriter::write(Process::ProcessModel& process)
{
  process.m_duration = fromJsonValue<TimeVal>(obj[strings.Duration]);
  // process.m_useParentDuration = obj["UseParentDuration"].toBool();
}
