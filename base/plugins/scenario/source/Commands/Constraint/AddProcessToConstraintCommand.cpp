#include "AddProcessToConstraintCommand.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

#include "ProcessInterface/ProcessSharedModelInterface.hpp"

using namespace iscore;

// TODO switch between content models.
AddProcessToConstraintCommand::AddProcessToConstraintCommand(ObjectPath&& constraintPath, QString process):
	SerializableCommand{"ScenarioControl",
						"AddProcessToConstraintCommand",
						"Add process"},
	m_path(std::move(constraintPath)),
	m_processName{process}
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
	m_createdProcessId = getNextId(constraint->processes());
/*	m_contentModelId = constraint->boxes().front()->id(); // TODO pass as arg of the command.

	m_createdDeckId = getNextId(constraint->box(m_contentModelId)->decks());
	m_createdProcessViewModelId = getNextId(); // Deck does not exist yet so we can safely do this.
*/
}

void AddProcessToConstraintCommand::undo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());
/*	auto contentModel = constraint->box(m_contentModelId);

	auto deck = contentModel->deck(m_createdDeckId);
	deck->deleteProcessViewModel(m_createdProcessViewModelId);
	contentModel->deleteDeck(deck->id());
*/
	constraint->removeProcess(m_createdProcessId);
}

void AddProcessToConstraintCommand::redo()
{
	auto constraint = static_cast<ConstraintModel*>(m_path.find());

	// Create process model
	constraint->createProcess(m_processName, m_createdProcessId);
/*	auto contentModel = constraint->box(m_contentModelId);

	// Create deck
	contentModel->createDeck(m_createdDeckId);
	auto deck = contentModel->deck(m_createdDeckId);

	// Create process view model in the deck
	deck->createProcessViewModel(m_createdProcessId, m_createdProcessViewModelId);
*/
}

int AddProcessToConstraintCommand::id() const
{
	return 1;
}

bool AddProcessToConstraintCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

// TODO idea: maybe put the data in a tuple so that it can be serialized automagically ?
void AddProcessToConstraintCommand::serializeImpl(QDataStream& s)
{
	s << m_path << m_processName << m_createdProcessId;
}

void AddProcessToConstraintCommand::deserializeImpl(QDataStream& s)
{
	s >> m_path >> m_processName >> m_createdProcessId;
}
