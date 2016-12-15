#include "LayerModel.hpp"
#include <Process/LayerModelPanelProxy.hpp>
#include <iscore/model/IdentifiedObject.hpp>

class QObject;
#include <iscore/model/Identifier.hpp>
namespace Process
{
LayerModel::~LayerModel()
{
  emit identified_object_destroying(this);
}

ProcessModel& LayerModel::processModel() const
{
  return m_sharedProcessModel;
}

LayerModel::LayerModel(
    const Id<LayerModel>& viewModelId,
    const QString& name,
    ProcessModel& sharedProcess,
    QObject* parent)
    : IdentifiedObject<LayerModel>{viewModelId, name, parent}
    , m_sharedProcessModel{sharedProcess}
{
}
}
