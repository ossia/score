#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <algorithm>
#include <vector>

#include "AddLayerModelToSlot.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>

namespace Scenario
{
namespace Command
{
AddLayerModelToSlot::AddLayerModelToSlot(
        Path<SlotModel>&& slotPath,
        Path<Process::ProcessModel>&& processPath) :
    m_slotPath {std::move(slotPath)},
    m_processPath {std::move(processPath)},
    m_processData{m_processPath.find().makeLayerConstructionData()},
    m_createdLayerId{getStrongId(m_slotPath.find().layers)}
{
}

AddLayerModelToSlot::AddLayerModelToSlot(
        Path<SlotModel>&& slotPath,
        Path<Process::ProcessModel>&& processPath,
        QByteArray processData) :
    m_slotPath {std::move(slotPath)},
    m_processPath {std::move(processPath)},
    m_processData{std::move(processData)},
    m_createdLayerId{getStrongId(m_slotPath.find().layers)}
{
}

AddLayerModelToSlot::AddLayerModelToSlot(
        Path<SlotModel>&& slot,
        Id<Process::LayerModel> layerid,
        Path<Process::ProcessModel>&& process,
        QByteArray processData):
    m_slotPath{std::move(slot)},
    m_processPath{std::move(process)},
    m_processData{std::move(processData)},
    m_createdLayerId{std::move(layerid)}
{
}

void AddLayerModelToSlot::undo() const
{
    auto slot = m_slotPath.try_find();
    if(slot)
        slot->layers.remove(m_createdLayerId);
}

void AddLayerModelToSlot::redo() const
{
    auto& slot = m_slotPath.find();
    auto& proc = m_processPath.find();

    slot.layers.add(proc.makeLayer(m_createdLayerId, m_processData, &slot));
}

void AddLayerModelToSlot::serializeImpl(DataStreamInput& s) const
{
    s << m_slotPath << m_processPath << m_processData << m_createdLayerId;
}

void AddLayerModelToSlot::deserializeImpl(DataStreamOutput& s)
{
    s >> m_slotPath >> m_processPath >> m_processData >> m_createdLayerId;
}
}
}
