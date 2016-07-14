 #include "WidgetLayerPanelProxy.hpp"
#include <Process/LayerModelPanelProxy.hpp>

namespace Process { class LayerModel; }
class QObject;

namespace WidgetLayer
{
LayerPanelProxy::LayerPanelProxy(
        const Process::LayerModel& vm,
        QObject* parent):
    Process::LayerModelPanelProxy{parent},
    m_layer{vm}
{
    m_widget = new QWidget;
}

const Process::LayerModel& LayerPanelProxy::layer()
{
    return m_layer;
}

QWidget* LayerPanelProxy::widget() const
{
    return m_widget;
}
}
