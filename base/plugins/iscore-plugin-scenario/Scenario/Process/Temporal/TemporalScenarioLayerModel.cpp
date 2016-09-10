#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <QVector>

#include <Process/LayerModel.hpp>
#include "TemporalScenarioLayerModel.hpp"
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

namespace Scenario
{
class ConstraintViewModel;
TemporalScenarioLayer::TemporalScenarioLayer(
        const Id<LayerModel>& viewModelId,
        const QMap<Id<ConstraintModel>, Id<ConstraintViewModel> >& constraintIds,
        Scenario::ProcessModel& model,
        QObject* parent) :
    AbstractScenarioLayer {viewModelId,
                              "TemporalScenarioLayer",
                              model,
                              parent}
{
    for(auto& key : constraintIds.keys())
    {
        makeConstraintViewModel(key, constraintIds.value(key));
    }
}

TemporalScenarioLayer::TemporalScenarioLayer(
        const TemporalScenarioLayer& source,
        const Id<LayerModel>& id,
        Scenario::ProcessModel& newScenario,
        QObject* parent) :
    AbstractScenarioLayer {source,
                              id,
                              "TemporalScenarioLayer",
                              newScenario,
                              parent
}
{
    for(TemporalConstraintViewModel* src_constraint : constraintsViewModels(source))
    {
        // OPTMIZEME (inside addConstraintViewModel)
        addConstraintViewModel(
                    src_constraint->clone(
                        src_constraint->id(),
                        newScenario.constraints.at(Id<ConstraintModel>{src_constraint->model().id()}),
                        this));
    }
}

void TemporalScenarioLayer::makeConstraintViewModel(
        const Id<ConstraintModel>& constraintModelId,
        const Id<ConstraintViewModel>& constraintViewModelId)
{
    auto& constraint_model = model(*this).constraint(constraintModelId);

    auto constraint_view_model =
        constraint_model.makeConstraintViewModel<constraint_layer_type> (
            constraintViewModelId,
            this);

    addConstraintViewModel(constraint_view_model);
}

void TemporalScenarioLayer::addConstraintViewModel(constraint_layer_type* constraint_view_model)
{
    m_constraints.push_back(constraint_view_model);

    emit constraintViewModelCreated(*constraint_view_model);
}

void TemporalScenarioLayer::on_constraintRemoved(const ConstraintModel& cstr)
{
    auto cvms = constraintsViewModels(*this);
    for(auto& constraint_view_model : cvms)
    {
        if(&constraint_view_model->model() == &cstr)
        {
            removeConstraintViewModel(constraint_view_model->id());
            return;
        }
    }
}
}
