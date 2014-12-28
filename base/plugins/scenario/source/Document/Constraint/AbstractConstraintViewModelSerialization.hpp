#pragma once
#include <interface/serialization/DataStreamVisitor.hpp>

#include "ConstraintModel.hpp"
#include "Process/AbstractScenarioProcessViewModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"

class AbstractConstraintViewModel;

template<typename ScenarioViewModelType>
typename ScenarioViewModelType::constraint_view_model_type*
	createConstraintViewModel(Deserializer<DataStream>& deserializer,
							  ScenarioViewModelType* svm)
{
	// Deserialize the required identifier
	SettableIdentifier constraint_model_id;
	deserializer.m_stream >> constraint_model_id;
	auto constraint = model(svm)->constraint(constraint_model_id);

	// Make it
	auto viewmodel =  new typename ScenarioViewModelType::constraint_view_model_type{deserializer, constraint, svm};

	// Make the required connections with the parent constraint
	constraint->setupConstraintViewModel(viewmodel);

	return viewmodel;
}

template<>
void Visitor<Reader<DataStream>>::visit(const AbstractConstraintViewModel&);


//template<>
//void Visitor<Writer<DataStream>>::visit(AbstractConstraintViewModel&);