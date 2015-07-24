#include "AddLayerModelToSlot.hpp"

#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/LayerModel.hpp"
#include "ProcessInterfaceSerialization/LayerModelSerialization.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace iscore;
using namespace Scenario::Command;

AddLayerModelToSlot::AddLayerModelToSlot(
        ObjectPath&& slotPath,
        ObjectPath&& processPath) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_slotPath {slotPath},
    m_processPath {processPath}
{
    auto& slot = m_slotPath.find<SlotModel>();
    m_createdLayerId = getStrongId(slot.layerModels());
    m_processData = m_processPath.find<Process>().makeViewModelConstructionData();
}

void AddLayerModelToSlot::undo()
{
    auto& slot = m_slotPath.find<SlotModel>();
    slot.deleteLayerModel(m_createdLayerId);
}

void AddLayerModelToSlot::redo()
{
    auto& slot = m_slotPath.find<SlotModel>();
    auto& proc = m_processPath.find<Process>();

    slot.addLayerModel(proc.makeLayer(m_createdLayerId, m_processData, &slot));
}

void AddLayerModelToSlot::serializeImpl(QDataStream& s) const
{
    s << m_slotPath << m_processPath << m_processData << m_createdLayerId;
}

void AddLayerModelToSlot::deserializeImpl(QDataStream& s)
{
    s >> m_slotPath >> m_processPath >> m_processData >> m_createdLayerId;
}
