#pragma once
#include <QJsonObject>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <Process/LayerModel.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_scenario_export.h>

template <typename VisitorType>
class Visitor;
namespace Scenario
{
class ProcessModel;
class ConstraintModel;

// Load a single constraint view model.
template <typename ScenarioViewModelType>
typename ScenarioViewModelType::constraint_layer_type* loadConstraintViewModel(
    DataStream::Deserializer& deserializer,
    ScenarioViewModelType* svm,
    const Id<ConstraintModel>& constraint_model_id)
{
  auto& constraint = model(*svm).constraint(constraint_model_id);

  // Make it
  auto viewmodel = new typename ScenarioViewModelType::constraint_layer_type{
      deserializer, constraint, svm};

  // Make the required connections with the parent constraint
  constraint.setupConstraintViewModel(viewmodel);

  return viewmodel;
}

template <typename ScenarioViewModelType>
typename ScenarioViewModelType::constraint_layer_type* loadConstraintViewModel(
    JSONObject::Deserializer& deserializer,
    ScenarioViewModelType* svm,
    const Id<ConstraintModel>& constraint_model_id)
{
  auto& constraint = model(*svm).constraint(constraint_model_id);

  // Make it
  auto viewmodel = new typename ScenarioViewModelType::constraint_layer_type{
      deserializer, constraint, svm};

  // Make the required connections with the parent constraint
  constraint.setupConstraintViewModel(viewmodel);

  return viewmodel;
}

// These functions are utilities to save / load
// constraint view models in commands
// NOTE : the implementation for now is in
// TemporalConstraintViewModelSerialization but
// should be in its own file.

ISCORE_PLUGIN_SCENARIO_EXPORT SerializedConstraintViewModels
serializeConstraintViewModels(
    const ConstraintModel& constraint, const Scenario::ProcessModel& scenario);

// Save all the constraint view models
// Load a group of constraint view models

ISCORE_PLUGIN_SCENARIO_EXPORT void deserializeConstraintViewModels(
    const SerializedConstraintViewModels& vms,
    const Scenario::ProcessModel& scenar);
}
