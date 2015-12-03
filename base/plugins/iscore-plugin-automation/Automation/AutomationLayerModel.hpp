#pragma once

#include <Process/LayerModel.hpp>

#include <iscore/serialization/VisitorInterface.hpp>

class AutomationModel;
class LayerModelPanelProxy;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

class AutomationLayerModel final : public LayerModel
{
        ISCORE_METADATA("AutomationLayerModel")
    public:
        AutomationLayerModel(
                AutomationModel& model,
                const Id<LayerModel>& id,
                QObject* parent);

        // Copy
        AutomationLayerModel(
                const AutomationLayerModel& source,
                AutomationModel& model,
                const Id<LayerModel>& id,
                QObject* parent);

        // Load
        template<typename Impl>
        AutomationLayerModel(
                Deserializer<Impl>& vis,
                AutomationModel& model,
                QObject* parent) :
            LayerModel {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        LayerModelPanelProxy* make_panelProxy(QObject* parent) const override;
        void serialize(const VisitorVariant&) const override;

        const AutomationModel& model() const;
};
