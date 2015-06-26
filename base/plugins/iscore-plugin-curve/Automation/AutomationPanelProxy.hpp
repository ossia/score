#pragma once
#include <ProcessInterface/LayerModelPanelProxy.hpp>
#include "AutomationViewModel.hpp"
class AutomationPanelProxy : public LayerModelPanelProxy
{
    public:
        AutomationPanelProxy(const AutomationViewModel& vm,
                             QObject* parent);

        const AutomationViewModel& viewModel() override;

    private:
        const AutomationViewModel& m_viewModel;
};
