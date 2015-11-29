#include <Process/LayerModelPanelProxy.hpp>
#include "Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp"
#include "TemporalScenarioPanelProxy.hpp"

class QObject;

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
