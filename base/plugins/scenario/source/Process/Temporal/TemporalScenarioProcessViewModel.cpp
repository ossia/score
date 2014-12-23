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
	qDebug() << "TODO (will crash): " << Q_FUNC_INFO;
}

void TemporalScenarioProcessViewModel::deserialize(QDataStream& s)
{
	qDebug() << "TODO (will crash): " << Q_FUNC_INFO;
}

void TemporalScenarioProcessViewModel::createConstraintViewModel(int constraintModelId, int constraintViewModelId)
{
	auto constraint_model = model(this)->constraint(constraintModelId);
	auto constraint_view_model = constraint_model->makeViewModel<
								 TemporalScenarioProcessViewModel::constraint_view_model_type>(
									 constraintViewModelId,
									 this);
	m_constraints.push_back(constraint_view_model);

	emit constraintViewModelCreated(constraintViewModelId);
}

void TemporalScenarioProcessViewModel::on_constraintRemoved(int constraintViewModelId)
{
	removeConstraintViewModel(constraintViewModelId);
}
