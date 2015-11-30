#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <core/application/ApplicationComponents.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <algorithm>
#include <vector>

#include "AddOnlyProcessToConstraint.hpp"
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <core/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>

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
        const Id<Process>& processId,
        const ProcessFactoryKey& process):
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
    auto proc = context.components.factory<ProcessList>().list().get(m_processName)
            ->makeModel(
                constraint.duration.defaultDuration(), // TODO should maybe be max ?
                m_createdProcessId,
                &constraint);

    constraint.processes.add(proc);
}

void AddOnlyProcessToConstraint::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_processName << m_createdProcessId;
}

void AddOnlyProcessToConstraint::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_processName >> m_createdProcessId;
}
