#pragma once
#include <Process/LayerModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

class TemporalConstraintViewModel;

class LoopProcessModel;
class LoopLayer final : public LayerModel
{
        ISCORE_METADATA("LoopLayer")

        ISCORE_SERIALIZE_FRIENDS(LoopLayer, DataStream)
        ISCORE_SERIALIZE_FRIENDS(LoopLayer, JSONObject)

        Q_OBJECT
    public:
        LoopLayer(
                LoopProcessModel& model,
                const Id<LayerModel>& id,
                QObject* parent);

        // Copy
        LoopLayer(
                const LoopLayer& source,
                LoopProcessModel& model,
                const Id<LayerModel>& id,
                QObject* parent);

        // Load
        template<typename Impl>
        LoopLayer(
                Deserializer<Impl>& vis,
                LoopProcessModel& model,
                QObject* parent) :
            LayerModel {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        LayerModelPanelProxy* make_panelProxy(QObject* parent) const override;
        void serialize(const VisitorVariant&) const override;

        const LoopProcessModel& model() const;

        const TemporalConstraintViewModel& constraint() const
        {
            return *m_constraint;
        }

    private:
        TemporalConstraintViewModel* m_constraint{};
};
