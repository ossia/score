#include "LayerModel.hpp"

LayerModel::~LayerModel()
{

}

Process& LayerModel::processModel() const
{ return m_sharedProcessModel; }


LayerModel::LayerModel(
        const Id<LayerModel>& viewModelId,
        const QString& name,
        Process& sharedProcess,
        QObject* parent) :
    IdentifiedObject<LayerModel> {viewModelId, name, parent},
    m_sharedProcessModel {sharedProcess}
{

}
