#pragma once
#include <Process/LayerModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

class DummyLayerModel final : public LayerModel
{
        ISCORE_SERIALIZE_FRIENDS(DummyLayerModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(DummyLayerModel, JSONObject)

    public:
        explicit DummyLayerModel(
                Process& model,
                const Id<LayerModel>& id,
                QObject* parent);

        // Copy
        explicit DummyLayerModel(
                const DummyLayerModel& source,
                Process& model,
                const Id<LayerModel>& id,
                QObject* parent);

        // Load
        template<typename Impl>
        explicit DummyLayerModel(
                Deserializer<Impl>& vis,
                Process& model,
                QObject* parent) :
            LayerModel {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        void serialize(const VisitorVariant&) const override;
        LayerModelPanelProxy* make_panelProxy(QObject* parent) const override;
};
