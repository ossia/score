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
/*
TemporalScenarioProcessViewModel::TemporalScenarioProcessViewModel(QDataStream& s,
																   ScenarioProcessSharedModel* model,
																   QObject* parent):
	AbstractScenarioProcessViewModel{s,
									 model,
									 parent}
{
	s >> *this;
}

void TemporalScenarioProcessViewModel::serialize(QDataStream& s) const
{
	s << *this;
}
*/
void TemporalScenarioProcessViewModel::makeConstraintViewModel(int constraintModelId,
															   int constraintViewModelId)
{
	qDebug() << constraintViewModelId << "created.";
	auto constraint_model = model(this)->constraint(constraintModelId);

	int __warn;
	/* TODO
	auto constraint_view_model =
			constraint_model->makeViewModel<constraint_view_model_type>(
									 constraintViewModelId,
									 this);
	m_constraints.push_back(constraint_view_model);

	emit constraintViewModelCreated(constraintViewModelId);
	*/
}
/*
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
*/

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
