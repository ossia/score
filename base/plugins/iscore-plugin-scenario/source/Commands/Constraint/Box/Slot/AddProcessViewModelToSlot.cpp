#include "AddProcessViewModelToSlot.hpp"

#include "Document/Constraint/Box/Slot/SlotModel.hpp"
#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/ProcessViewModel.hpp"
#include "ProcessInterfaceSerialization/ProcessViewModelSerialization.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace iscore;
using namespace Scenario::Command;

AddProcessViewModelToSlot::AddProcessViewModelToSlot(
        ObjectPath&& slotPath,
        ObjectPath&& processPath) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_slotPath {slotPath},
    m_processPath {processPath}
{
    auto& slot = m_slotPath.find<SlotModel>();
    m_createdProcessViewId = getStrongId(slot.processViewModels());
    m_processData = m_processPath.find<ProcessModel>().makeViewModelConstructionData();
}

void AddProcessViewModelToSlot::undo()
{
    auto& slot = m_slotPath.find<SlotModel>();
    slot.deleteProcessViewModel(m_createdProcessViewId);
}

void AddProcessViewModelToSlot::redo()
{
    auto& slot = m_slotPath.find<SlotModel>();
    auto& proc = m_processPath.find<ProcessModel>();

    slot.addProcessViewModel(proc.makeViewModel(m_createdProcessViewId, m_processData, &slot));
}

void AddProcessViewModelToSlot::serializeImpl(QDataStream& s) const
{
    s << m_slotPath << m_processPath << m_processData << m_createdProcessViewId;
}

void AddProcessViewModelToSlot::deserializeImpl(QDataStream& s)
{
    s >> m_slotPath >> m_processPath >> m_processData >> m_createdProcessViewId;
}
