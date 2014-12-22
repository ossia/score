#include "TemporalScenarioProcessViewModel.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"

TemporalScenarioProcessViewModel::TemporalScenarioProcessViewModel(int viewModelId, ScenarioProcessSharedModel* model, QObject* parent):
	ProcessViewModelInterface(parent, "TemporalScenarioProcessViewModel", viewModelId, model)
{
}

TemporalScenarioProcessViewModel::TemporalScenarioProcessViewModel(QDataStream& s, ScenarioProcessSharedModel* model, QObject* parent):
	ProcessViewModelInterface(nullptr, "TemporalScenarioProcessViewModel", -1, model) // TODO pourquoi ne pas utiliser QDataStream dans le ctor parent ?
{
	s >> static_cast<ProcessViewModelInterface&>(*this);
	this->setParent(parent);
}

void TemporalScenarioProcessViewModel::serialize(QDataStream& s) const
{
	s << m_constraintToBox;
}

void TemporalScenarioProcessViewModel::deserialize(QDataStream& s)
{
	s >> m_constraintToBox;
}

QPair<bool, int>& TemporalScenarioProcessViewModel::boxDisplayedForConstraint(int constraintId)
{
	if(m_constraintToBox.contains(constraintId))
		return m_constraintToBox[constraintId];

	throw std::runtime_error("Constraint not found in TemporalScenarioProcessViewModel map.");
}

// If the constraint had a box ? (case undo - redo)
void TemporalScenarioProcessViewModel::on_constraintCreated(int constraintId)
{
	m_constraintToBox[constraintId] = {false, {}};
	emit constraintCreated(constraintId);
}

void TemporalScenarioProcessViewModel::on_constraintRemoved(int constraintId)
{
	m_constraintToBox.remove(constraintId);
	emit constraintRemoved(constraintId);
}
