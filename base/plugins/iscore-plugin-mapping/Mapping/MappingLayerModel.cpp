#include "MappingLayerModel.hpp"
#include "MappingModel.hpp"
#include "MappingPanelProxy.hpp"
#include <Process/LayerModel.hpp>

class LayerModelPanelProxy;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

ISCORE_METADATA_IMPL(Mapping::MappingLayerModel)
namespace Mapping
{
MappingLayerModel::MappingLayerModel(
        MappingModel& model,
        const Id<LayerModel>& id,
        QObject* parent) :
    LayerModel {id,
                className.c_str(),
                model,
                parent}
{

}

MappingLayerModel::MappingLayerModel(
        const MappingLayerModel& source,
        MappingModel& model,
        const Id<LayerModel>& id,
        QObject* parent) :
    LayerModel {id,
                className.c_str(),
                model,
                parent}
{
    // Nothing to copy
}

Process::LayerModelPanelProxy* MappingLayerModel::make_panelProxy(
        QObject* parent) const
{
    return new MappingPanelProxy{*this, parent};
}

void MappingLayerModel::serialize(const VisitorVariant&) const
{
    // Nothing to save
}

const MappingModel& MappingLayerModel::model() const
{
    return static_cast<const MappingModel&>(processModel());
}
}
