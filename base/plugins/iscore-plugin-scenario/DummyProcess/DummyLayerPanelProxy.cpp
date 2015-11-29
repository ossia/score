#include "DummyLayerPanelProxy.hpp"
#include "Process/LayerModelPanelProxy.hpp"

class LayerModel;
class QObject;

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
