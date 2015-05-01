#include "SwapDecks.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
using namespace Scenario::Command;


SwapDecks::SwapDecks(ObjectPath&& box,
                     id_type<DeckModel> first,
                     id_type<DeckModel> second):
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_boxPath{std::move(box)},
    m_first{first},
    m_second{second}
{

}


void SwapDecks::undo()
{
    redo();
}


void SwapDecks::redo()
{
    auto box = m_boxPath.find<BoxModel>();
    box->swapDecks(m_first, m_second);
}


void SwapDecks::serializeImpl(QDataStream& s) const
{
    s << m_boxPath << m_first << m_second;
}


void SwapDecks::deserializeImpl(QDataStream& s)
{
    s >> m_boxPath >> m_first >> m_second;
}
