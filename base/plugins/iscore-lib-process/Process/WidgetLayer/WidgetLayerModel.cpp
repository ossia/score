#include <iscore/serialization/VisitorCommon.hpp>

#include "WidgetLayerModel.hpp"
#include "WidgetLayerPanelProxy.hpp"
#include <Process/LayerModel.hpp>

class LayerModelPanelProxy;
namespace Process { class ProcessModel; }
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace WidgetLayer
{
Layer::Layer(
        Process::ProcessModel& model,
        const Id<LayerModel>& id,
        QObject* parent):
    LayerModel{id, "WidgetLayerModel", model, parent}
{
}

Layer::Layer(
        const Layer&,
        Process::ProcessModel& model,
        const Id<LayerModel>& id,
        QObject* parent):
    LayerModel{id, "WidgetLayerModel", model, parent}
{

}

void Layer::serialize_impl(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

Process::LayerModelPanelProxy* Layer::make_panelProxy(QObject* parent) const
{
    return new LayerPanelProxy{*this, parent};
}
}
