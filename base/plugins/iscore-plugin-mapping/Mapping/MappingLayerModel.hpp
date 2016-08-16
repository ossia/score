#pragma once

#include <iscore/tools/Metadata.hpp>
#include <Process/LayerModel.hpp>

#include <iscore/serialization/VisitorInterface.hpp>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Mapping
{
class ProcessModel;
class Layer : public Process::LayerModel
{
        public:
            Layer(
                ProcessModel& model,
                const Id<Process::LayerModel>& id,
                QObject* parent);

        // Copy
        Layer(
                const Layer& source,
                ProcessModel& model,
                const Id<Process::LayerModel>& id,
                QObject* parent);

        // Load
        template<typename Impl>
        Layer(
                Deserializer<Impl>& vis,
                ProcessModel& model,
                QObject* parent) :
            Process::LayerModel {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        void serialize_impl(const VisitorVariant&) const override;

        const ProcessModel& model() const;
};
}

DEFAULT_MODEL_METADATA(Mapping::Layer, "Mapping layer")
