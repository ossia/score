#pragma once
#include <Process/LayerModelPanelProxy.hpp>

#include "TemporalScenarioLayerModel.hpp"

class QObject;

namespace Scenario
{
class TemporalScenarioPanelProxy final :
        public Process::GraphicsViewLayerModelPanelProxy
{
    public:
        TemporalScenarioPanelProxy(
                const TemporalScenarioLayerModel& lm,
                QObject* parent);
};
}
