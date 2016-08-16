#pragma once

#include <QMap>

#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
#include <Scenario/Process/AbstractScenarioLayerModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

namespace Process {
class LayerModel;
class LayerModelPanelProxy;
}
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Scenario
{
class ConstraintModel;
class ConstraintViewModel;
class TemporalScenarioLayer final : public AbstractScenarioLayer
{
        Q_OBJECT
    public:
        using model_type = Scenario::ProcessModel;
        using constraint_layer_type = TemporalConstraintViewModel;

        TemporalScenarioLayer(const Id<LayerModel>& id,
                              const QMap<Id<ConstraintModel>,
                              Id<ConstraintViewModel>>& constraintIds,
                              Scenario::ProcessModel& model,
                              QObject* parent);

        // Copy
        TemporalScenarioLayer(const TemporalScenarioLayer& source,
                              const Id<LayerModel>& id,
                              Scenario::ProcessModel& model,
                              QObject* parent);

        // Load
        template<typename Impl>
        TemporalScenarioLayer(Deserializer<Impl>& vis,
                              Scenario::ProcessModel& model,
                              QObject* parent) :
            AbstractScenarioLayer {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        ~TemporalScenarioLayer() = default;

        void serialize_impl(const VisitorVariant&) const override;

        void makeConstraintViewModel(
                const Id<ConstraintModel>& constraintModelId,
                const Id<ConstraintViewModel>& constraintViewModelId) override;

        void addConstraintViewModel(constraint_layer_type* constraint_view_model);

    signals:
        void constraintViewModelCreated(const TemporalConstraintViewModel&);

    public:
        void on_constraintRemoved(const ConstraintModel&) override;

};
}
