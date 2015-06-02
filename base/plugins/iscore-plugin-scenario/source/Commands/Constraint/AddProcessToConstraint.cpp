#include "AddProcessToConstraint.hpp"
#include "AddProcessViewInNewDeck.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

#include "ProcessInterface/ProcessModel.hpp"

#include "ProcessInterface/ProcessList.hpp"
#include "ProcessInterface/ProcessFactory.hpp"

using namespace iscore;
using namespace Scenario::Command;
AddProcessToConstraint::AddProcessToConstraint(ObjectPath&& constraintPath, QString process) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_path {std::move(constraintPath) },
m_processName {process}
{
    auto& constraint = m_path.find<ConstraintModel>();
    m_createdProcessId = getStrongId(constraint.processes());
    m_noBoxes = (constraint.boxes().empty() && constraint.objectName() != "BaseConstraintModel" );
}

void AddProcessToConstraint::undo()
{
    auto& constraint = m_path.find<ConstraintModel>();
    constraint.removeProcess(m_createdProcessId);
    if(m_noBoxes)
    {
        m_cmd->undo();
        delete m_cmd;
    }
}

void AddProcessToConstraint::redo()
{
    auto& constraint = m_path.find<ConstraintModel>();

    // Create process model
    auto proc = ProcessList::getFactory(m_processName)
                 ->makeModel(
                     constraint.defaultDuration(),
                     m_createdProcessId,
                     &constraint);

    constraint.addProcess(proc);
    if(m_noBoxes)
    {
        m_cmd = new AddProcessViewInNewDeck{ObjectPath{m_path}, m_createdProcessId};
        m_cmd->redo();
    }
}

void AddProcessToConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_processName << m_createdProcessId;
}

void AddProcessToConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_processName >> m_createdProcessId;
}
