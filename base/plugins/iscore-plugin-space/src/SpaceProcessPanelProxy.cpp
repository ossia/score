#include "SpaceProcessPanelProxy.hpp"

#include <src/SpaceProcessProxyLayerModel.hpp>

namespace Space
{
ProcessPanelProxy::ProcessPanelProxy(
        const Process::LayerModel &vm,
        QObject *parent):
    LayerModelPanelProxy{parent}
{
    m_layer = new ProcessProxyLayerModel(Id<Process::LayerModel>(), vm, this);
}

const Process::LayerModel& ProcessPanelProxy::layer()
{
    return *m_layer;
}
}
