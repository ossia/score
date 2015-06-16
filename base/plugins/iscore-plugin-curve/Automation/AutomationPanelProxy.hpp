#pragma once
#include <ProcessInterface/ProcessViewModelPanelProxy.hpp>
#include "AutomationViewModel.hpp"
class AutomationPanelProxy : public ProcessViewModelPanelProxy
{
    public:
        AutomationPanelProxy(const AutomationViewModel& vm,
                             QObject* parent);

        const AutomationViewModel& viewModel() override;

    private:
        const AutomationViewModel& m_viewModel;
};
