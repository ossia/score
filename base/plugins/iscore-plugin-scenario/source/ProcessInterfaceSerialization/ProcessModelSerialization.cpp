#include "ProcessModelSerialization.hpp"
#include "ProcessInterface/ProcessFactory.hpp"

#include "ProcessInterface/ProcessList.hpp"
#include "ProcessInterface/ProcessModel.hpp"


template<>
void Visitor<Reader<DataStream>>::readFrom(const Process& process)
{
    // To allow recration using createProcess
    m_stream << process.processName();

    readFrom(static_cast<const IdentifiedObject<Process>&>(process));

    readFrom(process.duration());

    // Save the subclass
    process.serialize(toVariant());

    insertDelimiter();
}

// We only load the members of the process here.

template<>
void Visitor<Writer<DataStream>>::writeTo(Process& process)
{
    writeTo(process.m_duration);

    // Delimiter checked on createProcess
}

template<>
Process* createProcess(Deserializer<DataStream>& deserializer,
        QObject* parent)
{
    QString processName;
    deserializer.m_stream >> processName;

    auto model = ProcessList::getFactory(processName)
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
    m_obj["ProcessName"] = process.processName();

    readFrom(static_cast<const IdentifiedObject<Process>&>(process));

    m_obj["Duration"] = toJsonValue(process.duration());

    // Save the subclass
    process.serialize(toVariant());
}


template<>
void Visitor<Writer<JSONObject>>::writeTo(Process& process)
{
    process.m_duration = fromJsonValue<TimeValue>(m_obj["Duration"]);
}

template<>
Process* createProcess(Deserializer<JSONObject>& deserializer,
        QObject* parent)
{
    auto model = ProcessList::getFactory(
                     deserializer.m_obj["ProcessName"].toString())
                        ->loadModel(
                            deserializer.toVariant(),
                            parent);

    return model;
}

