#include "AddProcessToConstraint.hpp"
#include "AddLayerInNewSlot.hpp"
#include "Box/Slot/AddLayerModelToSlot.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Slot/SlotModel.hpp"

#include "ProcessInterface/ProcessModel.hpp"

#include "ProcessInterface/ProcessList.hpp"
#include "ProcessInterface/ProcessFactory.hpp"

#include "iscore/document/DocumentInterface.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace iscore;
using namespace Scenario::Command;
AddProcessToConstraint::AddProcessToConstraint(ObjectPath&& constraintPath, QString process) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
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
    if(m_noBoxes)
    {
        m_cmdNewSlot->undo();
        delete m_cmdNewSlot;
    }
    else if (constraint.objectName() != "BaseConstraintModel")
    {
        m_cmdFirstSlot->undo();
        delete m_cmdFirstSlot;
    }
    constraint.removeProcess(m_createdProcessId);
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
        m_cmdNewSlot = new AddLayerInNewSlot{ObjectPath{m_path}, m_createdProcessId};
        m_cmdNewSlot->redo();
    }
    else if (constraint.objectName() != "BaseConstraintModel")
    {
        auto firstSlotModel = *(*constraint.boxes().begin())->getSlots().begin();
        m_cmdFirstSlot = new AddLayerModelToSlot(iscore::IDocument::path(*firstSlotModel),
                                                       iscore::IDocument::path(*proc) );

        m_cmdFirstSlot->redo();
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
