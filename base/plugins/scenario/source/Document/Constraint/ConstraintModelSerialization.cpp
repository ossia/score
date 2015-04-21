#include "Document/Constraint/ConstraintModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "source/ProcessInterfaceSerialization/ProcessSharedModelInterfaceSerialization.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelSerialization.hpp>

// Note : comment gérer le cas d'un process shared model qui ne sait se sérializer qu'en binaire, dans du json?
// Faire passer l'info en base64 ?

template<> void Visitor<Reader<DataStream>>::readFrom(const ConstraintModel& constraint)
{
    readFrom(static_cast<const IdentifiedObject<ConstraintModel>&>(constraint));

    // Metadata
    m_stream	<< constraint.metadata
                << constraint.heightPercentage();

    // Processes
    auto processes = constraint.processes();
    m_stream	<< (int) processes.size();

    for(auto process : processes)
    {
        readFrom(*process);
    }

    // Boxes
    auto boxes = constraint.boxes();
    m_stream	<< (int) boxes.size();

    for(auto box : boxes)
    {
        readFrom(*box);
    }

    readFrom(*constraint.fullView());

    // Events
    m_stream << constraint.startEvent();
    m_stream << constraint.endEvent();

    m_stream << constraint.defaultDuration()
             << constraint.startDate()
             << constraint.minDuration()
             << constraint.maxDuration();

    m_stream << constraint.isRigid();

    m_stream << constraint.pluginMetadatas().size();
    for(auto& plug : constraint.pluginMetadatas())
    {
        readFrom(*plug);
    }

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(ConstraintModel& constraint)
{
    double heightPercentage;
    m_stream >> constraint.metadata >> heightPercentage;

    constraint.setHeightPercentage(heightPercentage);

    // Processes
    int process_count;
    m_stream >> process_count;

    for(; process_count -- > 0;)
    {
        constraint.addProcess(createProcess(*this, &constraint));
    }

    // Boxes
    int box_count;
    m_stream >> box_count;

    for(; box_count -- > 0;)
    {
        constraint.addBox(new BoxModel(*this, &constraint));
    }

    id_type<ConstraintModel> savedConstraintId;
    m_stream >> savedConstraintId; // Necessary because it is saved; however it is not required here. (todo how to fix this ?)
    constraint.setFullView(new FullViewConstraintViewModel {*this, &constraint, &constraint});

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

    // TODO put this behaviour in an encapsulated sub-class instead.
    int plugin_count;
    m_stream >> plugin_count;

    for(; plugin_count -- > 0;)
    {
        constraint.addPluginMetadata(
                    deserializeElementPluginModel(*this,
                                                  ConstraintModel::staticMetaObject.className(),
                                                  &constraint));
    }


    checkDelimiter();
}





template<> void Visitor<Reader<JSON>>::readFrom(const ConstraintModel& constraint)
{
    readFrom(static_cast<const IdentifiedObject<ConstraintModel>&>(constraint));
    m_obj["Metadata"] = toJsonObject(constraint.metadata);
    m_obj["HeightPercentage"] = constraint.heightPercentage();
    m_obj["StartEvent"] = toJsonObject(constraint.startEvent());
    m_obj["EndEvent"] = toJsonObject(constraint.endEvent());

    // Processes
    m_obj["Processes"] = toJsonArray(constraint.processes());

    // Boxes
    m_obj["Boxes"] = toJsonArray(constraint.boxes());


    m_obj["FullView"] = toJsonObject(*constraint.fullView());

    m_obj["DefaultDuration"] = toJsonObject(constraint.defaultDuration());
    m_obj["StartDate"] = toJsonObject(constraint.startDate());
    m_obj["MinDuration"] = toJsonObject(constraint.minDuration());
    m_obj["MaxDuration"] = toJsonObject(constraint.maxDuration());

    m_obj["Rigidity"] = constraint.isRigid();

    m_obj["PluginsMetadata"] = toJsonArray(constraint.pluginMetadatas());
}

template<> void Visitor<Writer<JSON>>::writeTo(ConstraintModel& constraint)
{
    constraint.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"].toObject());
    constraint.setHeightPercentage(m_obj["HeightPercentage"].toDouble());
    constraint.setStartEvent(fromJsonObject<id_type<EventModel>> (m_obj["StartEvent"].toObject()));
    constraint.setEndEvent(fromJsonObject<id_type<EventModel>> (m_obj["EndEvent"].toObject()));

    QJsonArray process_array = m_obj["Processes"].toArray();

    for(const auto& json_vref : process_array)
    {
        Deserializer<JSON> deserializer {json_vref.toObject() };
        constraint.addProcess(createProcess(deserializer, &constraint));
    }

    QJsonArray box_array = m_obj["Boxes"].toArray();

    for(const auto& json_vref : box_array)
    {
        Deserializer<JSON> deserializer {json_vref.toObject() };
        constraint.addBox(new BoxModel(deserializer, &constraint));
    }

    constraint.setFullView(new FullViewConstraintViewModel {
                               Deserializer<JSON>{m_obj["FullView"].toObject() },
                               &constraint,
                               &constraint});

    constraint.setDefaultDuration(fromJsonObject<TimeValue> (m_obj["DefaultDuration"].toObject()));
    constraint.setStartDate(fromJsonObject<TimeValue> (m_obj["StartDate"].toObject()));
    constraint.setMinDuration(fromJsonObject<TimeValue> (m_obj["MinDuration"].toObject()));
    constraint.setMaxDuration(fromJsonObject<TimeValue> (m_obj["MaxDuration"].toObject()));

    constraint.setRigid(m_obj["Rigidity"].toBool());


    QJsonArray plugin_array = m_obj["PluginsMetadata"].toArray();

    for(const auto& json_vref : plugin_array)
    {
        Deserializer<JSON> deserializer{json_vref.toObject()};
        qDebug() << Q_FUNC_INFO << "TODO";
        constraint.addPluginMetadata(
                    deserializeElementPluginModel(*this,
                                                  ConstraintModel::staticMetaObject.className(),
                                                  &constraint));
    }
}
