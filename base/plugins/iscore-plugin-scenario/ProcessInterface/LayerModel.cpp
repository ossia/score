#include "LayerModel.hpp"

Process&LayerModel::sharedProcessModel() const
{ return m_sharedProcessModel; }


LayerModel::LayerModel(
        const id_type<LayerModel>& viewModelId,
        const QString& name,
        Process& sharedProcess,
        QObject* parent) :
    IdentifiedObject<LayerModel> {viewModelId, name, parent},
    m_sharedProcessModel {sharedProcess}
{

}
