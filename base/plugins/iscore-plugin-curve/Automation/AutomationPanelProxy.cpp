#include "AutomationPanelProxy.hpp"

AutomationPanelProxy::AutomationPanelProxy(const AutomationViewModel &vm, QObject *parent):
    LayerModelPanelProxy{parent},
    m_viewModel{vm}
{

}

const AutomationViewModel &AutomationPanelProxy::layer()
{
    return m_viewModel;
}
