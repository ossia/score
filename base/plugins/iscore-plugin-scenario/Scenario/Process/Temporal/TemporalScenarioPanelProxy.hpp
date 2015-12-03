#pragma once
#include <Process/LayerModelPanelProxy.hpp>

#include "TemporalScenarioLayerModel.hpp"

class QObject;

class TemporalScenarioPanelProxy final : public LayerModelPanelProxy
{
    public:
        TemporalScenarioPanelProxy(
                const TemporalScenarioLayerModel& lm,
                QObject* parent);

        const TemporalScenarioLayerModel& layer() override;

    private:
        const TemporalScenarioLayerModel& m_viewModel;
};
