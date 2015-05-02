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
        AutomationPanelProxy(AutomationViewModel* parent):
            ProcessViewModelPanelProxy{parent}
        {

        }

        AutomationViewModel* viewModel() override
        {
            return static_cast<AutomationViewModel*>(parent());
        }
};

ProcessViewModelPanelProxy* AutomationViewModel::make_panelProxy()
{
    return new AutomationPanelProxy{this};
}

void AutomationViewModel::serialize(const VisitorVariant&) const
{
    // Nothing to save
}

const AutomationModel& AutomationViewModel::model()
{
    return static_cast<const AutomationModel&>(sharedProcessModel());
}
