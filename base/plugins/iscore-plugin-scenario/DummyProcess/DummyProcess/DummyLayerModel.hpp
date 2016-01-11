#pragma once
#include <Process/LayerModel.hpp>

#include <iscore/serialization/VisitorInterface.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
class DataStream;
class JSONObject;
namespace Process {
class ProcessModel;
class LayerModelPanelProxy;
}
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_lib_dummyprocess_export.h>

class ISCORE_LIB_DUMMYPROCESS_EXPORT DummyLayerModel final : public Process::LayerModel
{
        ISCORE_SERIALIZE_FRIENDS(DummyLayerModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(DummyLayerModel, JSONObject)

    public:
        explicit DummyLayerModel(
                Process::ProcessModel& model,
                const Id<LayerModel>& id,
                QObject* parent);

        // Copy
        explicit DummyLayerModel(
                const DummyLayerModel& source,
                Process::ProcessModel& model,
                const Id<LayerModel>& id,
                QObject* parent);

        // Load
        template<typename Impl>
        explicit DummyLayerModel(
                Deserializer<Impl>& vis,
                Process::ProcessModel& model,
                QObject* parent) :
            LayerModel {vis, model, parent}
        {
            vis.writeTo(*this);
        }

        void serialize(const VisitorVariant&) const override;
        Process::LayerModelPanelProxy* make_panelProxy(QObject* parent) const override;
};
