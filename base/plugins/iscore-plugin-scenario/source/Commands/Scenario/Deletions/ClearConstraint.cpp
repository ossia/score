#include "ClearConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioLayer.hpp"
#include "source/ProcessInterfaceSerialization/ProcessModelSerialization.hpp"
#include "Document/Constraint/ViewModels/ConstraintViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

ClearConstraint::ClearConstraint(ObjectPath&& constraintPath) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
m_path {std::move(constraintPath) }
{
    auto& constraint = m_path.find<ConstraintModel>();

    for(const RackModel* rack : constraint.racks())
    {
        QByteArray arr;
        Serializer<DataStream> s {&arr};
        s.readFrom(*rack);
        m_serializedRackes.push_back(arr);
    }

    for(const ProcessModel* process : constraint.processes())
    {
        QByteArray arr;
        Serializer<DataStream> s {&arr};
        s.readFrom(*process);
        m_serializedProcesses.push_back(arr);
    }

    // TODO save view model data instead
    for(const auto& viewmodel : constraint.viewModels())
    {
        m_rackMappings.insert(viewmodel->id(), viewmodel->shownRack());
    }
}

void ClearConstraint::undo()
{
    auto& constraint = m_path.find<ConstraintModel>();

    for(auto& serializedProcess : m_serializedProcesses)
    {
        Deserializer<DataStream> s {serializedProcess};
        constraint.addProcess(createProcess(s, &constraint));
    }

    for(auto& serializedRack : m_serializedRackes)
    {
        Deserializer<DataStream> s {serializedRack};
        constraint.addRack(new RackModel {s, &constraint});
    }

    auto bit = constraint.viewModels().begin(), eit = constraint.viewModels().end();
    for(const auto& cvmid : m_rackMappings.keys())
    {
        auto it = std::find(bit, eit, cvmid);
        Q_ASSERT(it != eit);
        (*it)->showRack(m_rackMappings.value(cvmid));
    }
}

void ClearConstraint::redo()
{
    auto& constraint = m_path.find<ConstraintModel>();

    // We make copies since the iterators might change.
    auto processes = constraint.processes();
    for(const auto& process : processes)
    {
        constraint.removeProcess(process->id());
    }

    auto rackes = constraint.racks();
    for(const auto& rack : rackes)
    {
        constraint.removeRack(rack->id());
    }
}

void ClearConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_serializedRackes << m_serializedProcesses << m_rackMappings;
}

void ClearConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_serializedRackes >> m_serializedProcesses >> m_rackMappings;
}
