#include "AddDeckToBox.hpp"

#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

AddDeckToBox::AddDeckToBox():
	SerializableCommand{"ScenarioControl",
						"AddDeckToBox",
						QObject::tr("Add empty deck")}
{
}

AddDeckToBox::AddDeckToBox(ObjectPath&& boxPath):
	SerializableCommand{"ScenarioControl",
						"AddDeckToBox",
						QObject::tr("Add empty deck")},
	m_path{boxPath}
{
	auto box = m_path.find<BoxModel>();
	m_createdDeckId = getStrongId(box->decks());
}

void AddDeckToBox::undo()
{
	auto box = m_path.find<BoxModel>();
	box->removeDeck(m_createdDeckId);
}

void AddDeckToBox::redo()
{
	auto box = m_path.find<BoxModel>();
	box->addDeck(new DeckModel{(int) box->decks().size(),
							   m_createdDeckId,
							   box});
}

int AddDeckToBox::id() const
{
	return 1;
}

bool AddDeckToBox::mergeWith(const QUndoCommand* other)
{
	return false;
}

void AddDeckToBox::serializeImpl(QDataStream& s)
{
	s << m_path << m_createdDeckId;
}

void AddDeckToBox::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_createdDeckId;
}
