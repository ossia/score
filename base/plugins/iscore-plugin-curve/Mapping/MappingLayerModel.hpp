#pragma once

#include <ProcessInterface/LayerModel.hpp>

class MappingModel;
class MappingLayerModel : public LayerModel
{
        Q_OBJECT
    public:
        MappingLayerModel(MappingModel& model,
                            const Id<LayerModel>& id,
                            QObject* parent);

        // Copy
        MappingLayerModel(const MappingLayerModel& source,
                            MappingModel& model,
                            const Id<LayerModel>& id,
                            QObject* parent);

        // Load
        template<typename Impl>
        MappingLayerModel(Deserializer<Impl>& vis,
                            MappingModel& model,
                            QObject* parent) :
            LayerModel {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        virtual LayerModelPanelProxy* make_panelProxy(QObject* parent) const override;
        virtual void serialize(const VisitorVariant&) const override;

        const MappingModel& model() const;
};
