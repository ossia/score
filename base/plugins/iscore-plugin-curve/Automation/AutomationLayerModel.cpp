#include "AutomationLayerModel.hpp"
#include "AutomationModel.hpp"
#include "AutomationPanelProxy.hpp"

AutomationLayerModel::AutomationLayerModel(AutomationModel& model,
                                         const Id<LayerModel>& id,
                                         QObject* parent) :
    LayerModel {id, AutomationLayerModel::staticMetaObject.className(), model, parent}
{

}

AutomationLayerModel::AutomationLayerModel(const AutomationLayerModel& source,
                                         AutomationModel& model,
                                         const Id<LayerModel>& id,
                                         QObject* parent) :
    LayerModel {id, AutomationLayerModel::staticMetaObject.className(), model, parent}
{
    // Nothing to copy
}

LayerModelPanelProxy* AutomationLayerModel::make_panelProxy(QObject* parent) const
{
    return new AutomationPanelProxy{*this, parent};
}

void AutomationLayerModel::serialize(const VisitorVariant&) const
{
    // Nothing to save
}

const AutomationModel& AutomationLayerModel::model() const
{
    return static_cast<const AutomationModel&>(processModel());
}
