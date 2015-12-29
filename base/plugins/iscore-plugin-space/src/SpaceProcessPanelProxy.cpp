#include "SpaceProcessPanelProxy.hpp"

SpaceProcessPanelProxy::SpaceProcessPanelProxy(
        const Process::LayerModel &vm,
        QObject *parent):
    LayerModelPanelProxy{parent}
{
    m_layer = new SpaceProcessProxyLayerModel(Id<Process::LayerModel>(), vm, this);
}

const SpaceProcessProxyLayerModel& SpaceProcessPanelProxy::layer()
{
    return *m_layer;
}
