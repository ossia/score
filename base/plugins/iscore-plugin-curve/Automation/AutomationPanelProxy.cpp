#include "AutomationPanelProxy.hpp"

AutomationPanelProxy::AutomationPanelProxy(const AutomationLayerModel& vm, QObject *parent):
    LayerModelPanelProxy{parent},
    m_viewModel{vm}
{

}

const AutomationLayerModel &AutomationPanelProxy::layer()
{
    return m_viewModel;
}
