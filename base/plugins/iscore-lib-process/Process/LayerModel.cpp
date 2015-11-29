#include "LayerModel.hpp"
#include <iscore/tools/IdentifiedObject.hpp>

class QObject;
template <typename tag, typename impl> class id_base_t;

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
