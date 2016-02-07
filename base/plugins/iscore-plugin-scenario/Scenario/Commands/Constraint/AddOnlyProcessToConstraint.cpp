#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>

#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <algorithm>
#include <vector>

#include "AddOnlyProcessToConstraint.hpp"
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>


namespace Scenario
{
namespace Command
{
AddOnlyProcessToConstraint::AddOnlyProcessToConstraint(
        Path<ConstraintModel>&& constraintPath,
        const ProcessFactoryKey& process) :
    AddOnlyProcessToConstraint{
        std::move(constraintPath),
        getStrongId(constraintPath.find().processes),
        process}
{

}

AddOnlyProcessToConstraint::AddOnlyProcessToConstraint(
        Path<ConstraintModel>&& constraintPath,
        const Id<Process::ProcessModel>& processId,
        const ProcessFactoryKey& process):
    m_path{std::move(constraintPath)},
    m_processName{process},
    m_createdProcessId{processId}
{
}

void AddOnlyProcessToConstraint::undo() const
{
    undo(m_path.find());
}

void AddOnlyProcessToConstraint::redo() const
{
    redo(m_path.find());
}

void AddOnlyProcessToConstraint::undo(ConstraintModel& constraint) const
{
    RemoveProcess(constraint, m_createdProcessId);
}

Process::ProcessModel& AddOnlyProcessToConstraint::redo(ConstraintModel& constraint) const
{
    // Create process model
    auto proc = context.components.factory<Process::ProcessList>().get(m_processName)
            ->makeModel(
                constraint.duration.defaultDuration(), // TODO should maybe be max ?
                m_createdProcessId,
                &constraint);

    AddProcess(constraint, proc);
    return *proc;
}

void AddOnlyProcessToConstraint::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_processName << m_createdProcessId;
}

void AddOnlyProcessToConstraint::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_processName >> m_createdProcessId;
}
}
}
