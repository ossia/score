#include "TemporalScenarioPanelProxy.hpp"
#include "TemporalScenarioLayer.hpp"

TemporalScenarioPanelProxy::TemporalScenarioPanelProxy(
        const TemporalScenarioLayer& lm,
        QObject* parent):
    LayerModelPanelProxy{parent},
    m_viewModel{lm}
{

}

const TemporalScenarioLayer& TemporalScenarioPanelProxy::layer()
{
    return m_viewModel;
}
