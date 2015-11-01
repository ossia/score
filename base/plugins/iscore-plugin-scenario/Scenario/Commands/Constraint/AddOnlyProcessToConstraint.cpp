#include "AddOnlyProcessToConstraint.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Process/ProcessList.hpp>
#include <Process/ProcessFactory.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>

AddOnlyProcessToConstraint::AddOnlyProcessToConstraint(
        Path<ConstraintModel>&& constraintPath,
        const QString& process) :
    AddOnlyProcessToConstraint{
        std::move(constraintPath),
        getStrongId(constraintPath.find().processes),
        process}
{

}

AddOnlyProcessToConstraint::AddOnlyProcessToConstraint(
        Path<ConstraintModel>&& constraintPath,
        const Id<Process>& processId,
        const QString& process):
    SerializableCommand {factoryName(),
                         commandName(),
                         description()},
    m_path{std::move(constraintPath)},
    m_processName{process},
    m_createdProcessId{processId}
{
}

void AddOnlyProcessToConstraint::undo() const
{
    auto& constraint = m_path.find();
    constraint.processes.remove(m_createdProcessId);
}

void AddOnlyProcessToConstraint::redo() const
{
    auto& constraint = m_path.find();

    // Create process model
    auto proc = ProcessList::getFactory(m_processName)
            ->makeModel(
                constraint.duration.defaultDuration(), // TODO should maybe be max ?
                m_createdProcessId,
                &constraint);

    constraint.processes.add(proc);
}

void AddOnlyProcessToConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_processName << m_createdProcessId;
}

void AddOnlyProcessToConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_processName >> m_createdProcessId;
}
