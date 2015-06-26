#include "SwapSlots.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
using namespace Scenario::Command;


SwapSlots::SwapSlots(ObjectPath&& box,
                     id_type<SlotModel> first,
                     id_type<SlotModel> second):
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()},
    m_boxPath{std::move(box)},
    m_first{first},
    m_second{second}
{

}


void SwapSlots::undo()
{
    redo();
}


void SwapSlots::redo()
{
    auto& box = m_boxPath.find<BoxModel>();
    box.swapSlots(m_first, m_second);
}


void SwapSlots::serializeImpl(QDataStream& s) const
{
    s << m_boxPath << m_first << m_second;
}


void SwapSlots::deserializeImpl(QDataStream& s)
{
    s >> m_boxPath >> m_first >> m_second;
}
