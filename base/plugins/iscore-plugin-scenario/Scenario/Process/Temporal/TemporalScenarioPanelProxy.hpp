#pragma once
#include <Process/LayerModelPanelProxy.hpp>
#include "TemporalScenarioLayerModel.hpp"

class TemporalScenarioPanelProxy : public LayerModelPanelProxy
{
    public:
        TemporalScenarioPanelProxy(
                const TemporalScenarioLayerModel& lm,
                QObject* parent);

        const TemporalScenarioLayerModel& layer() override;

    private:
        const TemporalScenarioLayerModel& m_viewModel;
};
