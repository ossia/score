#include "DummyLayerPanelProxy.hpp"
#include <Process/LayerModelPanelProxy.hpp>

namespace Process { class LayerModel; }
class QObject;

DummyLayerPanelProxy::DummyLayerPanelProxy(
        const Process::LayerModel& vm,
        QObject* parent):
    LayerModelPanelProxy{parent},
    m_layer{vm}
{

}

const Process::LayerModel& DummyLayerPanelProxy::layer()
{
    return m_layer;
}
