#include "SpaceProcessProxyLayerModel.hpp"
#include "SpaceLayerModel.hpp"
#include "SpaceProcessPanelProxy.hpp"

SpaceProcessProxyLayerModel::SpaceProcessProxyLayerModel(
        const Id<LayerModel>& id,
        const LayerModel &model,
        QObject *parent):
    LayerModel{id, staticMetaObject.className(), model.processModel(), parent},
    m_source{model}
{

}


void SpaceProcessProxyLayerModel::serialize(const VisitorVariant &) const
{
    ISCORE_TODO;
}

Process::LayerModelPanelProxy* SpaceProcessProxyLayerModel::make_panelProxy(QObject *parent) const
{
    return new SpaceProcessPanelProxy{m_source, parent};
}
