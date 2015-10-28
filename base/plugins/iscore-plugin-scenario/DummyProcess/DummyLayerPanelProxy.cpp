#include "DummyLayerPanelProxy.hpp"

DummyLayerPanelProxy::DummyLayerPanelProxy(
        const LayerModel& vm,
        QObject* parent):
    LayerModelPanelProxy{parent},
    m_layer{vm}
{

}

const LayerModel& DummyLayerPanelProxy::layer()
{
    return m_layer;
}
