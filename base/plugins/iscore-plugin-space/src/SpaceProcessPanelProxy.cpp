#include "SpaceProcessPanelProxy.hpp"

#include <src/SpaceProcessProxyLayerModel.hpp>

namespace Space
{
ProcessPanelProxy::ProcessPanelProxy(
        ProcessProxyLayerModel* vm,
        QObject *parent):
    GraphicsViewLayerModelPanelProxy{*vm, parent},
    m_layerImpl{vm}
{
    m_layerImpl->setParent(this);
}

}
