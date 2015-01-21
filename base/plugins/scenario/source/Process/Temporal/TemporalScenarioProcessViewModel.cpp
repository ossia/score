#include "TemporalScenarioProcessViewModel.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"

TemporalScenarioProcessViewModel::TemporalScenarioProcessViewModel(id_type<ProcessViewModelInterface> viewModelId,
																   ScenarioProcessSharedModel* model,
																   QObject* parent):
	AbstractScenarioProcessViewModel{viewModelId,
									 "TemporalScenarioProcessViewModel",
									 model,
									 parent}
{
}



void TemporalScenarioProcessViewModel::makeConstraintViewModel(id_type<ConstraintModel> constraintModelId,
															   id_type<AbstractConstraintViewModel> constraintViewModelId)
{
	auto constraint_model = model(this)->constraint(constraintModelId);


	auto constraint_view_model =
			constraint_model->makeConstraintViewModel<constraint_view_model_type>(
									 constraintViewModelId,
									 this);

	addConstraintViewModel(constraint_view_model);
}

void TemporalScenarioProcessViewModel::addConstraintViewModel(constraint_view_model_type* constraint_view_model)
{
	m_constraints.push_back(constraint_view_model);

	emit constraintViewModelCreated(constraint_view_model->id());
}

void TemporalScenarioProcessViewModel::on_constraintRemoved(id_type<ConstraintModel> constraintSharedModelId)
{	for(auto& constraint_view_model : constraintsViewModels(*this))
	{
		if(constraint_view_model->model()->id() == constraintSharedModelId)
		{
			removeConstraintViewModel(constraint_view_model->id());
			return;
		}
	}
}
