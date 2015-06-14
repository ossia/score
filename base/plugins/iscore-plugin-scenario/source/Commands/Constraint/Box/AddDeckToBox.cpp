#include "AddDeckToBox.hpp"

#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>

using namespace iscore;
using namespace Scenario::Command;

AddDeckToBox::AddDeckToBox(ObjectPath&& boxPath) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_path {boxPath}
{
    auto& box = m_path.find<BoxModel>();
    m_createdDeckId = getStrongId(box.decks());
}

void AddDeckToBox::undo()
{
    auto& box = m_path.find<BoxModel>();
    box.removeDeck(m_createdDeckId);
}

void AddDeckToBox::redo()
{
    auto& box = m_path.find<BoxModel>();
    box.addDeck(new DeckModel {m_createdDeckId,
                               &box});
}

void AddDeckToBox::serializeImpl(QDataStream& s) const
{
    s << m_path << m_createdDeckId;
}

void AddDeckToBox::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_createdDeckId;
}
