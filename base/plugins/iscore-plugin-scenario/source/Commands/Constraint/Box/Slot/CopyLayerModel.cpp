/*
#include "CopyLayerModel.hpp"

#include "Document/Constraint/Box/Slot/SlotModel.hpp"
#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/LayerModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

CopyLayerModel::CopyLayerModel(ObjectPath&& pvmPath,
        ObjectPath&& targetSlotPath) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_pvmPath {pvmPath},
    m_targetSlotPath {targetSlotPath}
{
    auto slot = m_targetSlotPath.find<SlotModel>();
    m_newLayerModelId = getStrongId(slot->layerModels());
}

void CopyLayerModel::undo()
{
    auto slot = m_targetSlotPath.find<SlotModel>();
    slot->deleteLayerModel(m_newLayerModelId);
}

void CopyLayerModel::redo()
{
    auto sourcePVM = m_pvmPath.find<LayerModel>();
    auto targetSlot = m_targetSlotPath.find<SlotModel>();

    const auto& proc = sourcePVM->sharedProcessModel();
    targetSlot->addLayerModel(
        proc.cloneViewModel(m_newLayerModelId,
                            *sourcePVM,
                            targetSlot));
}

void CopyLayerModel::serializeImpl(QDataStream& s) const
{
    s << m_pvmPath << m_targetSlotPath << m_newLayerModelId;
}

void CopyLayerModel::deserializeImpl(QDataStream& s)
{
    s >> m_pvmPath >> m_targetSlotPath >> m_newLayerModelId;
}
*/
