#include "SwapSlots.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
using namespace Scenario::Command;


SwapSlots::SwapSlots(
        Path<RackModel>&& rack,
        const Id<SlotModel>& first,
        const Id<SlotModel>& second):
    SerializableCommand {factoryName(),
                         commandName(),
                         description()},
    m_rackPath{std::move(rack)},
    m_first{first},
    m_second{second}
{

}


void SwapSlots::undo() const
{
    redo();
}


void SwapSlots::redo() const
{
    auto& rack = m_rackPath.find();
    rack.swapSlots(m_first, m_second);
}


void SwapSlots::serializeImpl(QDataStream& s) const
{
    s << m_rackPath << m_first << m_second;
}


void SwapSlots::deserializeImpl(QDataStream& s)
{
    s >> m_rackPath >> m_first >> m_second;
}
