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
#include <iscore_lib_process_export.h>

namespace Dummy
{
class ISCORE_LIB_PROCESS_EXPORT Layer final : public Process::LayerModel
{
        ISCORE_SERIALIZE_FRIENDS(Layer, DataStream)
        ISCORE_SERIALIZE_FRIENDS(Layer, JSONObject)

    public:
        explicit Layer(
                Process::ProcessModel& model,
                const Id<LayerModel>& id,
                QObject* parent);

        // Copy
        explicit Layer(
                const Layer& source,
                Process::ProcessModel& model,
                const Id<LayerModel>& id,
                QObject* parent);

        // Load
        template<typename Impl>
        explicit Layer(
                Deserializer<Impl>& vis,
                Process::ProcessModel& model,
                QObject* parent) :
            LayerModel{vis, model, parent}
        {
            vis.writeTo(*this);
        }

        void serialize_impl(const VisitorVariant&) const override;
};
}
