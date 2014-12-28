#include "TemporalScenarioProcessViewModel.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Constraint/Temporal/TemporalConstraintViewModel.hpp"

TemporalScenarioProcessViewModel::TemporalScenarioProcessViewModel(int viewModelId,
																   ScenarioProcessSharedModel* model,
																   QObject* parent):
	AbstractScenarioProcessViewModel{viewModelId,
									 "TemporalScenarioProcessViewModel",
									 model,
									 parent}
{
}

void TemporalScenarioProcessViewModel::serialize(SerializationIdentifier identifier, void* data) const
{
	// TODO how to abstract this since it will always be the same ?
	if(identifier == DataStream::type())
	{
		static_cast<Serializer<DataStream>*>(data)->visit(*this);
	}

	throw std::runtime_error("ScenarioProcessViewModel only supports DataStream serialization");
}

void TemporalScenarioProcessViewModel::makeConstraintViewModel(int constraintModelId,
															   int constraintViewModelId)
{
	qDebug() << constraintViewModelId << "created.";
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

void TemporalScenarioProcessViewModel::on_constraintRemoved(int constraintSharedModelId)
{	for(auto& constraint_view_model : constraintsViewModels(*this))
	{
		if(constraint_view_model->model()->id() == constraintSharedModelId)
		{
			removeConstraintViewModel(constraint_view_model->id());
			return;
		}
	}
}
