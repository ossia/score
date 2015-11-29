#include <Process/Process.hpp>
#include <boost/core/explicit_operator_bool.hpp>
#include <qjsonobject.h>
#include <qjsonvalue.h>

#include "Process/ModelMetadata.hpp"
#include "Process/ProcessFactory.hpp"
#include "Process/ProcessFactoryKey.hpp"
#include "Process/ProcessList.hpp"
#include "Process/TimeValue.hpp"
#include "ProcessModelSerialization.hpp"
#include "iscore/plugins/customfactory/FactoryFamily.hpp"
#include "iscore/plugins/customfactory/FactoryMap.hpp"
#include "iscore/plugins/customfactory/StringFactoryKey.hpp"
#include "iscore/serialization/DataStreamVisitor.hpp"
#include "iscore/serialization/JSONValueVisitor.hpp"
#include "iscore/serialization/JSONVisitor.hpp"

class QObject;
template <typename model> class IdentifiedObject;


template<>
void Visitor<Reader<DataStream>>::readFrom(const Process& process)
{
    // To allow recration using createProcess
    m_stream << process.key();

    readFrom(static_cast<const IdentifiedObject<Process>&>(process));

    readFrom(process.duration());

    readFrom(process.metadata);

    // Save the subclass
    process.serialize(toVariant());

    insertDelimiter();
}

// We only load the members of the process here.

template<>
void Visitor<Writer<DataStream>>::writeTo(Process& process)
{
    writeTo(process.m_duration);
    writeTo(process.metadata);

    // Delimiter checked on createProcess
}

template<>
Process* createProcess(
        const DynamicProcessList& pl,
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
void Visitor<Reader<JSONObject>>::readFrom(const Process& process)
{
    // To allow recration using createProcess
    m_obj["ProcessName"] = toJsonValue(process.key());

    readFrom(static_cast<const IdentifiedObject<Process>&>(process));

    m_obj["Duration"] = toJsonValue(process.duration());
    m_obj["Metadata"] = toJsonObject(process.metadata);

    // Save the subclass
    process.serialize(toVariant());
}


template<>
void Visitor<Writer<JSONObject>>::writeTo(Process& process)
{
    process.m_duration = fromJsonValue<TimeValue>(m_obj["Duration"]);
    process.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"].toObject());
}

template<>
Process* createProcess(
        const DynamicProcessList& pl,
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

