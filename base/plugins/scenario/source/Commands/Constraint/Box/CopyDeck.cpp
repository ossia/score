#include "CopyDeck.hpp"

#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

CopyDeck::CopyDeck(ObjectPath&& deckToCopy,
                   ObjectPath&& targetBoxPath) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_deckPath {deckToCopy},
    m_targetBoxPath {targetBoxPath}
{
    auto& box = m_targetBoxPath.find<BoxModel>();
    m_newDeckId = getStrongId(box.decks());
}

void CopyDeck::undo()
{
    auto& targetBox = m_targetBoxPath.find<BoxModel>();
    targetBox.removeDeck(m_newDeckId);
}


void CopyDeck::redo()
{
    auto& sourceDeck = m_deckPath.find<DeckModel>();
    auto& targetBox = m_targetBoxPath.find<BoxModel>();

    targetBox.addDeck(new DeckModel {&DeckModel::copyViewModelsInSameConstraint,
                                      sourceDeck,
                                      m_newDeckId,
                                      &targetBox});
}

void CopyDeck::serializeImpl(QDataStream& s) const
{
    s << m_deckPath << m_targetBoxPath << m_newDeckId;
}

void CopyDeck::deserializeImpl(QDataStream& s)
{
    s >> m_deckPath >> m_targetBoxPath >> m_newDeckId;
}
