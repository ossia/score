#include "ScenarioProcessViewModel.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"

ScenarioProcessViewModel::ScenarioProcessViewModel(int viewModelId, ScenarioProcessSharedModel* model, QObject* parent):
	ProcessViewModelInterface(parent, "ScenarioProcessViewModel", viewModelId, model)
{
}

ScenarioProcessViewModel::ScenarioProcessViewModel(QDataStream& s, ScenarioProcessSharedModel* model, QObject* parent):
	ProcessViewModelInterface(nullptr, "ScenarioProcessViewModel", -1, model) // TODO pourquoi ne pas utiliser QDataStream dans le ctor parent ?
{
	s >> static_cast<ProcessViewModelInterface&>(*this);
	this->setParent(parent);
}

void ScenarioProcessViewModel::serialize(QDataStream& s) const
{
	s << m_constraintToBox;
}

void ScenarioProcessViewModel::deserialize(QDataStream& s)
{
	s >> m_constraintToBox;
}

QPair<bool, int>& ScenarioProcessViewModel::boxDisplayedForConstraint(int constraintId)
{
	if(m_constraintToBox.contains(constraintId))
		return m_constraintToBox[constraintId];

	throw std::runtime_error("Constraint not found in ScenarioProcessViewModel map.");
}

// If the constraint had a box ? (case undo - redo)
void ScenarioProcessViewModel::on_constraintCreated(int constraintId)
{
	m_constraintToBox[constraintId] = {false, {}};
	emit constraintCreated(constraintId);
}

void ScenarioProcessViewModel::on_constraintRemoved(int constraintId)
{
	m_constraintToBox.remove(constraintId);
	emit constraintRemoved(constraintId);
}
