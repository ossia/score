#include "AddDeckToBox.hpp"

#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Storey/StoreyModel.hpp"

using namespace iscore;
using namespace Scenario::Command;

AddDeckToBox::AddDeckToBox(ObjectPath&& boxPath):
	SerializableCommand{"ScenarioControl",
						"AddDeckToBox",
						"Add empty deck"},
	m_path{boxPath}
{
	auto box = static_cast<BoxModel*>(m_path.find());
	m_createdDeckId = getNextId(box->storeys());
}

void AddDeckToBox::undo()
{
	auto box = static_cast<BoxModel*>(m_path.find());
	box->removeDeck(m_createdDeckId);
}

void AddDeckToBox::redo()
{
	auto box = static_cast<BoxModel*>(m_path.find());
	box->createDeck(m_createdDeckId);
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
