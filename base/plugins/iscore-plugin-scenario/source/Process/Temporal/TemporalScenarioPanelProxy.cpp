#include "TemporalScenarioPanelProxy.hpp"
#include "TemporalScenarioLayerModel.hpp"

TemporalScenarioPanelProxy::TemporalScenarioPanelProxy(
        const TemporalScenarioLayerModel& lm,
        QObject* parent):
    LayerModelPanelProxy{parent},
    m_viewModel{lm}
{

}

const TemporalScenarioLayerModel& TemporalScenarioPanelProxy::layer()
{
    return m_viewModel;
}
