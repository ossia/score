#include "Document/Constraint/ConstraintModel.hpp"
#include "ProcessInterface/Process.hpp"
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

    // Processes
    m_stream << (int) constraint.processes.size();
    for(const auto& process : constraint.processes)
    {
        readFrom(process);
    }

    // Rackes
    m_stream << (int) constraint.racks.size();
    for(const auto& rack : constraint.racks)
    {
        readFrom(rack);
    }

    // Full view
    readFrom(*constraint.m_fullViewModel);

    // Common data
    m_stream << constraint.duration
             << constraint.m_startState
             << constraint.m_endState

             << constraint.m_startDate
             << constraint.m_heightPercentage;

    readFrom(constraint.pluginModelList);

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(ConstraintModel& constraint)
{
    writeTo(constraint.metadata);

    // Processes
    int process_count;
    m_stream >> process_count;

    for(; process_count -- > 0;)
    {
        constraint.processes.add(createProcess(*this, &constraint));
    }

    // Rackes
    int rack_count;
    m_stream >> rack_count;

    for(; rack_count -- > 0;)
    {
        constraint.racks.add(new RackModel(*this, &constraint));
    }

    // Full view
    Id<ConstraintModel> savedConstraintId;
    m_stream >> savedConstraintId; // Necessary because it is saved; however it is not required here.
    //(todo how to fix this ?)
    constraint.setFullView(new FullViewConstraintViewModel {*this, constraint, &constraint});

    // Common data
    m_stream >> constraint.duration
             >> constraint.m_startState
             >> constraint.m_endState

             >> constraint.m_startDate
             >> constraint.m_heightPercentage;


    constraint.pluginModelList = iscore::ElementPluginModelList{*this, &constraint};

    checkDelimiter();
}



template<> void Visitor<Reader<JSONObject>>::readFrom(const ConstraintModel& constraint)
{
    readFrom(static_cast<const IdentifiedObject<ConstraintModel>&>(constraint));
    m_obj["Metadata"] = toJsonObject(constraint.metadata);

    // Processes
    m_obj["Processes"] = toJsonArray(constraint.processes);

    // Rackes
    m_obj["Rackes"] = toJsonArray(constraint.racks);

    // Full view
    m_obj["FullView"] = toJsonObject(*constraint.fullView());


    // Common data
    // The fields will go in the same level as the
    // rest of the constraint
    readFrom(constraint.duration);

    m_obj["StartState"] = toJsonValue(constraint.m_startState);
    m_obj["EndState"] = toJsonValue(constraint.m_endState);

    m_obj["StartDate"] = toJsonValue(constraint.m_startDate);
    m_obj["HeightPercentage"] = constraint.m_heightPercentage;

    m_obj["PluginsMetadata"] = toJsonValue(constraint.pluginModelList);
}

template<> void Visitor<Writer<JSONObject>>::writeTo(ConstraintModel& constraint)
{
    constraint.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"].toObject());

    QJsonArray process_array = m_obj["Processes"].toArray();
    for(const auto& json_vref : process_array)
    {
        Deserializer<JSONObject> deserializer{json_vref.toObject()};
        constraint.processes.add(createProcess(deserializer, &constraint));
    }

    QJsonArray rack_array = m_obj["Rackes"].toArray();
    for(const auto& json_vref : rack_array)
    {
        Deserializer<JSONObject> deserializer {json_vref.toObject() };
        constraint.racks.add(new RackModel(deserializer, &constraint));
    }

    constraint.setFullView(new FullViewConstraintViewModel {
                               Deserializer<JSONObject>{m_obj["FullView"].toObject() },
                               constraint,
                               &constraint});

    writeTo(constraint.duration);
    constraint.m_startState = fromJsonValue<Id<StateModel>> (m_obj["StartState"]);
    constraint.m_endState = fromJsonValue<Id<StateModel>> (m_obj["EndState"]);

    constraint.m_startDate = fromJsonValue<TimeValue> (m_obj["StartDate"]);
    constraint.m_heightPercentage = m_obj["HeightPercentage"].toDouble();



    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    constraint.pluginModelList = iscore::ElementPluginModelList{elementPluginDeserializer, &constraint};
}
