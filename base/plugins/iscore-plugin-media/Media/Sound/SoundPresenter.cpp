#include "SoundPresenter.hpp"
#include "SoundView.hpp"
#include <iscore/document/DocumentContext.hpp>
namespace Media
{
namespace Sound
{
LayerPresenter::LayerPresenter(
        const ProcessModel& layer,
        LayerView* view,
        const Process::ProcessPresenterContext& ctx,
        QObject* parent):
    Process::LayerPresenter{ctx, parent},
    m_layer{layer},
    m_view{view}
{
    connect(view, &LayerView::pressed,
            this, [&] () {
        m_context.context.focusDispatcher.focus(this);
    });

    con(layer, &ProcessModel::fileChanged,
        this, [&] () {
        m_view->setData(m_layer.file());
        m_view->recompute(m_layer.duration(), m_ratio);
    });

    m_view->setData(m_layer.file());
    m_view->recompute(m_layer.duration(), m_ratio);

    connect(
          m_view, &LayerView::askContextMenu, this,
          &LayerPresenter::contextMenuRequested);
}

void LayerPresenter::setWidth(qreal val)
{
    m_view->setWidth(val);
    m_view->recompute(m_layer.duration(), m_ratio);
}

void LayerPresenter::setHeight(qreal val)
{
    m_view->setHeight(val);
    m_view->recompute(m_layer.duration(), m_ratio);
}

void LayerPresenter::putToFront()
{
    m_view->recompute(m_layer.duration(), m_ratio);
    m_view->show();
}

void LayerPresenter::putBehind()
{
    m_view->hide();
}

void LayerPresenter::on_zoomRatioChanged(ZoomRatio r)
{
    m_ratio = r;
    m_view->recompute(m_layer.duration(), m_ratio);
}

void LayerPresenter::parentGeometryChanged()
{
    m_view->recompute(m_layer.duration(), m_ratio);
}

const ProcessModel& LayerPresenter::model() const
{
    return m_layer;
}

const Id<Process::ProcessModel>& LayerPresenter::modelId() const
{
    return m_layer.id();
}
}
}
