#include "TemporalScenarioViewModel.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"

TemporalScenarioViewModel::TemporalScenarioViewModel(id_type<ProcessViewModelInterface> viewModelId,
																   ScenarioModel* model,
																   QObject* parent):
	AbstractScenarioViewModel{viewModelId,
									 "TemporalScenarioViewModel",
									 model,
									 parent}
{
}

TemporalScenarioViewModel::TemporalScenarioViewModel(const TemporalScenarioViewModel *source,
																   id_type<ProcessViewModelInterface> id,
																   ScenarioModel *model,
																   QObject *parent):
	AbstractScenarioViewModel{source,
									 id,
									 "TemporalScenarioViewModel",
									 model,
									 parent}
{
	for(TemporalConstraintViewModel* constraint : constraintsViewModels(*source))
	{
		// TODO some room for optimization here
		addConstraintViewModel(constraint->clone(constraint->id(),
												 constraint->model(),
												 this));
	}
}


void TemporalScenarioViewModel::makeConstraintViewModel(id_type<ConstraintModel> constraintModelId,
															   id_type<AbstractConstraintViewModel> constraintViewModelId)
{
	auto constraint_model = model(this)->constraint(constraintModelId);

	auto constraint_view_model =
			constraint_model->makeConstraintViewModel<constraint_view_model_type>(
				constraintViewModelId,
				this);

	addConstraintViewModel(constraint_view_model);
}

void TemporalScenarioViewModel::addConstraintViewModel(constraint_view_model_type* constraint_view_model)
{
	m_constraints.push_back(constraint_view_model);

	emit constraintViewModelCreated(constraint_view_model->id());
}

void TemporalScenarioViewModel::on_constraintRemoved(id_type<ConstraintModel> constraintSharedModelId)
{	for(auto& constraint_view_model : constraintsViewModels(*this))
	{
		if(constraint_view_model->model()->id() == constraintSharedModelId)
		{
			removeConstraintViewModel(constraint_view_model->id());
			return;
		}
	}
}
