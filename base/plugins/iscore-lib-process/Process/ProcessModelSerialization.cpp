#include <Process/Process.hpp>

#include <QJsonObject>
#include <QJsonValue>

#include <Process/ModelMetadata.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessFactoryKey.hpp>
#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>
#include "ProcessModelSerialization.hpp"
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
void Visitor<Reader<DataStream>>::readFrom(const Process::ProcessModel& process)
{
    // To allow recration using createProcess
    m_stream << process.key();

    readFrom(static_cast<const IdentifiedObject<Process::ProcessModel>&>(process));

    readFrom(process.m_duration);
    m_stream << process.m_useParentDuration;

    readFrom(process.metadata);

    // Save the subclass
    process.serialize(toVariant());

    insertDelimiter();
}

// We only load the members of the process here.

template<>
void Visitor<Writer<DataStream>>::writeTo(Process::ProcessModel& process)
{
    writeTo(process.m_duration);
    m_stream >> process.m_useParentDuration;
    writeTo(process.metadata);

    // Delimiter checked on createProcess
}

template<>
ISCORE_LIB_PROCESS_EXPORT Process::ProcessModel* Process::createProcess(
        const ProcessList& pl,
        Deserializer<DataStream>& deserializer,
        QObject* parent)
{
    ProcessFactoryKey processName;
    deserializer.m_stream >> processName;

    auto model = pl.list().get(processName)
                 ->loadModel(deserializer.toVariant(),
                             parent);
    // Calls the concrete process's factory
    // which in turn calls its deserialization ctor
    // which in turn calls writeTo(ProcessModel&)

    deserializer.checkDelimiter();
    return model;
}



template<>
void Visitor<Reader<JSONObject>>::readFrom(const Process::ProcessModel& process)
{
    // To allow recration using createProcess
    m_obj["ProcessName"] = toJsonValue(process.key());

    readFrom(static_cast<const IdentifiedObject<Process::ProcessModel>&>(process));

    m_obj["Duration"] = toJsonValue(process.duration());
    m_obj["Metadata"] = toJsonObject(process.metadata);

    // Save the subclass
    process.serialize(toVariant());
}


template<>
void Visitor<Writer<JSONObject>>::writeTo(Process::ProcessModel& process)
{
    process.m_duration = fromJsonValue<TimeValue>(m_obj["Duration"]);
    process.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"].toObject());
}

template<>
ISCORE_LIB_PROCESS_EXPORT Process::ProcessModel* Process::createProcess(
        const ProcessList& pl,
        Deserializer<JSONObject>& deserializer,
        QObject* parent)
{
    auto model = pl.list().get(
                     fromJsonValue<ProcessFactoryKey>(deserializer.m_obj["ProcessName"]))
                        ->loadModel(
                            deserializer.toVariant(),
                            parent);

    return model;
}











// MOVEME
#include <Process/StateProcess.hpp>
#include <Process/ProcessList.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const Process::StateProcess& process)
{
    // To allow recration using createProcess
    m_stream << process.key();

    readFrom(static_cast<const IdentifiedObject<Process::StateProcess>&>(process));

    // Save the subclass
    process.serialize(toVariant());

    insertDelimiter();
}

// We only load the members of the process here.

template<>
void Visitor<Writer<DataStream>>::writeTo(Process::StateProcess&)
{
    // Delimiter checked on createProcess
}

template<>
ISCORE_LIB_PROCESS_EXPORT Process::StateProcess* Process::createStateProcess(
        const Process::StateProcessList& pl,
        Deserializer<DataStream>& deserializer,
        QObject* parent)
{
    StateProcessFactoryKey processName;
    deserializer.m_stream >> processName;

    auto model = pl.list().get(processName)
                 ->load(deserializer.toVariant(),
                             parent);
    // Calls the concrete process's factory
    // which in turn calls its deserialization ctor
    // which in turn calls writeTo(ProcessModel&)

    deserializer.checkDelimiter();
    return model;
}



template<>
void Visitor<Reader<JSONObject>>::readFrom(const Process::StateProcess& process)
{
    // To allow recration using createProcess
    m_obj["StateProcessName"] = toJsonValue(process.key());

    readFrom(static_cast<const IdentifiedObject<Process::StateProcess>&>(process));

    // Save the subclass
    process.serialize(toVariant());
}


template<>
void Visitor<Writer<JSONObject>>::writeTo(Process::StateProcess& process)
{
}


template<>
ISCORE_LIB_PROCESS_EXPORT Process::StateProcess* Process::createStateProcess(
        const StateProcessList& pl,
        Deserializer<JSONObject>& deserializer,
        QObject* parent)
{
    auto model = pl.list().get(
                     fromJsonValue<StateProcessFactoryKey>(deserializer.m_obj["StateProcessName"]))
                        ->load(
                            deserializer.toVariant(),
                            parent);

    return model;
}

// TODO --IMPORTANT-- The createStuff methods ought to go in the ProcessList / StateProcessList.

