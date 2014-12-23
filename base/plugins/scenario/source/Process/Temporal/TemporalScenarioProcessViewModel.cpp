#include "TemporalScenarioProcessViewModel.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"

TemporalScenarioProcessViewModel::TemporalScenarioProcessViewModel(int viewModelId, ScenarioProcessSharedModel* model, QObject* parent):
	AbstractScenarioProcessViewModel(parent, "TemporalScenarioProcessViewModel", viewModelId, model)
{
}

TemporalScenarioProcessViewModel::TemporalScenarioProcessViewModel(QDataStream& s, ScenarioProcessSharedModel* model, QObject* parent):
	AbstractScenarioProcessViewModel(nullptr, "TemporalScenarioProcessViewModel", -1, model) // TODO pourquoi ne pas utiliser QDataStream dans le ctor parent ?
{
	s >> static_cast<ProcessViewModelInterface&>(*this);
	this->setParent(parent);
}

void TemporalScenarioProcessViewModel::serialize(QDataStream& s) const
{
//	s << m_constraintToBox;
}

void TemporalScenarioProcessViewModel::deserialize(QDataStream& s)
{
	//	s >> m_constraintToBox;
}

void TemporalScenarioProcessViewModel::createConstraintViewModel(int constraintModelId, int constraintViewModelId)
{
	auto constraint_model = model(this)->constraint(constraintModelId);
	auto constraint_view_model = constraint_model->makeViewModel("Temporal", constraintViewModelId, this);
	m_constraints.push_back(static_cast<constraint_type*>(constraint_view_model));

	emit constraintViewModelCreated(constraintViewModelId);
}

/*
QPair<bool, int>& TemporalScenarioProcessViewModel::boxDisplayedForConstraint(int constraintId)
{
	if(m_constraintToBox.contains(constraintId))
		return m_constraintToBox[constraintId];

	throw std::runtime_error("Constraint not found in TemporalScenarioProcessViewModel map.");
}*/

// If the constraint had a box ? (case undo - redo)
void TemporalScenarioProcessViewModel::on_constraintCreated(int constraintId)
{/*
	m_constraints.push_back(model(this)->constraint(constraintId)->makeViewModel("Temporal")
//	m_constraintToBox[constraintId] = {false, {}};
	emit constraintCreated(constraintId);*/
}

void TemporalScenarioProcessViewModel::on_constraintRemoved(int constraintId)
{
	/*
	m_constraintToBox.remove(constraintId);
	emit constraintRemoved(constraintId);*/
}
