#pragma once
#include <ProcessInterface/LayerModelPanelProxy.hpp>
#include "TemporalScenarioLayer.hpp"

class TemporalScenarioPanelProxy : public LayerModelPanelProxy
{
    public:
        TemporalScenarioPanelProxy(
                const TemporalScenarioLayer& lm,
                QObject* parent);

        const TemporalScenarioLayer& layer() override;

    private:
        const TemporalScenarioLayer& m_viewModel;
};
