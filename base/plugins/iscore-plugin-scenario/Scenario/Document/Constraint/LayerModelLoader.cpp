#include "LayerModelLoader.hpp"
#include <Process/Process.hpp>
#include <Process/LayerModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

template<>
LayerModel* createLayerModel(Deserializer<DataStream>& deserializer,
        const ConstraintModel& constraint,
        QObject* parent)
{
    Id<Process> sharedProcessId;
    deserializer.m_stream >> sharedProcessId;

    auto& process = constraint.processes.at(sharedProcessId);
    auto viewmodel = process.loadLayer(deserializer.toVariant(),
                                       parent);

    deserializer.checkDelimiter();

    return viewmodel;
}


template<>
LayerModel* createLayerModel(
        Deserializer<JSONObject>& deserializer,
        const ConstraintModel& constraint,
        QObject* parent)
{
    auto& process = constraint.processes.at(
                fromJsonValue<Id<Process>>(deserializer.m_obj["SharedProcessId"]));
    auto viewmodel = process.loadLayer(deserializer.toVariant(),
                                            parent);

    return viewmodel;
}
