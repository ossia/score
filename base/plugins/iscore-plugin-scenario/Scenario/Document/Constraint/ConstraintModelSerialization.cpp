#include <Process/Process.hpp>
#include <Process/ProcessModelSerialization.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>

#include <boost/optional/optional.hpp>
#include <core/application/ApplicationComponents.hpp>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <sys/types.h>
#include <algorithm>

#include <Process/ModelMetadata.hpp>
#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>
#include <core/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class StateModel;
template <typename T> class IdentifiedObject;
template <typename T> class Reader;
template <typename T> class Writer;

// Note : comment gérer le cas d'un process shared model qui ne sait se sérializer qu'en binaire, dans du json?
// Faire passer l'info en base64 ?
template<> void Visitor<Reader<DataStream>>::readFrom(const ConstraintModel& constraint)
{
    readFrom(static_cast<const IdentifiedObject<ConstraintModel>&>(constraint));

    // Metadata
    readFrom(constraint.metadata);

    // Processes
    m_stream << (int32_t) constraint.processes.size();
    for(const auto& process : constraint.processes)
    {
        readFrom(process);
    }

    // Rackes
    m_stream << (int32_t) constraint.racks.size();
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
             << constraint.m_heightPercentage
             << constraint.m_looping;

    readFrom(constraint.pluginModelList);

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(ConstraintModel& constraint)
{
    writeTo(constraint.metadata);

    // Processes
    int32_t process_count;
    m_stream >> process_count;

    auto& pl = context.components.factory<ProcessList>();
    for(; process_count -- > 0;)
    {
        constraint.processes.add(createProcess(pl, *this, &constraint));
    }

    // Rackes
    int32_t rack_count;
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
             >> constraint.m_heightPercentage
             >> constraint.m_looping;


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
    m_obj["Looping"] = constraint.m_looping;

    m_obj["PluginsMetadata"] = toJsonValue(constraint.pluginModelList);
}

template<> void Visitor<Writer<JSONObject>>::writeTo(ConstraintModel& constraint)
{
    constraint.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"].toObject());

    auto& pl = context.components.factory<ProcessList>();

    QJsonArray process_array = m_obj["Processes"].toArray();
    for(const auto& json_vref : process_array)
    {
        Deserializer<JSONObject> deserializer{json_vref.toObject()};
        constraint.processes.add(createProcess(pl, deserializer, &constraint));
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
    constraint.m_looping = m_obj["Looping"].toBool();



    Deserializer<JSONValue> elementPluginDeserializer(m_obj["PluginsMetadata"]);
    constraint.pluginModelList = iscore::ElementPluginModelList{elementPluginDeserializer, &constraint};
}
