#include <iscore/serialization/VisitorCommon.hpp>

#include "DummyLayerModel.hpp"
#include "DummyLayerPanelProxy.hpp"
#include <Process/LayerModel.hpp>

class LayerModelPanelProxy;
namespace Process { class ProcessModel; }
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Dummy
{
Layer::Layer(
        Process::ProcessModel& model,
        const Id<LayerModel>& id,
        QObject* parent):
    LayerModel{id, "DummyLayerModel", model, parent}
{
}

Layer::Layer(
        const Layer&,
        Process::ProcessModel& model,
        const Id<LayerModel>& id,
        QObject* parent):
    LayerModel{id, "DummyLayerModel", model, parent}
{

}

void Layer::serialize_impl(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}
}
