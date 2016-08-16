#include "AutomationLayerModel.hpp"
#include "AutomationModel.hpp"
#include <Curve/Process/CurveProcessPanelProxy.hpp>
#include <Process/LayerModel.hpp>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

// TODO refactor with mapping ?
namespace Automation
{
Layer::Layer(
        ProcessModel& model,
        const Id<Process::LayerModel>& id,
        QObject* parent) :
    Process::LayerModel {
        id,
        Metadata<ObjectKey_k, Layer>::get(),
        model,
        parent}
{

}

Layer::Layer(
        const Layer& source,
        ProcessModel& model,
        const Id<Process::LayerModel>& id,
        QObject* parent) :
    Process::LayerModel {
        id,
        Metadata<ObjectKey_k, Layer>::get(),
        model,
        parent}
{
    // Nothing to copy
}

void Layer::serialize_impl(
        const VisitorVariant&) const
{
    // Nothing to save
}

const ProcessModel& Layer::model() const
{
    return static_cast<const ProcessModel&>(processModel());
}
}
