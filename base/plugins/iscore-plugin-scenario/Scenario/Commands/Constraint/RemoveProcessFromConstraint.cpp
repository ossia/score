#include <Process/Process.hpp>
#include <Process/ProcessModelSerialization.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/LayerModelLoader.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>



#include <QDataStream>
#include <QtGlobal>
#include <algorithm>
#include <vector>

#include <Process/ProcessList.hpp>
#include "RemoveProcessFromConstraint.hpp"
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/ObjectPath.hpp>

namespace Scenario
{
namespace Command
{

RemoveProcessFromConstraint::RemoveProcessFromConstraint(
        Path<ConstraintModel>&& constraintPath,
        const Id<Process::ProcessModel>& processId) :
    m_path {std::move(constraintPath) },
    m_processId {processId}
{
    auto& constraint = m_path.find();

    // Save the process
    Serializer<DataStream> s1{&m_serializedProcessData};
    auto& proc = constraint.processes.at(m_processId);
    s1.readFrom(proc);

    // Save ALL the view models!
    for(const auto& layer : proc.layers())
    {
        QByteArray vm_arr;
        Serializer<DataStream> s{&vm_arr};
        s.readFrom(*layer);

        m_serializedViewModels.append({*layer, vm_arr});
    }
}

void RemoveProcessFromConstraint::undo() const
{
    auto& constraint = m_path.find();
    Deserializer<DataStream> s {m_serializedProcessData};
    auto& fact = context.components.factory<Process::ProcessList>();
    AddProcess(constraint, createProcess(fact, s, &constraint));

    // Restore the view models
    for(const auto& it : m_serializedViewModels)
    {
        const auto& path = it.first.unsafePath().vec();

        auto& slot = constraint
                .racks.at(Id<RackModel>(path.at(path.size() - 3).id()))
                .slotmodels.at(Id<SlotModel>(path.at(path.size() - 2).id()));

        Deserializer<DataStream> stream {it.second};
        auto lm = Process::createLayerModel(stream,
                                   slot.parentConstraint(),
                                   &slot);
        slot.layers.add(lm);
    }
}

void RemoveProcessFromConstraint::redo() const
{
    auto& constraint = m_path.find();
    RemoveProcess(constraint, m_processId);

    // The view models will be deleted accordingly.
}

void RemoveProcessFromConstraint::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_processId << m_serializedProcessData << m_serializedViewModels;
}

void RemoveProcessFromConstraint::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_processId >> m_serializedProcessData >> m_serializedViewModels;
}
}
}
