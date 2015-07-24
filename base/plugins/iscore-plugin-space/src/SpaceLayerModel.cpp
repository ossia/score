#include "SpaceLayerModel.hpp"
#include "SpaceProcess.hpp"

SpaceLayerModel::SpaceLayerModel(
        const id_type<LayerModel> & id,
        SpaceProcess & proc,
        QObject *parent):
    LayerModel{id, "ScenarioLayerModel", proc, parent}
{

}

void SpaceLayerModel::serialize(const VisitorVariant &) const
{
    ISCORE_TODO;
}

LayerModelPanelProxy *SpaceLayerModel::make_panelProxy(QObject *parent) const
{
    return nullptr;
}
