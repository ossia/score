#include "EffectLayer.hpp"

#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Process.hpp>
#include <Process/Style/Pixmaps.hpp>

#include <score/graphics/GraphicWidgets.hpp>

#include <QMenu>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Process::EffectLayerPresenter)
namespace Process
{

EffectLayerView::EffectLayerView(QGraphicsItem* parent)
    : Process::LayerView{parent}
{
}

EffectLayerView::~EffectLayerView() {}

void EffectLayerView::paint_impl(QPainter*) const {}

void EffectLayerView::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  if (ev->button() == Qt::RightButton)
  {
    askContextMenu(ev->screenPos(), ev->scenePos());
  }
  else
  {
    pressed(ev->scenePos());
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
  askContextMenu(ev->screenPos(), ev->scenePos());
  ev->accept();
}

EffectLayerPresenter::EffectLayerPresenter(
    const ProcessModel& model,
    Process::LayerView* view,
    const Context& ctx,
    QObject* parent)
    : LayerPresenter{model, view, ctx, parent}, m_view{view}
{
  putToFront();
  connect(view, &Process::LayerView::pressed, this, [&] {
    m_context.context.focusDispatcher.focus(this);
  });
  connect(
      m_view,
      &Process::LayerView::askContextMenu,
      this,
      &Process::LayerPresenter::contextMenuRequested);
}
EffectLayerPresenter::~EffectLayerPresenter() {}

void EffectLayerPresenter::setWidth(qreal val, qreal defaultWidth)
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

void EffectLayerPresenter::on_zoomRatioChanged(ZoomRatio) {}

void EffectLayerPresenter::parentGeometryChanged() {}

void EffectLayerPresenter::fillContextMenu(
    QMenu& menu,
    QPoint pos,
    QPointF scenepos,
    const LayerContextMenuManager& mgr)
{
}

void setupExternalUI(
    const Process::ProcessModel& proc,
    const Process::LayerFactory& fact,
    const score::DocumentContext& ctx,
    bool show)
{
  if (show)
  {
    if (proc.externalUI)
      return;

    if (auto win = fact.makeExternalUI(proc, ctx, nullptr))
    {
      const_cast<QWidget*&>(proc.externalUI) = win;
      win->show();
    }
  }
  else
  {
    if (auto win = proc.externalUI)
    {
      win->close();
      delete win;
      const_cast<QWidget*&>(proc.externalUI) = nullptr;
    }
  }
}

void setupExternalUI(
    const Process::ProcessModel& proc,
    const score::DocumentContext& ctx,
    bool show)
{
  auto& facts = ctx.app.interfaces<Process::LayerFactoryList>();

  auto fact = facts.findDefaultFactory(proc);
  if (!fact || !fact->hasExternalUI(proc, ctx))
    return;

  setupExternalUI(proc, *fact, ctx, show);
}

QGraphicsItem* makeExternalUIButton(
    const ProcessModel& effect,
    const score::DocumentContext& context,
    QObject* self,
    QGraphicsItem* root)
{
  auto& pixmaps = Process::Pixmaps::instance();
  auto& facts = context.app.interfaces<Process::LayerFactoryList>();
  auto fact = facts.findDefaultFactory(effect);
  if (fact && fact->hasExternalUI(effect, context))
  {
    auto ui_btn = new score::QGraphicsPixmapToggle{
        pixmaps.show_ui_on, pixmaps.show_ui_off, root};
    QObject::connect(
        ui_btn,
        &score::QGraphicsPixmapToggle::toggled,
        self,
        [=, &effect, &context](bool b) {
          Process::setupExternalUI(effect, *fact, context, b);
        });

    if (effect.externalUI)
      ui_btn->setState(true);
    QObject::connect(
        &effect,
        &Process::ProcessModel::externalUIVisible,
        ui_btn,
        [=](bool v) { ui_btn->setState(v); });
    return ui_btn;
  }
  return nullptr;
}
}
