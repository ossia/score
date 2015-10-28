#include "DummyLayerPresenter.hpp"
#include <ProcessInterface/Process.hpp>
#include <ProcessInterface/LayerModel.hpp>

DummyLayerPresenter::DummyLayerPresenter(
        const LayerModel& model,
        QObject* parent):
    LayerPresenter{"DummyLayerPresenter", parent},
    m_layer{model}
{

}

void DummyLayerPresenter::setWidth(int)
{
}

void DummyLayerPresenter::setHeight(int)
{
}

void DummyLayerPresenter::putToFront()
{
}

void DummyLayerPresenter::putBehind()
{
}

void DummyLayerPresenter::on_zoomRatioChanged(ZoomRatio)
{
}

void DummyLayerPresenter::parentGeometryChanged()
{
}

const LayerModel&DummyLayerPresenter::layerModel() const
{
    return m_layer;
}

const Id<Process>&DummyLayerPresenter::modelId() const
{
    return m_layer.processModel().id();
}

void DummyLayerPresenter::fillContextMenu(
        QMenu*,
        const QPoint&,
        const QPointF&) const
{
}
