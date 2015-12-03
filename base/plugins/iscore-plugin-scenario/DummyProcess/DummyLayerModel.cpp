#include <iscore/serialization/VisitorCommon.hpp>

#include "DummyLayerModel.hpp"
#include "DummyLayerPanelProxy.hpp"
#include <Process/LayerModel.hpp>

class LayerModelPanelProxy;
class Process;
class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

DummyLayerModel::DummyLayerModel(
        Process& model,
        const Id<LayerModel>& id,
        QObject* parent):
    LayerModel{id, "DummyLayerModel", model, parent}
{
}

DummyLayerModel::DummyLayerModel(
        const DummyLayerModel&,
        Process& model,
        const Id<LayerModel>& id,
        QObject* parent):
    LayerModel{id, "DummyLayerModel", model, parent}
{

}

void DummyLayerModel::serialize(const VisitorVariant& vis) const
{
    serialize_dyn(vis, *this);
}

LayerModelPanelProxy* DummyLayerModel::make_panelProxy(QObject* parent) const
{
    return new DummyLayerPanelProxy{*this, parent};
}
