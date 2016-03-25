#include <Process/Process.hpp>

#include <QJsonObject>
#include <QJsonValue>

#include <Process/ModelMetadata.hpp>
#include <Process/ProcessFactory.hpp>

#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>


class QObject;
template <typename model> class IdentifiedObject;


template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const Process::ProcessModel& process)
{
    // To allow recration using createProcess
    readFrom(static_cast<const IdentifiedObject<Process::ProcessModel>&>(process));

    readFrom(process.duration());
    //m_stream << process.useParentDuration();

    readFrom(process.metadata);
}

// We only load the members of the process here.

template<>
void Visitor<Writer<DataStream>>::writeTo(Process::ProcessModel& process)
{
    writeTo(process.m_duration);
    //m_stream >> process.m_useParentDuration;
    writeTo(process.metadata);

    // Delimiter checked on createProcess
}



template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const Process::ProcessModel& process)
{
    readFrom(static_cast<const IdentifiedObject<Process::ProcessModel>&>(process));

    m_obj[iscore::StringConstant().Duration] = toJsonValue(process.duration());
    //m_obj["UseParentDuration"] = process.useParentDuration();
    m_obj[iscore::StringConstant().Metadata] = toJsonObject(process.metadata);
}


template<>
void Visitor<Writer<JSONObject>>::writeTo(Process::ProcessModel& process)
{
    process.m_duration = fromJsonValue<TimeValue>(m_obj[iscore::StringConstant().Duration]);
    //process.m_useParentDuration = m_obj["UseParentDuration"].toBool();
    process.metadata = fromJsonObject<ModelMetadata>(m_obj[iscore::StringConstant().Metadata]);
}









// MOVEME
#include <Process/StateProcess.hpp>
#include <Process/ProcessList.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const Process::StateProcess& process)
{
    readFrom(static_cast<const IdentifiedObject<Process::StateProcess>&>(process));
}

// We only load the members of the process here.

template<>
void Visitor<Writer<DataStream>>::writeTo(Process::StateProcess&)
{
    // Delimiter checked on createProcess
}



template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const Process::StateProcess& process)
{
    readFrom(static_cast<const IdentifiedObject<Process::StateProcess>&>(process));
}


template<>
void Visitor<Writer<JSONObject>>::writeTo(Process::StateProcess& process)
{
}
