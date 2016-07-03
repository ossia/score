 #include "DummyLayerPanelProxy.hpp"
#include <Process/LayerModelPanelProxy.hpp>

namespace Process { class LayerModel; }
class QObject;

namespace Dummy
{
DummyLayerPanelProxy::DummyLayerPanelProxy(
        const Process::LayerModel& vm,
        QObject* parent):
    Process::LayerModelPanelProxy{parent},
    m_layer{vm}
{
    m_widget = new QWidget;
}

const Process::LayerModel& DummyLayerPanelProxy::layer()
{
    return m_layer;
}

QWidget* DummyLayerPanelProxy::widget() const
{
    return m_widget;
}
}
