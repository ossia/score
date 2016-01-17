#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <vector>

#include "AddSlotToRack.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>

namespace Scenario
{
namespace Command
{

AddSlotToRack::AddSlotToRack(Path<RackModel>&& rackPath) :
    m_path {rackPath}
{
    auto rack = m_path.try_find(); // Because we use this in a macro, the rack may not be there yet

    if(rack)
        m_createdSlotId = getStrongId(rack->slotmodels);
    else
        m_createdSlotId = Id<SlotModel>{iscore::id_generator::getFirstId()};
}

void AddSlotToRack::undo() const
{
    auto& rack = m_path.find();
    rack.slotmodels.remove(m_createdSlotId);
}

void AddSlotToRack::redo() const
{
    auto& rack = m_path.find();
    rack.addSlot(new SlotModel {m_createdSlotId,
                               &rack});
}

void AddSlotToRack::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_createdSlotId;
}

void AddSlotToRack::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_createdSlotId;
}
}
}
