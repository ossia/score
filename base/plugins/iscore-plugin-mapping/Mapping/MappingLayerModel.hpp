#pragma once

#include <iscore/tools/Metadata.hpp>
#include <Process/LayerModel.hpp>

#include <iscore/serialization/VisitorInterface.hpp>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Mapping
{
class MappingModel;
class MappingLayerModel : public Process::LayerModel
{
        ISCORE_METADATA(Mapping::MappingLayerModel)
        public:
            MappingLayerModel(
                MappingModel& model,
                const Id<Process::LayerModel>& id,
                QObject* parent);

        // Copy
        MappingLayerModel(
                const MappingLayerModel& source,
                MappingModel& model,
                const Id<Process::LayerModel>& id,
                QObject* parent);

        // Load
        template<typename Impl>
        MappingLayerModel(
                Deserializer<Impl>& vis,
                MappingModel& model,
                QObject* parent) :
            Process::LayerModel {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        Process::LayerModelPanelProxy* make_panelProxy(QObject* parent) const override;
        void serialize(const VisitorVariant&) const override;

        const MappingModel& model() const;
};
}
