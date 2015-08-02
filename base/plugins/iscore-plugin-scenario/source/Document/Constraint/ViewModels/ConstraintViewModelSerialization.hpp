#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>

#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/AbstractScenarioLayerModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp"
class ConstraintViewModel;

// Load a single constraint view model.
template<typename ScenarioViewModelType>
typename ScenarioViewModelType::constraint_layer_type*
loadConstraintViewModel(Deserializer<DataStream>& deserializer,
                          ScenarioViewModelType* svm)
{
    // Deserialize the required identifier
    id_type<ConstraintModel> constraint_model_id;
    deserializer.m_stream >> constraint_model_id;
    auto& constraint = model(*svm).constraint(constraint_model_id);

    // Make it
    auto viewmodel =  new typename ScenarioViewModelType
    ::constraint_layer_type {deserializer,
                                  constraint,
                                  svm};

    // Make the required connections with the parent constraint
    constraint.setupConstraintViewModel(viewmodel);

    return viewmodel;
}

template<typename ScenarioViewModelType>
typename ScenarioViewModelType::constraint_layer_type*
loadConstraintViewModel(Deserializer<JSONObject>& deserializer,
                          ScenarioViewModelType* svm)
{
    // Deserialize the required identifier
    auto constraint_model_id = fromJsonValue<id_type<ConstraintModel>>(deserializer.m_obj["ConstraintId"]);
    auto& constraint = model(*svm).constraint(constraint_model_id);

    // Make it
    auto viewmodel =  new typename ScenarioViewModelType
    ::constraint_layer_type {deserializer,
                                  constraint,
                                  svm};

    // Make the required connections with the parent constraint
    constraint.setupConstraintViewModel(viewmodel);

    return viewmodel;
}


// These functions are utilities to save / load
// constraint view models in commands
// NOTE : the implementation for now is in TemporalConstraintViewModelSerialization but
// should be in its own file.

SerializedConstraintViewModels serializeConstraintViewModels(
        const ConstraintModel& constraint,
        const ScenarioModel& scenario);

// Save all the constraint view models
// Load a group of constraint view models

void deserializeConstraintViewModels(
        const SerializedConstraintViewModels& vms,
        const ScenarioModel& scenar);
