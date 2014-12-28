#pragma once
#include <interface/serialization/DataStreamVisitor.hpp>

#include "ConstraintModel.hpp"
#include "Process/AbstractScenarioProcessViewModel.hpp"
#include "Process/ScenarioProcessSharedModel.hpp"

class AbstractConstraintViewModel;

template<typename ViewModelType> // Arg might be an id or a datastream [
ViewModelType* createConstraintViewModel(Deserializer<DataStream>& deserializer,
										 AbstractScenarioProcessViewModel* svm,
										 QObject* parent)
{
	// Deserialize the required identifier
	SettableIdentifier constraint_model_id;
	deserializer.m_stream >> constraint_model_id;
	auto constraint = model(svm)->constraint(constraint_model_id);

	auto viewmodel =  new ViewModelType{deserializer, constraint, parent};
	constraint->setupConstraintViewModel(viewmodel);
	return viewmodel;
}

template<>
void Visitor<Reader<DataStream>>::visit(const AbstractConstraintViewModel&);


//template<>
//void Visitor<Writer<DataStream>>::visit(AbstractConstraintViewModel&);