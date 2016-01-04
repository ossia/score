#pragma once

#include <iscore/tools/Metadata.hpp>
#include <Process/LayerModel.hpp>

#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class QObject;

namespace Autom3D
{
class ProcessModel;
class LayerModel final : public Process::LayerModel
{
        ISCORE_METADATA(Autom3D::LayerModel)
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
