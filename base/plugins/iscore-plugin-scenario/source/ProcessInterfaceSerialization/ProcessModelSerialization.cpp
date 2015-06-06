#include "ProcessModelSerialization.hpp"
#include "ProcessInterface/ProcessFactory.hpp"

#include "ProcessInterface/ProcessList.hpp"
#include "ProcessInterface/ProcessModel.hpp"


template<>
void Visitor<Reader<DataStream>>::readFrom(const ProcessModel& process)
{
    // To allow recration using createProcess
    m_stream << process.processName();
    readFrom(process.duration());

    readFrom(static_cast<const IdentifiedObject<ProcessModel>&>(process));

    // ProcessModel doesn't have any particular data to save

    // Save the subclass
    process.serialize(toVariant());

    insertDelimiter();
}

template<>
ProcessModel* createProcess(Deserializer<DataStream>& deserializer,
        QObject* parent)
{
    QString processName;
    TimeValue duration;
    deserializer.m_stream >> processName;
    deserializer.writeTo(duration);

    auto model = ProcessList::getFactory(processName)
                 ->loadModel(deserializer.toVariant(),
                             parent);

    model->setDuration(duration);

    deserializer.checkDelimiter();
    return model;
}



template<>
void Visitor<Reader<JSONObject>>::readFrom(const ProcessModel& process)
{
    // To allow recration using createProcess
    m_obj["ProcessName"] = process.processName();
    m_obj["Duration"] = toJsonValue(process.duration());

    readFrom(static_cast<const IdentifiedObject<ProcessModel>&>(process));

    // ProcessModel doesn't have any particular data to save

    // Save the subclass
    process.serialize(toVariant());
}

template<>
ProcessModel* createProcess(Deserializer<JSONObject>& deserializer,
        QObject* parent)
{
    auto model = ProcessList::getFactory(
                     deserializer.m_obj["ProcessName"].toString())
                        ->loadModel(
                            deserializer.toVariant(),
                            parent);

    model->setDuration(fromJsonValue<TimeValue> (deserializer.m_obj["Duration"]));

    return model;
}

