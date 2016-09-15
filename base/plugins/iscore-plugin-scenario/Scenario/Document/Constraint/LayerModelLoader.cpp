#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <QJsonObject>
#include <algorithm>

#include "LayerModelLoader.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

template <typename VisitorType> class Visitor;
namespace Process
{
template<>
LayerModel* createLayerModel(
        const Process::ProcessFactoryList& processes,
        Deserializer<DataStream>& deserializer,
        const Scenario::ConstraintModel& constraint,
        QObject* parent)
{
    Id<ProcessModel> sharedProcessId;
    deserializer.m_stream >> sharedProcessId;

    auto& process = constraint.processes.at(sharedProcessId);

    auto proc_fac = processes.get(process.concreteFactoryKey());
    ISCORE_ASSERT(proc_fac);
    auto viewmodel = proc_fac->loadLayer(
                         process,
                         deserializer.toVariant(),
                         parent);

    deserializer.checkDelimiter();

    return viewmodel;
}


template<>
LayerModel* createLayerModel(
        const Process::ProcessFactoryList& processes,
        Deserializer<JSONObject>& deserializer,
        const Scenario::ConstraintModel& constraint,
        QObject* parent)
{
    auto proc_id = fromJsonValue<Id<ProcessModel>>(deserializer.m_obj["SharedProcessId"]);

    auto process_it = constraint.processes.find(proc_id);
    if(process_it == constraint.processes.end())
        return nullptr;

    auto proc_fac = processes.get(process_it->concreteFactoryKey());
    ISCORE_ASSERT(proc_fac);
    auto viewmodel = proc_fac->loadLayer(
                         *process_it, deserializer.toVariant(), parent);
    return viewmodel;
}

}
