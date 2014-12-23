#include "TemporalScenarioProcessViewModel.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"

QDataStream& operator <<(QDataStream& s, const TemporalScenarioProcessViewModel& pvm)
{
	// Note : this function must not be called directly :
	// it has to be called from serialize()
	// which is in turn called from ProcessViewModelInterface's operator <<
	// in order to save the SettableIdentifier, and parent classes's data.

	// Save all the constraint view models.
	// They are casted, so the correct serialization operator is called.
	auto vec = constraintsViewModels(pvm);

	s << (int) vec.size();
	for(auto cstraint : vec)
	{
		s << cstraint->model()->id(); // Is deserialized in makeConstraintViewModel
		s << cstraint;
	}

	return s;
}
QDataStream& operator >>(QDataStream& s, TemporalScenarioProcessViewModel& pvm)
{
	int count;
	s >> count;

	for(; count --> 0;)
	{
		pvm.makeConstraintViewModel(s);
	}

	return s;
}

TemporalScenarioProcessViewModel::TemporalScenarioProcessViewModel(int viewModelId,
																   ScenarioProcessSharedModel* model,
																   QObject* parent):
	AbstractScenarioProcessViewModel{viewModelId,
									 "TemporalScenarioProcessViewModel",
									 model,
									 parent}
{
}

TemporalScenarioProcessViewModel::TemporalScenarioProcessViewModel(QDataStream& s,
																   ScenarioProcessSharedModel* model,
																   QObject* parent):
	AbstractScenarioProcessViewModel{s,
									 "TemporalScenarioProcessViewModel",
									 model,
									 parent}
{
	s >> *this;
}

void TemporalScenarioProcessViewModel::serialize(QDataStream& s) const
{
	s << *this;
}

void TemporalScenarioProcessViewModel::makeConstraintViewModel(int constraintModelId,
															   int constraintViewModelId)
{
	auto constraint_model = model(this)->constraint(constraintModelId);
	auto constraint_view_model =
			constraint_model->makeViewModel<constraint_view_model_type>(
									 constraintViewModelId,
									 this);
	m_constraints.push_back(constraint_view_model);

	emit constraintViewModelCreated(constraintViewModelId);
}

void TemporalScenarioProcessViewModel::makeConstraintViewModel(QDataStream& s)
{
	// Deserialize the required identifier
	SettableIdentifier constraint_model_id;
	s >> constraint_model_id;
	auto constraint_model = model(this)->constraint(constraint_model_id);

	// Make it
	auto constraint_view_model =
			constraint_model->makeViewModel<constraint_view_model_type>(
									 s,
									 this);

	m_constraints.push_back(constraint_view_model);

	emit constraintViewModelCreated(constraint_view_model->id());
}

void TemporalScenarioProcessViewModel::on_constraintRemoved(int constraintViewModelId)
{
	removeConstraintViewModel(constraintViewModelId);
}
