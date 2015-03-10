#include "AutomationViewModel.hpp"
#include "AutomationModel.hpp"
#include <ProcessInterface/ProcessViewModelPanelProxy.hpp>

AutomationViewModel::AutomationViewModel(AutomationModel* model,
        id_type<ProcessViewModelInterface> id,
        QObject* parent) :
    ProcessViewModelInterface {id, "AutomationViewModel", model, parent},
m_model {model}
{

}

AutomationViewModel::AutomationViewModel(const AutomationViewModel* source,
        AutomationModel* model,
        id_type<ProcessViewModelInterface> id,
        QObject* parent) :
    ProcessViewModelInterface {id, "AutomationViewModel", model, parent},
m_model {model}
{
    // Nothing to copy
}

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

void AutomationViewModel::serialize(SerializationIdentifier identifier, void* data) const
{
}

AutomationModel* AutomationViewModel::model()
{
    return m_model;
}
