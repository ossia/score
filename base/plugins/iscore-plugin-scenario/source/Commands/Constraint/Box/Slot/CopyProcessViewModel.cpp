/*
#include "CopyProcessViewModel.hpp"

#include "Document/Constraint/Box/Slot/SlotModel.hpp"
#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/ProcessViewModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

CopyProcessViewModel::CopyProcessViewModel(ObjectPath&& pvmPath,
        ObjectPath&& targetSlotPath) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_pvmPath {pvmPath},
    m_targetSlotPath {targetSlotPath}
{
    auto slot = m_targetSlotPath.find<SlotModel>();
    m_newProcessViewModelId = getStrongId(slot->processViewModels());
}

void CopyProcessViewModel::undo()
{
    auto slot = m_targetSlotPath.find<SlotModel>();
    slot->deleteProcessViewModel(m_newProcessViewModelId);
}

void CopyProcessViewModel::redo()
{
    auto sourcePVM = m_pvmPath.find<ProcessViewModel>();
    auto targetSlot = m_targetSlotPath.find<SlotModel>();

    const auto& proc = sourcePVM->sharedProcessModel();
    targetSlot->addProcessViewModel(
        proc.cloneViewModel(m_newProcessViewModelId,
                            *sourcePVM,
                            targetSlot));
}

void CopyProcessViewModel::serializeImpl(QDataStream& s) const
{
    s << m_pvmPath << m_targetSlotPath << m_newProcessViewModelId;
}

void CopyProcessViewModel::deserializeImpl(QDataStream& s)
{
    s >> m_pvmPath >> m_targetSlotPath >> m_newProcessViewModelId;
}
*/
