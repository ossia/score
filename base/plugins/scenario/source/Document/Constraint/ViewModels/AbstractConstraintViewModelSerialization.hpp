#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include "source/Document/Constraint/ConstraintModel.hpp"
#include "Process/AbstractScenarioViewModel.hpp"
#include "Process/ScenarioModel.hpp"

class AbstractConstraintViewModel;

// Load a single constraint view model.
template<typename ScenarioViewModelType>
typename ScenarioViewModelType::constraint_view_model_type*
createConstraintViewModel(Deserializer<DataStream>& deserializer,
                          ScenarioViewModelType* svm)
{
    // Deserialize the required identifier
    id_type<ConstraintModel> constraint_model_id;
    deserializer.m_stream >> constraint_model_id;
    auto constraint = model(svm)->constraint(constraint_model_id);

    // Make it
    auto viewmodel =  new typename ScenarioViewModelType
    ::constraint_view_model_type {deserializer,
                                  constraint,
                                  svm
                                 };

    // Make the required connections with the parent constraint
    constraint->setupConstraintViewModel(viewmodel);

    return viewmodel;
}

template<typename ScenarioViewModelType>
typename ScenarioViewModelType::constraint_view_model_type*
createConstraintViewModel(Deserializer<JSON>& deserializer,
                          ScenarioViewModelType* svm)
{
    // Deserialize the required identifier
    id_type<ConstraintModel> constraint_model_id;
    fromJsonObject(deserializer.m_obj["ConstraintId"].toObject(), constraint_model_id);

    auto constraint = model(svm)->constraint(constraint_model_id);

    // Make it
    auto viewmodel =  new typename ScenarioViewModelType
    ::constraint_view_model_type {deserializer,
                                  constraint,
                                  svm};

    // Make the required connections with the parent constraint
    constraint->setupConstraintViewModel(viewmodel);

    return viewmodel;
}


// These functions are utilities to save / load
// constraint view models in commands
using SerializedConstraintViewModels = QMap<std::tuple<int, int, int>, QPair<QString, QByteArray>>;

SerializedConstraintViewModels serializeConstraintViewModels(ConstraintModel* constraint,
                                                             ScenarioModel* scenario);

// Save all the constraint view models
// Load a group of constraint view models

void deserializeConstraintViewModels(SerializedConstraintViewModels& vms,
                                     ScenarioModel* scenar);
