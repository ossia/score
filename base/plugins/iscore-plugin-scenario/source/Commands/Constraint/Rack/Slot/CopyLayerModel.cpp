/*
#include "CopyLayerModel.hpp"

#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/LayerModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

CopyLayerModel::CopyLayerModel(ObjectPath&& lmPath,
        ObjectPath&& targetSlotPath) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_lmPath {lmPath},
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
    auto sourceLM = m_lmPath.find<LayerModel>();
    auto targetSlot = m_targetSlotPath.find<SlotModel>();

    const auto& proc = sourceLM->processModel();
    targetSlot->addLayerModel(
        proc.cloneViewModel(m_newLayerModelId,
                            *sourceLM,
                            targetSlot));
}

void CopyLayerModel::serializeImpl(QDataStream& s) const
{
    s << m_lmPath << m_targetSlotPath << m_newLayerModelId;
}

void CopyLayerModel::deserializeImpl(QDataStream& s)
{
    s >> m_lmPath >> m_targetSlotPath >> m_newLayerModelId;
}
*/
