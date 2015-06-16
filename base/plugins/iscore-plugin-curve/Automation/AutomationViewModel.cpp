#include "AutomationViewModel.hpp"
#include "AutomationModel.hpp"
#include "AutomationPanelProxy.hpp"

AutomationViewModel::AutomationViewModel(AutomationModel& model,
                                         const id_type<ProcessViewModel>& id,
                                         QObject* parent) :
    ProcessViewModel {id, "AutomationViewModel", model, parent}
{

}

AutomationViewModel::AutomationViewModel(const AutomationViewModel& source,
                                         AutomationModel& model,
                                         const id_type<ProcessViewModel>& id,
                                         QObject* parent) :
    ProcessViewModel {id, "AutomationViewModel", model, parent}
{
    // Nothing to copy
}

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
