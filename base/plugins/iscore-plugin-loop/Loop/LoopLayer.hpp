#pragma once
#include <Process/LayerModel.hpp>

#include <iscore/tools/Metadata.hpp>
#include <iscore/serialization/VisitorInterface.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

namespace Process { class LayerModelPanelProxy; }
class QObject;
class TemporalConstraintViewModel;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Loop{
    class ProcessModel;

class Layer final : public Process::LayerModel
{
        ISCORE_METADATA(Loop::Layer)

        ISCORE_SERIALIZE_FRIENDS(Layer, DataStream)
        ISCORE_SERIALIZE_FRIENDS(Layer, JSONObject)

        Q_OBJECT
    public:
        Layer(
                Loop::ProcessModel& model,
                const Id<LayerModel>& id,
                QObject* parent);

        // Copy
        Layer(
                const Layer& source,
                Loop::ProcessModel& model,
                const Id<LayerModel>& id,
                QObject* parent);

        // Load
        template<typename Impl>
        Layer(
                Deserializer<Impl>& vis,
                Loop::ProcessModel& model,
                QObject* parent) :
            LayerModel {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        Process::LayerModelPanelProxy* make_panelProxy(QObject* parent) const override;
        void serialize(const VisitorVariant&) const override;

        const Loop::ProcessModel& model() const;

        const TemporalConstraintViewModel& constraint() const
        {
            return *m_constraint;
        }

    private:
        TemporalConstraintViewModel* m_constraint{};
};
}
