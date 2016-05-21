#pragma once
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>

#include <Process/LayerModel.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

template <typename VisitorType> class Visitor;
namespace Scenario
{
class ScenarioModel;
class ConstraintModel;

// Load a single constraint view model.
template<typename ScenarioViewModelType>
typename ScenarioViewModelType::constraint_layer_type*
loadConstraintViewModel(Deserializer<DataStream>& deserializer,
                          ScenarioViewModelType* svm)
{
    // Deserialize the required identifier
    Id<ConstraintModel> constraint_model_id;
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
    auto constraint_model_id = fromJsonValue<Id<ConstraintModel>>(deserializer.m_obj["ConstraintId"]);
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
        const Scenario::ScenarioModel& scenario);

// Save all the constraint view models
// Load a group of constraint view models

void deserializeConstraintViewModels(
        const SerializedConstraintViewModels& vms,
        const Scenario::ScenarioModel& scenar);
}
