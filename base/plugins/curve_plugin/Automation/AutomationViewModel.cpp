#include "AutomationViewModel.hpp"
#include "AutomationModel.hpp"
#include <ProcessInterface/ProcessViewModelPanelProxy.hpp>

AutomationViewModel::AutomationViewModel(AutomationModel& model,
                                         const id_type<ProcessViewModelInterface>& id,
                                         QObject* parent) :
    ProcessViewModelInterface {id, "AutomationViewModel", model, parent}
{

}

AutomationViewModel::AutomationViewModel(const AutomationViewModel& source,
                                         AutomationModel& model,
                                         const id_type<ProcessViewModelInterface>& id,
                                         QObject* parent) :
    ProcessViewModelInterface {id, "AutomationViewModel", model, parent}
{
    // Nothing to copy
}

// TODO Move somewhere else.
class AutomationPanelProxy : public ProcessViewModelPanelProxy
{
    public:
        AutomationPanelProxy(const AutomationViewModel& vm,
                             QObject* parent):
            ProcessViewModelPanelProxy{parent},
            m_viewModel{vm}
        {

        }

        const AutomationViewModel& viewModel() override
        {
            return m_viewModel;
        }

    private:
        const AutomationViewModel& m_viewModel;
};

ProcessViewModelPanelProxy* AutomationViewModel::make_panelProxy(QObject* parent) const
{
    return new AutomationPanelProxy{*this, parent};
}

void AutomationViewModel::serialize(const VisitorVariant&) const
{
    // Nothing to save
}

const AutomationModel& AutomationViewModel::model() const
{
    return static_cast<const AutomationModel&>(sharedProcessModel());
}
