#include "Autom3DPanelProxy.hpp"
#include <Autom3D/Panel/AutomWidget.hpp>


namespace Autom3D
{
PanelProxy::PanelProxy(
        const Process::LayerModel& vm,
        QObject* parent):
    Process::LayerModelPanelProxy{parent},
    m_layer{vm}
{
    m_widget = new AutomWidget;
}

const Process::LayerModel& PanelProxy::layer()
{
    return m_layer;
}

QWidget* PanelProxy::widget() const
{
    return m_widget;
}

}
