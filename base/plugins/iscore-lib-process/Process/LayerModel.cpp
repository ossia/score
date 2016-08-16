#include "LayerModel.hpp"
#include <iscore/tools/IdentifiedObject.hpp>
#include <Process/LayerModelPanelProxy.hpp>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>
namespace Process
{
LayerModel::~LayerModel() = default;

ProcessModel& LayerModel::processModel() const
{ return m_sharedProcessModel; }


LayerModelPanelProxy*LayerModel::make_panelProxy(QObject* parent) const
{
    return new Process::GraphicsViewLayerModelPanelProxy{*this, parent};
}


LayerModel::LayerModel(
        const Id<LayerModel>& viewModelId,
        const QString& name,
        ProcessModel& sharedProcess,
        QObject* parent) :
    IdentifiedObject<LayerModel> {viewModelId, name, parent},
    m_sharedProcessModel {sharedProcess}
{

}
}
