#pragma once
#include <Process/LayerModel.hpp>

#include <iscore/serialization/VisitorInterface.hpp>

class DataStream;
class JSONObject;
class LayerModelPanelProxy;
class QObject;
class TemporalConstraintViewModel;
template <typename tag, typename impl> class id_base_t;

namespace Loop{
    class ProcessModel;
}
class LoopLayer final : public LayerModel
{
        ISCORE_METADATA("LoopLayer")

        ISCORE_SERIALIZE_FRIENDS(LoopLayer, DataStream)
        ISCORE_SERIALIZE_FRIENDS(LoopLayer, JSONObject)

        Q_OBJECT
    public:
        LoopLayer(
                Loop::ProcessModel& model,
                const Id<LayerModel>& id,
                QObject* parent);

        // Copy
        LoopLayer(
                const LoopLayer& source,
                Loop::ProcessModel& model,
                const Id<LayerModel>& id,
                QObject* parent);

        // Load
        template<typename Impl>
        LoopLayer(
                Deserializer<Impl>& vis,
                Loop::ProcessModel& model,
                QObject* parent) :
            LayerModel {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        LayerModelPanelProxy* make_panelProxy(QObject* parent) const override;
        void serialize(const VisitorVariant&) const override;

        const Loop::ProcessModel& model() const;

        const TemporalConstraintViewModel& constraint() const
        {
            return *m_constraint;
        }

    private:
        TemporalConstraintViewModel* m_constraint{};
};
