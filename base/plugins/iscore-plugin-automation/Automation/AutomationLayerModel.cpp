

#include "AutomationLayerModel.hpp"
#include "AutomationModel.hpp"
#include "AutomationPanelProxy.hpp"
#include <Process/LayerModel.hpp>

class LayerModelPanelProxy;
class QObject;
template <typename tag, typename impl> class id_base_t;

// TODO refactor with mapping ?
constexpr const char AutomationLayerModel::className[];
AutomationLayerModel::AutomationLayerModel(AutomationModel& model,
                                         const Id<LayerModel>& id,
                                         QObject* parent) :
    LayerModel {id, AutomationLayerModel::className, model, parent}
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
