#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>

#include "DummyLayerPresenter.hpp"
#include "DummyLayerView.hpp"
#include <Process/LayerPresenter.hpp>

class QMenu;
class QObject;


DummyLayerPresenter::DummyLayerPresenter(
        const LayerModel& model,
        DummyLayerView* view,
        QObject* parent):
    LayerPresenter{"DummyLayerPresenter", parent},
    m_layer{model},
    m_view{view}
{
    putToFront();

}

void DummyLayerPresenter::setWidth(int val)
{
    m_view->setWidth(val);
}

void DummyLayerPresenter::setHeight(int val)
{
    m_view->setHeight(val);
}

void DummyLayerPresenter::putToFront()
{
    m_view->setText(m_layer.processModel().prettyName());
}

void DummyLayerPresenter::putBehind()
{
    m_view->setText({});
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
