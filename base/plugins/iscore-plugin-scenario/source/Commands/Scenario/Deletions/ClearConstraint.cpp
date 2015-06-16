#include "ClearConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "source/ProcessInterfaceSerialization/ProcessModelSerialization.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

ClearConstraint::ClearConstraint(ObjectPath&& constraintPath) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
m_path {std::move(constraintPath) }
{
    auto& constraint = m_path.find<ConstraintModel>();

    for(const BoxModel* box : constraint.boxes())
    {
        QByteArray arr;
        Serializer<DataStream> s {&arr};
        s.readFrom(*box);
        m_serializedBoxes.push_back(arr);
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
        m_boxMappings.insert(viewmodel->id(), viewmodel->shownBox());
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

    for(auto& serializedBox : m_serializedBoxes)
    {
        Deserializer<DataStream> s {serializedBox};
        constraint.addBox(new BoxModel {s, &constraint});
    }

    auto bit = constraint.viewModels().begin(), eit = constraint.viewModels().end();
    for(const auto& cvmid : m_boxMappings.keys())
    {
        auto it = std::find(bit, eit, cvmid);
        Q_ASSERT(it != eit);
        (*it)->showBox(m_boxMappings.value(cvmid));
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

    auto boxes = constraint.boxes();
    for(const auto& box : boxes)
    {
        constraint.removeBox(box->id());
    }
}

void ClearConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_serializedBoxes << m_serializedProcesses << m_boxMappings;
}

void ClearConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_serializedBoxes >> m_serializedProcesses >> m_boxMappings;
}
