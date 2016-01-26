#include "MappingLayerModel.hpp"
#include "MappingModel.hpp"
#include "MappingPanelProxy.hpp"
#include <Process/LayerModel.hpp>

class LayerModelPanelProxy;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Mapping
{
LayerModel::LayerModel(
        ProcessModel& model,
        const Id<Process::LayerModel>& id,
        QObject* parent) :
    Process::LayerModel {id,
                Metadata<ObjectKey_k, LayerModel>::get(),
                model,
                parent}
{

}

LayerModel::LayerModel(
        const LayerModel& source,
        ProcessModel& model,
        const Id<Process::LayerModel>& id,
        QObject* parent) :
    Process::LayerModel {id,
                Metadata<ObjectKey_k, LayerModel>::get(),
                model,
                parent}
{
    // Nothing to copy
}

Process::LayerModelPanelProxy* LayerModel::make_panelProxy(
        QObject* parent) const
{
    return new MappingPanelProxy{*this, parent};
}

void LayerModel::serialize(const VisitorVariant&) const
{
    // Nothing to save
}

const ProcessModel& LayerModel::model() const
{
    return static_cast<const ProcessModel&>(processModel());
}
}
