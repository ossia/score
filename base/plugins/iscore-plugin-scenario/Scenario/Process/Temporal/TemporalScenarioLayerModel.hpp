#pragma once

#include <qmap.h>

#include "Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Scenario/Process/AbstractScenarioLayerModel.hpp"
#include "Scenario/Process/ScenarioModel.hpp"
#include "iscore/serialization/VisitorInterface.hpp"

class ConstraintModel;
class ConstraintViewModel;
class LayerModel;
class LayerModelPanelProxy;
class QObject;
template <typename tag, typename impl> class id_base_t;

namespace Scenario
{
}
class TemporalScenarioLayerModel final : public AbstractScenarioLayerModel
{
        Q_OBJECT
    public:
        using model_type = Scenario::ScenarioModel;
        using constraint_layer_type = TemporalConstraintViewModel;

        TemporalScenarioLayerModel(const Id<LayerModel>& id,
                              const QMap<Id<ConstraintModel>,
                              Id<ConstraintViewModel>>& constraintIds,
                              Scenario::ScenarioModel& model,
                              QObject* parent);

        // Copy
        TemporalScenarioLayerModel(const TemporalScenarioLayerModel& source,
                              const Id<LayerModel>& id,
                              Scenario::ScenarioModel& model,
                              QObject* parent);

        // Load
        template<typename Impl>
        TemporalScenarioLayerModel(Deserializer<Impl>& vis,
                              Scenario::ScenarioModel& model,
                              QObject* parent) :
            AbstractScenarioLayerModel {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        LayerModelPanelProxy* make_panelProxy(QObject* parent) const override;

        ~TemporalScenarioLayerModel() = default;

        void serialize(const VisitorVariant&) const override;

        void makeConstraintViewModel(
                const Id<ConstraintModel>& constraintModelId,
                const Id<ConstraintViewModel>& constraintViewModelId) override;

        void addConstraintViewModel(constraint_layer_type* constraint_view_model);

    signals:
        void constraintViewModelCreated(const TemporalConstraintViewModel&);

    public:
        void on_constraintRemoved(const ConstraintModel&) override;

};
