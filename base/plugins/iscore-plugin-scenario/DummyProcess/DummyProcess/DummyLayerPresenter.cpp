#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>

#include "DummyLayerPresenter.hpp"
#include "DummyLayerView.hpp"
#include <Process/LayerPresenter.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/document/DocumentContext.hpp>
class QMenu;
class QObject;


namespace Dummy
{
DummyLayerPresenter::DummyLayerPresenter(
        const Process::LayerModel& model,
        DummyLayerView* view,
        const Process::ProcessPresenterContext& ctx,
        QObject* parent):
    LayerPresenter{ctx, parent},
    m_layer{model},
    m_view{view}
{
    putToFront();
    connect(view, &DummyLayerView::pressed,
            this, [&] () {
        m_context.context.focusDispatcher.focus(this);
    });
    con(model.processModel().metadata, &ModelMetadata::nameChanged,
            this, [&] (QString s)
    {
        putToFront();
    });
}

void DummyLayerPresenter::setWidth(qreal val)
{
    m_view->setWidth(val);
}

void DummyLayerPresenter::setHeight(qreal val)
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

const Process::LayerModel& DummyLayerPresenter::layerModel() const
{
    return m_layer;
}

const Id<Process::ProcessModel>& DummyLayerPresenter::modelId() const
{
    return m_layer.processModel().id();
}

void DummyLayerPresenter::fillContextMenu(
        QMenu*,
        const QPoint&,
        const QPointF&) const
{
}
}
