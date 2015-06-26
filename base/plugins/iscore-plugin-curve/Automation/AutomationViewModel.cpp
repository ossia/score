#include "AutomationViewModel.hpp"
#include "AutomationModel.hpp"
#include "AutomationPanelProxy.hpp"

AutomationViewModel::AutomationViewModel(AutomationModel& model,
                                         const id_type<LayerModel>& id,
                                         QObject* parent) :
    LayerModel {id, "AutomationViewModel", model, parent}
{

}

AutomationViewModel::AutomationViewModel(const AutomationViewModel& source,
                                         AutomationModel& model,
                                         const id_type<LayerModel>& id,
                                         QObject* parent) :
    LayerModel {id, "AutomationViewModel", model, parent}
{
    // Nothing to copy
}

LayerModelPanelProxy* AutomationViewModel::make_panelProxy(QObject* parent) const
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
