#pragma once

#include <ProcessInterface/LayerModel.hpp>

class AutomationModel;
class AutomationLayerModel : public LayerModel
{
        Q_OBJECT
    public:
        AutomationLayerModel(AutomationModel& model,
                            const id_type<LayerModel>& id,
                            QObject* parent);

        // Copy
        AutomationLayerModel(const AutomationLayerModel& source,
                            AutomationModel& model,
                            const id_type<LayerModel>& id,
                            QObject* parent);

        // Load
        template<typename Impl>
        AutomationLayerModel(Deserializer<Impl>& vis,
                            AutomationModel& model,
                            QObject* parent) :
            LayerModel {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        virtual LayerModelPanelProxy* make_panelProxy(QObject* parent) const override;
        virtual void serialize(const VisitorVariant&) const override;

        const AutomationModel& model() const;
};
