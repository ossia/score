#include <iscore/serialization/VisitorCommon.hpp>

#include "DummyLayerModel.hpp"
#include "DummyLayerPanelProxy.hpp"
#include <Process/LayerModel.hpp>

class LayerModelPanelProxy;
namespace Process { class ProcessModel; }
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace Dummy
{
DummyLayerModel::DummyLayerModel(
        Process::ProcessModel& model,
        const Id<LayerModel>& id,
        QObject* parent):
    LayerModel{id, "DummyLayerModel", model, parent}
{
}

DummyLayerModel::DummyLayerModel(
        const DummyLayerModel&,
        Process::ProcessModel& model,
        const Id<LayerModel>& id,
        QObject* parent):
    LayerModel{id, "DummyLayerModel", model, parent}
{

}

void DummyLayerModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

Process::LayerModelPanelProxy* DummyLayerModel::make_panelProxy(QObject* parent) const
{
    return new DummyLayerPanelProxy{*this, parent};
}
}
