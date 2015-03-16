#include "AddProcessToConstraint.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

#include "ProcessInterface/ProcessSharedModelInterface.hpp"

#include "ProcessInterface/ProcessList.hpp"
#include "ProcessInterface/ProcessFactoryInterface.hpp"

using namespace iscore;
using namespace Scenario::Command;
AddProcessToConstraint::AddProcessToConstraint(ObjectPath&& constraintPath, QString process) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_path {std::move(constraintPath) },
m_processName {process}
{
    auto constraint = m_path.find<ConstraintModel>();
    m_createdProcessId = getStrongId(constraint->processes());
}

void AddProcessToConstraint::undo()
{
    auto constraint = m_path.find<ConstraintModel>();
    constraint->removeProcess(m_createdProcessId);
}

void AddProcessToConstraint::redo()
{
    auto constraint = m_path.find<ConstraintModel>();

    // Create process model
    auto proc = ProcessList::getFactory(m_processName)->makeModel(constraint->defaultDuration(),
                                                                  m_createdProcessId,
                                                                  constraint);

    constraint->addProcess(proc);
}

bool AddProcessToConstraint::mergeWith(const Command* other)
{
    return false;
}

void AddProcessToConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_processName << m_createdProcessId;
}

void AddProcessToConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_processName >> m_createdProcessId;
}
