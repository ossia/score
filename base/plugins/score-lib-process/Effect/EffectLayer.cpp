#include "EffectLayer.hpp"
#include <QGraphicsSceneEvent>
#include <Process/Process.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <QMenu>
#include <QWindow>

namespace Process
{

EffectLayerView::EffectLayerView(QGraphicsItem* parent)
  : Process::LayerView{parent}
{

}

EffectLayerView::~EffectLayerView()
{

}

void EffectLayerView::paint_impl(QPainter*) const
{

}

void EffectLayerView::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  if(ev->button() == Qt::RightButton)
  {
    emit askContextMenu(ev->screenPos(), ev->scenePos());
  }
  else
  {
    emit pressed(ev->scenePos());
  }
  ev->accept();
}

void EffectLayerView::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
}

void EffectLayerView::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
}

void EffectLayerView::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
  emit askContextMenu(ev->screenPos(), ev->scenePos());
  ev->accept();
}

EffectLayerPresenter::EffectLayerPresenter(const ProcessModel& model, EffectLayerView* view, const ProcessPresenterContext& ctx, QObject* parent)
  : LayerPresenter{ctx, parent}, m_layer{model}, m_view{view}
{
  m_showUI = new QAction{tr("Show")};
  m_showUI->setCheckable(true);
  putToFront();
  connect(view, &Process::LayerView::pressed, this, [&] {
    m_context.context.focusDispatcher.focus(this);
  });

  connect(
        m_view, &Process::LayerView::askContextMenu, this,
        &Process::LayerPresenter::contextMenuRequested);
}
EffectLayerPresenter::~EffectLayerPresenter()
{

}

void EffectLayerPresenter::setWidth(qreal val)
{
  m_view->setWidth(val);
}

void EffectLayerPresenter::setHeight(qreal val)
{
  m_view->setHeight(val);
}

void EffectLayerPresenter::putToFront()
{
  m_view->setVisible(true);
}

void EffectLayerPresenter::putBehind()
{
  m_view->setVisible(false);
}

void EffectLayerPresenter::on_zoomRatioChanged(ZoomRatio)
{
}

void EffectLayerPresenter::parentGeometryChanged()
{
}

const ProcessModel&EffectLayerPresenter::model() const
{
  return m_layer;
}

const Id<ProcessModel>&EffectLayerPresenter::modelId() const
{
  return m_layer.id();
}

void EffectLayerPresenter::fillContextMenu(
    QMenu& menu,
    QPoint pos,
    QPointF scenepos,
    const LayerContextMenuManager& mgr)
{
  menu.addAction(m_showUI);
  m_showUI->setCheckable(true);
  connect(m_showUI, &QAction::triggered,
          this, [=] {
    auto b = m_showUI->isChecked();
    if(b)
    {
      auto& facts = context().context.processList;
      if(auto fact = facts.findDefaultFactory(m_layer))
      {
        if(auto win = fact->makeExternalUI(m_layer, context().context, nullptr))
        {
          win->show();
          auto c0 = connect(win->windowHandle(), &QWindow::visibilityChanged, m_showUI, [=] (auto vis) {
            if(vis == QWindow::Hidden)
            {
              m_showUI->toggle();
              win->deleteLater();
            }
          });

          connect(m_showUI, &QAction::toggled,
                  win, [=] (bool b) {
            QObject::disconnect(c0);
            win->close();
            delete win;
          });
        }
      }
    }

  });

}

}
