#include "AddLayerModelToSlot.hpp"

#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include "ProcessInterface/Process.hpp"
#include "ProcessInterface/LayerModel.hpp"
#include "ProcessInterfaceSerialization/LayerModelSerialization.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <ProcessInterface/ProcessFactory.hpp>
#include <ProcessInterface/ProcessList.hpp>
using namespace iscore;
using namespace Scenario::Command;


AddLayerModelToSlot::AddLayerModelToSlot(
        Path<SlotModel>&& slotPath,
        Path<Process>&& processPath) :
    SerializableCommand {factoryName(),
                         commandName(),
                         description()},
    m_slotPath {slotPath},
    m_processPath {processPath}
{
    auto slot = m_slotPath.try_find();
    if(slot)
        m_createdLayerId = iscore::id_generator::getStrongId(slot->layers);
    else
        m_createdLayerId = Id<LayerModel>{iscore::id_generator::getNextId()};

    m_processData = m_processPath.find().makeLayerConstructionData();
}


AddLayerModelToSlot::AddLayerModelToSlot(
        Path<SlotModel>&& slotPath,
        Path<Process>&& processPath,
        const QString& processName) :
    SerializableCommand {factoryName(),
                         commandName(),
                         description()},
    m_slotPath {slotPath},
    m_processPath {processPath}
{
    auto slot = m_slotPath.try_find();
    if(slot)
        m_createdLayerId = iscore::id_generator::getStrongId(slot->layers);
    else
        m_createdLayerId = Id<LayerModel>{iscore::id_generator::getNextId()};

    auto fact = ProcessList::getFactory(processName);
    ISCORE_ASSERT(fact);

    m_processData = fact->makeStaticLayerConstructionData();
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
