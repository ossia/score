#include "SpaceProcessProxyLayerModel.hpp"
#include "SpaceLayerModel.hpp"
#include "SpaceProcessPanelProxy.hpp"

namespace Space
{

ProcessProxyLayerModel::ProcessProxyLayerModel(
        const Id<LayerModel>& id,
        const LayerModel &model,
        QObject *parent):
    LayerModel{id, staticMetaObject.className(), model.processModel(), parent},
    m_source{model}
{

}


void ProcessProxyLayerModel::serialize(const VisitorVariant &) const
{
    ISCORE_TODO;
}

Process::LayerModelPanelProxy* ProcessProxyLayerModel::make_panelProxy(QObject *parent) const
{
    auto lm = new ProcessProxyLayerModel(Id<Process::LayerModel>(), *this, nullptr);
    return new ProcessPanelProxy{lm, parent};
}

}
