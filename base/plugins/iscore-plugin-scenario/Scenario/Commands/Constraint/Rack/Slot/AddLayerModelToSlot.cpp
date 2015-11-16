#include "AddLayerModelToSlot.hpp"

#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Process/Process.hpp>
#include <Process/LayerModel.hpp>
#include <Scenario/Document/Constraint/LayerModelLoader.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
using namespace iscore;
using namespace Scenario::Command;


AddLayerModelToSlot::AddLayerModelToSlot(
        Path<SlotModel>&& slotPath,
        Path<Process>&& processPath) :
    m_slotPath {std::move(slotPath)},
    m_processPath {std::move(processPath)},
    m_processData{m_processPath.find().makeLayerConstructionData()},
    m_createdLayerId{getStrongId(m_slotPath.find().layers)}
{
}

AddLayerModelToSlot::AddLayerModelToSlot(
        Path<SlotModel>&& slotPath,
        Path<Process>&& processPath,
        const QByteArray& processData) :
    m_slotPath {std::move(slotPath)},
    m_processPath {std::move(processPath)},
    m_processData{processData},
    m_createdLayerId{getStrongId(m_slotPath.find().layers)}
{
    /*
    auto fact = list.get(processkey);
    ISCORE_ASSERT(fact);
    m_processData = fact->makeStaticLayerConstructionData();
    */
}

AddLayerModelToSlot::AddLayerModelToSlot(
        Path<SlotModel>&& slot,
        const Id<LayerModel>& layerid,
        Path<Process>&& process,
        const QByteArray& processData):
    m_slotPath{std::move(slot)},
    m_processPath{std::move(process)},
    m_processData{processData},
    m_createdLayerId{layerid}
{
    /*
    auto fact = SingletonProcessList::instance().get(processKey);
    ISCORE_ASSERT(fact);
    m_processData = fact->makeStaticLayerConstructionData();
    */
}

void AddLayerModelToSlot::undo() const
{
    auto& slot = m_slotPath.find();
    slot.layers.remove(m_createdLayerId);
}

void AddLayerModelToSlot::redo() const
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
