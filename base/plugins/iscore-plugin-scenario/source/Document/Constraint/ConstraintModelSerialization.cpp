#include "Document/Constraint/ConstraintModel.hpp"
#include "ProcessInterface/ProcessModel.hpp"
#include "source/ProcessInterfaceSerialization/ProcessModelSerialization.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelSerialization.hpp>

// Note : comment gérer le cas d'un process shared model qui ne sait se sérializer qu'en binaire, dans du json?
// Faire passer l'info en base64 ?

template<> void Visitor<Reader<DataStream>>::readFrom(const ConstraintModel& constraint)
{
    readFrom(static_cast<const IdentifiedObject<ConstraintModel>&>(constraint));

    // Metadata
    readFrom(constraint.metadata);
    m_stream << constraint.heightPercentage();

    // Processes
    const auto& processes = constraint.processes();
    m_stream << (int) processes.size();

    for(const auto& process : processes)
    {
        readFrom(*process);
    }

    // Rackes
    const auto& rackes = constraint.racks();
    m_stream << (int) rackes.size();

    for(const auto& rack : rackes)
    {
        readFrom(*rack);
    }

    // Full view
    readFrom(*constraint.fullView());

    // Events
    m_stream << constraint.startEvent();
    m_stream << constraint.endEvent();

    m_stream << constraint.defaultDuration()
             << constraint.startDate()
             << constraint.minDuration()
             << constraint.maxDuration();

    m_stream << constraint.isRigid();

    readFrom(*constraint.m_pluginModelList);

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(ConstraintModel& constraint)
{
    double heightPercentage;
    writeTo(constraint.metadata);
    m_stream >> heightPercentage;

    constraint.setHeightPercentage(heightPercentage);

    // Processes
    int process_count;
    m_stream >> process_count;

    for(; process_count -- > 0;)
    {
        constraint.addProcess(createProcess(*this, &constraint));
    }

    // Rackes
    int rack_count;
    m_stream >> rack_count;

    for(; rack_count -- > 0;)
    {
        constraint.addRack(new RackModel(*this, &constraint));
    }

    // Full view
    id_type<ConstraintModel> savedConstraintId;
    m_stream >> savedConstraintId; // Necessary because it is saved; however it is not required here.
    //(todo how to fix this ?)
    constraint.setFullView(new FullViewConstraintViewModel {*this, constraint, &constraint});

    // Events
    id_type<EventModel> startId {}, endId {};
    m_stream >> startId;
    m_stream >> endId;
    constraint.setStartEvent(startId);
    constraint.setEndEvent(endId);

    TimeValue width {}, startDate {}, minDur {}, maxDur {};
    m_stream >> width
             >> startDate
             >> minDur
             >> maxDur;

    bool rigidity;
    m_stream >> rigidity;

    constraint.setDefaultDuration(width);
    constraint.setStartDate(startDate);
    constraint.setMinDuration(minDur);
    constraint.setMaxDuration(maxDur);

    constraint.setRigid(rigidity);

    constraint.m_pluginModelList = new iscore::ElementPluginModelList{*this, &constraint};

    checkDelimiter();
}





template<> void Visitor<Reader<JSONObject>>::readFrom(const ConstraintModel& constraint)
{
    readFrom(static_cast<const IdentifiedObject<ConstraintModel>&>(constraint));
    m_obj["Metadata"] = toJsonObject(constraint.metadata);
    m_obj["HeightPercentage"] = constraint.heightPercentage();
    m_obj["StartEvent"] = toJsonValue(constraint.startEvent());
    m_obj["EndEvent"] = toJsonValue(constraint.endEvent());

    // Processes
    m_obj["Processes"] = toJsonArray(constraint.processes());

    // Rackes
    m_obj["Rackes"] = toJsonArray(constraint.racks());


    m_obj["FullView"] = toJsonObject(*constraint.fullView());

    m_obj["DefaultDuration"] = toJsonValue(constraint.defaultDuration());
    m_obj["StartDate"] = toJsonValue(constraint.startDate());
    m_obj["MinDuration"] = toJsonValue(constraint.minDuration());
    m_obj["MaxDuration"] = toJsonValue(constraint.maxDuration());

    m_obj["Rigidity"] = constraint.isRigid();

    m_obj["PluginsMetadata"] = toJsonValue(*constraint.m_pluginModelList);
}

template<> void Visitor<Writer<JSONObject>>::writeTo(ConstraintModel& constraint)
{
    constraint.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"].toObject());
    constraint.setHeightPercentage(m_obj["HeightPercentage"].toDouble());
    constraint.setStartEvent(fromJsonValue<id_type<EventModel>> (m_obj["StartEvent"]));
    constraint.setEndEvent(fromJsonValue<id_type<EventModel>> (m_obj["EndEvent"]));

    QJsonArray process_array = m_obj["Processes"].toArray();

    for(const auto& json_vref : process_array)
    {
        Deserializer<JSONObject> deserializer {json_vref.toObject() };
        constraint.addProcess(createProcess(deserializer, &constraint));
    }

    QJsonArray rack_array = m_obj["Rackes"].toArray();

    for(const auto& json_vref : rack_array)
    {
        Deserializer<JSONObject> deserializer {json_vref.toObject() };
        constraint.addRack(new RackModel(deserializer, &constraint));
    }

    constraint.setFullView(new FullViewConstraintViewModel {
                               Deserializer<JSONObject>{m_obj["FullView"].toObject() },
                               constraint,
                               &constraint});

    constraint.setDefaultDuration(fromJsonValue<TimeValue> (m_obj["DefaultDuration"]));
    constraint.setStartDate(fromJsonValue<TimeValue> (m_obj["StartDate"]));
    constraint.setMinDuration(fromJsonValue<TimeValue> (m_obj["MinDuration"]));
    constraint.setMaxDuration(fromJsonValue<TimeValue> (m_obj["MaxDuration"]));

    constraint.setRigid(m_obj["Rigidity"].toBool());


    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    constraint.m_pluginModelList = new iscore::ElementPluginModelList{elementPluginDeserializer, &constraint};
}
