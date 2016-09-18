#include <Process/Process.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <Process/ProcessList.hpp>
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
        const SlotModel& slot,
        const Process::ProcessModel& process) :
    m_slotPath {slot},
    m_processPath {process},
    m_createdLayerId{getStrongId(m_slotPath.find().layers)}
{
    // Without further precision, we should take the first layer model that comes
    // up when looking for the matching process.
    auto& procs = this->context.components.factory<Process::LayerFactoryList>();
    auto fact = procs.findDefaultFactory(process);
    ISCORE_ASSERT(fact);
    m_layerFactory = fact->concreteFactoryKey();
    m_processData = fact->makeLayerConstructionData(process);
}

AddLayerModelToSlot::AddLayerModelToSlot(
        Path<SlotModel>&& slotPath,
        const Process::ProcessModel& process,
        QByteArray processData) :
    m_slotPath {std::move(slotPath)},
    m_processPath {process},
    m_processData{std::move(processData)},
    m_createdLayerId{getStrongId(m_slotPath.find().layers)}
{
    auto& procs = this->context.components.factory<Process::LayerFactoryList>();
    auto fact = procs.findDefaultFactory(process);
    ISCORE_ASSERT(fact);
    m_layerFactory = fact->concreteFactoryKey();
}

AddLayerModelToSlot::AddLayerModelToSlot(
        Path<SlotModel>&& slot,
        Id<Process::LayerModel> layerid,
        Path<Process::ProcessModel> process,
        UuidKey<Process::LayerFactory> uid,
        QByteArray processData):
    m_slotPath{std::move(slot)},
    m_processPath{std::move(process)},
    m_layerFactory{std::move(uid)},
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
    auto& process = m_processPath.find();

    auto& procs = this->context.components.factory<Process::LayerFactoryList>();

    auto fact = procs.get(m_layerFactory);
    slot.layers.add(
                fact->make(
                    process, m_createdLayerId, m_processData, &slot));
}

void AddLayerModelToSlot::serializeImpl(DataStreamInput& s) const
{
    s << m_slotPath << m_processPath << m_layerFactory << m_processData << m_createdLayerId;
}

void AddLayerModelToSlot::deserializeImpl(DataStreamOutput& s)
{
    s >> m_slotPath >> m_processPath >> m_layerFactory >> m_processData >> m_createdLayerId;
}
}
}
