#include "SpaceLayerModel.hpp"
#include "SpaceProcess.hpp"
namespace Space
{
LayerModel::LayerModel(
        const Id<Process::LayerModel> & id,
        Space::ProcessModel & proc,
        QObject *parent):
    Process::LayerModel{id, staticMetaObject.className(), proc, parent}
{

}

void LayerModel::serialize(const VisitorVariant &) const
{
    ISCORE_TODO;
}

Process::LayerModelPanelProxy *LayerModel::make_panelProxy(QObject *parent) const
{
    ISCORE_TODO;
    return nullptr;
}

}
