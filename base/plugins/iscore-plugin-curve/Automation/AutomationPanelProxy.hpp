#pragma once
#include <ProcessInterface/LayerModelPanelProxy.hpp>
#include "AutomationLayerModel.hpp"
class AutomationPanelProxy : public LayerModelPanelProxy
{
    public:
        AutomationPanelProxy(const AutomationLayerModel& vm,
                             QObject* parent);

        const AutomationLayerModel& layer() override;

    private:
        const AutomationLayerModel& m_viewModel;
};
