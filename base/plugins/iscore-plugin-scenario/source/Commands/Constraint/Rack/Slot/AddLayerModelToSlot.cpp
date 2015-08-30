#include "AddLayerModelToSlot.hpp"

#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "ProcessInterface/Process.hpp"
#include "ProcessInterface/LayerModel.hpp"
#include "ProcessInterfaceSerialization/LayerModelSerialization.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace iscore;
using namespace Scenario::Command;

AddLayerModelToSlot::AddLayerModelToSlot(
        Path<SlotModel>&& slotPath,
        Path<Process>&& processPath) :
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_slotPath {slotPath},
    m_processPath {processPath}
{
    auto& slot = m_slotPath.find();
    m_createdLayerId = getStrongId(slot.layers);
    m_processData = m_processPath.find().makeViewModelConstructionData();
}

void AddLayerModelToSlot::undo()
{
    auto& slot = m_slotPath.find();
    slot.layers.remove(m_createdLayerId);
}

void AddLayerModelToSlot::redo()
{
    auto& slot = m_slotPath.find();
    auto& proc = m_processPath.find();

    slot.layers.add(proc.makeLayer(m_createdLayerId, m_processData, &slot));
}

void AddLayerModelToSlot::serializeImpl(QDataStream& s) const
{
    s << m_slotPath << m_processPath << m_processData << m_createdLayerId;
}

void AddLayerModelToSlot::deserializeImpl(QDataStream& s)
{
    s >> m_slotPath >> m_processPath >> m_processData >> m_createdLayerId;
}
