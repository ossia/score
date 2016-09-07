#include <Process/Process.hpp>

#include <QJsonObject>
#include <QJsonValue>

#include <iscore/model/ModelMetadata.hpp>
#include <Process/ProcessFactory.hpp>

#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>

template<>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Reader<DataStream>>::readFrom_impl(const Process::ProcessModel& process)
{
    // To allow recration using createProcess
    readFrom(static_cast<const IdentifiedObject<Process::ProcessModel>&>(process));

    readFrom(process.duration());
    //m_stream << process.useParentDuration();

    readFrom(process.metadata());
}

// We only load the members of the process here.
template<>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Writer<DataStream>>::writeTo(Process::ProcessModel& process)
{
    writeTo(process.m_duration);
    //m_stream >> process.m_useParentDuration;
    writeTo(process.metadata());

    // Delimiter checked on createProcess
}


template<>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Reader<JSONObject>>::readFrom_impl(const Process::ProcessModel& process)
{
    readFrom(static_cast<const IdentifiedObject<Process::ProcessModel>&>(process));

    m_obj[strings.Duration] = toJsonValue(process.duration());
    //m_obj["UseParentDuration"] = process.useParentDuration();
    m_obj[strings.Metadata] = toJsonObject(process.metadata());
}

template<>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Writer<JSONObject>>::writeTo(Process::ProcessModel& process)
{
    process.m_duration = fromJsonValue<TimeValue>(m_obj[strings.Duration]);
    //process.m_useParentDuration = m_obj["UseParentDuration"].toBool();
    process.metadata() = fromJsonObject<iscore::ModelMetadata>(m_obj[strings.Metadata]);
}
