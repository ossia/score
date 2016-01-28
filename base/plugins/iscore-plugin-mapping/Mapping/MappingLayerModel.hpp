#pragma once

#include <iscore/tools/Metadata.hpp>
#include <Process/LayerModel.hpp>

#include <iscore/serialization/VisitorInterface.hpp>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Mapping
{
class ProcessModel;
class LayerModel : public Process::LayerModel
{
        public:
            LayerModel(
                ProcessModel& model,
                const Id<Process::LayerModel>& id,
                QObject* parent);

        // Copy
        LayerModel(
                const LayerModel& source,
                ProcessModel& model,
                const Id<Process::LayerModel>& id,
                QObject* parent);

        // Load
        template<typename Impl>
        LayerModel(
                Deserializer<Impl>& vis,
                ProcessModel& model,
                QObject* parent) :
            Process::LayerModel {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        Process::LayerModelPanelProxy* make_panelProxy(QObject* parent) const override;
        void serialize(const VisitorVariant&) const override;

        const ProcessModel& model() const;
};
}

DEFAULT_MODEL_METADATA(Mapping::LayerModel, "Mapping layer")
